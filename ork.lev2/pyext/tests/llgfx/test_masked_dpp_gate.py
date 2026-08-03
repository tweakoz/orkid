#!/usr/bin/env ork.python
################################################################################
# SKYLIGHT lane A gate (A3): MASKED (alpha-tested) depth prepass.
#
# Mono offscreen scene: a lit ground plane (grid _V4 — full forward lighting
# path) + an alpha-cutoff textured quad CASTER (RGBA texture generated at
# runtime via PIL: opaque frame, fully-transparent circular hole; material
# alphaCutoff=0.5, selecting the FWD_DEPTHPREPASS_MASKED technique) + ONE
# directional sun angled so the quad's shadow falls on visible ground.
#
# SHADOW-ISOLATING oracle: TWO captures in one run — UNSHADOWED
# (sun.shadowCaster=False; cascade_count=0 -> shadow factor 1) and SHADOWED
# (shadowCaster toggled True live). Their darkening diff is the PURE sun-shadow
# contribution: everything shadow-independent (the caster's own screen
# footprint, its probe/specular reflection on the glossy ground — both dark
# and caster-shaped, both of which fooled fixed-crop and per-cluster-centroid
# oracles) cancels exactly. On the diff mask (closed+opened, small clusters
# dropped):
#   holes = binary_fill_holes(mask) & ~mask   # the ENCLOSED bright hole
#   PASS: shadow_px >= SHADOW_MIN_PX (a real shadow exists)
#         and hole_px >= HOLE_MIN_PX
#         and mean(shadowed[holes]) / mean(shadowed[mask]) >= RATIO_MIN
# Pre-A3 the depth prepass wrote depth unconditionally (vif_DPP carried no uv;
# the DPP fragment never sampled/discarded), so the shadow is a SOLID blob:
# hole_px ~ 0 — proven-failing oracle. (Measured post-A3: shadow ~10.4k px,
# hole ~3.8k px, ratio ~22.)
#
# Machine verdict line via the ork.testing protocol, emitted BEFORE teardown
# (known pre-existing defect D1: this scene class can SIGSEGV during
# mainThreadLoop teardown AFTER the verdict is flushed — verdict-first).
#
# Lifecycle: ComponentizedApplication + StandardSceneGraphComponent — the
# proven offscreen scenegraph-capture pattern (same hand-rolled-lifecycle
# reason as test_sun_cascades_gate.py: capture_app cannot yet host a
# scenegraph composite offscreen).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys

# prepend THIS checkout's scripts dir so ork.testing / ork.app resolve from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine.core import vec2, vec3, vec4, CrcStringProxy, Path as CorePath  # core FIRST
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

OUTDIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.environ.get("TMPDIR", "/tmp"), "masked_dpp_gate")
SETTLE_FRAMES = 300   # texture/skybox residency + warm-up (residency-race margin)
TOGGLE_FRAMES = 80    # settle window after enabling shadow casting

DIFF_THRESH   = 25.0  # per-pixel darkening (unshadowed - shadowed) counting as shadow
MIN_CLUSTER   = 1500  # px — keeps axis lines / speckle out of the shadow mask
SHADOW_MIN_PX = 3000  # a real shadow must exist at all
HOLE_MIN_PX   = 800   # enclosed bright hole area (masked DPP punches ~3.8k px here)
RATIO_MIN     = 2.0   # hole brightness vs shadow darkness (measured ~22)


def _make_cutout_png(path):
  """Opaque colored card with a fully-transparent circular hole in the middle."""
  import numpy
  from PIL import Image as PILImage
  N = 256
  yy, xx = numpy.mgrid[0:N, 0:N]
  r = numpy.sqrt((xx - N / 2.0) ** 2 + (yy - N / 2.0) ** 2)
  a = numpy.where(r < N * 0.30, 0, 255).astype(numpy.uint8)
  img = numpy.zeros((N, N, 4), dtype=numpy.uint8)
  img[..., 0] = 90    # muted warm card
  img[..., 1] = 140
  img[..., 2] = 60
  img[..., 3] = a
  PILImage.fromarray(img, "RGBA").save(path)
  return path


