#!/usr/bin/env python3
###############################################################################
# JUL09 S1 gate gB — editor construction + LIVE-HOST smoke (headless, NO window).
#
# Two segments, one headless process:
#  (1) DOCUMENT models over a loop/switch/iter document — TerrainDocOutlinerModel +
#      TerrainNodePropertyModel (editable scalar via set_param, DocLoop.count Int,
#      DocSwitch.selected Enum+choices, read-only L.i params).
#  (2) LIVE-HOST path (the segment that crashed the editor at startup): the runtime
#      builds an in-code ECS scene from the DOCUMENT and stands up a real Simulation.
#      This exercises build_scene_data() + create_live_scene()/_start_simulation() +
#      the schedule_rebuild -> apply_pending_rebuild state machine + clean teardown.
#
# ECS system-class registration: the EDITOR gets it from createEzApp(pre_init_fns=
# [ecs.ecsInitCallback]) (window bring-up — owner-eyeball); this headless gate gets
# the SAME registration outcome from ecs.headless_appinit, then drives the runtime's
# OWN live-sim path. The editor's createEzApp WINDOW init is the only window-only
# segment (named, not faked). A static check confirms the editor injects the pre-init.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, tempfile, textwrap

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

# module import (resolves the full editor import graph without opening a window)
import ork.editor.terrainedit  # noqa: F401
from ork.editor.terrainedit import TerrainEditor
from ork.editor.terrain_runtime import TerrainRuntime
from ork.editor.terrain_doc_model import (
    TerrainDocOutlinerModel, TerrainNodePropertyModel,
    TerrainParamsPropertyModel, TERRAIN_PARAMS_KEY)
from ork.hypergraph.dflow.terrain.doc import (
    DocNode, DocLoop, DocSwitch, TerrainDocParamError)

_ui = lev2.ui

FIXTURE = textwrap.dedent("""
    from ork.hypergraph.dflow.terrain import HeightField
    from ork.hypergraph.dflow import terrain as T

    class SmokeHF(HeightField):
        HEIGHT_M = 2000.0
        def __init__(self, which="smooth", count=3):
            super().__init__()
            h = T.Fbm(frequency=5.0, octaves=5) * 0.5 + 0.5
            with T.loop(count, h=h) as L:
                L.h = T.terrace(L.h, step_m=6.0, sharpness=2.0 + L.i * 0.5)
            smooth = T.lpf(L.h, cutoff_texels=6.0)
            ridged = T.terrace(L.h, step_m=8.0, sharpness=6.0)
            self.capture(T.switch(which, smooth=smooth, ridged=ridged), "height")
""")


def _find_scalar(doc):
  def walk(children):
    for ch in children:
      if isinstance(ch, DocNode):
        for (kind, name, value) in ch.editable_params():
          if isinstance(value, float) and not isinstance(value, bool):
            return (ch, kind, name, value)
      sub = getattr(ch, "children", None)
      if sub is not None:
        r = walk(sub)
        if r is not None:
          return r
    return None
  return walk(doc._root)


