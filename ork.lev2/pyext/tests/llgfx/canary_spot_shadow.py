#!/usr/bin/env ork.python
################################################################################
# CANARY PRODUCER (committed 2026-07-23, ex-S0 scratch): static spotlight
# shadow-map capture. Offscreen, deterministic (fixed light pose, no per-frame
# animation). This is the CANONICAL producer of the blessed spot_shadow.png
# byte-identity baseline recorded in ork.dox/gfx/SKYLIGHT_SHADOWS_JUL22.md —
# run WARM (twice, sha the second) with ORKID_VR_DRIVER unset.
# usage: canary_spot_shadow.py [out.png] [stats.json]
# DO NOT edit scene content without re-blessing the recorded sha.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys, hashlib, json
import numpy
from PIL import Image as PILImage
from orkengine.core import vec3, vec4, CrcStringProxy, Path as CorePath
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

OUT = sys.argv[1] if len(sys.argv) > 1 else "/tmp/s0_spot_shadow.png"
STATS_OUT = sys.argv[2] if len(sys.argv) > 2 else None
SETTLE_FRAMES = 90     # fixed frame budget after light/model placement -- deterministic

_STATUS = os.environ.get("S0_SPOT_STATUS", "/tmp/s0_spot_shadow_status.txt")
def _note(s):
  try:
    with open(_STATUS, "a") as f:
      f.write(s + "\n"); f.flush()
  except Exception:
    pass


class SpotShadowApp(ComponentizedApplication):
  def __init__(self, out_path):
    super().__init__()
    self._out_path = out_path
    self._frame = 0
    self._built = False
    self._built_frame = 0
    self._cap = 0
    self._done = False
    self._retries = 0
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(5, 8, 10), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant="_V3",
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0.4),
        })
    self.createEzApp(width=640, height=480, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  def _onGpuInit(self, ctx):
    self._ctx = ctx
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    try:
      SGC.pbr_common.enable_skybox = True
    except Exception as e:
      _note("enable_skybox skipped: %r" % e)
    SGC.grid_data.modcolor = vec3(0.3)
    SGC.grid_data.intensityA = 0.1
    SGC.grid_data.intensityB = 0.2
    SGC.grid_data.intensityC = 0
    SGC.grid_data.intensityD = 0
    SGC.grid_data.lineWidth = 0.025

    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.drawable_model = model.createDrawable()
    self.modelnode = SGC.scenegraph.createDrawableNodeOnLayers(
        SGC.fwd_layers, "model-node", self.drawable_model)
    self.modelnode.worldTransform.scale = 1
    self.modelnode.worldTransform.translation = vec3(0, 2, 0)

    # ONE static shadow-casting spotlight, fixed pose (no time-driven motion).
    # shadowCaster=True REQUIRES a depthCookie render-target slice -- omitting it
    # trips OrkAssert(depcookie) in ForwardPbrNodeImpl::_update_shadow_maps
    # (fwdnode_impl_sub.cpp:237); mirrors the cookie setup in
    # spotlight_rigid_model.py (StdSpotLight).
    color_cookies = lev2.TextureArray(w=1024, h=1024, slices=1, fmt=tokens.RGB8, mipmapped=True)
    depth_cookies = lev2.TextureArray(w=1024, h=1024, slices=1, fmt=tokens.Z32F, mipmapped=True)
    color_cookies.needsRadianceCache = False
    cookie1 = color_cookies.load("src://effect_textures/L0D.png")
    ctx.TXI.updateTextureArray(color_cookies)
    depth1 = depth_cookies.slice(0)
    self.color_cookies = color_cookies
    self.depth_cookies = depth_cookies

    self.spot_light = lev2.DynamicSpotLight()
    self.spot_light.data.color = vec3(100, 2500, 100)
    self.spot_light.data.fovy = 55
    self.spot_light.data.range = 100.0
    self.spot_light.data.shadowBias = 1e-5
    self.spot_light.data.shadowMapSize = 2048
    self.spot_light.colorCookie = cookie1
    self.spot_light.depthCookie = depth1
    self.spot_light.shadowCaster = True
    self.spot_light.lookAt(
        vec3(6, 15, 6),   # eye (fixed pose)
        vec3(0, 2, 0),    # tgt (the model)
        vec3(0, 1, 0))    # up
    self.lnode = SGC.layer_fwd.createLightNode("spotlight0", self.spot_light)

    lmgr = SGC.scenegraph.lightingmanager
    lmgr.spot_cookies_color = color_cookies
    lmgr.spot_cookies_depth = depth_cookies
    _note("scene built")

  def _onUpdate(self, updinfo):
    pass   # nothing time-varying -- fully static scene, deterministic captures

  def _rtg(self, ctx):
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
    try:
      if not self._cap:
        ctx.FBI.captureToFile(self._rtg(ctx).buffer(0), CorePath(self._out_path))
        self._cap = self._frame
        return
      if self._frame < self._cap + 20:
        return
      rng = -1.0
      mean = 0.0
      mx = 0
      if os.path.isfile(self._out_path) and os.path.getsize(self._out_path) > 0:
        arr = numpy.asarray(PILImage.open(self._out_path).convert("RGB"), dtype=numpy.float32)
        g = arr.mean(2) / 255.0
        rng = float(g.max() - g.min())
        mean = float(arr.mean())
        mx = int(arr.max())
      if rng < 0.02 and self._retries < 10:
        self._retries += 1
        self._cap = 0
        _note("retry %d (range=%.4f)" % (self._retries, rng))
        return
      if STATS_OUT:
        with open(STATS_OUT, "w") as f:
          json.dump({"path": self._out_path, "mean": mean, "max": mx, "range": rng,
                     "settle_frames": SETTLE_FRAMES, "retries": self._retries,
                     "frame_at_capture": self._cap}, f)
      _note("saved mean=%.4f max=%d range=%.4f retries=%d -> %s" %
            (mean, mx, rng, self._retries, self._out_path))
      self._done = True
      self.ezapp.signalExit()
    except Exception:
      import traceback
      _note("capture EXC:\n" + traceback.format_exc())
      self._done = True
      self.ezapp.signalExit()


def main():
  open(_STATUS, "w").close()
  app = SpotShadowApp(os.path.abspath(OUT))
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  ok = os.path.isfile(OUT) and os.path.getsize(OUT) > 0
  if ok:
    h = hashlib.sha256(open(OUT, "rb").read()).hexdigest()
    print("CAPTURE=%s sha256=%s" % (OUT, h), flush=True)
  print("[s0_spot_shadow] %s (%s)" % ("OK" if ok else "FAILED", OUT), flush=True)
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
