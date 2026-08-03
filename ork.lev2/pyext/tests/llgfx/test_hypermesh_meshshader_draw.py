#!/usr/bin/env python3
################################################################################
# hypermesh MESH-SHADER draw smoke — the same tri-only hypermesh rendered offscreen twice, once
# through the SSBO-pull vertex path (ORKID_HYPERMESH_MESHSHADER=0) and once through the
# VK_EXT_mesh_shader path (=1), which draws one workgroup per CPU-built meshlet.
#
# Both the C++ drawable and the ptex3d codegen read that env ONCE per process, so the A/B is TWO
# CHILD PROCESSES of this script; the parent compares what they produced.
#
# Each mode runs TWICE: WARM (whatever the shader cache holds) and COLD (ORKID_DISABLE_SHADER_CACHE=1,
# the sanctioned knob — full JIT of every stage, mesh stage included). The cold leg exists because a
# crash and a missing-technique step-down were both first seen on a first-ever cold compile of the
# mesh stage and then failed to reproduce warm: anything that lives in the cold compile / first-use
# path must be reachable on demand, on any machine, instead of depending on incidental cache state.
#
# What it proves:
#   * both paths render a non-degenerate frame of the same object (coverage + luminance floors)
#   * the mesh run ACTUALLY took the mesh path — its log carries the "driving N meshlets" line the
#     drawable only emits once a published partition is bound. A silent step-down to the pull-VS
#     path (the failure mode the loud refusal exists to prevent) fails this gate instead of quietly
#     passing it as "both frames look fine".
#   * a cold compile of the mesh stage survives (rc, and the same two checks)
#   * the two frames are close (the difference is REPORTED, not asserted tight — the pixel-parity
#     battery belongs to the parity gate, not to a smoke).
#
# The children run with ORKID_LOGCHAN_0=1: without it a crash AFTER the path engaged reads exactly
# like a path that never engaged, which cost one whole battery round.
#
# TRI-ONLY on purpose: the icosphere's faces are triangles, so the CPU snapshot's fan
# triangulation and the GPU triangulator's ear-clip agree exactly. Quad/ngon assets pick different
# diagonals per path — a known, deliberately deferred difference.
#
# Not ork.testing capture_app: that harness cannot yet host a scenegraph app (the reason
# test_sky_floor_gate.py and test_sun_cascades_gate.py state); the verdict-before-teardown
# protocol is honoured.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import re
import subprocess

WIDTH, HEIGHT = 384, 288
SETTLE_FRAMES = 60          # scene build + the microtask slices that publish the first partition
OUT_DIR       = os.environ.get("MESHAB_OUT", "/tmp/hypermesh_meshshader_ab")
CHILD_ENV     = "MESHAB_CHILD"
LEG_ENV       = "MESHAB_LEG"   # "warm" | "cold"

MIN_COVERAGE  = 0.02        # fraction of pixels lit by the object
MIN_MEAN      = 1.0         # 8-bit mean over the whole frame


################################################################################
# CHILD — render one mode, print machine-readable stats
################################################################################

