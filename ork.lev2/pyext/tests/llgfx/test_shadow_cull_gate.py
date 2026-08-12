#!/usr/bin/env ork.python
################################################################################
# cascade-cull gate: sun-cascade shadows of a GPU-CULLED instanced caster must
# survive when the caster leaves the EYE frustum.
#
# The affected family: InstancedRigidPrimitiveDrawable with cull=True — a GPU
# stream-compaction cull whose survivors feed the indirect draw (same bug shape
# as the hypermesh scatter path: every pass drew the EYE-compacted subset, so a
# caster outside the eye frustum vanished from the sun cascades). A plain model
# does NOT exercise this bug (it re-culls CPU-side per pass).
#
# Scene: a lit ground plane (grid _V4) + a cluster of instanced cubes off to one
# side (x=-10) + ONE shadow-casting sun angled so the cubes' shadow streaks
# across the ground toward the origin. Two captures from ONE process:
#   pose A: camera sees the cubes (casters IN the eye frustum) — shadow center.
#   pose B: camera turned to the origin; the cubes are OUTSIDE the eye frustum
#           (off the left edge) but their shadow still streaks onto visible
#           ground. pose B is THE detector.
# Oracle: a fixed shadow crop is significantly darker than a fixed lit crop, in
# BOTH poses. Pre-fix, pose B's eye cull drops the off-view cubes -> the cascade
# (which pre-fix consumed the eye-culled set) has no caster -> no shadow -> the
# pose-B ratio collapses to ~1 -> FAIL. Post-fix, the union-sun shadow cull keeps
# them -> shadow present in both.
#
# Machine verdict line via ork.testing, emitted BEFORE teardown (defect D1: this
# scene class can SIGSEGV during mainThreadLoop teardown after the verdict is
# flushed). Lifecycle: ComponentizedApplication + StandardSceneGraphComponent
# (same hand-rolled-lifecycle reason as test_sun_cascades_gate.py).
#
# The caster is built from a SubMesh (not a MicroMesh) so the RigidPrimitive's
# local AABB is populated — the instance cull's bound-sphere test needs it.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.pop("ORKID_VR_DRIVER", None)             # ambient openxr trap -> headless
os.environ.pop("ORKID_DISABLE_FRUSTUM_CULL", None)  # the cull MUST run for this gate to mean anything
import sys
import numpy as np

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine.core import vec3, vec4, quat, mtx4, CrcStringProxy, Path as CorePath
from orkengine import lev2
from orkengine.lev2 import RigidPrimitive, meshutil
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

OUTDIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.environ.get("TMPDIR", "/tmp"), "shadow_cull_gate")
SETTLE_FRAMES = 120
SWITCH_FRAMES = 90

# camera poses. A: 3/4 view including the cubes (x=-10). B: look at the origin;
# the cubes fall off the left frustum edge while their shadow streaks in.
POSE_A = (vec3(6, 8, 12), vec3(-4, 1, 0))
POSE_B = (vec3(0, 8, 10), vec3(0, 0, 0))

# fixed crops (x0,y0,x1,y1); calibrated from the rendered composition (see the
# grid dump in the lane notes). The shadow crop straddles the cast shadow; the
# lit crop is open ground at similar depth. Kept inside the 633x473 capture.
CROP_A_SHADOW = (240, 200, 320, 300)
CROP_A_LIT    = (490, 200, 570, 280)
CROP_B_SHADOW = (20, 200, 120, 275)
CROP_B_LIT    = (420, 200, 520, 275)
RATIO_MIN     = 1.5   # lit/shadow mean proving a real cast shadow


def _crop_mean(arr, rect):
  x0, y0, x1, y1 = rect
  h, w = arr.shape[0], arr.shape[1]
  return float(arr[min(y0, h):min(y1, h), min(x0, w):min(x1, w)].mean())


def _cube_submesh():
  # 24 verts (hard-normal cube), 12 tris. SubMesh -> RigidPrimitive has a valid AABB.
  h = 0.5
  fd = [
    ((0, 0, 1),  [(-h, -h, h), (h, -h, h), (h, h, h), (-h, h, h)]),
    ((0, 0, -1), [(h, -h, -h), (-h, -h, -h), (-h, h, -h), (h, h, -h)]),
    ((1, 0, 0),  [(h, -h, h), (h, -h, -h), (h, h, -h), (h, h, h)]),
    ((-1, 0, 0), [(-h, -h, -h), (-h, -h, h), (-h, h, h), (-h, h, -h)]),
    ((0, 1, 0),  [(-h, h, h), (h, h, h), (h, h, -h), (-h, h, -h)]),
    ((0, -1, 0), [(-h, -h, -h), (h, -h, -h), (h, -h, h), (-h, -h, h)]),
  ]
  verts, faces = [], []
  for (nx, ny, nz), corners in fd:
    base = len(verts)
    for c in corners:
      verts.append({"p": vec3(*c), "n": vec3(nx, ny, nz)})
    faces.append([base, base + 1, base + 2])
    faces.append([base, base + 2, base + 3])
  return meshutil.SubMesh.createFromDict({"vertices": verts, "faces": faces})