class GateApp(ComponentizedApplication):

  def __init__(self, outdir):
    super().__init__()
    self._outdir = outdir
    self._frame = 0
    self._built = False
    self._phase = 0        # 0: settle, 1: cap unshadowed, 2: settle, 3: cap shadowed, 4: verdict
    self._phase_frame = 0
    self._done = False
    self._caps = {}
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(5, 8, 10), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant="_V4",
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0.1),
        })
    self.createEzApp(width=640, height=480, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = True

    # alpha-cutoff CASTER: horizontal 4x4 quad, RGBA cutout texture, cutoff 0.5.
    # alphaCutoff > 0 selects the MASKED depth-prepass technique — the hole
    # must neither z-occlude nor shadow.
    tex_png = _make_cutout_png(os.path.join(self._outdir, "cutout_rgba.png"))
    material = lev2.PBRMaterial()
    cutout = lev2.Image.createFromFile(tex_png)
    nrmap = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")
    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    material.assignImages(ctx, color=cutout, normal=nrmap, mtlruf=white, doConform=True)
    material.alphaCutoff = 0.5
    material.doubleSided = True
    # explicit gpuInit: the FIRST pass to touch this drawable is the prologue
    # cascade depth pass, and an un-inited material segfaults pipeline lookup.
    material.gpuInit(ctx)
    self.material = material

    quad = lev2.meshutil.SubMesh.createFromDict({
        "vertices": [
          {"p": vec3(-2, 0, -2), "n": vec3(0, 1, 0), "b0": vec3(1, 0, 0), "uv0": vec2(0, 0)},
          {"p": vec3( 2, 0, -2), "n": vec3(0, 1, 0), "b0": vec3(1, 0, 0), "uv0": vec2(1, 0)},
          {"p": vec3( 2, 0,  2), "n": vec3(0, 1, 0), "b0": vec3(1, 0, 0), "uv0": vec2(1, 1)},
          {"p": vec3(-2, 0,  2), "n": vec3(0, 1, 0), "b0": vec3(1, 0, 0), "uv0": vec2(0, 1)},
        ],
        "faces": [[0, 1, 2], [0, 2, 3]],
    })
    self.prim = lev2.RigidPrimitive(quad, ctx)
    self.drawable_quad = self.prim.createDrawable(material)
    # a drawable sun-shadows only if it plays the depth_prepass role -> fwd_layers.
    self.quadnode = SGC.scenegraph.createDrawableNodeOnLayers(
        SGC.fwd_layers, "cutout_caster", self.drawable_quad)
    self.quadnode.worldTransform.translation = vec3(2.0, 4.5, 1.0)

    # THE sun — starts LIT but NOT SHADOW-CASTING (cascade_count=0 path);
    # casting toggles on live between the two captures so the diff isolates
    # the pure shadow term.
    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 4.0
    sun.data.shadowBias = 2e-4
    sun.data.shadowMapSize = 2048
    sun.data.shadowCascadeCount = 3
    sun.data.shadowMaxDistance = 250.0
    sun.data.pcfDither = 1.0
    sun.shadowCaster = False
    sun.lookAt(vec3(30, 50, 20), vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    lmgr = SGC.scenegraph.lightingmanager
    lmgr.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass  # static scene — deterministic captures (toggle happens on the gpu thread)

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _capture(self, ctx, name):
    path = os.path.join(self._outdir, "masked_dpp_%s.png" % name)
    ctx.FBI.captureToFile(self._rtg(ctx).buffer(0), CorePath(path))
    self._caps[name] = path

  def _verdict(self):
    import numpy
    from PIL import Image
    import scipy.ndimage as ndi
    from ork.testing import verdict

    for name in ("unshadowed", "shadowed"):
      p = self._caps.get(name)
      if not (p and os.path.isfile(p) and os.path.getsize(p) > 0):
        self._verdict_code = verdict(False, "masked dpp gate | %s capture MISSING" % name)
        return
    uns = numpy.asarray(Image.open(self._caps["unshadowed"]).convert("RGB"), dtype=numpy.float32).mean(axis=2)
    shd = numpy.asarray(Image.open(self._caps["shadowed"]).convert("RGB"), dtype=numpy.float32).mean(axis=2)
    if uns.shape != shd.shape or int(uns.max()) == 0:
      self._verdict_code = verdict(False, "masked dpp gate | bad captures shapes=%s/%s max=%d"
                                   % (uns.shape, shd.shape, int(uns.max())))
      return

    diff = (uns - shd) > DIFF_THRESH
    diff = ndi.binary_closing(diff, iterations=2)   # bridge grid-line gaps in the shadow ring
    diff = ndi.binary_opening(diff, iterations=2)   # drop speckle
    labels, nlab = ndi.label(diff)
    mask = numpy.zeros_like(diff)
    for i in range(1, nlab + 1):
      ys, xs = numpy.where(labels == i)
      if len(xs) >= MIN_CLUSTER:
        mask[ys, xs] = True
    shadow_px = int(mask.sum())
    filled = ndi.binary_fill_holes(mask)
    holes = filled & ~mask
    hole_px = int(holes.sum())
    hole_mean = float(shd[holes].mean()) if hole_px else 0.0
    shadow_mean = float(shd[mask].mean()) if shadow_px else 1.0
    ratio = hole_mean / max(shadow_mean, 1e-3)
    ok = (shadow_px >= SHADOW_MIN_PX) and (hole_px >= HOLE_MIN_PX) and (ratio >= RATIO_MIN)
    self._verdict_code = verdict(
        ok,
        "masked dpp gate | shadow_px=%d(min %d) hole_px=%d(min %d) hole=%.1f "
        "shadow=%.1f ratio=%.2f(min %.2f) | %s %s"
        % (shadow_px, SHADOW_MIN_PX, hole_px, HOLE_MIN_PX, hole_mean,
           shadow_mean, ratio, RATIO_MIN,
           self._caps["unshadowed"], self._caps["shadowed"]))

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._phase_frame = self._frame
      return
    if self._done:
      return
    if self._phase == 0:
      if self._frame >= self._phase_frame + SETTLE_FRAMES:
        self._capture(ctx, "unshadowed")
        self._phase = 1
        self._phase_frame = self._frame
    elif self._phase == 1:
      if self._frame >= self._phase_frame + 20:  # let the async write land
        self.sun.shadowCaster = True             # LIVE toggle: cascades start rendering
        self._phase = 2
        self._phase_frame = self._frame
    elif self._phase == 2:
      if self._frame >= self._phase_frame + TOGGLE_FRAMES:
        self._capture(ctx, "shadowed")
        self._phase = 3
        self._phase_frame = self._frame
    elif self._phase == 3:
      if self._frame >= self._phase_frame + 20:
        # verdict evidence computed + FLUSHED BEFORE teardown (D1 protocol)
        self._verdict()
        self._done = True
        self.ezapp.signalExit()


def main():
  from ork.testing import Watchdog
  os.makedirs(OUTDIR, exist_ok=True)
  wd = Watchdog(300.0, label="masked_dpp_gate").arm()
  app = GateApp(os.path.abspath(OUTDIR))
  app.ezapp.mainThreadLoop()
  wd.disarm()
  code = getattr(app, "_verdict_code", None)
  if code is None:
    from ork.testing import verdict
    code = verdict(False, "loop exited before captures (no frame evidence)")
  app.ezapp.shutdown()
  sys.exit(code)


if __name__ == "__main__":
  main()
