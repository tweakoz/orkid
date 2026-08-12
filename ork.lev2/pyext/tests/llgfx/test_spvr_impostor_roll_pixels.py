#!/usr/bin/env ork.python
################################################################################
# SPVR — THE IMPOSTOR BILLBOARD UNDER A ROLLED HEAD, IN PIXELS.
#
# WHAT THIS CLOSES. The billboard basis fix (world-up anchored off the view axis)
# was proven STRUCTURALLY — by reading the emitted vertex stage — and the stereo
# peer's matrix binding was the explicitly unverified corner: the _ST stage reads
# the SAME `inv_v`, but nothing had ever shown, in pixels, that the quad a
# single-pass-stereo draw puts on screen stands up in the WORLD when the head is
# tilted. A source gate cannot answer that; the matrix that reaches the stage is
# decided by the compositor, not by the shader text.
#
# THE RIG. A tall thin post (an anisotropic silhouette, so its apparent rotation
# is a measurable number) is drawn as an impostor BILLBOARD — the real path: a
# hemi-octahedral atlas baked in-frame from the mesh, the LOD tier routed to the
# quad by the GPU cull, the FWD_SSBO_CUSTOM_IMPOSTOR_ST technique inside a real
# single-pass-stereo pass on a NoVr device. The head is tilted by writing the
# device's own "hmd" pose — the same channel a runtime writes.
#
# THE MEASUREMENT. Tilting the head rotates the whole WORLD on screen by the roll
# angle. A world-anchored billboard rotates with it; a screen-glued one (the
# defect) stays upright on screen. So:
#   (1) ANGLE      the silhouette's principal axis must move by the roll angle,
#                  in BOTH eyes, for +roll and -roll.
#   (2) REGISTER   counter-rotating the rolled frame by the roll angle must land
#                  the silhouette back on the level frame's (IoU), and must do so
#                  MUCH better than the un-rotated comparison (discrimination).
#   (3) CONTROL    the pre-fix basis (right/up = inverse-view rows 0/1), restored
#                  in-process by patching the emitted stage, must FAIL (1) — the
#                  post stays screen-upright. Without this leg a green could mean
#                  "the metric cannot see roll leakage at all".
#
# Self-configuring: no arguments, no environment.
#   ork.python test_spvr_impostor_roll_pixels.py [outdir]
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")

import sys
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

W = H = 512
ROLLS = (25.0, -25.0)        # head tilt, degrees (both signs: a sign error cannot pass)
POST_DIST = 30.0             # eye -> post; past the LOD switch, so the tier IS the billboard
LOD_SWITCH = 10.0

ANGLE_TOL = 5.0              # deg; leg (1). resampled silhouette + atlas tile step
IOU_FLOOR = 0.70             # leg (2) registration after counter-rotation
IOU_MARGIN = 0.15            # leg (2) counter-rotated must beat un-rotated by this
CONTROL_CEIL = 8.0           # leg (3) the screen-glued basis must move < this
MIN_LIT = 200                # a blank capture cannot pass any leg


################################################################################
# CHILD — build the impostor rig under SPVR at one head roll, capture both eyes.
################################################################################

