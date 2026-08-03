#!/usr/bin/env ork.python
################################################################################
# CANARY PRODUCER (committed 2026-07-23, ex-S0 scratch): reflection-probe
# capture. CANONICAL producer of the blessed probe.png byte-identity baseline
# recorded in ork.dox/gfx/SKYLIGHT_SHADOWS_JUL22.md — run WARM (twice, sha the
# second) with ORKID_VR_DRIVER unset.
# usage: canary_probe.py [out.png] [stats.json]
# DO NOT edit scene content without re-blessing the recorded sha.
#
# (original header follows)
# Offscreen, static scene (fixed node positions -- no orbit/time animation) so
# the probe recapture/refilter path settles to a fixed result.
#
# Modeled on ork.lev2/pyext/tests/renderer/lighting/probe.py (LightProbe /
# createLightProbeNode usage, type REFLECTION) and asphalt_road_bake.py's
# offscreen settle+captureToFile idiom. Uses StandardSceneGraphComponent's
# createBallNode helper for the surrounding colored markers and a mirror-like
# center sphere so the probe's specular contribution is visible on it.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys, hashlib, json
import numpy
from PIL import Image as PILImage
from orkengine.core import vec3, vec4, mtx4, CrcStringProxy, Path as CorePath
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

OUT = sys.argv[1] if len(sys.argv) > 1 else "/tmp/s0_probe.png"
STATS_OUT = sys.argv[2] if len(sys.argv) > 2 else None
# Generous settle budget: the refilter/prefilter path (RadiancePrefilterMicrotask)
# is a time-sliced multi-frame job (spec JUL22_skylight.md S2/S3) -- give it room
# to converge before capture. Recorded verbatim so the post-change pass reuses it.
SETTLE_FRAMES = 240

_STATUS = os.environ.get("S0_PROBE_STATUS", "/tmp/s0_probe_status.txt")
def _note(s):
  try:
    with open(_STATUS, "a") as f:
      f.write(s + "\n"); f.flush()
  except Exception:
    pass


class ProbeCaptureApp(ComponentizedApplication):
  def __init__(self, out_path):
    super().__init__()
    self._out_path = out_path
    self._frame = 0
    self._built = False
    self._built_frame = 0
    self._invalidated = False
    self._cap = 0
    self._done = False
    self._retries = 0
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

    # Four fixed colored markers around the probe (static -- no orbit).
    self.node_px = SGC.createBallNode("mk_px", ctx=ctx, position=vec3(5, 2, 0), color=vec4(1, 0, 0, 1), scale=1.0)
    self.node_nx = SGC.createBallNode("mk_nx", ctx=ctx, position=vec3(-5, 2, 0), color=vec4(0, 1, 0, 1), scale=1.0)
    self.node_pz = SGC.createBallNode("mk_pz", ctx=ctx, position=vec3(0, 2, 5), color=vec4(0, 0, 1, 1), scale=1.0)
    self.node_nz = SGC.createBallNode("mk_nz", ctx=ctx, position=vec3(0, 2, -5), color=vec4(1, 1, 1, 1), scale=1.0)
    # Mirror-like center sphere: metallic=1, roughness~0 -- reads the probe's
    # specular content directly.
    self.node_ctr = SGC.createBallNode("mk_ctr", ctx=ctx, position=vec3(0, 2, 0),
                                       color=vec4(1, 1, 1, 1), metallic=1.0, roughness=0.05, scale=1.2)

    # A simple non-shadow-casting point light so the markers + mirror sphere
    # are actually visible (skybox residency alone was insufficient offscreen
    # within the settle budget tried).
    self.point_light = lev2.DynamicPointLight()
    self.point_light.data.color = vec3(1200, 1200, 1200)
    self.point_light.data.radius = 40.0
    self.pnode = SGC.layer_fwd.createLightNode("pointlight0", self.point_light)
    self.pnode.setMatrix(mtx4.transMatrix(2, 10, 6))

    self.probe = lev2.LightProbe()
    self.probe.type = tokens.REFLECTION
    self.probe.imageDim = 512
    self.probe.worldMatrix = mtx4.transMatrix(0, 2, 0)
    self.probe.name = "probe1"
    self.probe_node = SGC.layer_fwd.createLightProbeNode("probe", self.probe)

    SGC.scenegraph.lightingmanager.gpuInit(ctx)
    _note("scene built")

  def _onUpdate(self, updinfo):
    if not self._invalidated:
      self.probe.invalidate()   # trigger exactly ONE recapture cycle -- static scene after
      self._invalidated = True

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
  app = ProbeCaptureApp(os.path.abspath(OUT))
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  ok = os.path.isfile(OUT) and os.path.getsize(OUT) > 0
  if ok:
    h = hashlib.sha256(open(OUT, "rb").read()).hexdigest()
    print("CAPTURE=%s sha256=%s" % (OUT, h), flush=True)
  print("[s0_probe_capture] %s (%s)" % ("OK" if ok else "FAILED", OUT), flush=True)
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
