#!/usr/bin/env ork.python
################################################################################
# ONE AMBIENT PIPELINE gate (W4-S9, owner ruling 10).
#
# Every sky the engine can light a fragment with — the procedural probe's and an
# authored equirect's alike — now reaches shading as nine L2 coefficients. The
# prefiltered irradiance equirect that used to serve the authored path does not
# exist any more, so an authored map's projection is not a second-class
# approximation of the probe's: it has to BE the same measurement of the same
# sky, in the same world orientation and the same units.
#
# THIS GATE MEASURES THAT EQUALITY, on a procedural sky, where both projections
# exist at once and one of them is already proven:
#
#   the PROBE       ProbeSHProjector::projectEquirect, run on the GPU over the
#                   sky's equirect snapshot. Its correctness (basis, order,
#                   world orientation, capture decode) is pinned against a CPU
#                   integral of that snapshot by test_env_diffuse_convolution_
#                   gate, which is what makes it usable as a reference here.
#
#   the MAP SET     projectEquirectImageSH, run on the CPU over specular
#                   roughness level 0 — the prefilter chain's identity level.
#                   This is the code path a BAKED scene's whole ambient comes
#                   from: the .xir loader projects exactly this image, in
#                   exactly this call, at load. A procedural cycle publishes the
#                   same product from the same source, which is what lets a
#                   baked-path convention be measured without a baked-path
#                   oracle.
#
# The two integrate the same sky through completely separate code (GPU kernel vs
# host loop) and, more to the point, through separate SOURCES: the snapshot as
# the sky wrote it, and the snapshot after a roughness-0 GGX resample into the
# specular array. So they are compared the way an orientation question has to be
# — as a family. The reconstructed irradiance is scored over a sphere of normals
# against the reference evaluated at each of eight rigid transforms of that
# normal, and the IDENTITY has to win outright. A mirrored or yawed convention
# in the authored path — the exact defect that would light a baked scene from
# the wrong side of its own sky while its reflections stayed right — lands as a
# different winner, by a wide margin, instead of as a plausible-looking number.
#
# WHAT ELSE IS PINNED HERE
#
#   units      the map set's coefficients are RAW (they carry the capture
#              pre-scale) and pbr_common resolves them at the read. The gate
#              reads both and checks the ratio IS the pre-scale — a decode
#              applied twice or not at all is a pure gain error, which the
#              shape-only orientation score would otherwise absorb.
#
#   available  RadianceMaps::_measuredLuminance is now the L0 coefficient's
#   light      sphere-mean, not a separate 8x4 render. The gate recomputes it
#              from the coefficients it read and requires the engine's number to
#              match — the scene-adaptation input has to keep meaning what it
#              says it means.
#
#   the grid   grid.fxv2 is a lib_fwd consumer bound through a different
#              drawable than any mesh, and it was the suspected silent-fallback
#              path. Its ambient is measured against the SH the frame published
#              the grid tracks the procedural sky
#              quantitatively, or this gate fails.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import math
import time
import numpy
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

tokens = CrcStringProxy()

WIDTH, HEIGHT = 384, 288
SETTLE_FRAMES = 24
WAIT_SECONDS  = 90.0

# The sun is placed LOW and OBLIQUE on purpose, and both halves of that matter.
# A high sun makes the sky azimuthally symmetric, which cannot tell any azimuth
# convention from its mirror image at all. A low sun ON an axis is worse than it
# looks: with the sun aimed down +X (azimuth 90) the sky is symmetric about the
# XY plane, and MEASURED, mirrorZ then scored identically to the identity
# (0.00461 both) — a degenerate discriminator that would have passed anything.
# An oblique azimuth leaves no symmetry plane for a rival transform to hide in.
SUN_AZIM_DEG = 35.0
SUN_ELEV_DEG = 18.0