def _render(outdir, roll, legacy):
  import numpy
  from PIL import Image
  from orkengine import lev2
  from orkengine.core import vec3, mtx4, quat, VarMap
  from ork.testing import headless_app, ensure_parent_dir

  if legacy:
    # NEGATIVE CONTROL: restore the pre-fix SCREEN-GLUED basis in the emitted stage.
    # Fails loudly if the stage moved — a control that silently no-ops is not a control.
    from ork.hypergraph.ptex3d import fxv2_template as T
    src = T._IMPOSTOR_VS
    pat = (("  vec3 right  = normalize(cross(upref, vaxis));",
            "  vec3 right  = normalize(inv_v[0].xyz);   // CONTROL: pre-fix camera right"),
           ("  vec3 up     = cross(vaxis, right);",
            "  vec3 up     = normalize(inv_v[1].xyz);   // CONTROL: pre-fix camera up"))
    for old, new in pat:
      if old not in src:
        raise RuntimeError("roll control: billboard basis line not found (%r) — the control "
                           "would have measured the CURRENT basis twice" % old)
      src = src.replace(old, new)
    T._IMPOSTOR_VS = src

  from ork.hypergraph.dflow.hypermesh import Hypermesh, GpuMeshRenderSource
  from ork.hypergraph.ecs.scene.assets import Ptex3d as Ptex3dAsset
  from ork.hypergraph.assets.materials.terrain.solid import Solid

  class Post(Hypermesh):
    """a tall thin post — an anisotropic silhouette whose principal axis IS the
    apparent rotation of the billboard that carries it."""
    def __init__(self):
      super().__init__()
      self.output(self.transform(self.box(size=1.0), scale=(0.6, 4.0, 0.6)))

  out = {"roll": roll, "legacy": bool(legacy)}
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2'], width=W, height=H) as app:
    ctx = app.ctx
    out["multiview"] = bool(ctx.supports_multiview)
    if not ctx.supports_multiview:
      with open(os.path.join(outdir, "child.json"), "w") as f:
        json.dump(out, f, indent=1)
      return 0

    # HEAD POSE: the roll goes into the device's own "hmd" pose (what a runtime writes),
    #  so the eye view matrices carry it exactly as they do on a headset.
    th  = math.radians(roll)
    eye = vec3(0, 2.0, POST_DIST)
    tgt = vec3(0, 0.0, 0.0)
    up  = vec3(math.sin(th), math.cos(th), 0.0)
    vrdev = lev2.orkidvr.novr_device()
    vrdev.width = W; vrdev.height = H
    vrdev.FOVD = 90.0; vrdev.IPD = 0.064
    vrdev.near = 0.5; vrdev.far = 2000.0
    vrdev.setPoseMatrix("hmd", mtx4.lookAt(eye, tgt, up))

    camlut = lev2.CameraDataLut()
    cam = lev2.CameraData()
    cam.perspective(0.5, 2000.0, 90.0)
    cam.lookAt(eye, tgt, up)
    camlut.addCamera("spawncam", cam)   # the per-view cull fan-out resolves this name

    params = VarMap()
    params.preset = "FWDPBRSPVR"
    params.SkyboxIntensity = 1.0
    params.SpecularIntensity = 1.0
    params.DiffuseIntensity = 1.0
    params.AmbientLight = vec3(0.6)
    params.DepthFogDistance = float(1e6)
    params.SkyboxTexPathStr = "<ork_envmaps2>/cold4k.xir"
    scene = lev2.scenegraph.Scene(params)
    layer = scene.createLayer("std_forward")
    scene.createLayer("depth_prepass")

    wrap = Ptex3dAsset(dsl_class=Solid,
                       vertex_source=GpuMeshRenderSource(instanced=True),
                       albedo=vec3(0.9, 0.85, 0.2), roughness=0.6,
                       impostor=True)   # emits the capture technique the atlas bake needs
    wrap._ctx = ctx
    mtl = wrap.as_gfx_material

    # 2 instances (the impostor tier is routed by the INSTANCE cull): the measured post at
    #  the origin, and one parked far overhead so the draw is genuinely instanced.
    inst = []
    for pos in (vec3(0, 0, 0), vec3(0, 400, 0)):
      m = mtx4.composed(pos, quat(), 1.0)
      for c in range(4):
        v = m.getColumn(c)
        inst += [v.x, v.y, v.z, v.w]
    graph = Post().generatedflow()
    dd = lev2.HypermeshDrawableData(
        graph=graph,
        lod_graphs=[graph],            # tier 1 = the impostor band (its mesh is the fallback)
        lod_distances=[LOD_SWITCH],
        impostor_lods=[0],
        impostor_grid=8, impostor_tile=256, impostor_ssaa=1, impostor_msaa=2,
        instance_matrices=inst,
        cull=True, cull_distance=4000.0)
    dd.resolved_material = mtl
    layer.createDrawableNodeFromData("posts", dd)
    scene.lightingmanager.gpuInit(ctx)

    for _ in range(24):                # atlas bakes in-frame; a few frames to settle
      scene.updateScene(camlut)
      ctx.beginFrame()
      scene.renderOnContext(ctx)
      ctx.endFrame()

    # eye layers are read back in a FRESH frame (capturing inside the producing frame
    #  leaves them in a host-read layout the composite then asserts on).
    outnode = scene.compositoroutputnode
    ctx.beginFrame()
    caps, futs = {}, []
    for (nm, left) in (("L", True), ("R", False)):
      cb = lev2.CaptureBuffer()
      caps[nm] = cb
      futs.append((nm, ctx.FBI.captureAsFormat(outnode.downsampledEyeRtGroup(left).buffer(0), cb, "RGBA8")))
    ctx.endFrame()
    for (nm, fut) in futs:
      assert fut.wait(caps[nm]), "impostor roll: capture never landed for %s" % nm
      cb = caps[nm]
      arr = numpy.array(cb, dtype=numpy.uint8).reshape(cb.height, cb.width, 4)
      p = os.path.join(outdir, "%s.png" % nm)
      ensure_parent_dir(p)
      Image.fromarray(arr[..., :3]).save(p)
      out[nm] = p
    out["validation_errors"] = int(ctx.validation_errors)

  with open(os.path.join(outdir, "child.json"), "w") as f:
    json.dump(out, f, indent=1)
  return 0