def _dump_provenance(mtl, leg, mode):
  """Persist the EXACT generated shader source this child is about to JIT, plus where its codegen
  came from. Two runs of the same command from two checkouts resolve `sys.path[0]` (and therefore
  the ptex3d/hypermesh codegen modules) to DIFFERENT trees — same git sha is not the same input if
  either tree is dirty. When one run compiles and another does not, the diff of these two files is
  the bug; without them the difference is invisible and every theory is a guess."""
  import hashlib
  import shutil
  from orkengine import lev2
  from ork.hypergraph.ptex3d import fxv2_template
  from ork.hypergraph.dflow.hypermesh import gpu_meshlet
  import ork.hypergraph.dflow.hypermesh as hmdsl
  os.makedirs(OUT_DIR, exist_ok=True)
  stem = "%s_mode%d" % (leg, mode)
  spath = ""
  sha   = ""
  has_mesh_tek = False
  try:
    spath = mtl.shaderpath
    disk  = os.path.join(fxv2_template._dslcache_dir("ptex3d"), os.path.basename(spath))
    text  = open(disk).read()
    sha   = hashlib.sha1(text.encode("utf-8")).hexdigest()[:16]
    has_mesh_tek = ("technique FWD_SSBO_CUSTOM_MESH {" in text)
    shutil.copyfile(disk, os.path.join(OUT_DIR, stem + ".fxv2"))
  except Exception as e:
    print("MESHAB_PROV_ERROR %r" % (e,), flush=True)
  print("MESHAB_PROV leg=%s mode=%d shader_sha=%s mesh_tek=%d caps=%d/%d codegen_caps=%d/%d "
        "cwd=%s workspace=%s codegen=%s dsl=%s template=%s shaderpath=%s"
        % (leg, mode, sha, int(has_mesh_tek),
           lev2.hypermesh.meshlet_max_verts, lev2.hypermesh.meshlet_max_prims,
           gpu_meshlet.MESHLET_MAX_VERTS, gpu_meshlet.MESHLET_MAX_PRIMS,
           os.getcwd(), os.environ.get("ORKID_WORKSPACE_DIR", "?"),
           gpu_meshlet.__file__, hmdsl.__file__, fxv2_template.__file__, spath),
        flush=True)


def child(mode, leg):
  import math
  import numpy
  from PIL import Image as PILImage
  from orkengine.core import vec3
  from orkengine import lev2
  from ork.app.application import ComponentizedApplication
  from ork.app.std_scenegraph import StandardSceneGraphComponent
  from ork.hypergraph.dflow.hypermesh import Hypermesh, make_drawable
  from ork.hypergraph.assets.materials.terrain.solid import Solid
  from ork.testing import verdict

  class Sphere(Hypermesh):
    def __init__(self):
      super().__init__()
      # TRI-ONLY: icosphere faces are triangles (see the header note on quad diagonals)
      self.output(self.face_normals(self.icosphere(radius=1.5, subdivisions=3)))

  class ABApp(ComponentizedApplication):

    def __init__(self):
      super().__init__()
      self._frame = 0
      self._done = False
      self._inflight = None
      self._issued = False
      self.SGC = self.addComponent(
          "std_scenegraph", StandardSceneGraphComponent,
          eye=vec3(0, 1.2, 4.5), tgt=vec3(0, 0, 0), up=vec3(0, 1, 0),
          grid_variant=None,
          sg_params={"SkyboxIntensity": 1.0, "DiffuseIntensity": 1.0,
                     "SpecularIntensity": 1.0, "AmbientLevel": vec3(0.35)})
      self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                       use_subsystems=['opq', 'core', 'gpu', 'lev2'])

    def _onGpuInit(self, ctx):
      self.ezapp.topWidget.enableUiDraw()
      SGC = self.SGC
      SGC.pbr_common.enable_skybox = False
      sun = lev2.DynamicDirectionalLight()
      sun.data.color = vec3(1, 1, 1)
      sun.data.intensity = 2.0
      sun.shadowCaster = False
      sun.lookAt(vec3(60, 90, 60), vec3(0, 0, 0), vec3(0, 1, 0))
      self.sun = sun
      SGC.layer_fwd.createLightNode("sun", sun)
      SGC.scenegraph.lightingmanager.gpuInit(ctx)
      SGC.camera.perspective(0.1, 100.0, 45.0)
      SGC.camera.lookAt(vec3(0, 1.2, 4.5), vec3(0, 0, 0), vec3(0, 1, 0))
      self.live = Sphere().materialize_live(ctx)
      cdd, mtl = make_drawable(self.live, ctx, material_cls=Solid, roughness=0.6)
      self.node = SGC.layer_fwd.createDrawableNodeFromData("hm_ab", cdd)
      print("MESHAB_MESH verts=%d faces=%d" % (self.live.mesh.num_verts,
                                               self.live.mesh.num_faces), flush=True)
      _dump_provenance(mtl, leg, mode)

    def _onUpdate(self, updinfo):
      pass

    def _rtg(self, ctx):
      rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
      if rtg is None or rtg.numBuffers < 1:
        rtg = ctx.FBI.main_RTG
      return rtg

    def onGpuPostFrame(self, ctx):
      super().onGpuPostFrame(ctx)
      self._frame += 1
      if self._done:
        return
      if not self._issued:
        if self._frame >= SETTLE_FRAMES:
          buf = lev2.CaptureBuffer()
          fut = ctx.FBI.captureAsFormat(self._rtg(ctx).buffer(0), buf, "RGBA8")
          self._inflight = (fut, buf)
          self._issued = True
        return
      fut, buf = self._inflight
      if not bool(fut.is_ready):
        return
      w, h = buf.width, buf.height
      img = numpy.array(buf, dtype=numpy.uint8).reshape(h, w, 4)[..., :3].copy()
      os.makedirs(OUT_DIR, exist_ok=True)
      stem = "%s_mode%d" % (leg, mode)
      path = os.path.join(OUT_DIR, stem + ".png")
      PILImage.fromarray(img).transpose(PILImage.FLIP_TOP_BOTTOM).save(path)
      numpy.save(os.path.join(OUT_DIR, stem + ".npy"), img)
      lum = img.astype(numpy.int32).sum(axis=2)
      coverage = float((lum > 24).mean())
      ok = (coverage >= MIN_COVERAGE) and (float(img.mean()) >= MIN_MEAN)
      print("MESHAB leg=%s mode=%d mean=%.4f max=%d coverage=%.4f png=%s"
            % (leg, mode, float(img.mean()), int(img.max()), coverage, path), flush=True)
      verdict(ok, "leg=%s mode=%d coverage=%.4f mean=%.4f"
              % (leg, mode, coverage, float(img.mean())))
      self._exit_code = 0 if ok else 1
      self._done = True
      self.ezapp.signalExit()

  app = ABApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    print("MESHAB leg=%s mode=%d NO-CAPTURE" % (leg, mode), flush=True)
    rc = 1
  return rc


