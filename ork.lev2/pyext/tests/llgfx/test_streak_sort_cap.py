#!/usr/bin/env ork.python
###############################################################################
# test_streak_sort_cap — bug #92 gate: an editor-reachable sort toggle on a particle
# pool larger than the old fixed 32768-entry sort LUT must NEVER assert (owner law:
# editor-reachable state changes never abort the render thread).
#
# The owner repro: ork.dflow.edit.py exprcolor -> click the streak `sort` property ->
# rebake -> StreakRendererInst::_render OrkAssert(icnt<32768) -> abort on the render
# thread. exprcolor authors an 80000-particle pool (steady-state alive ~= EmissionRate *
# LifeSpan ~= 60000 > 32768), so the sorted streak path hit a cap the unsorted path
# (icnt<=262144) never did.
#
# THREE isolated subprocess leaves (each a fresh process — the sibling dflowedit-gate
# discipline; a sort-toggle abort in one leaf never taints another):
#
#   --leaf model : the pool_size property carries an editor.range reaching the propsheet
#     model with max >= 65536 (the coordinator's slider fix) AND the VdbLevelSetRenderer
#     rows resolve declared ranges; exprcolor's authored pool exceeds 32768; the streak
#     `sort` toggle round-trips through the reflected-property path the propsheet writes;
#     a pool_size=50000 edit round-trips + instantiates.
#
#   --leaf render_sortoff : exprcolor (sort=False) advanced past the cap then rendered
#     offscreen -> a non-black capture (baseline: the big pool renders fine unsorted).
#
#   --leaf render_sorton  : exprcolor with sort TOGGLED True through the reflected-property
#     path, advanced past the cap then rendered -> render DOES NOT abort (rc=0) and produces
#     a non-black capture. PRE-FIX this leaf SIGABRTs during rendering (rc!=0, no RENDERCAP);
#     POST-FIX it completes. particle_alive_count asserts the render actually crossed 32768
#     (so the gate genuinely exercises the former cap).
#
# ork.python only (orkengine.core before lev2); shell-family scripts shadowed from THIS
# repo's obt.project/scripts (worktree/lane runs test the code next to them).
###############################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import subprocess
import shutil

# Prefer THIS repo's obt.project/scripts (lane/worktree shadowing), like test_dflowedit.
_SCRIPTS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                         "..", "..", "..", "..", "obt.project", "scripts"))
if _SCRIPTS not in sys.path[:1]:
  sys.path.insert(0, _SCRIPTS)

SOURCE = "exprcolor"           # the owner's exact repro asset (pool_size=80000 authored)
POOL_MIN = 32768               # the former hard sorted-path cap
WARMUP_FRAMES = 40             # min rendered frames before a capture is considered
CAP_W, CAP_H = 480, 360
_SKYBOX = "<ork_envmaps2>/blender_night.xir"


###############################################################################
# shared helpers (run inside each subprocess after engine init)
###############################################################################

def _exprcolor_graph():
  """The live exprcolor GraphData (the owner's repro asset), authored via its DSL class."""
  from ork.hypergraph.dflow.particles import resolve as R
  path = R.resolve_dsl_file(SOURCE)
  mod = R.load_dsl_module(path)
  cls = R.class_in_module(mod, path)
  return cls().generatedflow()


def _pool_authored_size(graph):
  from orkengine import lev2
  pool = graph.findModuleByClass(lev2.particles.Pool)
  assert pool is not None, "exprcolor graph has no Pool module"
  return int(pool.properties.pool_size)


def _set_streak_sort(graph, flag):
  """Toggle the streak renderer `sort` through the reflected-property proxy the propsheet
  writes (graphdata_document.set_param -> setattr(mod.properties, 'sort', ...))."""
  from orkengine import lev2
  strk = graph.findModuleByClass(lev2.particles.StreakRenderer)
  assert strk is not None, "exprcolor graph has no StreakRenderer module"
  strk.properties.sort = bool(flag)
  return strk


###############################################################################
# --leaf model : property-metadata + document round-trip (no crash-under-test)
###############################################################################

