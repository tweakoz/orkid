#!/usr/bin/env ork.python
################################################################################
# COMFORT-1 — IBL refilter SLICE COST gate.
#
# The owner's VR verdict on the procedural sky feed was "sun update FAR too slow
# + frame glitches". The glitch is structural: a refilter slice runs inside
# beginFrame and BLOCKS the render thread on its own fence (that is what makes a
# slice's CPU wall equal to its GPU cost), so a slice that costs more than a
# frame IS a dropped frame. This gate measures that number and holds the line on
# it:
#
#   a) COST      the worst slice of a steady-state (warm) refilter cycle stays
#                under the budget a frame can absorb. A cycle's slices are two
#                cost populations and (since COMFORT-2) two registry KEYS:
#                  RadiancePrefilter:WxH:S/D  the filter steps — per-level tile
#                     batches and per-level readbacks, GPU work + fence wait
#                  RadiancePrefilterPkg:WxH:N the package steps — per-level XIR
#                     serialize, container assemble/decode, per-level upload mip
#                     chain, and the publish (both upload issues + the swap)
#                Both are scored: the filter population on second_worst_ms
#                (recurring), the package population on worst_ms — which used to
#                be ONE ~30ms step and is now the tightened line below.
#   b) PERSIST   the scheduler's learned per-slice estimate SURVIVES the cycle
#                that learned it — cycle N+1's task instance is seeded from the
#                MicrotaskCostRegistry rather than starting over, which is what
#                lets the est>budget gate defer the heavy slices at all. Scored
#                as "cycle 2 was seeded with what cycle 1 ended up believing",
#                per key. NOT as "the scale exceeds 1x": with COMFORT-2's
#                per-step granularity the slices are now CHEAPER than their
#                seed, so the honest learned scale sits at the 1x floor and a
#                >1x assertion would measure the estimate being too small.
#   c) DEFER     ... and deferral never DROPS: the cycle still completes, within
#                a bounded number of frames, deferrals and escapes included.
#
# Both cycles are measured, and cycle 2 is the SCORED one: cycle 1 pays a
# one-time cold-JIT cost (a first filter-material build measured ~650ms on this
# machine) that no persistent estimate should — or does — carry forward.
#
# KNOBS (so the same file produces both rows of a before/after table):
#   ORKID_IBL_GATE_SNAP=WxH           snapshot extent  (default: engine default)
#   ORKID_IBL_GATE_SAMPLES=spec      specular filter samples (default: engine default)
#   ORKID_IBL_GATE_MAX_SLICE_MS=f     recurring-slice ceiling (default 5.0)
#   ORKID_IBL_GATE_MAX_OUTLIER_MS=f   package-step ceiling    (default 40.0)
#   ORKID_MT_FORCE_REALTIME=1        (engine) budget this offscreen context as if
#                                    it presented frames — exercises the deferral
#                                    path a WINDOW context lives under.
#
# Not ork.testing capture_app: no capture is involved, and the gate needs a
# multi-cycle state machine with waits on engine-driven async completion inside
# one warm process. The verdict-before-teardown protocol (#57) is honoured.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing
# resolves from the same tree as this test (worktree-shadowing idiom).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import math
import time
from orkengine.core import vec3
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

WIDTH, HEIGHT = 320, 240
CAM_DIST      = 7.0

# SETTLES ARE IN FRAMES, WAITS ARE IN SECONDS (see test_sky_ibl_gate for why).
STEP_SETTLE   = 8
WAIT_SECONDS  = 120.0
# A cycle is ~18 slices; even one slice per frame plus deferral escapes must
# land well inside this. It is a DROP detector, not a pace target.
MAX_CYCLE_FRAMES = 20000

SUN_ELEV_DEG = 20.0
SUN_AZIM_A   = 0.0
SUN_AZIM_B   = 25.0   # well past the 2 degree refilter threshold

