#!/usr/bin/env ork.python
################################################################################
# SPVR — THE MULTIVIEW-MESH CAPABILITY GATE ([SPVR:MESHMV-STEPDOWN]).
#
# THE DEFECT THIS CLOSES. Nothing in the engine asked whether the MESH stage is
# legal inside a TWO-VIEW pass. The capability existed and was probed correctly
# (Context::supportsMultiviewMeshShader / maxMeshMultiviewViewCount) and had no
# C++ consumer at all: the drawables asked only "does this GPU do mesh shaders"
# (terrain_chunk_drawable, hmdflow_render) and then built the two-view mesh
# pipeline anyway. On a device that grants VK_EXT_mesh_shader but excludes the
# mesh stage from multiview passes — MoltenVK today — vkCreateGraphicsPipelines
# fails and the backend asserts. No fallback, no skip: a hard crash on the
# owner's mac bench, and an ENGINE defect, not a driver gap.
#
# WHERE THE GATE HAD TO GO, AND WHY THIS TEST HAS TWO KINDS OF LEG. Refusing at
# TECHNIQUE SELECTION is not enough and was measured not to be: the draw-call
# KIND belongs to the drawable (ComputeDrawable::_renderIndirect branches on its
# own _meshTechnique), and the pull-VS path's compute passes + indirect args are
# wired only on the drawable's non-mesh arm — so a mesh-resolved drawable has
# nothing to fall back TO. Stepping only the technique down turned the device
# rejection into "_createPipelineMesh with no mesh stage in the pass". The real
# gate therefore sits at the RESOLVE sites; selection keeps a backstop.
#
#   SELECTION legs  ask the real pipeline cache, on a real generated mesh
#                   material, which technique a mesh draw WOULD take.
#   RESOLVE legs    render a real hypermesh drawable and read which DRAW PATH the
#                   drawable resolved — the half that a selection answer cannot
#                   prove and that the crash actually lived in.
#
# THE LEVER. The capability cannot be revoked on a device that grants it, so on
# every machine able to run the mesh path the refusal would be unreachable and
# therefore unprovable. ORKID_TEST_FORCE_NO_MULTIVIEW_MESH=1 (engine-side, the
# gate0ForceMonoTechnique idiom, default OFF, announces itself when armed) forces
# the capability to read false at both gates. This file sets it for its own
# children only — every child's environment is built here, so a value left in the
# caller's shell cannot decide a leg.
#
# LEGS (all must pass)
#   (a) SEL_GRANTED   a stereo mesh permutation selects FWD_SSBO_CUSTOM_MESH_ST,
#                     and a mono one still selects FWD_SSBO_CUSTOM_MESH. The
#                     unchanged-behaviour direction: the gate must be invisible on
#                     a device that grants the capability.
#   (b) SEL_FORCED    with the lever armed the SAME stereo permutation selects
#                     FWD_SSBO_CUSTOM_ST — the pull-VS PER-VIEW peer. Stepping
#                     down must never mean stepping down to mono: a device without
#                     multiview-mesh still renders stereo correctly through the
#                     vertex stage, and mono would trade a crash for a silently
#                     wrong second eye. The [SPVR:MESHMV-STEPDOWN] line must
#                     appear and the child must exit 0.
#   (c) RES_GRANTED   the hypermesh drawable really resolves the mesh draw path
#                     (the drawable's own "driving N meshlets" line). Without this
#                     control, leg (d)'s absence-of-mesh assertion would pass on a
#                     rig that never had a mesh path to lose.
#   (d) RES_FORCED    the same render, lever armed and two-view declared possible:
#                     the drawable steps down to the pull-VS path, says so with the
#                     [SPVR:MESHMV-STEPDOWN] token, draws a non-degenerate frame,
#                     and exits 0. THIS is the leg that was a SIGSEGV before the
#                     resolve-site gate.
#
# WHAT THE STEP-DOWN LINE MUST SAY. Metal is proven capable of exactly this work
# (a 2-layer stereo target from one drawMeshThreadgroups via vertex amplification),
# so the refusal names the MoltenVK gap rather than implying a hardware limit. The
# wording is asserted here, not merely emitted: a log line nobody checks drifts.
#
# Self-configuring: no arguments, no external environment.
#   ork.python test_spvr_meshmv_stepdown.py
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")

import sys
import json
import re
import time
import tempfile
import subprocess

sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "bin"))

MESH_TEK    = "FWD_SSBO_CUSTOM_MESH"
MESH_ST_TEK = "FWD_SSBO_CUSTOM_MESH_ST"
PULL_ST_TEK = "FWD_SSBO_CUSTOM_ST"