################################################################################
# METRICS (parent side)
################################################################################

def silhouette(path):
  """the post is the only saturated-YELLOW content (the sky/ground are blue-grey)."""
  import numpy
  from PIL import Image
  a = numpy.asarray(Image.open(path).convert("RGB"), dtype=numpy.float64) / 255.0
  return (a[..., 0] > 0.25) & (a[..., 0] > a[..., 2] * 2.0) & (a[..., 1] > a[..., 2] * 1.8)


def principal_deg(mask):
  """principal axis of the lit pixels, degrees, image coords (y down). Wraps at 180."""
  import numpy
  ys, xs = numpy.nonzero(mask)
  if len(xs) < MIN_LIT:
    return None
  x = xs - xs.mean(); y = ys - ys.mean()
  cxx = float((x * x).mean()); cyy = float((y * y).mean()); cxy = float((x * y).mean())
  return math.degrees(0.5 * math.atan2(2 * cxy, cxx - cyy))


def dangle(a, b):
  """signed a-b folded into (-90,90] — a principal axis has no head or tail."""
  d = (a - b + 90.0) % 180.0 - 90.0
  return d


def iou(m0, m1):
  import numpy
  u = numpy.logical_or(m0, m1).sum()
  return float(numpy.logical_and(m0, m1).sum()) / float(u) if u else 0.0


def rotate_mask(mask, deg):
  """rotate a silhouette about the principal point (PIL angle convention). Undoing a
  head roll of +r is rotate_mask(mask, -r): image y is DOWN, so the world's image
  rotates by -r under that roll."""
  from PIL import Image
  import numpy
  im = Image.fromarray((mask * 255).astype("uint8"))
  return numpy.asarray(im.rotate(deg, resample=Image.NEAREST, expand=False)) > 127


################################################################################
# DRIVER
################################################################################

def _run_child(outdir, roll, legacy):
  os.makedirs(outdir, exist_ok=True)
  env = dict(os.environ)
  env["ORKID_VULKAN_VALIDATE"] = "2"
  for k in ("ORKID_SPVR_NO_MULTIVIEW", "ORKID_GATE0_FORCE_MONO_TEK"):
    env.pop(k, None)
  argv = [sys.executable, os.path.abspath(__file__), "--render", outdir,
          str(roll), "1" if legacy else "0"]
  proc = subprocess.run(argv, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=300)
  rec = {}
  cj = os.path.join(outdir, "child.json")
  if os.path.exists(cj):
    rec = json.load(open(cj))
  return proc.returncode, proc.stdout.decode("utf-8", "replace"), rec


