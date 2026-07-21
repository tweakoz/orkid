#!/usr/bin/env ork.python
###############################################################################
# Asphalt road-surface preview bake (offscreen ezapp).
#
# Renders the parametric Asphalt ptex3d material on a flat quad whose UV chart
# matches the R-family road contract: U = uv.x normalized 0..1 across width_m
# (lateral), V = uv.y in METERS (arc-length). A top-down offscreen render captures
# the strip so the painted markings, matte asphalt and lane dividers can be read
# directly from the PNG. One process = one variant (--preset / --lane+--width).
#
#   ork.python asphalt_road_bake.py --preset 2lane --out /tmp/road_2lane.png
###############################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.chdir(os.path.expanduser("~"))
import sys, argparse, time
# ork.python resolves ork.hypergraph from the checkout on PATH; when run from a dev
# worktree (pre-merge) prepend THIS repo's scripts dir so the local material wins.
_SCRIPTS = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                        "../../../../../obt.project/scripts"))
if os.path.isdir(_SCRIPTS) and _SCRIPTS not in sys.path:
  sys.path.insert(0, _SCRIPTS)

import numpy
from PIL import Image as PILImage
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from orkengine.lev2 import Geometry, RigidPrimitive, CaptureBuffer
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.hypergraph.ecs.scene.assets import Ptex3d as Ptex3dAsset
from ork.hypergraph.assets.materials.asphalt import Asphalt, road_preset

tokens = CrcStringProxy()
ROAD_LENGTH_M = 30.0
_STATUS = os.environ.get("ROAD_STATUS", "/tmp/road_bake_status.txt")
def _note(s):
  try:
    with open(_STATUS, "a") as f:
      f.write(s + "\n"); f.flush()
  except Exception:
    pass


def _quad_geometry(width_m, length_m):
  hw = width_m * 0.5
  P  = numpy.array([[-hw, 0.0, 0.0], [hw, 0.0, 0.0],
                    [-hw, 0.0, length_m], [hw, 0.0, length_m]], dtype=numpy.float32)
  N  = numpy.tile(numpy.array([0.0, 1.0, 0.0], dtype=numpy.float32), (4, 1))
  B  = numpy.tile(numpy.array([1.0, 0.0, 0.0], dtype=numpy.float32), (4, 1))
  UV = numpy.array([[0.0, 0.0], [1.0, 0.0],
                    [0.0, length_m], [1.0, length_m]], dtype=numpy.float32)   # V = arc-length meters
  Cd = numpy.array([[UV[i, 0], UV[i, 1], 0.0, 1.0] for i in range(4)], dtype=numpy.float32)
  tris = numpy.array([0, 2, 1, 1, 2, 3], dtype=numpy.int32)   # CCW about +Y so the top face renders
  geo = Geometry()
  geo.point["P"] = P; geo.point["N"] = N; geo.point["binormal"] = B
  geo.point["uv"] = UV; geo.point["Cd"] = Cd
  geo.addPolys(tris, sides=3)
  return geo