TOKEN = "[SPVR:MESHMV-STEPDOWN]"

# the child env switch: "sel" = pipeline-cache selection, "res" = a real render
CHILD_ENV = "MESHMV_CHILD"

W = H = 256
SETTLE_FRAMES = 60
MIN_COVERAGE  = 0.02
MIN_MEAN      = 1.0


################################################################################
# CHILD "sel" — the real pipeline cache, on a real generated mesh material
################################################################################

def _child_sel(outdir):
  from orkengine import core   # core before lev2
  from orkengine import lev2
  from ork.testing import headless_app
  from ork.hypergraph.ptex3d import materialize_surface_fxv2
  from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource

  # the terrain chunk source with mesh=True FORCED (not env-derived): this file must
  #  compile the same material no matter what toggle the caller's shell carries.
  kwargs = TerrainChunkVertexSource(dim=1024, extent_m=1000.0, chunk=128,
                                    mesh=True).as_material_kwargs()
  body = ("o.albedo   = vec3(uv.x, uv.y, 0.5);\n"
          "o.emissive = vec3(0.0);")

  results = {}
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx
    results["supports_mesh_shader"] = bool(ctx.supports_mesh_shader)
    results["supports_multiview"] = bool(ctx.supports_multiview)
    results["supports_multiview_mesh_shader"] = bool(ctx.supports_multiview_mesh_shader)
    results["max_mesh_multiview_views"] = int(ctx.max_mesh_multiview_views)
    if not ctx.supports_mesh_shader:
      json.dump(results, open(os.path.join(outdir, "child.json"), "w"), indent=1)
      return 0

    path = materialize_surface_fxv2(body, name_hint="spvr_meshmv_sel", **kwargs)
    print("meshmv: generated %s" % path, flush=True)

    mtl = lev2.PBRMaterial()
    mtl.shaderpath = path
    mtl.gpuInit(ctx)
    cache = mtl.fxcache

    def select(stereo):
      permu = lev2.FxPipelinePermutation()
      permu.rendermodel = "FORWARD_PBR"
      # BOTH flags, because a real mesh-sourced drawable raises both (the same storage
      #  feeds the mesh stage and the pull VS). A permutation carrying only
      #  is_mesh_shader models no drawable that exists, and would make the step-down
      #  land somewhere the engine never lands.
      permu.is_mesh_shader = True
      permu.is_vertex_ssbo = True
      permu.stereo = stereo
      pipe = cache.findPipeline(permu)
      return pipe.technique_name if pipe else None

    results["stereo"] = select(True)
    results["mono"] = select(False)
    print("meshmv sel: stereo -> %s  mono -> %s" % (results["stereo"], results["mono"]), flush=True)
    results["validation_armed"] = bool(ctx.validation_armed)
    results["validation_errors"] = int(ctx.validation_errors)
    del cache, mtl

  json.dump(results, open(os.path.join(outdir, "child.json"), "w"), indent=1)
  return 0


################################################################################
# CHILD "res" — a real hypermesh drawable, rendered offscreen. What is read back is
# which DRAW PATH the drawable resolved, which is the half a selection answer cannot
# reach: the drawable decides its draw kind outside any pass, before a permutation
# exists.
################################################################################