################################################################################
# PARENT — run both modes, compare
################################################################################

def run_child(mode, leg):
  env = dict(os.environ)
  env[CHILD_ENV] = str(mode)
  env[LEG_ENV]   = leg
  env["ORKID_HYPERMESH_MESHSHADER"] = str(mode)
  # the child's own log channels, forced ON by the gate: a crash after the mesh path engaged must
  # not be indistinguishable from a path that never engaged.
  env["ORKID_LOGCHAN_0"] = "1"
  if leg == "cold":
    env["ORKID_DISABLE_SHADER_CACHE"] = "1"   # full JIT, mesh stage included
  else:
    env.pop("ORKID_DISABLE_SHADER_CACHE", None)
  print("[meshab] launching leg=%s mode=%d" % (leg, mode), flush=True)
  p = subprocess.run([sys.executable, os.path.abspath(__file__)], env=env,
                     capture_output=True, text=True, timeout=900)
  log = p.stdout + p.stderr
  for line in log.splitlines():
    if line.startswith("MESHAB") or "HYPERMESH-MESHSHADER" in line or "hypermesh meshlets<" in line:
      print("  [%s/mode%d] %s" % (leg, mode, line.strip()), flush=True)
  # a child that dies inside pipeline creation says nothing on stdout — surface the driver's own
  # complaint (mvk-error / VK_ERROR_) so the log carries the failure, not just the return code.
  if p.returncode != 0:
    for line in log.splitlines():
      if ("mvk-error" in line) or ("VK_ERROR" in line) or ("Assert At" in line):
        print("  [%s/mode%d] %s" % (leg, mode, line.strip()), flush=True)
  return p.returncode, log