def main():
  results = {}
  d = tempfile.mkdtemp(prefix="tered_smoke_")
  path = os.path.join(d, "smoke.py")
  with open(path, "w") as f:
    f.write(FIXTURE)

  # headless subsystem init == the ECS registration outcome the editor's pre_init gives.
  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  # ---- static: the editor injects the ECS pre-init (the startup-crash fix) ---
  # _getPreInitFns ignores self, so an unbound call proves it without opening a window.
  editor_preinit_ok = ecs.ecsInitCallback in TerrainEditor._getPreInitFns(None)
  print(f"[gB.init] TerrainEditor pre-init registers ECS: {editor_preinit_ok}", flush=True)
  results["editor_ecs_preinit"] = editor_preinit_ok

  # ---- (1) document models --------------------------------------------------
  rt = TerrainRuntime()
  doc = rt.load(path)
  om = TerrainDocOutlinerModel(doc)
  roots = om.getChildren("")
  loop_key = next((k for k in roots if isinstance(om.object_for_key(k), DocLoop)), None)
  switch_key = next((k for k in roots if isinstance(om.object_for_key(k), DocSwitch)), None)
  loop_kids = om.getChildren(loop_key) if loop_key else []
  outliner_ok = (len(roots) > 0 and loop_key and switch_key
                 and om.hasChildren(loop_key) and len(loop_kids) > 0
                 and all(isinstance(om.object_for_key(k), DocNode) for k in loop_kids)
                 and isinstance(om.getDisplayName(roots[0]), str))
  print(f"[gB.outliner] roots={len(roots)} loop={loop_key!r} switch={switch_key!r} "
        f"loop_kids={len(loop_kids)} -> {bool(outliner_ok)}", flush=True)
  results["outliner_model"] = bool(outliner_ok)

  # capture-visibility toggle (the 'Caps' header filter): hidden -> no capture rows
  # anywhere in the tree; shown -> restored exactly.
  def _cap_keys():
    out = []
    def _walk(pk):
      for k in om.getChildren(pk):
        obj = om.object_for_key(k)
        if isinstance(obj, DocNode) and obj.clazz_name == "CaptureModule":
          out.append(k)
        _walk(k)
    _walk("")
    return out
  caps_before = _cap_keys()
  om.set_show_captures(False)
  caps_hidden = _cap_keys()
  om.set_show_captures(True)
  caps_after = _cap_keys()
  filt_ok = (len(caps_before) > 0 and caps_hidden == [] and caps_after == caps_before)
  print(f"[gB.outliner] capture-filter: before={len(caps_before)} hidden={len(caps_hidden)} "
        f"restored={caps_after == caps_before} -> {filt_ok}", flush=True)
  results["outliner_capture_filter"] = filt_ok

  terr = om.object_for_key(loop_kids[0])
  pm = TerrainNodePropertyModel(terr)
  keys = pm.getChildren("")
  steps_key = next((k for k in keys if pm.getDisplayName(k).startswith("step_m")), None)
  before = pm.getValue(steps_key)
  pm.setValue(steps_key, 9.0)
  after = pm.getValue(steps_key)
  node_ok = (steps_key is not None
             and pm.getPropertyType(steps_key) == _ui.PropertyType.Float
             and before != after and abs(after - 9.0) < 1e-4)
  print(f"[gB.node] steps {before}->{after} -> {node_ok}", flush=True)
  results["node_param_setvalue"] = node_ok

  iter_view = terr.iter_param_view()
  iter_key = next((k for k in keys if "#iter" in k), None)
  raised = False
  try:
    terr.set_param("inputs", "sharpness", 3.0)
  except TerrainDocParamError:
    raised = True
  iter_ok = (len(iter_view) >= 1 and iter_key is not None
             and pm.getPropertyType(iter_key) == _ui.PropertyType.String and raised)
  print(f"[gB.iter] iter_view={iter_view} set_param_raised={raised} -> {iter_ok}", flush=True)
  results["iter_param_readonly"] = iter_ok

  loop = om.object_for_key(loop_key)
  lpm = TerrainNodePropertyModel(loop)
  cnt_ok = (lpm.getChildren("") == ["count"]
            and lpm.getPropertyType("count") == _ui.PropertyType.Int
            and lpm.getValue("count") == loop.count)
  lpm.setValue("count", 7)
  cnt_ok = cnt_ok and (loop.count == 7)
  print(f"[gB.loop] count -> 7 -> loop.count={loop.count} -> {cnt_ok}", flush=True)
  results["loop_count_int"] = cnt_ok

  sw = om.object_for_key(switch_key)
  spm = TerrainNodePropertyModel(sw)
  sel_choices = spm.getChoices("selected")
  before_sel = spm.getValue("selected")
  other = next(c for c in sel_choices if c != before_sel)
  spm.setValue("selected", other)
  sw_ok = (spm.getPropertyType("selected") == _ui.PropertyType.Enum
           and set(sel_choices) == {"smooth", "ridged"} and sw.selected == other)
  print(f"[gB.switch] choices={sel_choices} {before_sel}->{sw.selected} -> {sw_ok}", flush=True)
  results["switch_enum_select"] = sw_ok

  # ---- (1b) Terrain Parameters model (DSL ctor kwargs) ----------------------
  # warp's frequency/octaves/ring_amp_m/ring_period_m/center are ctor kwargs (the
  # motivating case). The params model surfaces them as top-level rows (vec-ish center
  # as center[0]/center[1] floats), and the outliner shows a "Terrain Parameters" entry.
  rt_w = TerrainRuntime()
  rt_w.load("warp")
  ppm = TerrainParamsPropertyModel(rt_w)
  pkeys = ppm.getChildren("")
  # natural units add extent_m + derived texel_m as session rows alongside dim.
  want = {"dim", "extent_m", "texel_m", "frequency", "octaves", "ring_amp_m",
          "ring_period_m", "center[0]", "center[1]"}
  types_ok = (ppm.getPropertyType("frequency") == _ui.PropertyType.Float
              and ppm.getPropertyType("octaves") == _ui.PropertyType.Int
              and ppm.getPropertyType("center[0]") == _ui.PropertyType.Float)
  om_w = TerrainDocOutlinerModel(rt_w.document)
  om_w.set_top_extras([(TERRAIN_PARAMS_KEY, "Terrain Parameters")])
  extra_present = (TERRAIN_PARAMS_KEY in om_w.getChildren("")
                   and om_w.getDisplayName(TERRAIN_PARAMS_KEY) == "Terrain Parameters"
                   and om_w.object_for_key(TERRAIN_PARAMS_KEY) is None)
  params_ok = (set(pkeys) == want and types_ok and extra_present)
  print(f"[gB.params] rows={pkeys} types_ok={types_ok} outliner_extra={extra_present} "
        f"-> {params_ok}", flush=True)
  results["terrain_params_model"] = params_ok

  # doc-JSON session: NO DSL source -> params model empty, no "Terrain Parameters" entry.
  jd = os.path.join(tempfile.mkdtemp(prefix="tered_smoke_json_"), "warp_doc.json")
  rt_w.save_doc_json(jd)
  rt_j = TerrainRuntime()
  rt_j.load(jd, extent_m=rt_w.extent_m)
  jpm = TerrainParamsPropertyModel(rt_j)
  om_j = TerrainDocOutlinerModel(rt_j.document)      # no set_top_extras (no kwargs)
  # a doc-JSON session has NO DSL kwargs — only the session rows (dim + natural-units
  # extent_m / derived texel_m) remain.
  json_absent = (jpm.getChildren("") == ["dim", "extent_m", "texel_m"]
                 and rt_j.editable_dsl_kwargs() == {}
                 and TERRAIN_PARAMS_KEY not in om_j.getChildren(""))
  print(f"[gB.params] doc-JSON params rows={jpm.getChildren('')} "
        f"kwargs_empty={rt_j.editable_dsl_kwargs() == {}} -> {json_absent}", flush=True)
  results["docjson_params_absent"] = json_absent

  # ---- (2) LIVE-HOST path (the startup-crash segment) -----------------------
  rt2 = TerrainRuntime(preview_dim=256, chunk=128)
  rt2.load("voronoi", extent_m=512.0, amplitude=60.0)
  rt2.set_context(ctx)
  rt2.setup_camera()

  # build_scene_data lowers the in-code scene -> SceneData (scene.build/declareSystem
  # need the ECS registry; this is exactly what crashed with an empty registry).
  sd = rt2.build_scene_data(dim=256, simple_material=True)
  build_ok = (sd is not None) and (len(sd.serializeJson()) > 0)
  print(f"[gB.livehost] build_scene_data -> SceneData json ok: {build_ok}", flush=True)
  results["build_scene_data"] = build_ok

  # stand up a real Simulation (the runtime's OWN live path — never exercised by the
  # player-based snapshot gate). _sys_ref proves the SceneGraphSystem materialized.
  rt2.create_live_scene(ctx, dim=256)
  live_ok = (rt2.controller is not None and rt2.scenegraph is not None
             and rt2._sys_ref is not None)
  print(f"[gB.livehost] create_live_scene: controller={rt2.controller is not None} "
        f"scenegraph={rt2.scenegraph is not None} sys_ref={rt2._sys_ref is not None} -> {live_ok}",
        flush=True)
  results["create_live_scene"] = live_ok
  # single-threaded pump: the sim's GPU phase runs INLINE on this thread (which holds
  # ctx) — update()+gpuUpdate() split would deadlock the NEW->READY GPU rendezvous here.
  for _ in range(3):
    rt2.update_with_gpu(ctx)

  # two-phase rebuild state machine (single-threaded here — proves the SM, not the race):
  # Phase A prepare_rebuild (GPU build) -> Phase B apply_pending_rebuild (sim swap) ->
  # FRESH scene_data + FRESH scenegraph; the old scenegraph is retained (not dereferenced).
  no_pending = (rt2.apply_pending_rebuild() is False)   # nothing prepared -> no-op
  target = _find_scalar(rt2.document)
  if target is not None:
    node, kind, name, old = target
    node.set_param(kind, name, float(old) + 2.0)
  sd_before, sg_before = rt2.scene_data, rt2.scenegraph
  rt2.schedule_rebuild()
  pending_set = rt2._needs_rebuild
  prepared = rt2.prepare_rebuild(ctx, dim=256)          # PHASE A (GPU)
  ready = rt2._pending_ready
  applied = rt2.apply_pending_rebuild()                 # PHASE B (sim swap)
  sm_ok = (no_pending and pending_set and prepared is True and ready
           and applied is True
           and rt2._needs_rebuild is False and rt2._pending_ready is False
           and rt2.scene_data is not sd_before
           and rt2.scenegraph is not sg_before
           and sg_before in rt2.dead_scenegraphs)
  print(f"[gB.livehost] rebuild SM: no_pending={no_pending} pending_set={pending_set} "
        f"prepared={prepared} ready={ready} applied={applied} "
        f"fresh_sd={rt2.scene_data is not sd_before} fresh_sg={rt2.scenegraph is not sg_before} "
        f"old_sg_retained={sg_before in rt2.dead_scenegraphs} -> {sm_ok}", flush=True)
  results["rebuild_state_machine"] = sm_ok
  for _ in range(2):
    rt2.update_with_gpu(ctx)

  rt2._destroy_simulation()      # clean teardown before process exit

  ez.mainThreadEnd()
  ok = all(results.values())
  print(f"\n=== terrain editor smoke (gB) {'PASSED' if ok else 'FAILED'} ===", flush=True)
  for k, v in results.items():
    print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


main()
