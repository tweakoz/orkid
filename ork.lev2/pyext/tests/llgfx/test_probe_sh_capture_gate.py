#!/usr/bin/env ork.python
"""
SKYLIGHT lane C slice C1 step 2 — LightProbeType::SH_Radiance INTEGRATION gate.

test_probe_sh_projection_gate.py proves the projection MATH against closed forms on
synthetic cubemaps. This gate proves the ENGINE WIRING: that a scenegraph probe typed
SH_Radiance (a bare enum falling through `default: break` before this slice) gets a cube
RTG and an SH SSBO slot in _update_env_probes phase 1, is captured face by face in phase
2, and is projected on the following frame — with the coefficients reachable through
LightProbe::shCoefficients.

WHY THE SCENE CARRIES ITS OWN GEOMETRY ON THE PROBE LAYER: a probe pass renders only
the probe's render layer plus the skybox, and offscreen the skybox env map is frequently
still loading for the whole run — an empty probe layer therefore captures a BLACK cube
and every coefficient is legitimately 0.0, which would make this gate pass vacuously.
Four lit balls on a layer literally named "probe" (LightProbe's default render layer)
guarantee a non-degenerate, directionally varying radiance field.

Assertions are structural + physical; the closed-form gate owns the numbers:
  - a slot is claimed and nine coefficients come back;
  - the DC term is positive on every channel (with content on the probe layer, a zero
    DC term is a real failure);
  - no coefficient exceeds c0 * max|Y_lm|/Y_00 = 2.236 — the hard bound a non-negative
    radiance field imposes (the extreme case is a delta light, c_lm = w*Y_lm(d));
  - at least one directional coefficient is well away from zero (the field is not flat);
  - re-capturing the same static scene reproduces the coefficients bit for bit.
"""
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

from orkengine.core import vec3, vec4, mtx4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

PROBE_DIM     = 64
SETTLE_FRAMES = 120
RECAP_FRAMES  = 40

# max|Y_lm| over l<=2 is |Y20| at the poles = 0.6307831; divided by Y00 = 0.2820948.
NONNEG_BAND_BOUND = 2.236

_RESULT = {"failures": [], "notes": []}


def check(label, ok, detail=""):
  print(f"  {'PASS' if ok else 'FAIL'} {label} {detail}", flush=True)
  if not ok:
    _RESULT["failures"].append(label)
  return ok


class SHProbeApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._phase = 0
    self._mark = 0
    self._first = None
    self._done = False
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(0, 6, 14), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant="_V3",
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0.3),
        })
    self.createEzApp(width=320, height=240, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  def _onGpuInit(self, ctx):
    self._ctx = ctx
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = True

    # "probe" is LightProbe's default render layer; SGC does not create it, and
    # createBallNode places on SGC.fwd_layers — widen that so the markers are
    # visible BOTH to the main view and to the probe's cubemap pass.
    self.layer_probe = SGC.scenegraph.createLayer("probe")
    SGC.fwd_layers = [SGC.layer_fwd, SGC.layer_dpp, self.layer_probe]

    self.balls = [
      SGC.createBallNode("mk_px", ctx=ctx, position=vec3(5, 2, 0), color=vec4(1, 0, 0, 1), scale=2.0),
      SGC.createBallNode("mk_nx", ctx=ctx, position=vec3(-5, 2, 0), color=vec4(0, 1, 0, 1), scale=2.0),
      SGC.createBallNode("mk_pz", ctx=ctx, position=vec3(0, 2, 5), color=vec4(0, 0, 1, 1), scale=2.0),
      SGC.createBallNode("mk_nz", ctx=ctx, position=vec3(0, 2, -5), color=vec4(1, 1, 1, 1), scale=2.0),
    ]
    self.point_light = lev2.DynamicPointLight()
    self.point_light.data.color = vec3(2000, 2000, 2000)
    self.point_light.data.radius = 60.0
    self.pnode = SGC.layer_fwd.createLightNode("pointlight0", self.point_light)
    self.pnode.setMatrix(mtx4.transMatrix(2, 10, 6))

    self.probe = lev2.LightProbe()
    self.probe.type = tokens.SH_Radiance
    self.probe.imageDim = PROBE_DIM
    self.probe.worldMatrix = mtx4.transMatrix(0, 2, 0)
    self.probe.name = "sh_probe"
    self.probe_node = SGC.layer_fwd.createLightProbeNode("sh_probe", self.probe)

    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    # models load asynchronously; a static probe bakes ONCE, so a capture taken before
    # the meshes are resident would freeze a black cube in. Stay dirty through settle
    # (canary_probe.py's invalidate-during-settle idiom).
    if self._frame < SETTLE_FRAMES:
      self.probe.invalidate()

  def _sample(self, ctx):
    return [(c.x, c.y, c.z) for c in self.probe.shCoefficients(ctx)]

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    try:
      if self._phase == 0:
        if self._frame < SETTLE_FRAMES:
          return
        check("sh_slot_claimed", self.probe.shSlot >= 0, "slot=%d" % self.probe.shSlot)
        coeffs = self._sample(ctx)
        check("nine_coefficients_returned", len(coeffs) == 9, "len=%d" % len(coeffs))
        if len(coeffs) == 9:
          c0 = coeffs[0]
          check("dc_term_positive_all_channels", min(c0) > 1.0e-3,
                "c0=(%.5f,%.5f,%.5f)" % c0)
          worst_ratio = 0.0
          best_dir    = 0.0
          for i in range(1, 9):
            for ch in range(3):
              if c0[ch] > 1.0e-6:
                r = abs(coeffs[i][ch]) / c0[ch]
                worst_ratio = max(worst_ratio, r)
                best_dir    = max(best_dir, r)
          check("bands_within_nonneg_radiance_bound", worst_ratio <= NONNEG_BAND_BOUND,
                "worst=%.4f bound=%.3f" % (worst_ratio, NONNEG_BAND_BOUND))
          check("directional_content_present", best_dir > 0.05, "best=%.4f" % best_dir)
          _RESULT["notes"].append("c0=(%.4f,%.4f,%.4f) band_ratio=%.4f" % (c0 + (worst_ratio,)))
          self._first = coeffs
        self.probe.invalidate()   # force exactly one more capture+projection cycle
        self._mark = self._frame
        self._phase = 1
        return

      if self._phase == 1:
        if self._frame < self._mark + RECAP_FRAMES:
          return
        second = self._sample(ctx)
        same = (self._first is not None and len(second) == 9 and second == self._first)
        check("recapture_is_bit_identical", same,
              "" if same else "first=%s second=%s" % (str(self._first), str(second)))
        self._done = True
        self.ezapp.signalExit()
    except Exception:
      import traceback
      traceback.print_exc()
      _RESULT["failures"].append("exception")
      self._done = True
      self.ezapp.signalExit()


def main():
  app = SHProbeApp()
  app.ezapp.mainThreadLoop()
  ok = (len(_RESULT["failures"]) == 0) and app._done
  if not app._done:
    _RESULT["failures"].append("never_reached_verdict")
    ok = False
  detail = "dim=%d %s" % (PROBE_DIM, " ".join(_RESULT["notes"]))
  if _RESULT["failures"]:
    detail += " failed=" + ",".join(_RESULT["failures"])
  # VERDICT BEFORE TEARDOWN (#57)
  print("TESTVERDICT=%s detail=%s" % ("PASS" if ok else "FAIL", detail), flush=True)
  app.ezapp.shutdown()
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