class GateApp(ComponentizedApplication):

  def __init__(self, outdir):
    super().__init__()
    self._outdir = outdir
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_frame = 0
    self._done = False
    self._caps = {}
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=POSE_A[0], tgt=POSE_A[1], up=vec3(0, 1, 0),
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

    prim = RigidPrimitive(_cube_submesh(), ctx)
    mtl = lev2.PBRMaterial()
    mtl.baseColor = vec4(0.8, 0.8, 0.8, 1)
    mtl.roughnessFactor = 0.7
    mtl.metallicFactor = 0.0
    mtl.gpuInit(ctx)

    # cull=True enables the per-view GPU stream-compaction cull (the fix target).
    self.node_color = prim.createInstancedNode(9, "cubes", SGC.layer_fwd, mtl, cull=True)
    drw = self.node_color.drawable
    # a drawable sun-shadows only on the depth_prepass role: same drawable, 2nd layer.
    self.node_depth = SGC.layer_dpp.createDrawableNode("cubes_dpp", drw)
    i = 0
    for dx in (-1.5, 0, 1.5):
      for dz in (-1.5, 0, 1.5):
        self.node_color.setInstanceMatrix(i, mtx4.composed(vec3(-10 + dx, 3.0, dz), quat(), 1.5))
        i += 1

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 10.0
    sun.data.shadowBias = 0.05  # metres
    sun.data.shadowMapSize = 2048
    sun.data.shadowCascadeCount = 3
    sun.data.shadowMaxDistance = 250.0
    sun.data.pcfDither = 1.0
    sun.shadowCaster = True
    # light travels ~ (+x, down): cubes at x=-10 cast their shadow toward the origin.
    sun.lookAt(vec3(-30, 20, 5), vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _set_pose(self, pose):
    eye, tgt = pose
    self.SGC.uicam.lookAt(eye, tgt, vec3(0, 1, 0))
    self.SGC.camera.copyFrom(self.SGC.uicam.cameradata)

  def _onUpdate(self, updinfo):
    pass

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _capture(self, ctx, tag):
    path = os.path.join(self._outdir, "shadow_cull_%s.png" % tag)
    ctx.FBI.captureToFile(self._rtg(ctx).buffer(0), CorePath(path))
    self._caps[tag] = path

  def _verdict(self):
    from PIL import Image
    from ork.testing import verdict
    crops = {"A": (CROP_A_SHADOW, CROP_A_LIT), "B": (CROP_B_SHADOW, CROP_B_LIT)}
    results = []
    ok = True
    for tag in ("A", "B"):
      path = self._caps.get(tag)
      if not (path and os.path.isfile(path) and os.path.getsize(path) > 0):
        results.append("pose=%s MISSING" % tag); ok = False; continue
      arr = np.asarray(Image.open(path).convert("RGB"), dtype=np.float32).mean(axis=2)
      csh, clit = crops[tag]
      m_sh = _crop_mean(arr, csh)
      m_lit = _crop_mean(arr, clit)
      ratio = m_lit / max(m_sh, 1e-3)
      this_ok = (float(arr.max()) > 0) and (ratio >= RATIO_MIN)
      ok = ok and this_ok
      results.append("pose=%s lit=%.1f shadow=%.1f ratio=%.2f %s"
                     % (tag, m_lit, m_sh, ratio, "OK" if this_ok else "FAIL"))
    self._verdict_code = verdict(ok, "shadow cull gate | " + " | ".join(results))

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
        self._capture(ctx, "A")
        self._phase = 1; self._phase_frame = self._frame
    elif self._phase == 1:
      if self._frame >= self._phase_frame + 20:
        self._set_pose(POSE_B)          # LIVE pose switch: cubes leave the eye frustum
        self._phase = 2; self._phase_frame = self._frame
    elif self._phase == 2:
      if self._frame >= self._phase_frame + SWITCH_FRAMES:
        self._capture(ctx, "B")
        self._phase = 3; self._phase_frame = self._frame
    elif self._phase == 3:
      if self._frame >= self._phase_frame + 20:
        self._verdict()
        self._done = True
        self.ezapp.signalExit()


def main():
  from ork.testing import Watchdog
  os.makedirs(OUTDIR, exist_ok=True)
  wd = Watchdog(300.0, label="shadow_cull_gate").arm()
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