def leaf_model():
  from orkengine import core            # noqa: F401  (core before lev2)
  from orkengine import lev2
  from orkengine import ecs
  from orkengine.core import dataflow as _dflow, Object

  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    fails = []
    def _ck(c, m):
      print(("  ok  : " if c else "  FAIL: ") + m, flush=True)
      if not c:
        fails.append(m)

    from ork.hypergraph.dflow.document import prop_meta, plug_meta
    classes = _dflow.moduleClasses()

    def _prop_anns(reflected_class, prop):
      entry = next((c for c in classes if c.get("name") == reflected_class), None)
      if entry is None:
        return None
      p = next((p for p in entry.get("properties", ()) if p["name"] == prop), None)
      return p.get("annotations", {}) if p else None

    # 1. pool_size editor.range reaches the propsheet model with max >= 65536 (slider fix).
    anns = _prop_anns("psys::ParticlePoolData", "pool_size")
    _ck(anns is not None, "moduleClasses() surfaces ParticlePoolData.pool_size")
    anns = anns or {}
    rng_max = float(anns.get("editor.range.max", -1))
    rng_min = float(anns.get("editor.range.min", -1))
    _ck(rng_max >= 65536, "pool_size editor.range.max=%.0f reaches model (>= 65536)" % rng_max)
    _ck(0 <= rng_min < rng_max, "pool_size editor.range.min=%.0f is a sane floor" % rng_min)
    meta = prop_meta("psys::ParticlePoolData", "pool_size")
    _ck(meta is not None and meta.get("max", 0) >= 65536,
        "prop_meta('pool_size') max=%s >= 65536 (propsheet-visible)" % (meta and meta.get("max")))

    # 1b. VdbLevelSetRenderer plug/property ranges resolve in the model metadata (spot-check):
    #     the voxel_size PROPERTY (the newly-annotated range, via prop_meta) plus two declared
    #     PLUGS (via plug_meta / plugSpec). Proves rows resolve real ranges, not the fallback.
    vdb = "psys::VdbLevelSetRendererData"
    def _rng_ok(m, lo, hi):
      return m is not None and abs(float(m.get("min", 1e9)) - lo) < 1e-3 and abs(float(m.get("max", -1e9)) - hi) < 1e-3
    vm_vox = prop_meta(vdb, "voxel_size")
    _ck(_rng_ok(vm_vox, 0.01, 1.0), "VdbLevelSetRenderer.voxel_size range resolves %s (want [0.01,1.0])" % vm_vox)
    vm_rad = plug_meta(vdb, "Radius")
    _ck(_rng_ok(vm_rad, 0.01, 10.0), "VdbLevelSetRenderer.Radius plug range resolves %s (want [0.01,10.0])" % vm_rad)
    vm_iso = plug_meta(vdb, "IsoLevel")
    _ck(_rng_ok(vm_iso, 0.0, 10.0), "VdbLevelSetRenderer.IsoLevel plug range resolves %s (want [0.0,10.0])" % vm_iso)

    # 2. exprcolor authors a pool bigger than the former cap.
    graph = _exprcolor_graph()
    authored = _pool_authored_size(graph)
    _ck(authored > POOL_MIN, "exprcolor authored pool_size=%d exceeds cap %d" % (authored, POOL_MIN))

    # 3. the streak `sort` toggle (owner's action) round-trips through the document path.
    strk = _set_streak_sort(graph, True)
    _ck(bool(strk.properties.sort) is True, "streak sort=True took on the live module")
    clone = Object.deserializeJson(graph.serializeJson())
    cstrk = clone.findModuleByClass(lev2.particles.StreakRenderer)
    _ck(cstrk is not None and bool(cstrk.properties.sort) is True,
        "streak sort=True survives graph serialize/deserialize round-trip")

    # 4. a pool_size=50000 edit through the document path round-trips + instantiates.
    cpool = clone.findModuleByClass(lev2.particles.Pool)
    cpool.properties.pool_size = 50000
    clone2 = Object.deserializeJson(clone.serializeJson())
    c2pool = clone2.findModuleByClass(lev2.particles.Pool)
    _ck(int(c2pool.properties.pool_size) == 50000, "pool_size=50000 survives round-trip")
    dd = lev2.ParticlesDrawableData()
    dd.graphdata = clone2
    drw = dd.createDrawable()
    _ck(drw is not None, "pool_size=50000 graph instantiates (createDrawable != None)")

    ok = not fails
    print("MODEL=%s" % ("PASS" if ok else "FAIL"), flush=True)
  except Exception:
    import traceback
    traceback.print_exc()
    print("MODEL=FAIL", flush=True)
  finally:
    ezapp.mainThreadEnd()
    ecs.headless_exit()
  sys.exit(0 if ok else 1)


