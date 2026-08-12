#!/usr/bin/env ork.python
################################################################################
# SPVR HAZE STEREO GATE (G4) — aerial perspective under the single-pass node.
#
# WHAT CAN GO WRONG THAT A SCREENSHOT WILL NOT SHOW. The haze seam is applied
# inside the forward lighting composite and takes the eye position as an
# ARGUMENT (it must never reach for EyePostion/ublk_stereo itself), and the C++
# binder arms it from one lambda that is attached to every forward arm. Under
# single-pass stereo one draw carries both views, so three things can be wrong
# while a still frame from either eye looks perfect:
#   * the draw takes the MONO technique inside the stereo pass -> one image is
#     written into both eye layers, parallax is zero, no validation error;
#   * the haze binds arm for one eye's pass and not the other -> one hazed eye
#     and one clear eye, which in a headset is nausea, not a look;
#   * the seam reads a per-eye uniform that the stereo path does not fill ->
#     NaN/black in one layer.
# So this gate reads the bind-time arbiter, differences the two eye layers, and
# differences haze-on against haze-off SEPARATELY PER EYE.
#
# LEGS (all must pass)
#   (a) SELECTION  every forward DRAW the arbiter [SPVR:FWDSEL] announces must
#                  name an _ST technique with permu_stereo<1> AND pass_stereo<1>.
#                  This is the line that fires at pipeline BIND, so it is the
#                  proof that the stereo arm of the haze bind lambda ran; an _ST
#                  technique that merely exists proves nothing.
#   (b) HAZE PER EYE  haze-on vs haze-off must move a real population of pixels
#                  in EACH eye, by a real number of levels, and by comparable
#                  amounts in the two eyes. Only geometry pixels may move: the
#                  skybox runs no artist haze layer in v1, so this is counted in
#                  PIXELS, never as a fraction of a frame that is mostly sky.
#   (c) NOT DEGENERATE  no eye layer may be black, flat or riddled with zeros in
#                  either take. "It got darker" is what a NaN looks like once
#                  the tonemapper has had it.
#   (d) BOUNDED PARALLAX  four measurements, because "differs only by bounded
#                  horizontal parallax" is four statements and each one alone is
#                  satisfied by a bug: the layers must DIFFER at all (a mono
#                  technique makes them identical); the SKY BAND must NOT (a sky
#                  at infinity cannot move under a translation, so if it does,
#                  the view ray has picked up the eye position); the NEAR sphere
#                  must be displaced horizontally by between PARALLAX_MIN_PX_SHIFT
#                  and PARALLAX_MAX_PX pixels and by essentially nothing
#                  vertically, in BOTH takes (arming a shading term may not move
#                  geometry); and the KILOMETRE ladder must not be displaced at
#                  all, because at this IPD it subtends far under a pixel.
#                  Displacement is read as a CENTROID difference, not a
#                  correlation search -- see _centroid().
#   (e) NO NEW VALIDATION CLASS  this platform (MoltenVK) emits a pre-existing
#                  stream of validation errors on ANY spvr scene, haze or not:
#                  the haze-free sibling test_spvr_rigidprim_stereo raises the
#                  same four classes here and fails on the count alone. Gating on
#                  a zero count would therefore gate someone else's defect, so
#                  what is gated is the VUID SET (KNOWN_VUIDS): any class this
#                  rig has not raised before fails, which is what a broken
#                  descriptor or binding would announce.
#
# THE RIG. The G2 convergence scene under SPVR: procedural sky, sun 25 deg up and
# behind the camera, a distance ladder of lit PBR spheres carrying the cranked
# ground haze. Parallax needs an object CLOSE to the eyes -- at a human IPD the
# kilometre-scale content that shows haze is at sub-pixel parallax and leg (d)
# would have nothing to bound -- so one near sphere rides at PARALLAX_DIST_M
# beside the ladder. IPD stays physical (the sibling stereo gates' 0.25 m); the
# near sphere is what makes the number, exactly as the rigid-primitive gate's
# 2 m cube cluster does.
#
# Self-configuring: ORKID_VULKAN_VALIDATE=2 (continue mode — =1 traps with no
# printed evidence), ibl_crossfade_frames=0 (a half-faded IBL is not a reading).
#
#   ork.python test_spvr_haze_stereo_gate.py [outdir]
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")