# scoring
N_NORMALS      = 512
IDENT_MARGIN   = 4.0    # identity must beat the best rival by this factor
IDENT_REL_TOL  = 0.06   # identity's own residual (prefilter resample + fp16)
SCALE_REL_TOL  = 0.02   # raw/resolved coefficient ratio vs the capture pre-scale
LUM_REL_TOL    = 0.02   # engine's measured luminance vs L0's sphere-mean
GRID_REL_TOL   = 0.15   # grid response vs the SH prediction, across a sun move


def dir_to_sun(azimuth_deg, elevation_deg):
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


ORIENTATIONS = [
    ("identity",      lambda n: n),
    ("mirrorX",       lambda n: n * numpy.array([-1.0, 1.0, 1.0])),
    ("mirrorZ",       lambda n: n * numpy.array([1.0, 1.0, -1.0])),
    ("yaw180",        lambda n: n * numpy.array([-1.0, 1.0, -1.0])),
    ("negY",          lambda n: n * numpy.array([1.0, -1.0, 1.0])),
    ("mirrorXnegY",   lambda n: n * numpy.array([-1.0, -1.0, 1.0])),
    ("swapYZ",        lambda n: n[:, [0, 2, 1]]),
    ("mirrorXswapYZ", lambda n: n[:, [0, 2, 1]] * numpy.array([-1.0, 1.0, 1.0])),
]


def sh_irradiance(coeffs, normals):
  """stdtools lib_env_sh env_shIrradiance, vectorized over normals. Same band
  weights (1, 2/3, 1/4) and the same negative clamp — this is what the fragment
  shader computes, not an independent formula."""
  c = numpy.asarray(coeffs)                       # (9,3)
  n = numpy.asarray(normals)                      # (N,3)
  x, y, z = n[:, 0], n[:, 1], n[:, 2]
  basis = numpy.stack([
      numpy.full(len(n), 0.2820948),
      0.4886025 * y * 0.6666667,
      0.4886025 * z * 0.6666667,
      0.4886025 * x * 0.6666667,
      1.0925484 * x * y * 0.25,
      1.0925484 * y * z * 0.25,
      0.3153916 * (3.0 * z * z - 1.0) * 0.25,
      1.0925484 * x * z * 0.25,
      0.5462742 * (x * x - y * y) * 0.25,
  ], axis=1)                                      # (N,9)
  return numpy.maximum(basis @ c, 0.0)            # (N,3)


def fibonacci_normals(n):
  i = numpy.arange(n) + 0.5
  y = 1.0 - 2.0 * i / n
  r = numpy.sqrt(numpy.maximum(1.0 - y * y, 0.0))
  ga = math.pi * (3.0 - math.sqrt(5.0))
  return numpy.stack([r * numpy.cos(ga * i), y, r * numpy.sin(ga * i)], axis=1)


def fit_residual(meas, ref):
  """best-fit gain and the relative residual it leaves. Shape only — the two
  projections carry the same units by construction, but the scale check below
  is the one that says so, and mixing the two questions would let a gain error
  hide inside an orientation score."""
  den = float((ref * ref).sum())
  if den <= 0.0:
    return 0.0, float("inf")
  a = float((meas * ref).sum()) / den
  resid = meas - a * ref
  denom = a * float(numpy.linalg.norm(ref))
  return a, (float(numpy.linalg.norm(resid)) / denom if denom > 0 else float("inf"))


class FloatOutSGC(StandardSceneGraphComponent):
  """scenegraph compositing into an RGBA32F RtGroup: the grid reading has to be
  PRE-TONEMAP or a brightness RATIO is a reading of the curve, not of the sky
  (recipe + failure modes documented in test_env_hdr_range_gate.py)."""

  def _onGpuInit(self, ctx):
    rtg = lev2.RtGroup(ctx, WIDTH, HEIGHT)
    rtg.name = "ambient_f32"
    rtg.createBuffer(tokens.RGBA32F, tokens.color)
    self.float_rtg = rtg
    self.sg_params.outputRTG = rtg
    super()._onGpuInit(ctx)


class AmbientPipelineApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._state = 0
    self._state_frame = 0
    self._state_time = time.time()
    self._done = False
    self._exit_code = None
    self._built = False
    self._marks = {}
    self._grid = {}
    self._sh_frame = {}
    self._inflight = None
    self._realized = False
    self.SGC = self.addComponent(
        "std_scenegraph", FloatOutSGC,
        # looking down at the grid: the grid fills the lower frame and its
        # normal is +Y, which is the direction the SH prediction is read at.
        eye=vec3(0, 6, -14), tgt=vec3(0, 0, 0), up=vec3(0, 1, 0),
        grid_variant="_V4",
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          # specular off + no flat ambient: the grid's pixels are then the
          # diffuse ambient and nothing else.
          "SpecularIntensity": 0.0,
          "AmbientLevel":     vec3(0),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    # SKYBOX OFF: this gate drives the scenegraph's render itself (to composite
    # into its float RTG), and a manual render does not run the forward prologue
    # that publishes the procedural sky's frame state — the skybox technique
    # asserts on that, loudly, rather than drawing a stale sky. The grid does
    # not need the sky drawn; it needs the sky's SH, which the prologue keeps
    # publishing on the normal frames.
    SGC.pbr_common.enable_skybox = False

    self.atmo = lev2.SkyAtmosphereData()
    # HARD SWAP: a capture taken a settle after the publish must read the sky
    # the publish carried, not a blend of it with the previous one.
    self.atmo.ibl_crossfade_frames = 0
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    # DIRECTION SOURCE ONLY (intensity 0): every photon in the frame is
    # image-based, so the grid measurement is a reading of the ambient.
    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 0.0
    sun.shadowCaster = False
    self.sun = sun
    self._aimSun(SUN_AZIM_DEG, SUN_ELEV_DEG)
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass

  ##############################################################

  def _aimSun(self, azimuth_deg, elevation_deg):
    d = dir_to_sun(azimuth_deg, elevation_deg)
    self.sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))

  def _pbr(self):
    return self.SGC.pbr_common

  def _fail(self, why):
    verdict(False, why)
    self._exit_code = 1
    self._done = True
    self.ezapp.signalExit()

  def _restate(self, s):
    self._state = s
    self._state_frame = self._frame
    self._state_time = time.time()

  def _published(self, mark, count):
    want = self._marks[mark] + count
    gen = int(self._pbr().sky_ibl_generation)
    if (gen >= want) and (not bool(self._pbr().sky_ibl_inflight)):
      return True
    if (time.time() - self._state_time) > WAIT_SECONDS:
      self._fail("generation never reached %d (gen=%d)" % (want, gen))
    return False

  ##############################################################
  # capture plumbing (async: the future is polled, never waited on)
  ##############################################################

  def _issueCapture(self, ctx, key):
    buf = lev2.CaptureBuffer()
    fut = ctx.FBI.captureAsFormat(self.SGC.float_rtg.buffer(0), buf, "RGBA32F")
    self._inflight = (fut, buf, key)

  def _collect(self):
    if self._inflight is None:
      return None
    fut, buf, key = self._inflight
    if not bool(fut.is_ready):
      return None
    img = numpy.array(buf, dtype=numpy.float32).reshape(buf.height, buf.width, 4)[..., :3]
    # the grid plane, no sky: lower band, center columns
    h, w, _ = img.shape
    band = img[int(h * 0.72):int(h * 0.95), int(w * 0.35):int(w * 0.65), :]
    self._grid[key] = float(band.mean())
    self._inflight = None
    print("[ambient] %s: grid=%.6g frame_mean=%.6g" % (key, self._grid[key], float(img.mean())), flush=True)
    return key

  def _readSH(self, key):
    """the frame's ambient as the engine holds it, from both sides of the seam.
    Read on the SAME frame the capture is issued on."""
    p = self._pbr()
    if not bool(p.sky_sh_valid):
      self._fail("%s: the frame has no resolved ambient at all" % key)
      return False
    maps = p.active_radiance_maps
    if (maps is None) or (not bool(maps.sh_valid)):
      self._fail("%s: the bound radiance maps carry no SH projection - the "
                 "authored/baked path would have no ambient" % key)
      return False
    self._sh_frame[key] = dict(
        resolved=[numpy.array([c.x, c.y, c.z]) for c in p.sky_sh_coefficients],
        raw=[numpy.array([c.x, c.y, c.z]) for c in maps.sh_coefficients],
        lum_engine=float(maps.measured_luminance),
        lum_resolved=float(p.sky_measured_luminance),
        scale=float(self.atmo.ibl_capture_scale))
    return True

  ##############################################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return

    if not self._built:
      if bool(self._pbr().sky_ibl_ready) and (not bool(self._pbr().sky_ibl_inflight)):
        self._built = True
        self._restate(0)
      elif (time.time() - self._state_time) > WAIT_SECONDS:
        self._fail("the first procedural refilter never published")
      return

    # HIGH sun -> publish -> settle -> read; LOW sun -> publish -> settle ->
    # read. Two skies of deliberately different shape: the orientation score
    # runs on the low one (a low sun is the only azimuthally asymmetric sky),
    # and the pair is what the grid's tracking check needs.
    if self._state == 0:
      self._marks["a"] = int(self._pbr().sky_ibl_generation)
      self._aimSun(SUN_AZIM_DEG, 60.0)
      self._restate(1)
      return

    if self._state == 1:
      if not self._published("a", 1):
        return
      self._restate(2)
      return

    if self._state == 2:                        # settle, then render + capture
      if (self._frame - self._state_frame) < SETTLE_FRAMES:
        return
      self.SGC.scenegraph.renderOnContext(ctx)
      if not self._realized:
        # captureAsFormat asserts natively on an unbuilt RtBuffer impl
        ctx.FBI.rtGroupInit(self.SGC.float_rtg)
        self._realized = True
        return
      if not self._readSH("high"):
        return
      self._issueCapture(ctx, "high")
      self._restate(3)
      return

    if self._state == 3:
      if self._collect() is None:
        return
      self._marks["b"] = int(self._pbr().sky_ibl_generation)
      self._aimSun(SUN_AZIM_DEG, SUN_ELEV_DEG)
      self._restate(4)
      return

    if self._state == 4:
      if not self._published("b", 1):
        return
      self._restate(5)
      return

    if self._state == 5:
      if (self._frame - self._state_frame) < SETTLE_FRAMES:
        return
      self.SGC.scenegraph.renderOnContext(ctx)
      if not self._readSH("low"):
        return
      self._issueCapture(ctx, "low")
      self._restate(6)
      return

    if self._state == 6:
      if self._collect() is None:
        return
      self._emitVerdict()
      return

  ##############################################################

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    if ("low" not in self._sh_frame) or ("high" not in self._sh_frame):
      self._fail("missing readings")
      return

    normals = fibonacci_normals(N_NORMALS)
    S = self._sh_frame["low"]

    ##########################################################
    # ORIENTATION — the authored-path projection against the proven probe
    ##########################################################
    print("=== authored-path projection vs the sky probe ===", flush=True)
    probe = numpy.array(S["resolved"])
    # the map set's raw coefficients, decoded the way pbr_common decodes them
    mapsh = numpy.array(S["raw"]) / max(S["scale"], 1e-30)
    ref = sh_irradiance(probe, normals).mean(axis=1)
    scores = []
    for name, xf in ORIENTATIONS:
      meas = sh_irradiance(mapsh, xf(normals)).mean(axis=1)
      a, rel = fit_residual(meas, ref)
      scores.append((name, a, rel))
      print("    orientation %-14s gain=%.6g  rel=%.5f" % (name, a, rel), flush=True)
    ident = [s for s in scores if s[0] == "identity"][0]
    rivals = sorted([s for s in scores if s[0] != "identity"], key=lambda s: s[2])
    margin = rivals[0][2] / max(ident[2], 1e-30)
    check("authored_projection_is_world_oriented",
          (ident[2] < IDENT_REL_TOL) and (margin > IDENT_MARGIN),
          "identity rel=%.5f (tol %.2f), best rival %s rel=%.5f, margin=%.1fx (min %.1f)"
          % (ident[2], IDENT_REL_TOL, rivals[0][0], rivals[0][2], margin, IDENT_MARGIN))

    ##########################################################
    # UNITS — one decode seam
    ##########################################################
    print("=== units ===", flush=True)
    num = float(numpy.linalg.norm(numpy.array(S["raw"])))
    den = float(numpy.linalg.norm(probe))
    ratio = num / max(den, 1e-30)
    check("capture_prescale_decoded_exactly_once",
          abs(ratio - S["scale"]) / max(S["scale"], 1e-30) < SCALE_REL_TOL,
          "raw/resolved=%.4f vs capture_scale=%.4f" % (ratio, S["scale"]))

    ##########################################################
    # AVAILABLE LIGHT — L0's sphere-mean, recomputed
    ##########################################################
    print("=== available light ===", flush=True)
    mean_rgb = numpy.array(S["raw"][0]) * 0.2820948
    want = float(mean_rgb @ numpy.array([0.2126, 0.7152, 0.0722]))
    got = S["lum_engine"]
    check("measured_luminance_is_L0_sphere_mean",
          abs(got - want) / max(abs(want), 1e-30) < LUM_REL_TOL,
          "engine=%.6g recomputed=%.6g" % (got, want))
    check("resolved_luminance_decodes",
          abs(S["lum_resolved"] - got / max(S["scale"], 1e-30)) / max(abs(got / max(S["scale"], 1e-30)), 1e-30) < LUM_REL_TOL,
          "resolved=%.6g expected=%.6g" % (S["lum_resolved"], got / max(S["scale"], 1e-30)))

    ##########################################################
    # THE GRID — a lib_fwd consumer that is NOT a mesh drawable
    ##########################################################
    print("=== grid tracks the ambient ===", flush=True)
    up = numpy.array([[0.0, 1.0, 0.0]])
    pred_hi = float(sh_irradiance(numpy.array(self._sh_frame["high"]["resolved"]), up).mean())
    pred_lo = float(sh_irradiance(numpy.array(self._sh_frame["low"]["resolved"]), up).mean())
    g_hi, g_lo = self._grid["high"], self._grid["low"]
    have = (g_hi is not None) and (g_lo is not None) and (g_hi > 0) and (g_lo > 0)
    check("grid_sampled", have, "high=%s low=%s" % (g_hi, g_lo))
    if have:
      want_ratio = pred_lo / max(pred_hi, 1e-30)
      got_ratio = g_lo / max(g_hi, 1e-30)
      print("    predicted(+Y irradiance) hi=%.6g lo=%.6g  ratio=%.4f" % (pred_hi, pred_lo, want_ratio), flush=True)
      print("    measured (grid pixels)   hi=%.6g lo=%.6g  ratio=%.4f" % (g_hi, g_lo, got_ratio), flush=True)
      check("grid_tracks_procedural_ambient",
            abs(got_ratio - want_ratio) / max(want_ratio, 1e-30) < GRID_REL_TOL,
            "measured ratio=%.4f vs SH-predicted %.4f (tol %.2f) — the grid is "
            "lit by the sky the frame published, not by a stale authored map"
            % (got_ratio, want_ratio, GRID_REL_TOL))

    ok = (len(failures) == 0)
    detail = ("ident_rel=%.5f margin=%.1fx scale_ratio=%.3f lum=%.6g grid_ratio=%.4f"
              % (ident[2], margin, ratio, got, (self._grid["low"] / max(self._grid["high"], 1e-30))
                 if have else -1.0))
    if failures:
      detail += " failed=" + ",".join(failures)
    verdict(ok, detail)                          # VERDICT BEFORE TEARDOWN
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = AmbientPipelineApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