def _child_res(outdir):
  import numpy
  from orkengine.core import vec3
  from orkengine import lev2
  from ork.app.application import ComponentizedApplication
  from ork.app.std_scenegraph import StandardSceneGraphComponent
  from ork.hypergraph.dflow.hypermesh import Hypermesh, make_drawable
  from ork.hypergraph.assets.materials.terrain.solid import Solid

  class Sphere(Hypermesh):
    def __init__(self):
      super().__init__()
      # TRI-ONLY (icosphere faces are triangles) — the same choice, for the same
      #  reason, as test_hypermesh_meshshader_draw.py
      self.output(self.face_normals(self.icosphere(radius=1.5, subdivisions=3)))

  class ResApp(ComponentizedApplication):

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
      self.createEzApp(width=W, height=H, offscreen=True,
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
      cdd, _mtl = make_drawable(self.live, ctx, material_cls=Solid, roughness=0.6)
      self.node = SGC.layer_fwd.createDrawableNodeFromData("hm_meshmv", cdd)

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
      img = numpy.array(buf, dtype=numpy.uint8).reshape(h, w, 4)[..., :3]
      lum = img.astype(numpy.int32).sum(axis=2)
      print("MESHMV_RES mean=%.4f coverage=%.4f"
            % (float(img.mean()), float((lum > 24).mean())), flush=True)
      self._exit_code = 0
      self._done = True
      self.ezapp.signalExit()

  app = ResApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    print("MESHMV_RES NO-CAPTURE", flush=True)
    return 1
  return 0


################################################################################
# DRIVER
################################################################################

def _run_child(kind, outdir, forced):
  os.makedirs(outdir, exist_ok=True)
  env = dict(os.environ)
  env["ORKID_VULKAN_VALIDATE"] = "2"
  # every switch this file's verdict depends on is set or cleared HERE, so a value
  #  left in the caller's shell cannot decide a leg.
  for k in ("ORKID_TEST_FORCE_NO_MULTIVIEW_MESH", "ORKID_SPVR_NO_MULTIVIEW",
            "ORKID_GATE0_FORCE_MONO_TEK"):
    env.pop(k, None)
  if forced:
    env["ORKID_TEST_FORCE_NO_MULTIVIEW_MESH"] = "1"
  if kind == "res":
    env["ORKID_HYPERMESH_MESHSHADER"] = "1"
    # the drawable's own path lines are on a default-off channel; without them a
    #  crash after the mesh path engaged reads exactly like a path that never did.
    env["ORKID_LOGCHAN_0"] = "1"
  env[CHILD_ENV] = kind
  argv = [sys.executable, os.path.abspath(__file__), "--child", outdir]
  p = subprocess.run(argv, env=env, stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, timeout=900)
  out = p.stdout.decode("utf-8", "replace")
  rec = {}
  cjson = os.path.join(outdir, "child.json")
  if os.path.exists(cjson):
    rec = json.load(open(cjson))
  return p.returncode, out, rec


def main():
  outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
      tempfile.gettempdir(), "spvr_meshmv_stepdown")
  os.makedirs(outdir, exist_ok=True)
  t0 = time.time()
  fails = []

  def check(label, ok, why="", info=""):
    # `why` is the FAILURE explanation and prints only on failure; `info` is the
    #  measurement and always prints. Printing a failure sentence next to PASS is how
    #  a green transcript ends up reading like a red one.
    print("  %-30s %s %s" % (label, "PASS" if ok else "FAIL", info if ok else (why or info)),
          flush=True)
    if not ok:
      fails.append(label)

  # ---------------------------------------------------------------- selection
  rc_a, out_a, rec_a = _run_child("sel", os.path.join(outdir, "sel_granted"), forced=False)
  if rc_a != 0:
    print(out_a, flush=True)
    print("TESTVERDICT FAIL: the capability-granting selection child exited rc=%d" % rc_a, flush=True)
    return 1
  print("caps: mesh=%s multiview=%s multiview_mesh=%s max_mesh_views=%s"
        % (rec_a.get("supports_mesh_shader"), rec_a.get("supports_multiview"),
           rec_a.get("supports_multiview_mesh_shader"), rec_a.get("max_mesh_multiview_views")),
        flush=True)
  if not rec_a.get("supports_mesh_shader"):
    print("SKIP: device reports no VK_EXT_mesh_shader — there is no mesh arm to gate here",
          flush=True)
    print("test_spvr_meshmv_stepdown: SKIP in %.1fs" % (time.time() - t0), flush=True)
    return 0
  if not rec_a.get("supports_multiview_mesh_shader"):
    # a device that ALREADY lacks the capability cannot show leg (a); it is the very
    #  configuration the gate exists for, and the forced legs alone are then vacuous.
    print("SKIP: device grants mesh but not multiview-mesh — the unchanged-behaviour leg has "
          "no subject on this box (this IS the configuration the gate defends; the mac seat "
          "verifies it by rendering, not by forcing)", flush=True)
    print("test_spvr_meshmv_stepdown: SKIP in %.1fs" % (time.time() - t0), flush=True)
    return 0

  # ---- leg (a) SEL_GRANTED
  print("LEG_SEL_GRANTED", flush=True)
  check("sel_granted_stereo_is_mesh_ST", rec_a.get("stereo") == MESH_ST_TEK,
        info="selected <%s> (want <%s>)" % (rec_a.get("stereo"), MESH_ST_TEK))
  check("sel_granted_mono_untouched", rec_a.get("mono") == MESH_TEK,
        info="selected <%s> (want <%s>)" % (rec_a.get("mono"), MESH_TEK))
  check("sel_granted_silent", TOKEN not in out_a,
        why="a step-down was announced on a device that grants the capability")
  check("sel_granted_validation_clean",
        bool(rec_a.get("validation_armed")) and int(rec_a.get("validation_errors", -1)) == 0,
        info="armed=%s errors=%s" % (rec_a.get("validation_armed"), rec_a.get("validation_errors")))

  # ---- leg (b) SEL_FORCED
  rc_b, out_b, rec_b = _run_child("sel", os.path.join(outdir, "sel_forced"), forced=True)
  print("LEG_SEL_FORCED", flush=True)
  check("sel_forced_child_exits_clean", rc_b == 0, info="rc=%d" % rc_b)
  check("sel_forced_stereo_is_pull_ST", rec_b.get("stereo") == PULL_ST_TEK,
        info="selected <%s> (want <%s>)" % (rec_b.get("stereo"), PULL_ST_TEK))
  # the step-down must not become a step-down to MONO: that trades a crash for a
  #  silently wrong second eye, which is worse than the crash.
  check("sel_forced_not_mono", (rec_b.get("stereo") or "").endswith("_ST"),
        why="stereo permutation took <%s>, which carries no _ST suffix" % rec_b.get("stereo"))
  check("sel_forced_announced", TOKEN in out_b, why="no %s line" % TOKEN)
  check("sel_forced_mono_untouched", rec_b.get("mono") == MESH_TEK,
        info="selected <%s> (want <%s>)" % (rec_b.get("mono"), MESH_TEK))

  # ---------------------------------------------------------------- resolve
  rc_c, out_c, _ = _run_child("res", os.path.join(outdir, "res_granted"), forced=False)
  drove_c = re.search(r"driving (\d+) meshlets", out_c)
  print("LEG_RES_GRANTED", flush=True)
  check("res_granted_child_exits_clean", rc_c == 0, info="rc=%d" % rc_c)
  check("res_granted_drove_mesh_path", drove_c is not None,
        why="no 'driving N meshlets' line — the rig has no mesh path to lose, so leg (d) "
            "would pass vacuously",
        info="meshlets=%s" % (drove_c.group(1) if drove_c else "?"))
  check("res_granted_silent", TOKEN not in out_c,
        why="a step-down was announced on a device that grants the capability")

  rc_d, out_d, _ = _run_child("res", os.path.join(outdir, "res_forced"), forced=True)
  drove_d = re.search(r"driving (\d+) meshlets", out_d)
  stat_d = re.search(r"MESHMV_RES mean=([\d.]+) coverage=([\d.]+)", out_d)
  print("LEG_RES_FORCED", flush=True)
  # rc is the whole point: before the resolve-site gate this leg was a SIGSEGV
  #  (_createPipelineMesh with no mesh stage in the pass), reached through a
  #  technique-only step-down.
  check("res_forced_child_exits_clean", rc_d == 0, info="rc=%d" % rc_d)
  check("res_forced_stepped_down", drove_d is None,
        why="still drove %s meshlets — the resolve site did not refuse"
            % (drove_d.group(1) if drove_d else "?"),
        info="no mesh dispatch")
  check("res_forced_announced", TOKEN in out_d, why="no %s line" % TOKEN)
  # the WORDING is part of the contract: Metal renders this workload today, so a line
  #  that reads like a hardware limit would send future work the wrong way.
  named_lines = [l for l in out_d.splitlines() if TOKEN in l]
  check("res_forced_names_moltenvk_gap",
        any(("MOLTENVK GAP" in l) and ("maxMeshMultiviewViewCount=1" in l) for l in named_lines),
        why="the refusal never names the MoltenVK gap; a reader would conclude the hardware cannot")
  if stat_d:
    check("res_forced_frame_nondegenerate",
          float(stat_d.group(2)) >= MIN_COVERAGE and float(stat_d.group(1)) >= MIN_MEAN,
          info="coverage=%s mean=%s" % (stat_d.group(2), stat_d.group(1)))
  else:
    check("res_forced_frame_nondegenerate", False, why="no MESHMV_RES line — nothing rendered")

  dt = time.time() - t0
  if fails:
    print(out_d, flush=True)
    print("TESTVERDICT FAIL (%d): %s" % (len(fails), ", ".join(fails)), flush=True)
    print("test_spvr_meshmv_stepdown: FAIL in %.1fs" % dt, flush=True)
    return 1
  print("TESTVERDICT PASS -- with multiview-mesh granted a stereo mesh draw still selects %s and "
        "the hypermesh drawable still drives the mesh path; with it forced away both the "
        "technique (%s) and the DRAW PATH step down to pull-VS, the refusal names the MoltenVK "
        "gap, and the render completes (%.1fs)" % (MESH_ST_TEK, PULL_ST_TEK, dt), flush=True)
  return 0


if __name__ == "__main__":
  kind = os.environ.get(CHILD_ENV)
  if kind is not None:
    _outdir = sys.argv[2] if len(sys.argv) > 2 else tempfile.gettempdir()
    sys.exit(_child_sel(_outdir) if kind == "sel" else _child_res(_outdir))
  sys.exit(main())