import sys
import re
import json
import math
import time
import tempfile
import subprocess

sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "bin"))

W = H = 256
IPD = 0.25             # physical; parallax scales linearly with it
FOVD = 90.0
EYE_Y = 2.0
FRAMES = 14            # the procedural LUT chain needs a few frames to publish

SUN_ELEV_DEG  = 25.0
SUN_AZIM_DEG  = 180.0
SUN_INTENSITY = 8.0
SKY_EXPOSURE  = 7.0

HAZE_DENSITY      = 0.30
HAZE_SCALE_HEIGHT = 0.5
HAZE_PHASE_G      = 0.70
HAZE_SCATTER_TINT = (1.00, 0.86, 0.66)
HAZE_INSCAT_TINT  = (1.10, 0.92, 0.70)
HAZE_MAX_DIST_KM  = 160.0

PARALLAX_DIST_M = 4.0    # the near sphere, below the sight line and to one side
NEAR_COLOR = (0.85, 0.10, 0.70)
LADDER = ((800.0, -0.16, (0.95, 0.42, 0.04)),
          (6400.0,  0.00, (0.88, 0.85, 0.06)),
          (51200.0, 0.16, (0.10, 0.80, 0.12)))
BALL_ANG_TAN = 0.05
BALL_MODEL_R = 1.09      # pbr_calib.glb ball radius at scale 1

# ---- bars, in 0..1 luma unless stated (all measured; see the verdict line) ----
DIFF_EPS         = 2.0 / 255.0   # one 8-bit code either way is not a difference
PARALLAX_MIN_PX  = 100           # leg (d): px that must differ between the eyes.
                                 # Counted, not fractioned: only geometry can
                                 # differ and geometry is ~1% of this frame.
SKY_AGREE_CEIL   = 0.005         # leg (d): the sky band is at infinity
PARALLAX_MIN_PX_SHIFT = 2.0      # leg (d): IPD/PARALLAX_DIST_M predicts ~8 px
PARALLAX_MAX_PX  = 24            # leg (d): ... and nothing may exceed this
PARALLAX_DY_MAX  = 0.5           # leg (d): the displacement is horizontal
FAR_SHIFT_MAX    = 1.0           # leg (d): km-distant content is sub-pixel
RANGE_FLOOR      = 0.05          # leg (c): a flat layer matches any other
BLACK_FRAC_CEIL  = 0.02          # leg (c): a NaN reads as a hole, not a colour
HAZE_MIN_PX      = 150           # leg (b): geometry pixels haze must move
HAZE_MIN_LEVELS  = 32.0          # leg (b): by this many 8-bit levels, peak
HAZE_EYE_BALANCE = 0.25          # leg (b): |L-R| population mismatch ceiling
HAZE_PIX_EPS     = 8.0           # 8-bit: what counts as "this pixel got hazed"
HUE_TOL          = 0.40          # normalized-RGB radius for the sphere finder
CHROMA_MIN       = 30.0          # 8-bit max-min; the sky sits far below this
MIN_BLOB_PX      = 30            # a sphere the finder cannot see is a FAIL

# The pre-existing platform noise, and the ONLY classes allowed (leg (e)). This
# exact set comes back from test_spvr_rigidprim_stereo on this machine -- a rig
# with a BAKED skybox, no procedural sky, no SKY_FRAME and therefore no haze
# binds at all -- so none of it is this feature's. 08608/06886 are the recorder
# setting depth-write dynamically on pipelines that also declare it statically;
# 03016 is the forward layout's combined-sampler count over MoltenVK's 16, which
# HAZE.md 3.6 records as already out of spec on mac BEFORE the haze samplers and
# accepted for the Vulkan/VR target.
KNOWN_VUIDS = ("VUID-vkCmdDraw-None-08608",
               "VUID-vkCmdDrawIndexed-None-08608",
               "VUID-vkCmdDrawIndexed-None-06886",
               "VUID-VkPipelineLayoutCreateInfo-descriptorType-03016")

