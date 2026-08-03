#!/usr/bin/env ork.python
################################################################################
# S0 frame-prologue gate (JUL22 skylight milestone).
#
# The ForwardNode's view-independent work (light enumeration + lighting-SSBO
# packing, spotlight shadow-map renders, env-probe cube captures) runs in
# RenderCompositingNode::renderPrologue — exactly ONCE per composited frame,
# fail-loudly stamped on Context::GetTargetFrame():
#   * a double prologue for one target frame OrkAsserts (process aborts),
#   * _render_top without a same-frame prologue OrkAsserts (process aborts).
#
# This gate renders a mono offscreen scene exercising BOTH absorbed sub-passes
# (one shadow-casting spotlight WITH a depth cookie + one REFLECTION light
# probe) for SETTLE_FRAMES, then captures. PASS requires ALL of:
#   * neither prologue assertion fired across the settle window (an
#     aborted/skipped prologue dies before the verdict → CRASH_NO_VERDICT),
#   * the capture is a real, non-flat frame (max>0, range>=0.02),
#   * a REAL spot shadow lands on the LIT ground receiver: a fixed shadowed
#     crop (the model's shadow disc) is vastly darker than a fixed lit crop
#     (open cookie-lit ground) — lit/shadow mean ratio >= RATIO_MIN, with the
#     lit crop above LIT_MIN.
#
# The shadow oracle is the S0.1 regression tripwire (skylight §5b D4): S0
# relocated _update_shadow_maps into the prologue, whose CPD carries no layer
# set — so enqueueLayerToRenderQueue's HasLayer gate admitted ZERO renderables
# and spot shadow maps rendered EMPTY. An empty shadow map means shadow_factor
# is 1.0 everywhere in the cone (the whole receiver is cookie-lit, no dark
# disc) → the shadow crop reads as bright as the lit crop → ratio ~1 → this
# gate FAILS. The prior _V3 (unlit) receiver made the empty map invisible;
# a LIT _V4 ground plane surfaces it. Verified: reverting the D4 fix drops the
# shadow crop from ~0 to lit-parity and the ratio below RATIO_MIN.
#
# Machine verdict line via the ork.testing protocol, emitted BEFORE teardown.
#
# Lifecycle: ComponentizedApplication + StandardSceneGraphComponent — the
# proven offscreen scenegraph-capture pattern (the S0 baseline captures).
# capture_app was tried first and cannot yet host a scenegraph composite (its
# UI-canvas app never receives the SGVP frame offscreen; migrating it is
# harness work outside this slice) — hand-rolled lifecycle reason, stated per
# the testing law. D1 (this scene class SIGSEGV'ing in teardown AFTER the
# verdict, rc=139) is FIXED as of JUL24 — this gate now exits rc=0. The
# verdict-before-teardown ordering below stays regardless (it is what makes a
# teardown regression legible); test_teardown_rc_gate.py is the tripwire that
# actually scores process exit.
#
# DMVR coverage lives in test_dmvr_capture_gate.py (2026-07-23). The earlier
# claim here that "NoVrDevice + FWDPBRVRDM offscreen stalls after one frame"
# was a MISDIAGNOSIS — the compositor renders continuously in that config;
# only onGpuPostFrame doesn't fire in the createScene path (D2 family).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys

# prepend THIS checkout's scripts dir so ork.testing / ork.app resolve from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine.core import vec3, vec4, mtx4, CrcStringProxy, Path as CorePath  # core FIRST
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.environ.get("TMPDIR", "/tmp"), "fwd_prologue_gate", "capture.png")
SETTLE_FRAMES = 240   # model/skybox residency + ≥1 full probe capture cycle

# Fixed crop rects (x0,y0,x1,y1) in the rendered frame — measured from the
# composition (camera (5,8,10)->(0,2,0), spot from (-7,16,-9) aimed at ground):
# the model floats at (0,2,0) and images at screen-center (a black disc); its
# shadow disc lands on the cookie-lit ground BEYOND it (higher in screen).
# CROP_SHADOW is the shadow disc interior (dark only when the shadow map is
# populated); CROP_LIT is open unoccluded cookie-lit ground below the model.
# NB: do NOT put CROP_SHADOW on the model disc itself — that is black
# regardless of the shadow map and would mask D4 (the very bug this catches).
CROP_SHADOW = (322, 85, 368, 128)    # model's shadow disc on the lit ground
CROP_LIT    = (360, 205, 425, 250)   # open cookie-lit ground, unoccluded
RATIO_MIN   = 1.5    # lit/shadow mean ratio proving a real spot shadow (empty
                     # shadow map → ~1; real shadow → hundreds)
LIT_MIN     = 50.0   # lit crop floor — guards against an all-dark frame gaming
                     # the ratio


def _crop_mean(arr, rect):
  x0, y0, x1, y1 = rect
  return float(arr[y0:y1, x0:x1].mean())