###############################################################################
# --leaf render_sortoff / render_sorton : the crash repro (offscreen OrkEzApp)
#
# Offscreen OrkEzApp render loop (the proven pixel-capture path): the sim is advanced
# on the update thread (external_compute=True), the scene is composited to the offscreen
# main_RTG in onDraw, and main_RTG is captured once the pool has climbed past the cap.
###############################################################################

class _RenderApp:

  def __init__(self, do_sort):
    from orkengine import lev2
    from orkengine.core import UpdateData
    self._lev2 = lev2
    self._do_sort = do_sort
    self.ezapp = lev2.OrkEzApp.create(self, width=CAP_W, height=CAP_H, offscreen=True, ssaa=0)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self._updata = UpdateData()
    self._updata.absolutetime = 0.0
    self._updata.deltatime = 1.0 / 60.0
    self._ginst = None
    self._frame = 0
    self._alive = 0
    self._cap_wait = 0
    self._cap_inflight = False
    self._cap_buf = None
    self._cap_async = None
    self.result = None      # dict(mean, max, w, h, alive) once captured

  def onGpuInit(self, ctx):
    from orkengine.core import vec3, VarMap, lev2_pyexdir
    lev2 = self._lev2
    graph = _exprcolor_graph()
    self._authored = _pool_authored_size(graph)
    if self._do_sort:
      _set_streak_sort(graph, True)          # the owner's exact action (reflected-prop path)
    dd = lev2.ParticlesDrawableData()
    dd.graphdata = graph
    dd.external_compute = True               # THIS host owns the sim advance (not the enqueue lambda)
    drawable = dd.createDrawable()
    assert drawable is not None, "createDrawable() returned None"
    self._ginst = lev2.particles_drawable_graphinst(drawable)
    assert self._ginst is not None, "particles_drawable_graphinst() returned None"

    lev2_pyexdir.addToSysPath()
    from lev2utils.scenegraph import createSceneGraph
    from lev2utils.cameras import setupUiCamera
    vm = VarMap()
    vm.SkyboxTexPathStr = _SKYBOX
    vm.SkyboxIntensity = 1.0
    vm.AmbientLevel = vec3(0.15)
    vm.ssaa = 0
    vm.msaa = 0
    createSceneGraph(app=self, rendermodel="ForwardPBR", vars=vm, use_float_buffer=True)
    self.layer1.createDrawableNode("ptc_sortcap", drawable)
    setupUiCamera(app=self, near=0.1, far=20000.0, fov_deg=45,
                  eye=vec3(0, 3.5, 17), tgt=vec3(0, 3, 0), up=vec3(0, 1, 0))

  def onUpdate(self, updinfo):
    if self._ginst is not None:
      self._ginst.compute(self._updata)      # advance the sim (pushes the render buffer)
      self._updata.absolutetime = float(self._updata.absolutetime) + (1.0 / 60.0)
    self.scene.updateScene(self.cameralut)

  def onDraw(self, drawevent):
    # PRE-FIX + sort: StreakRendererInst::_render OrkAssert(icnt<32768) aborts HERE once the
    # pool climbs past 32768. POST-FIX: composites the sorted streaks to the offscreen main_RTG.
    # (this init path drives onDraw, not onGpuPostFrame, so the capture state machine lives here.)
    ctx = drawevent.context
    self.ezapp.processMainSerialQueue()
    self.scene.renderOnContext(ctx)
    self._frame += 1
    if self.result is not None:
      return
    self._maybeCapture(ctx)

  def _maybeCapture(self, ctx):
    lev2 = self._lev2
    if not self._cap_inflight:
      # keep warming until the render has genuinely crossed the former cap (else the gate
      # would not exercise the sorted-path overflow at all).
      if self._frame < WARMUP_FRAMES or self._ginst is None:
        return
      self._alive = int(lev2.particle_alive_count(self._ginst))
      if self._alive <= POOL_MIN:
        return
      # the render has genuinely crossed the former 32768 sorted-path cap by now.
      print("[streak-sort-cap] crossed cap at frame=%d alive=%d (> %d)"
            % (self._frame, self._alive, POOL_MIN), flush=True)
      self._cap_buf = lev2.CaptureBuffer()
      rtg = ctx.FBI.main_RTG
      self._cap_async = ctx.FBI.captureAsFormat(rtg.buffer(0), self._cap_buf, "RGBA8")
      self._cap_inflight = True
      self._cap_wait = 0
      return
    self._cap_wait += 1
    if self._cap_async is None or bool(self._cap_async.is_ready) or self._cap_wait > 120:
      import numpy
      w, h = self._cap_buf.width, self._cap_buf.height
      rgb = numpy.array(self._cap_buf, dtype=numpy.uint8).reshape(h, w, 4)[..., :3]
      self.result = {"mean": float(rgb.mean()), "max": int(rgb.max()),
                     "w": w, "h": h, "alive": self._alive}
      self.ezapp.signalExit()