ARBITER = re.compile(
    r"\[SPVR:FWDSEL\] forward DRAW material<([^>]*)> technique<([^>]*)> "
    r"permu_stereo<(\d)> pass_stereo<(\d)>")
VUID = re.compile(r"(VUID-[A-Za-z0-9_\-]+)")


################################################################################
# CHILD: the real single-pass node, the real procedural sky, haze off then on.
################################################################################


def _render(outdir):
  from orkengine import lev2
  from orkengine.core import vec3, vec4, mtx4, VarMap
  from ork.testing import headless_app, ensure_parent_dir
  import numpy
  from PIL import Image

  results = {}

  def dir_to_sun(az_deg, el_deg):
    a, e = math.radians(az_deg), math.radians(el_deg)
    return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2'], width=W, height=H) as app:
    ctx = app.ctx
    results["multiview"] = bool(ctx.supports_multiview)
    results["max_views"] = int(ctx.max_multiview_views)
    print("haze_stereo: supports_multiview=%s max_views=%d"
          % (ctx.supports_multiview, ctx.max_multiview_views), flush=True)
    if not ctx.supports_multiview:
      with open(os.path.join(outdir, "child.json"), "w") as f:
        json.dump(results, f, indent=1)
      return 0

    # pure-IPD rig: identical orientation and projection per eye, so the only
    #  thing that can move between the layers is the per-view matrix.
    vrdev = lev2.orkidvr.novr_device()
    vrdev.width = W
    vrdev.height = H
    vrdev.FOVD = FOVD
    vrdev.IPD = IPD
    vrdev.near = 0.1
    vrdev.far = 300000.0
    vrdev.setPoseMatrix("hmd", mtx4.lookAt(vec3(0, EYE_Y, 0), vec3(0, EYE_Y, 100),
                                           vec3(0, 1, 0)))

    params = VarMap()
    params.preset = "FWDPBRSPVR"
    params.SkyboxIntensity = 1.0
    params.SpecularIntensity = 1.0
    params.DiffuseIntensity = 1.0
    params.AmbientLight = vec3(0.15)
    params.DepthFogDistance = float(1e6)
    # a REAL baked environment behind the procedural one: through the warm-up
    #  window the IBL still reads the baked maps and the forward state lambda
    #  dereferences them unconditionally.
    params.SkyboxTexPathStr = "<ork_envmaps2>/cold4k.xir"
    scene = lev2.scenegraph.Scene(params)
    layer = scene.createLayer("std_forward")
    scene.createLayer("depth_prepass")

    pbc = scene.pbr_common
    pbc.enable_skybox = True
    atmo = lev2.SkyAtmosphereData()
    atmo.sky_exposure = SKY_EXPOSURE
    atmo.ibl_crossfade_frames = 0
    atmo.haze_density = HAZE_DENSITY
    atmo.haze_scale_height = HAZE_SCALE_HEIGHT
    atmo.haze_phase_g = HAZE_PHASE_G
    atmo.haze_scatter_tint = vec3(*HAZE_SCATTER_TINT)
    atmo.haze_inscatter_tint = vec3(*HAZE_INSCAT_TINT)
    atmo.haze_max_distance_km = HAZE_MAX_DIST_KM
    atmo.aerial_perspective_enable = False
    pbc.atmosphere = atmo
    pbc.sky_source = "procedural"
    results["sky_source"] = str(pbc.sky_source)

    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    nrm = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    def sphere(name, pos, scale, color):
      drawable = model.createDrawable()
      for subinst in drawable.modelinst.submeshinsts:
        mtl = subinst.material.clone()
        mtl.assignImages(ctx, color=white, normal=nrm, mtlruf=white, doConform=True)
        mtl.baseColor = vec4(color[0], color[1], color[2], 1)
        mtl.metallicFactor = 0.0
        mtl.roughnessFactor = 0.55
        subinst.overrideMaterial(mtl)
      node = scene.createDrawableNodeOnLayers([layer], name, drawable)
      node.worldTransform.translation = pos
      node.worldTransform.scale = scale
      return node

    keep = [sphere("near", vec3(PARALLAX_DIST_M * 0.35, EYE_Y - 0.55, PARALLAX_DIST_M),
                   BALL_ANG_TAN * PARALLAX_DIST_M / BALL_MODEL_R, (0.85, 0.10, 0.70))]
    for (dist, off, col) in LADDER:
      keep.append(sphere("d%d" % int(dist), vec3(dist * off, EYE_Y, dist),
                         BALL_ANG_TAN * dist / BALL_MODEL_R, col))

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = SUN_INTENSITY
    sun.shadowCaster = False
    d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)
    sun.lookAt(vec3(d.x, d.y, d.z) * 1000.0, vec3(0, 0, 0), vec3(0, 1, 0))
    _sun_node = layer.createLightNode("sun", sun)
    scene.lightingmanager.gpuInit(ctx)

    camlut = lev2.CameraDataLut()
    cam = lev2.CameraData()
    cam.perspective(0.1, 300000.0, FOVD)
    cam.lookAt(vec3(0, EYE_Y, 0), vec3(0, EYE_Y, 100), vec3(0, 1, 0))
    # the per-view cull fan-out resolves its camera from "spawncam" outside XR
    #  presentation; without it the scene renders but no cull ever runs.
    camlut.addCamera("spawncam", cam)

    def take(tag):
      for _ in range(FRAMES):
        scene.updateScene(camlut)
        ctx.beginFrame()
        scene.renderOnContext(ctx)
        ctx.endFrame()
      # the eye buffers are read back in a FRESH frame: capturing inside the
      #  frame that produced them leaves them in a host-read layout the
      #  composite then asserts on.
      outnode = scene.compositoroutputnode
      ctx.beginFrame()
      caps, futs = {}, []
      for (nm, left) in (("L", True), ("R", False)):
        rtg = outnode.downsampledEyeRtGroup(left)
        cb = lev2.CaptureBuffer()
        caps[nm] = cb
        futs.append((nm, ctx.FBI.captureAsFormat(rtg.buffer(0), cb, "RGBA8")))
      ctx.endFrame()
      for (nm, fut) in futs:
        ok = fut.wait(caps[nm])
        assert ok, "haze_stereo: capture never landed for %s %s" % (tag, nm)
        cb = caps[nm]
        arr = numpy.array(cb, dtype=numpy.uint8).reshape(cb.height, cb.width, 4)
        path = os.path.join(outdir, "%s_%s.png" % (tag, nm))
        ensure_parent_dir(path)
        Image.fromarray(arr[..., :3]).save(path)
      print("haze_stereo: captured %s (haze armed=%s)"
            % (tag, atmo.aerial_perspective_enable), flush=True)

    take("hazeoff")
    atmo.aerial_perspective_enable = True
    pbc.atmosphere = atmo
    take("hazeon")

    results["validation_armed"] = bool(ctx.validation_armed)
    results["validation_errors"] = int(ctx.validation_errors)

  with open(os.path.join(outdir, "child.json"), "w") as f:
    json.dump(results, f, indent=1)
  return 0