# COMFORT TARGET vs GATE CEILING. The target for a recurring slice is ~5ms (an
# 11ms VR frame with room for the frame itself), and the warm mean measures
# 1.7-1.8ms on an idle RTX 5090 at the engine defaults (worst 3.3-3.4ms, which
# is a level's READBACK — see below). The CEILING is looser on purpose: this
# gate shares its GPU with other lanes, and a contended run pushes individual
# slices to ~10ms with nothing wrong. So the ceiling catches a REGRESSION (a
# doubling), and the printed mean is what a comfort conversation quotes.
COMFORT_TARGET_MS = 5.0
MAX_SLICE_MS = float(os.environ.get("ORKID_IBL_GATE_MAX_SLICE_MS", "12.0"))
# The package population's ceiling. COMFORT-2 TIGHTENED THIS 60.0 -> 5.0: the
# package was ONE inline step (serialize + decode + the texture array's CPU mip
# generation + upload issue + swap, measured 25-32ms warm, 72ms cold) and is now
# 23 slices measuring 0.03-2.6ms warm. 5.0 is the comfort target itself, ~2x the
# measured worst — the same contention headroom MAX_SLICE_MS carries.
#
# WARM only, deliberately: cycle 1's publish step measures 36ms because the
# first live swap in a process builds the five BRDF integration maps (a
# one-time, process-wide, statically-cached cost that has nothing to do with the
# refilter — cycle 2's publish is 2.1ms). Scoring cycle 2 is what makes this
# ceiling a comfort line rather than a cold-start line.
MAX_OUTLIER_MS = float(os.environ.get("ORKID_IBL_GATE_MAX_OUTLIER_MS", "5.0"))


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


def prefilter_stats(prefix):
  """one population's entry in the process-wide microtask cost registry. Keyed
  by extent (+ sample counts / level count), so a run only ever has one of
  each — 'RadiancePrefilter:' filter steps, 'RadiancePrefilterPkg:' package."""
  stats = lev2.microtaskCostStats()
  for key, val in stats.items():
    if key.startswith(prefix):
      return key, val
  return None, None


class SliceCostApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_frame = 0
    self._phase_time = time.time()
    self._done = False
    self._marks = {}
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(0, 2, -CAM_DIST), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = False

    self.atmo = lev2.SkyAtmosphereData()
    # knob overrides FIRST, so the very first cycle already runs the shape this
    # invocation is measuring (the cost registry keys on it).
    snap = os.environ.get("ORKID_IBL_GATE_SNAP")
    if snap:
      w, h = snap.lower().split("x")
      self.atmo.ibl_snapshot_width = int(w)
      self.atmo.ibl_snapshot_height = int(h)
    samples = os.environ.get("ORKID_IBL_GATE_SAMPLES")
    if samples:
      self.atmo.ibl_specular_samples = int(samples.split(",")[0])
    chain_hz = os.environ.get("ORKID_IBL_GATE_CHAIN_MAX_HZ")
    if chain_hz:
      self.atmo.ibl_chain_max_hz = float(chain_hz)
    self._config = (self.atmo.ibl_snapshot_width, self.atmo.ibl_snapshot_height,
                    self.atmo.ibl_specular_samples)
    print("[slicecost] snapshot=%dx%d spec_samples=%d ceiling=%.1fms" %
          (self._config + (MAX_SLICE_MS,)), flush=True)

    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 0.0
    sun.shadowCaster = False
    self.sun = sun
    self._aimSun(SUN_AZIM_A)
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass

  ##############################################################

  def _aimSun(self, azimuth_deg):
    d = dir_to_sun(azimuth_deg, SUN_ELEV_DEG)
    self.sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))

  def _pbr(self):
    return self.SGC.pbr_common

  ##############################################################
  # phases
  ##############################################################

  def _phases(self):
    return [
        # 0: cycle 1 — includes the process's one-time cold-JIT material build.
        dict(name="cycle1", wait=lambda: int(self._pbr().sky_ibl_generation) >= 1,
             after=self._startCycle2),
        # 1: cycle 2 — the STEADY-STATE cycle, and the one that is scored.
        dict(name="cycle2", wait=lambda: int(self._pbr().sky_ibl_generation) >= 2,
             after=None),
    ]

  def _startCycle2(self):
    # cycle 1's numbers are already recorded; clear the MEASUREMENTS (the learned
    # estimate scale deliberately survives — that persistence is check (b)) so
    # cycle 2 is measured on its own.
    lev2.resetMicrotaskCostStats()
    self._marks["f_trigger2"] = self._frame
    self._marks["t_trigger2"] = time.time()
    self._aimSun(SUN_AZIM_B)
    print("[slicecost] sun moved to azimuth %.1f deg (cycle 2)" % SUN_AZIM_B, flush=True)

  def _recordCycle(self, name):
    for pop, prefix in (("", "RadiancePrefilter:"), ("pkg_", "RadiancePrefilterPkg:")):
      key, st = prefilter_stats(prefix)
      if st is None:
        self._fail("no %s entry in the microtask cost registry after %s" % (prefix, name))
        return
      self._marks[pop + "key"] = key
      for field in ("slices", "worst_ms", "second_worst_ms", "total_ms", "deferrals",
                    "escapes", "instances_seeded", "scale", "seed_scale"):
        self._marks["%s%s_%s" % (pop, name, field)] = st[field]
      print("[slicecost] %s%s key=%s slices=%d worst=%.2fms 2nd_worst=%.2fms total=%.2fms "
            "deferrals=%d escapes=%d seeded=%d scale=%.2f seed_scale=%.2f" %
            (pop, name, key, st["slices"], st["worst_ms"], st["second_worst_ms"], st["total_ms"],
             st["deferrals"], st["escapes"], st["instances_seeded"], st["scale"],
             st["seed_scale"]),
            flush=True)

  ##############################################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._phase_frame = self._frame
        self._phase_time = time.time()
        self._marks["f_trigger1"] = self._frame
        self._marks["t_trigger1"] = time.time()
      return

    phases = self._phases()
    step = self._phase // 2
    if step >= len(phases):
      return
    ph = phases[step]

    ########################################
    # sub 0: wait for the cycle to publish
    ########################################
    if (self._phase % 2) == 0:
      if not ph["wait"]():
        if (self._frame - self._phase_frame) > MAX_CYCLE_FRAMES:
          self._fail("%s never completed in %d frames — a deferred slice was DROPPED, not deferred" %
                     (ph["name"], MAX_CYCLE_FRAMES))
        elif (time.time() - self._phase_time) > WAIT_SECONDS:
          self._fail("%s never completed in %.1f s" % (ph["name"], time.time() - self._phase_time))
        return
      self._marks["f_ready_" + ph["name"]] = self._frame
      self._marks["t_ready_" + ph["name"]] = time.time()
      self._phase += 1
      self._phase_frame = self._frame
      return

    ########################################
    # sub 1: settle, then sample the registry and act
    ########################################
    if (self._frame - self._phase_frame) < STEP_SETTLE:
      return
    self._recordCycle(ph["name"])
    if self._done:
      return
    self._phase += 1
    self._phase_frame = self._frame
    self._phase_time = time.time()
    if ph["after"] is not None:
      ph["after"]()
    if (self._phase // 2) >= len(self._phases()):
      self._emitVerdict()

  ##############################################################

  def _fail(self, why):
    verdict(False, why)
    self._exit_code = 1
    self._done = True
    self.ezapp.signalExit()

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    m = self._marks
    wall1 = m["t_ready_cycle1"] - m["t_trigger1"]
    wall2 = m["t_ready_cycle2"] - m["t_trigger2"]
    frames1 = m["f_ready_cycle1"] - m["f_trigger1"]
    frames2 = m["f_ready_cycle2"] - m["f_trigger2"]

    ##########################################################
    print("=== measurement table ===", flush=True)
    def mean_slice(k):
      n = m[k + "slices"]
      return m[k + "total_ms"] / n if n > 0 else 0.0

    print("  config snapshot=%dx%d spec_samples=%d" % self._config, flush=True)
    print("  keys filter=%s package=%s" % (m["key"], m["pkg_key"]), flush=True)
    print("  cycle/pop     | slices | worst_ms | 2nd_worst | mean_ms | total_ms | wall_s | frames | defer | escape",
          flush=True)
    for name, key, wall, frames in (("1(cold) filter ", "cycle1_", wall1, frames1),
                                    ("1(cold) package", "pkg_cycle1_", wall1, frames1),
                                    ("2(warm) filter ", "cycle2_", wall2, frames2),
                                    ("2(warm) package", "pkg_cycle2_", wall2, frames2)):
      print("  %s|%7d |%9.2f |%10.2f |%8.2f |%9.2f |%7.3f |%7d |%6d |%7d" %
            (name, m[key + "slices"], m[key + "worst_ms"], m[key + "second_worst_ms"],
             mean_slice(key), m[key + "total_ms"], wall, frames,
             m[key + "deferrals"], m[key + "escapes"]),
            flush=True)
    print("  comfort target %.1fms: warm filter mean=%.2fms worst=%.2fms | package worst=%.2fms" %
          (COMFORT_TARGET_MS, mean_slice("cycle2_"), m["cycle2_worst_ms"],
           m["pkg_cycle2_worst_ms"]), flush=True)

    ##########################################################
    # a) every slice of the steady-state cycle fits in a frame
    ##########################################################
    print("=== slice cost ===", flush=True)
    check("cycle2_slices_measured", m["cycle2_slices"] > 0,
          "slices=%d filter + %d package" % (m["cycle2_slices"], m["pkg_cycle2_slices"]))
    check("recurring_slice_within_ceiling", m["cycle2_second_worst_ms"] <= MAX_SLICE_MS,
          "worst recurring=%.2fms ceiling=%.2fms" % (m["cycle2_second_worst_ms"], MAX_SLICE_MS))
    check("package_slice_within_ceiling", m["pkg_cycle2_worst_ms"] <= MAX_OUTLIER_MS,
          "worst package slice=%.2fms ceiling=%.2fms (was ONE ~30ms step pre-COMFORT-2)" %
          (m["pkg_cycle2_worst_ms"], MAX_OUTLIER_MS))

    ##########################################################
    # b) the learned estimate survived the cycle that learned it
    ##########################################################
    print("=== estimate persistence ===", flush=True)
    for pop, label in (("", "filter"), ("pkg_", "package")):
      check("second_instance_seeded_" + label, m[pop + "cycle2_instances_seeded"] >= 2,
            "instances_seeded=%d" % m[pop + "cycle2_instances_seeded"])
      # what cycle 1 ended up believing IS what cycle 2 was seeded with (the
      # scale floors at 1x, so compare against the floored value).
      learned = max(1.0, m[pop + "cycle1_scale"])
      check("seeded_from_learned_scale_" + label,
            abs(m[pop + "cycle2_seed_scale"] - learned) < 0.01,
            "cycle1 scale=%.3fx -> cycle2 seed_scale=%.3fx" %
            (m[pop + "cycle1_scale"], m[pop + "cycle2_seed_scale"]))

    ##########################################################
    # c) deferral defers, it never drops
    ##########################################################
    print("=== defer, don't drop ===", flush=True)
    check("both_cycles_completed", int(self._pbr().sky_ibl_generation) >= 2,
          "generation=%d" % int(self._pbr().sky_ibl_generation))
    check("cycle2_bounded_frames", frames2 <= MAX_CYCLE_FRAMES,
          "frames=%d limit=%d (deferrals=%d escapes=%d)" %
          (frames2, MAX_CYCLE_FRAMES, m["cycle2_deferrals"], m["cycle2_escapes"]))

    ok = (len(failures) == 0)
    detail = ("snap=%dx%d spec_samples=%d recur_ms=%.2f worst_ms=%.2f pkg_worst_ms=%.2f "
              "slices=%d+%d cycle_wall=%.3f/%.3f s frames=%d/%d defer=%d escape=%d seed_scale=%.2f" %
              (self._config + (m["cycle2_second_worst_ms"], m["cycle2_worst_ms"],
                               m["pkg_cycle2_worst_ms"],
                               m["cycle2_slices"], m["pkg_cycle2_slices"], wall1, wall2,
                               frames1, frames2,
                               m["cycle2_deferrals"], m["cycle2_escapes"],
                               m["cycle2_seed_scale"])))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = SliceCostApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