class GateApp(ComponentizedApplication):

  def __init__(self, out_path):
    super().__init__()
    self._out_path = out_path
    self._frame = 0
    self._built = False
    self._built_frame = 0
    self._probe_kicked = False
    self._cap = 0
    self._done = False
    self.result = None
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(5, 8, 10), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant="_V4",   # LIT ground receiver — surfaces the spot shadow
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

    # occluder — on fwd_layers (std_forward + depth_prepass): a drawable only
    # spot-shadows if it plays the depth_prepass role.
    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.drawable_model = model.createDrawable()
    self.modelnode = SGC.scenegraph.createDrawableNodeOnLayers(
        SGC.fwd_layers, "model-node", self.drawable_model)
    self.modelnode.worldTransform.translation = vec3(0, 2, 0)

    # ONE static shadow-casting spotlight aimed at the ground so its cone lights
    # the receiver and the model casts a visible shadow disc onto it.
    # shadowCaster=True REQUIRES a depth cookie slice — without it
    # _update_shadow_maps trips OrkAssert(depcookie).
    color_cookies = lev2.TextureArray(w=512, h=512, slices=1, fmt=tokens.RGB8, mipmapped=True)
    depth_cookies = lev2.TextureArray(w=512, h=512, slices=1, fmt=tokens.Z32F, mipmapped=True)
    color_cookies.needsRadianceCache = False
    cookie1 = color_cookies.load("src://effect_textures/L0D.png")
    ctx.TXI.updateTextureArray(color_cookies)
    self.color_cookies = color_cookies
    self.depth_cookies = depth_cookies

    spot = lev2.DynamicSpotLight()
    spot.data.color = vec3(30000, 30000, 30000)
    spot.data.fovy = 45
    spot.data.range = 100.0
    spot.data.shadowBias = 1e-5
    spot.data.shadowMapSize = 2048
    spot.colorCookie = cookie1
    spot.depthCookie = depth_cookies.slice(0)
    spot.shadowCaster = True
    spot.lookAt(vec3(-7, 16, -9), vec3(0, 0, 0), vec3(0, 1, 0))
    self.spot = spot
    self.lnode = SGC.layer_fwd.createLightNode("spotlight0", spot)

    # ONE reflection probe over the model
    probe = lev2.LightProbe()
    probe.type = tokens.REFLECTION
    probe.imageDim = 128
    probe.worldMatrix = mtx4.transMatrix(0, 2, 0)
    probe.name = "probe1"
    self.probe = probe
    self.probe_node = SGC.layer_fwd.createLightProbeNode("probe", probe)

    lmgr = SGC.scenegraph.lightingmanager
    lmgr.spot_cookies_color = color_cookies
    lmgr.spot_cookies_depth = depth_cookies
    lmgr.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    if not self._probe_kicked:
      self.probe.invalidate()   # exactly ONE probe recapture cycle — static after
      self._probe_kicked = True

  def _rtg(self, ctx):
    # offscreen, the scene composites into the SGVP's own rtgroup
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._built_frame = self._frame
      return
    if self._frame < self._built_frame + SETTLE_FRAMES or self._done:
      return
    if not self._cap:
      ctx.FBI.captureToFile(self._rtg(ctx).buffer(0), CorePath(self._out_path))
      self._cap = self._frame
      return
    if self._frame < self._cap + 20:
      return   # let the async write land
    # verdict evidence computed + FLUSHED here, BEFORE signalExit lets the loop
    # exit into teardown: any future teardown crash must leave the verdict on
    # record — the verdict-before-teardown protocol.
    import numpy
    from PIL import Image
    mean = -1.0
    mx = -1
    rng = -1.0
    m_lit = -1.0
    m_shadow = -1.0
    ratio = -1.0
    if os.path.isfile(self._out_path) and os.path.getsize(self._out_path) > 0:
      arr = numpy.asarray(Image.open(self._out_path).convert("RGB"), dtype=numpy.float32)
      g = arr.mean(2) / 255.0
      rng = float(g.max() - g.min())
      mean = float(arr.mean())
      mx = int(arr.max())
      m_lit = _crop_mean(arr, CROP_LIT)
      m_shadow = _crop_mean(arr, CROP_SHADOW)
      ratio = m_lit / max(m_shadow, 1e-3)
    self.result = {"mean": mean, "max": mx, "range": rng,
                   "lit": m_lit, "shadow": m_shadow, "ratio": ratio}
    print("CAPTURE=%s mean=%.4f max=%d range=%.4f lit=%.1f shadow=%.1f ratio=%.2f"
          % (self._out_path, mean, mx, rng, m_lit, m_shadow, ratio), flush=True)
    from ork.testing import verdict
    # a real frame with a real spot shadow: pixels present AND non-flat AND the
    # shadow disc is far darker than open lit ground (D4 tripwire).
    ok = (mx > 0) and (rng >= 0.02) and (m_lit >= LIT_MIN) and (ratio >= RATIO_MIN)
    self._verdict_code = verdict(
        ok, "prologue mono spot+probe mean=%.2f max=%d range=%.3f lit=%.1f shadow=%.1f ratio=%.1f %s"
        % (mean, mx, rng, m_lit, m_shadow, ratio, self._out_path))
    self._done = True
    self.ezapp.signalExit()


def main():
  from ork.testing import ensure_parent_dir, Watchdog
  ensure_parent_dir(OUT)
  wd = Watchdog(240.0, label="fwd_prologue_gate").arm()
  app = GateApp(os.path.abspath(OUT))
  app.ezapp.mainThreadLoop()
  wd.disarm()
  code = getattr(app, "_verdict_code", None)
  if code is None:
    # loop exited without reaching the capture path (e.g. a prologue assertion
    # aborted rendering) — no verdict was emitted; fail loudly.
    from ork.testing import verdict
    code = verdict(False, "loop exited before capture (no frame evidence)")
  app.ezapp.shutdown()
  sys.exit(code)


if __name__ == "__main__":
  main()