################################################################################
# DRIVER
################################################################################


def _rgb(path):
  import numpy
  from PIL import Image
  return numpy.asarray(Image.open(path).convert("RGB"), dtype=numpy.float64)


def _luma(rgb):
  return (0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]) / 255.0


def _frac(a, b):
  import numpy
  return float((numpy.abs(a - b) > DIFF_EPS).mean())


def _centroid(rgb, templates):
  """the pixel centroid of everything matching one of `templates`, by hue.
  Returns (count, cx, cy) or (count, None, None) if the blob is too small.

  A CENTROID, not a cross-correlation: the near sphere is ~110 px inside a box
  that is otherwise sky, and the sky carries enough horizontal gradient that
  sliding the whole box to align the sphere costs more than it gains -- a
  correlation search on this rig answers 0 px while the sphere is provably
  displaced 8. The centroid asks only about the object."""
  import numpy
  mx = rgb.max(axis=2)
  norm = rgb / numpy.maximum(mx, 1.0)[..., None]
  hit = numpy.zeros(rgb.shape[:2], dtype=bool)
  for t in templates:
    tmpl = numpy.array(t, dtype=numpy.float64) / max(t)
    hit |= (numpy.linalg.norm(norm - tmpl[None, None, :], axis=2) < HUE_TOL)
  hit &= (mx - rgb.min(axis=2)) > CHROMA_MIN
  n = int(hit.sum())
  if n < MIN_BLOB_PX:
    return n, None, None
  ys, xs = numpy.nonzero(hit)
  return n, float(xs.mean()), float(ys.mean())


