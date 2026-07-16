#!/usr/bin/env python3
###############################################################################
# E1 INTROSPECTION + LAYOUT gate (fast, headless).
#
# Proves the reflection-driven dataflow introspection surface a visual node
# editor consumes, plus GRAPH-level editor layout persistence:
#   (a) moduleClasses() enumerates registered DgModuleData subclasses across
#       families (terrain + hypermesh + particle at least),
#   (b) plugSpec("<class>") returns a plug schema (names/types/rates) WITHOUT
#       the caller instantiating a module,
#   (c) a small graph round-trips: node positions AND typed edges re-enumerate
#       after serializeJson -> deserializeJson,
#   (d) HASH-NEUTRALITY: setting layout does NOT perturb any MODULE's serialized
#       identity (hypermeshModuleIdentityHash = module JSON, uuid-stripped), so
#       dragging a node is never a cook-cache miss. (The end-to-end warm-cache
#       proof is the separate test_hypermesh_cookcache.py run.)
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, json
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import Object

dflow = core.dataflow


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    ###########################################################################
    # (a) node-type enumeration across families
    ###########################################################################
    classes = dflow.moduleClasses()
    names   = {c["name"] for c in classes}
    assert len(classes) > 20, "moduleClasses() returned a trivial list (%d)" % len(classes)
    assert any(n.startswith("terrain::")   for n in names), "no terrain module classes enumerated"
    assert any(n.startswith("hypermesh::") for n in names), "no hypermesh module classes enumerated"
    assert any(n.startswith("psys::")      for n in names), "no particle module classes enumerated"
    # family tags derive from the reflected-name namespace prefix (drift-proof)
    fbm_desc = next(c for c in classes if c["name"] == "terrain::FbmModuleData")
    assert fbm_desc["family"] == "terrain", "family tag wrong: %r" % fbm_desc["family"]
    fbm_props = {p["name"] for p in fbm_desc["properties"]}
    assert "octaves" in fbm_props and "seed" in fbm_props, \
        "Fbm reflected props missing octaves/seed: %r" % fbm_props
    # inherited props are included (own+parent chain)
    assert "bypassed" in fbm_props, "inherited DgModuleData props not enumerated"
    print("INTROSPECT moduleClasses PASS (%d classes; terrain+hypermesh+particle present)" % len(classes), flush=True)

    ###########################################################################
    # (b) class-level plug schema WITHOUT instantiating a module (palette need)
    ###########################################################################
    fbm_spec = dflow.plugSpec("terrain::FbmModuleData")
    assert fbm_spec is not None, "plugSpec(terrain::FbmModuleData) returned None"
    fbm_ins  = {p["name"] for p in fbm_spec["inputs"]}
    fbm_outs = {p["name"] for p in fbm_spec["outputs"]}
    assert {"frequency", "amplitude"} <= fbm_ins, "Fbm input plug names wrong: %r" % fbm_ins
    assert "Out" in fbm_outs, "Fbm output plug 'Out' missing: %r" % fbm_outs
    for p in fbm_spec["inputs"] + fbm_spec["outputs"]:
      assert p["type"], "plug %r missing type name" % p["name"]
      assert p["rate"], "plug %r missing rate" % p["name"]

    noz_spec = dflow.plugSpec("psys::NozzleEmitterData")
    assert noz_spec is not None, "plugSpec(psys::NozzleEmitterData) returned None"
    noz_ins = {p["name"] for p in noz_spec["inputs"]}
    assert {"LifeSpan", "EmissionRate"} <= noz_ins, "Nozzle input plug names wrong: %r" % noz_ins
    print("INTROSPECT plugSpec PASS (Fbm + NozzleEmitter schemas w/ types+rates)", flush=True)

    ###########################################################################
    # build a small round-trippable graph from concrete core modules
    # (reshapeIOs rebuilds their plugs on deserialize, so edges resolve).
    #   srcA:MinModule.value  -->  dst:MaxModule.A
    ###########################################################################
    gd   = dflow.GraphData.createShared()
    gd.cacheable = True
    srcA = gd.create("srcA", dflow.MinModule)
    dst  = gd.create("dst",  dflow.MaxModule)
    gd.connect(dst.inputs.A, srcA.outputs.value)

    ###########################################################################
    # edge introspection (enough to redraw): one typed edge, correct endpoints
    ###########################################################################
    edges = gd.edges()
    assert len(edges) == 1, "expected 1 edge, got %d" % len(edges)
    e = edges[0]
    assert e["out_module"] == "srcA" and e["out_plug"] == "value", "edge producer wrong: %r" % e
    assert e["in_module"] == "dst" and e["in_plug"] == "A", "edge consumer wrong: %r" % e
    assert e["out_type"] and e["in_type"], "edge missing typed plug names: %r" % e
    # per-plug introspection symmetry
    assert dst.inputs.A.is_connected, "input plug reports not connected"
    assert dst.inputs.A.connected_output.module_name == "srcA", "connected_output producer wrong"
    fanout = srcA.outputs.value.connections
    assert len(fanout) == 1 and fanout[0]["module"] == "dst" and fanout[0]["plug"] == "A", \
        "output fan-out wrong: %r" % fanout
    print("INTROSPECT edges PASS (typed edge + per-plug fan-out)", flush=True)

    ###########################################################################
    # (d) HASH-NEUTRALITY: layout is graph-level, never in a module's JSON
    ###########################################################################
    js_src_before = srcA.serializeJson()
    js_dst_before = dst.serializeJson()
    gd.setNodePos("srcA", 10.0, 20.0)
    gd.setNodePos("dst",  30.0, 40.0)
    assert srcA.serializeJson() == js_src_before, "layout perturbed srcA module JSON (cook-hash risk!)"
    assert dst.serializeJson()  == js_dst_before, "layout perturbed dst module JSON (cook-hash risk!)"
    print("INTROSPECT hash-neutrality PASS (module JSON identical with/without layout)", flush=True)

    ###########################################################################
    # (c) round-trip: positions + edges survive serializeJson->deserializeJson
    ###########################################################################
    js  = gd.serializeJson()
    assert "editor_layout" in js, "editor_layout absent from serialized graph"
    gd2 = Object.deserializeJson(js)
    # byte-identical re-serialization (layout + edges + reflected state)
    assert gd2.serializeJson() == js, "graph re-serialization diverged"
    p_src = gd2.nodePos("srcA")
    p_dst = gd2.nodePos("dst")
    assert p_src is not None and abs(p_src.x - 10.0) < 1e-4 and abs(p_src.y - 20.0) < 1e-4, \
        "srcA position did not round-trip: %r" % p_src
    assert p_dst is not None and abs(p_dst.x - 30.0) < 1e-4 and abs(p_dst.y - 40.0) < 1e-4, \
        "dst position did not round-trip: %r" % p_dst
    assert set(gd2.node_layout.keys()) == {"srcA", "dst"}, "node_layout dict wrong: %r" % gd2.node_layout
    edges2 = gd2.edges()
    assert len(edges2) == 1, "edge lost across round-trip"
    e2 = edges2[0]
    assert (e2["out_module"], e2["out_plug"], e2["in_module"], e2["in_plug"]) == \
           ("srcA", "value", "dst", "A"), "edge diverged across round-trip: %r" % e2
    print("INTROSPECT round-trip PASS (positions + typed edges re-enumerate)", flush=True)

    ###########################################################################
    # additive/back-compat: an OLD save lacking editor_layout loads w/ empty layout
    ###########################################################################
    doc = json.loads(js)
    # strip the layout key wherever it appears (nested under the graph object)
    def _strip(o):
      if isinstance(o, dict):
        o.pop("editor_layout", None)
        for v in o.values(): _strip(v)
      elif isinstance(o, list):
        for v in o: _strip(v)
    _strip(doc)
    gd3 = Object.deserializeJson(json.dumps(doc))
    assert gd3.nodePos("srcA") is None, "missing-layout save should yield empty layout"
    assert len(gd3.node_layout) == 0, "missing-layout save should yield empty layout dict"
    assert len(gd3.edges()) == 1, "edges must still load when layout key is absent"
    print("INTROSPECT back-compat PASS (layout-less save loads empty, edges intact)", flush=True)

    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== dflow introspection gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