def leaf_render(mode):
  from orkengine import core            # noqa: F401
  from orkengine import lev2            # noqa: F401
  do_sort = (mode == "render_sorton")
  app = _RenderApp(do_sort)
  app.ezapp.mainThreadLoop()             # self-terminating on the in-callback signalExit
  r = app.result
  ok = False
  if r is None:
    print("RENDER=FAIL (no capture — loop exited early)", flush=True)
  else:
    nonblack = r["max"] > 0 and r["mean"] > 1.0
    print("RENDERCAP mode=%s sort=%d alive=%d cap=%d mean=%.3f max=%d size=%dx%d nonblack=%d"
          % (mode, int(do_sort), r["alive"], POOL_MIN, r["mean"], r["max"], r["w"], r["h"],
             int(nonblack)), flush=True)
    ok = bool(nonblack and r["alive"] > POOL_MIN)
    print("RENDER=%s" % ("PASS" if ok else "FAIL"), flush=True)
  try:
    app.ezapp.shutdown()
  except Exception:
    pass
  sys.exit(0 if ok else 1)


###############################################################################
# orchestrator
###############################################################################

def _spawn(orkpython, leaf, timeout=180):
  cmd = [orkpython, os.path.abspath(__file__), "--leaf", leaf]
  r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
  out = r.stdout or ""
  for ln in out.splitlines():
    if any(k in ln for k in ("MODEL=", "RENDER=", "RENDERCAP", "  FAIL:", "OrkAssert", "Assert")):
      print("    [%s] %s" % (leaf, ln.strip()), flush=True)
  return r, out


def _orchestrate():
  orkpython = shutil.which("ork.python") or sys.executable
  results = {}

  # 1. model leaf (property metadata + document round-trip).
  r, out = _spawn(orkpython, "model")
  results["model"] = (r.returncode == 0 and "MODEL=PASS" in out)
  if not results["model"]:
    print("    [model] rc=%d stderr tail:\n%s" % (r.returncode, (r.stderr or "")[-700:]), flush=True)

  # 2. baseline: big pool renders unsorted (non-black).
  r, out = _spawn(orkpython, "render_sortoff")
  results["render_sortoff"] = (r.returncode == 0 and "RENDER=PASS" in out)
  if not results["render_sortoff"]:
    print("    [render_sortoff] rc=%d stderr tail:\n%s" % (r.returncode, (r.stderr or "")[-700:]),
          flush=True)

  # 3. THE FIX: sort toggled True on the big pool renders WITHOUT aborting (rc=0, non-black).
  #    A pre-fix binary SIGABRTs here (rc!=0, no RENDER=PASS) — this leaf IS the bug gate.
  r, out = _spawn(orkpython, "render_sorton")
  crashed = (r.returncode != 0)
  results["render_sorton"] = (not crashed and "RENDER=PASS" in out)
  if not results["render_sorton"]:
    print("    [render_sorton] rc=%d (crash=%s) stderr tail:\n%s"
          % (r.returncode, crashed, (r.stderr or "")[-700:]), flush=True)

  allok = all(results.values())
  print("STREAK_SORT_CAP: " + "  ".join("%s=%s" % (k, "PASS" if v else "FAIL")
                                        for k, v in results.items()), flush=True)
  print("STREAK_SORT_CAP_RESULT=%s" % ("PASS" if allok else "FAIL"), flush=True)
  return 0 if allok else 1


def main(argv):
  if len(argv) >= 2 and argv[0] == "--leaf":
    leaf = argv[1]
    if leaf == "model":
      leaf_model()
    elif leaf in ("render_sortoff", "render_sorton"):
      leaf_render(leaf)
    else:
      print("unknown leaf %r" % leaf, flush=True)
      sys.exit(2)
    return 0
  return _orchestrate()


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