def parse_stats(log, mode, leg):
  m = re.search(r"MESHAB leg=%s mode=%d mean=([\d.]+) max=(\d+) coverage=([\d.]+)" % (leg, mode), log)
  if not m:
    return None
  return dict(mean=float(m.group(1)), max=int(m.group(2)), coverage=float(m.group(3)))


def main():
  import numpy
  from ork.testing import verdict
  failures = []

  def check(label, ok, detail=""):
    print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
    if not ok:
      failures.append(label)

  stats = {}
  prov  = {}
  for leg in ("warm", "cold"):
    for mode in (0, 1):
      rc, log = run_child(mode, leg)
      s = parse_stats(log, mode, leg)
      stats[(leg, mode)] = s
      tag = "%s_%s" % (leg, "mesh" if mode else "pullvs")
      check("%s_child_ok" % tag, rc == 0 and s is not None, "rc=%d" % rc)
      if s:
        check("%s_frame_nondegenerate" % tag,
              s["coverage"] >= MIN_COVERAGE and s["mean"] >= MIN_MEAN,
              "coverage=%.4f mean=%.4f" % (s["coverage"], s["mean"]))
      prov[(leg, mode)] = re.search(r"MESHAB_PROV .*", log)
      if mode == 1:
        # the anti-masquerade check: the mesh run must have actually driven DrawMeshTasks
        drove = re.search(r"driving (\d+) meshlets \((\d+) tris\)", log)
        check("%s_path_actually_drove_the_draw" % tag, drove is not None,
              ("meshlets=%s tris=%s" % (drove.group(1), drove.group(2))) if drove else
              "no 'driving N meshlets' line — the drawable stayed on the pull-VS path")
        # the material served must really carry the stage (source-level, before any driver has an
        # opinion) — this separates "wrong/stale material" from "Metal rejected a valid one"
        m = re.search(r"MESHAB_PROV leg=%s mode=1 shader_sha=(\w*) mesh_tek=(\d)" % leg, log)
        check("%s_material_carries_the_stage" % tag, bool(m) and m.group(2) == "1",
              ("shader_sha=%s" % m.group(1)) if m else "no provenance line")
  # informational: how far apart the two paths are, per leg (the tight parity oracle is its own gate)
  for leg in ("warm", "cold"):
    try:
      a = numpy.load(os.path.join(OUT_DIR, "%s_mode0.npy" % leg)).astype(numpy.int32)
      b = numpy.load(os.path.join(OUT_DIR, "%s_mode1.npy" % leg)).astype(numpy.int32)
      if a.shape == b.shape:
        d = numpy.abs(a - b)
        print("[meshab] %s frame delta: mean=%.3f max=%d exact=%.4f"
              % (leg, float(d.mean()), int(d.max()), float((d.max(axis=2) == 0).mean())), flush=True)
    except Exception as e:
      print("[meshab] %s delta unavailable: %r" % (leg, e), flush=True)

  # the source-identity summary: if two runs of this gate disagree, THIS is what differed
  for k in sorted(prov):
    if prov[k]:
      print("[meshab] %s" % prov[k].group(0), flush=True)
  shas = {k: (prov[k].group(0).split("shader_sha=")[1].split()[0] if prov[k] else "?") for k in prov}
  if shas.get(("warm", 1)) and shas.get(("cold", 1)):
    check("mesh_source_identical_across_legs", shas[("warm", 1)] == shas[("cold", 1)],
          "warm=%s cold=%s" % (shas[("warm", 1)], shas[("cold", 1)]))

  ok = (len(failures) == 0)
  detail = " ".join("%s/%s=%s" % (leg, mode, stats[(leg, mode)]) for (leg, mode) in sorted(stats))
  if failures:
    detail += " failed=" + ",".join(failures)
  verdict(ok, detail)
  return 0 if ok else 1


if os.environ.get(CHILD_ENV) is not None:
  sys.exit(child(int(os.environ[CHILD_ENV]), os.environ.get(LEG_ENV, "warm")))
sys.exit(main())