def _run_child(outdir):
  os.makedirs(outdir, exist_ok=True)
  env = dict(os.environ)
  env["ORKID_VULKAN_VALIDATE"] = "2"
  env.pop("ORKID_GATE0_FORCE_MONO_TEK", None)
  env.pop("ORKID_SPVR_NO_MULTIVIEW", None)
  argv = [sys.executable, os.path.abspath(__file__), "--render", outdir]
  proc = subprocess.run(argv, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=600)
  out = proc.stdout.decode("utf-8", "replace")
  rec = {}
  cjson = os.path.join(outdir, "child.json")
  if os.path.exists(cjson):
    rec = json.load(open(cjson))
  return proc.returncode, out, rec


def main():
  import numpy
  from ork.testing import verdict

  outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
      tempfile.gettempdir(), "spvr_haze_stereo")
  os.makedirs(outdir, exist_ok=True)
  t0 = time.time()
  fails = []

  rc, out, rec = _run_child(outdir)
  print(out, flush=True)
  if rc != 0:
    return verdict(False, "child exited rc=%d (a missing <ork_envmaps2>/cold4k.xir "
                          "aborts it before any capture)" % rc)
  if not rec.get("multiview"):
    print("SKIP: device reports multiview=%s max_views=%s -- single-pass stereo has no "
          "meaningful answer here" % (rec.get("multiview"), rec.get("max_views")), flush=True)
    return 0
  if rec.get("sky_source") != "procedural":
    fails.append("the scene reports sky_source=%s -- this file measures the PROCEDURAL arm"
                 % rec.get("sky_source"))

  # ---- leg (a): SELECTION, from the bind-time arbiter
  seen = {}
  for m in ARBITER.finditer(out):
    seen[m.group(2)] = (m.group(1), int(m.group(3)), int(m.group(4)))
  print("LEG_SELECTION techniques=%s" % sorted(seen.keys()), flush=True)
  st = [t for t in seen if t.endswith("_ST")]
  mono = [t for t in seen if not t.endswith("_ST")]
  if not st:
    fails.append("SELECTION: no forward DRAW took an _ST technique (%s) -- the stereo arm "
                 "of the forward bind path, haze binds included, never ran"
                 % (", ".join(sorted(seen)) or "no announced technique"))
  for t in st:
    _mtl, permu, pas = seen[t]
    if permu != 1 or pas != 1:
      fails.append("SELECTION: %s bound with permu_stereo<%d> pass_stereo<%d> -- both must "
                   "be 1" % (t, permu, pas))
  if mono:
    fails.append("SELECTION: forward DRAW(s) took the MONO technique(s) %s inside the stereo "
                 "pass -- those draws write one image into both eye layers" % ", ".join(mono))

  # ---- the captures
  try:
    img = {(tag, eye): _rgb(os.path.join(outdir, "%s_%s.png" % (tag, eye)))
           for tag in ("hazeoff", "hazeon") for eye in ("L", "R")}
  except Exception as ex:
    return verdict(False, "captures unreadable (%s)" % ex)
  lum = {k: _luma(v) for k, v in img.items()}

  # ---- leg (c): NOT DEGENERATE
  for (tag, eye), l in sorted(lum.items()):
    rng = float(l.max() - l.min())
    blk = float((l < 1.0e-6).mean())
    print("LEG_SANE %-7s %s mean=%.4f range=%.4f black_frac=%.4f"
          % (tag, eye, float(l.mean()), rng, blk), flush=True)
    if not numpy.isfinite(l).all():
      fails.append("%s %s: non-finite pixels" % (tag, eye))
    if rng < RANGE_FLOOR:
      fails.append("%s %s: the layer is FLAT (range %.4f < %.4f) -- a flat layer matches "
                   "any other flat layer, so every other leg would be meaningless"
                   % (tag, eye, rng, RANGE_FLOOR))
    if blk > BLACK_FRAC_CEIL:
      fails.append("%s %s: %.1f%% of the layer is pure black (ceiling %.1f%%) -- what a NaN "
                   "looks like after the tonemapper"
                   % (tag, eye, 100.0 * blk, 100.0 * BLACK_FRAC_CEIL))

  # ---- leg (b): HAZE PRESENT, PER EYE. Counted in PIXELS: only geometry may
  #      move (the v1 skybox runs no artist haze), and geometry is a few hundred
  #      pixels of a frame that is otherwise sky.
  pops = {}
  for eye in ("L", "R"):
    d = numpy.abs(img[("hazeon", eye)] - img[("hazeoff", eye)]).max(axis=2)
    n = int((d > HAZE_PIX_EPS).sum())
    peak = float(d.max())
    pops[eye] = n
    print("LEG_HAZE %s moved_px=%d peak_levels=%.1f" % (eye, n, peak), flush=True)
    if n < HAZE_MIN_PX:
      fails.append("HAZE %s: only %d pixel(s) moved when the haze was armed (floor %d) -- "
                   "this eye's draws never took the armed uniform" % (eye, n, HAZE_MIN_PX))
    if peak < HAZE_MIN_LEVELS:
      fails.append("HAZE %s: the strongest pixel moved %.1f levels (floor %.1f)"
                   % (eye, peak, HAZE_MIN_LEVELS))
  bal = abs(pops["L"] - pops["R"]) / max(max(pops.values()), 1)
  print("LEG_HAZE balance L=%d R=%d mismatch=%.4f" % (pops["L"], pops["R"], bal), flush=True)
  if bal > HAZE_EYE_BALANCE:
    fails.append("HAZE: the two eyes were hazed by different amounts (mismatch %.3f > %.3f) "
                 "-- one eye's forward pass is not arming the seam" % (bal, HAZE_EYE_BALANCE))

  # ---- leg (d): BOUNDED HORIZONTAL PARALLAX
  L, R = lum[("hazeon", "L")], lum[("hazeon", "R")]
  h = L.shape[0]
  sky_band = slice(0, h // 6)                 # above every sphere in the rig
  n_diff = int((numpy.abs(L - R) > DIFF_EPS).sum())
  f_sky = _frac(L[sky_band], R[sky_band])
  print("LEG_PARALLAX differing_px=%d sky_band_frac=%.5f" % (n_diff, f_sky), flush=True)
  if n_diff < PARALLAX_MIN_PX:
    fails.append("PARALLAX: the two eye layers carry the SAME image (%d differing px < %d) "
                 "-- the scene rendered mono into both eyes" % (n_diff, PARALLAX_MIN_PX))
  if f_sky > SKY_AGREE_CEIL:
    fails.append("PARALLAX: the SKY BAND differs between the eyes (frac %.5f > %.5f) -- a "
                 "sky at infinity cannot move under a pure eye translation, so this is a "
                 "view ray picking up the eye position" % (f_sky, SKY_AGREE_CEIL))

  # the near sphere must be DISPLACED HORIZONTALLY, by a bounded amount, in both
  # takes (haze may not move geometry); the kilometre ladder must not move at all.
  shifts = {}
  for tag in ("hazeoff", "hazeon"):
    got = {}
    for eye in ("L", "R"):
      got[eye] = _centroid(img[(tag, eye)], (NEAR_COLOR,))
    if got["L"][1] is None or got["R"][1] is None:
      fails.append("PARALLAX: the near sphere could not be located by colour in %s "
                   "(%d/%d px) -- the rig, not the engine, is broken"
                   % (tag, got["L"][0], got["R"][0]))
      continue
    dx = got["L"][1] - got["R"][1]
    dy = got["L"][2] - got["R"][2]
    shifts[tag] = (dx, dy)
    print("LEG_PARALLAX near %-7s px L=%d R=%d dx=%+.2f dy=%+.2f"
          % (tag, got["L"][0], got["R"][0], dx, dy), flush=True)
    if not (PARALLAX_MIN_PX_SHIFT <= abs(dx) <= PARALLAX_MAX_PX):
      fails.append("PARALLAX %s: the near sphere is displaced %+.2f px between the eyes "
                   "(must be between %.1f and %d) -- either one view matrix served both "
                   "eyes or the per-eye projection is wrong"
                   % (tag, dx, PARALLAX_MIN_PX_SHIFT, PARALLAX_MAX_PX))
    if abs(dy) > PARALLAX_DY_MAX:
      fails.append("PARALLAX %s: the near sphere is displaced %+.2f px VERTICALLY (ceiling "
                   "%.1f) -- an IPD displacement is horizontal by construction"
                   % (tag, dy, PARALLAX_DY_MAX))
  if len(shifts) == 2:
    drift = abs(shifts["hazeon"][0] - shifts["hazeoff"][0])
    print("LEG_PARALLAX near haze_drift=%.3f px" % drift, flush=True)
    if drift > PARALLAX_DY_MAX:
      fails.append("PARALLAX: arming the haze moved the near sphere's parallax by %.2f px -- "
                   "the seam is a shading term and may not touch geometry" % drift)

  # the ladder is read off the haze-off take, where those spheres still carry
  # albedo to be found by; the view matrices under test are the same in both.
  fn, fxL, fyL = _centroid(img[("hazeoff", "L")], tuple(c for (_d, _o, c) in LADDER))
  _fn2, fxR, fyR = _centroid(img[("hazeoff", "R")], tuple(c for (_d, _o, c) in LADDER))
  if fxL is None or fxR is None:
    fails.append("PARALLAX: the distance ladder could not be located by colour (%d px)" % fn)
    fdx = fdy = float("nan")
  else:
    fdx, fdy = fxL - fxR, fyL - fyR
    print("LEG_PARALLAX far  px=%d dx=%+.2f dy=%+.2f" % (fn, fdx, fdy), flush=True)
    if max(abs(fdx), abs(fdy)) > FAR_SHIFT_MAX:
      fails.append("PARALLAX far: the kilometre-distant spheres moved (%+.2f,%+.2f) px "
                   "between the eyes (ceiling %.1f); at IPD %.2f m they subtend far under a "
                   "pixel, so a shift there is a projection error, not parallax"
                   % (fdx, fdy, FAR_SHIFT_MAX, IPD))
  sx = shifts.get("hazeon", (float("nan"), float("nan")))[0]
  sy = shifts.get("hazeon", (float("nan"), float("nan")))[1]

  # ---- leg (e): no NEW validation class
  vuids = sorted(set(VUID.findall(out)))
  novel = [v for v in vuids if v not in KNOWN_VUIDS]
  armed = rec.get("validation_armed", False)
  print("VALIDATION armed=%s errors=%d classes=%s"
        % (armed, int(rec.get("validation_errors", -1)), vuids or ["none"]), flush=True)
  if not armed:
    fails.append("validation layer was not armed -- the error classes prove nothing")
  if novel:
    fails.append("validation raised class(es) this rig has never raised: %s" % ", ".join(novel))

  dt = time.time() - t0
  detail = ("st_techniques=%s haze_px L=%d R=%d near_parallax dx=%+.2f dy=%+.2f "
            "far_parallax dx=%+.2f dy=%+.2f differing_px=%d sky_frac=%.5f vuids=%s (%.1fs)"
            % (",".join(sorted(st)) or "none", pops["L"], pops["R"], sx, sy,
               fdx, fdy, n_diff, f_sky, ",".join(vuids) or "none", dt))
  if fails:
    detail += " | failed: " + "; ".join(fails[:6])
  return verdict(len(fails) == 0, detail)


if __name__ == "__main__":
  if len(sys.argv) > 2 and sys.argv[1] == "--render":
    sys.exit(_render(sys.argv[2]))
  sys.exit(main())