def main():
  outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
      tempfile.gettempdir(), "spvr_impostor_roll_pixels")
  os.makedirs(outdir, exist_ok=True)
  t0 = time.time()
  fails = []

  runs = {}
  plan = [("level", 0.0, False)] + \
         [("roll%+g" % r, r, False) for r in ROLLS] + \
         [("ctl_level", 0.0, True), ("ctl_roll%+g" % ROLLS[0], ROLLS[0], True)]
  for (tag, roll, legacy) in plan:
    rc, out, rec = _run_child(os.path.join(outdir, tag), roll, legacy)
    runs[tag] = rec
    runs[tag]["_stdout"] = out
    runs[tag]["_rc"] = rc
    if rc != 0:
      fails.append("%s: child exited %d" % (tag, rc))
      print(out)

  if not runs["level"].get("multiview", False):
    print("SKIP: no multiview on this device — single-pass stereo unavailable")
    print("VERDICT: SKIP (no multiview)")
    return 0

  # ---- leg A: the stereo IMPOSTOR technique is what drew -------------------
  sel = "technique<FWD_SSBO_CUSTOM_IMPOSTOR_ST>"
  for tag in runs:
    if sel not in runs[tag]["_stdout"]:
      fails.append("%s: no impostor _ST draw announced — the measurement is not of the "
                   "stereo billboard path" % tag)
  print("A  stereo impostor technique announced in every run: %s"
        % all(sel in runs[t]["_stdout"] for t in runs))

  # ---- leg B/C: angle + registration ---------------------------------------
  base = {e: silhouette(runs["level"][e]) for e in ("L", "R")}
  base_ang = {e: principal_deg(base[e]) for e in ("L", "R")}
  for e in ("L", "R"):
    if base_ang[e] is None:
      fails.append("level %s: silhouette below %d px — nothing was measured" % (e, MIN_LIT))
  print("B  level principal: L %.2f  R %.2f" % (base_ang["L"] or 0, base_ang["R"] or 0))

  for r in ROLLS:
    tag = "roll%+g" % r
    for e in ("L", "R"):
      m = silhouette(runs[tag][e])
      a = principal_deg(m)
      if a is None:
        fails.append("%s %s: silhouette below %d px" % (tag, e, MIN_LIT))
        continue
      moved = dangle(a, base_ang[e])          # image-space rotation of the silhouette
      err = abs(moved - (-r))                 # world roll +r shows as -r in image angle
      reg = iou(rotate_mask(m, -r), base[e])  # counter-rotate the rolled frame back
      raw = iou(m, base[e])
      print("   %s %s: principal %.2f  moved %+.2f (want %+.2f, err %.2f)  "
            "IoU counter-rot %.3f vs raw %.3f" % (tag, e, a, moved, -r, err, reg, raw))
      if err > ANGLE_TOL:
        fails.append("%s %s: the billboard moved %+.2f deg under a %+g deg head roll "
                     "(world-anchored would be %+.2f) — roll leakage" % (tag, e, moved, r, -r))
      if reg < IOU_FLOOR:
        fails.append("%s %s: counter-rotated silhouette does not register on the level "
                     "frame (IoU %.3f < %.2f)" % (tag, e, reg, IOU_FLOOR))
      if reg < raw + IOU_MARGIN:
        fails.append("%s %s: counter-rotation is not what registers the frames "
                     "(IoU %.3f vs un-rotated %.3f) — the metric proves nothing"
                     % (tag, e, reg, raw))

  # ---- leg D: NEGATIVE CONTROL — the pre-fix basis must be seen to roll -----
  cbase = {e: principal_deg(silhouette(runs["ctl_level"][e])) for e in ("L", "R")}
  ctag  = "ctl_roll%+g" % ROLLS[0]
  for e in ("L", "R"):
    a = principal_deg(silhouette(runs[ctag][e]))
    if a is None or cbase[e] is None:
      fails.append("control %s: silhouette below %d px" % (e, MIN_LIT))
      continue
    moved = dangle(a, cbase[e])
    print("D  control (pre-fix basis) %s: moved %+.2f deg under %+g deg roll" % (e, moved, ROLLS[0]))
    if abs(moved) > CONTROL_CEIL:
      fails.append("control %s: the pre-fix screen-glued basis ALSO rotated %+.2f deg — the "
                   "measurement cannot distinguish a rolling billboard from a standing one"
                   % (e, moved))

  for f in fails:
    print("  FAIL: %s" % f)
  print("VERDICT: %s (impostor billboard under head roll, single-pass stereo; %d failure(s), %.1fs)"
        % ("PASS" if not fails else "FAIL", len(fails), time.time() - t0))
  return 0 if not fails else 1


if __name__ == "__main__":
  if len(sys.argv) > 2 and sys.argv[1] == "--render":
    sys.exit(_render(sys.argv[2], float(sys.argv[3]), sys.argv[4] == "1"))
  sys.exit(main())