class RoadBake(ComponentizedApplication):
  def __init__(self, lane_count, width_m, out_path):
    super().__init__()
    self._lane_count = lane_count
    self._width_m    = width_m
    self._out_path   = out_path
    self._frame      = 0
    self._done       = False
    self._built      = False
    self._built_frame = 0
    self._cap        = 0
    self._retries    = 0
    H = (ROAD_LENGTH_M * 0.5) / 0.4142
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(0.0, H, ROAD_LENGTH_M * 0.5),
        tgt=vec3(0.0, 0.0, ROAD_LENGTH_M * 0.5),
        up=vec3(0.0, 0.0, 1.0),
        near=1.0, far=400.0,
        grid_variant=None,
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.6,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0.35),
        })
    self.createEzApp(width=512, height=1024, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  def _onGpuInit(self, ctx):
    self._ctx = ctx
    self.ezapp.topWidget.enableUiDraw()
    try:
      self.SGC.pbr_common.enable_skybox = True     # draw the skybox (env light + non-black bg)
    except Exception as e:
      _note("enable_skybox skipped: %r" % e)
    _note("gpuinit (deferred build)")

  def _build_road(self, ctx):
    import traceback
    try:
      wrap = Ptex3dAsset(dsl_class=Asphalt, lane_count=self._lane_count, width_m=self._width_m)
      wrap._ctx = ctx
      self._mat = wrap.build()
      _note("material built")
      self._prim = RigidPrimitive()
      self._prim.updateWithMicroMesh(_quad_geometry(self._width_m, ROAD_LENGTH_M).toMicroMesh(),
                                     ctx, tokens.TRIANGLES)
      self._node = self._prim.createNode("road", self.SGC.layer1, self._mat)
      _note("road built lane=%d width=%.2f" % (self._lane_count, self._width_m))
    except Exception:
      _note("build EXC:\n" + traceback.format_exc())
      self.ezapp.signalExit()

  def _onUpdate(self, updinfo):
    self.SGC.scenegraph.updateScene(self.SGC.cameralut)

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _skybox_resident(self):
    try:
      return self.SGC.pbr_common.RadianceMaps.specular is not None
    except Exception:
      return False

  def onGpuPostFrame(self, ctx):
    import traceback
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if not self._built:
      if self._frame >= 2:
        self._build_road(ctx)
        self._built = True
        self._built_frame = self._frame
        _note("skybox wait...")
      return
    # capture only once the skybox IBL is resident (async filtered specular), else the
    # frame is black; a generous frame cap avoids an infinite wait offscreen.
    ready_env = self._skybox_resident()
    settle_done = ready_env and (self._frame >= self._built_frame + 30)
    settle_done = settle_done or (self._frame >= self._built_frame + 900)
    if not settle_done or self._done:
      return
    # captureToFile writes the PNG asynchronously; keep RENDERING the static scene after
    # issuing it so the readback samples a stable, drawn frame (not a mid-clear one), then
    # verify the file is non-black before exiting (re-issue if it raced to black).
    from orkengine.core import Path as CorePath
    try:
      if not self._cap:
        ctx.FBI.captureToFile(self._rtg(ctx).buffer(0), CorePath(self._out_path))
        self._cap = self._frame
        return
      if self._frame < self._cap + 20:      # let the async readback+write settle
        return
      rng = -1.0
      if os.path.isfile(self._out_path) and os.path.getsize(self._out_path) > 0:
        g = numpy.asarray(PILImage.open(self._out_path).convert("RGB"), dtype=numpy.float32).mean(2) / 255.0
        rng = float(g.max() - g.min())
      if rng < 0.02 and self._retries < 10:
        self._retries += 1
        self._cap = 0                        # black/not-written -> re-render + re-issue
        _note("retry %d (range=%.4f)" % (self._retries, rng))
        return
      _note("saved range=%.4f retries=%d -> %s" % (rng, self._retries, self._out_path))
      self._done = True
      _note("signalExit")
      self.ezapp.signalExit()
    except Exception:
      _note("capture EXC:\n" + traceback.format_exc())
      self._done = True
      self.ezapp.signalExit()


def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--preset", default=None)
  ap.add_argument("--lane", type=int, default=None)
  ap.add_argument("--width", type=float, default=None)
  ap.add_argument("--out", required=True)
  a = ap.parse_args()
  if a.preset is not None:
    kw = road_preset(a.preset); lane, width = kw["lane_count"], kw["width_m"]
  else:
    lane, width = a.lane, a.width
  open(_STATUS, "w").close()
  app = RoadBake(lane, width, os.path.abspath(a.out))
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  ok = os.path.isfile(a.out) and os.path.getsize(a.out) > 0
  print("[bake] %s (%s)" % ("OK" if ok else "FAILED", a.out), flush=True)
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
