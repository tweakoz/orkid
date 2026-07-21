#!/usr/bin/env python3
###############################################################################
# E1-close SCHEMA gate (fast, headless): dflow.to_json_schema() — the E8/MCP feed.
#
# Golden ORACLE over the terrain family's KNOWN shape. Asserts STRUCTURE, never
# full-byte equality (the schema grows as modules/annotations are added):
#   s1  the document is one json-serializable dict (schema/version/modules) and
#       enumerates all families (terrain + hypermesh + psys), name-sorted.
#   s2  lpf: cutoff input plug carries range 0..16384; cutoff_units property is an
#       enum with choices [texels, meters].
#   s3  enum choice lists: Combine.op + NoiseModule.basis (value-ordered).
#   s4  offset_vel (fbm + noise) is display_only (bake-inert marker).
#   s5  flow_erode.blend — a reflected module PROPERTY (not a plug) — carries the
#       editor.range.min/max 0..1 annotations (the params-lane carve-out).
#   s6  the reflection-carried add palette (class annotations dsl.verb /
#       editor.palette / .sort / .source / .recipe) reproduces EXACTLY the curated
#       13-verb menu the deleted hand-curated add-ops tuple encoded.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, json
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

dflow = core.dataflow

# the pre-change hand-curated add-ops tuple, captured VERBATIM (menu-parity oracle).
LEGACY_ADD_MENU = ("erode_thermal", "erox", "pha", "flow_erode", "basin_fill",
                   "fill_closed_basins", "terrace", "lpf", "normalize", "slope",
                   "expr", "fbm", "voronoi")


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    ###########################################################################
    # s1: one json document, all families, name-sorted
    ###########################################################################
    schema = dflow.to_json_schema()
    js = json.dumps(schema)              # json-serializable end to end
    assert len(js) > 1000, "schema dump suspiciously small"
    assert schema.get("schema") == "dflow.modules" and schema.get("version") == 1, \
        "schema envelope wrong: %r" % {k: schema.get(k) for k in ("schema", "version")}
    mods = schema["modules"]
    assert len(mods) > 20, "schema enumerates a trivial module list (%d)" % len(mods)
    names = [m["name"] for m in mods]
    assert names == sorted(names), "modules not name-sorted"
    fams = {m["family"] for m in mods}
    assert {"terrain", "hypermesh", "psys"} <= fams, "families missing: %r" % fams
    for m in mods:
      for k in ("name", "family", "annotations", "properties", "inputs", "outputs"):
        assert k in m, "module %r missing key %r" % (m.get("name"), k)
    by_name = {m["name"]: m for m in mods}
    def _prop(clazz, prop):
      return next(p for p in by_name[clazz]["properties"] if p["name"] == prop)
    def _in(clazz, plug):
      return next(p for p in by_name[clazz]["inputs"] if p["name"] == plug)
    print("SCHEMA envelope PASS (%d modules; terrain+hypermesh+psys; sorted)" % len(mods),
          flush=True)

    ###########################################################################
    # s2: lpf — cutoff plug range + cutoff_units enum choices
    ###########################################################################
    cutoff = _in("terrain::LpfModuleData", "cutoff")
    assert cutoff.get("min") == 0.0 and cutoff.get("max") == 16384.0, \
        "lpf cutoff range missing/wrong: %r" % cutoff
    cu = _prop("terrain::LpfModuleData", "cutoff_units")
    assert cu["type"] == "enum" and list(cu.get("choices", [])) == ["texels", "meters"], \
        "lpf cutoff_units enum wrong: %r" % cu
    print("SCHEMA lpf PASS (cutoff 0..16384; cutoff_units [texels,meters])", flush=True)

    ###########################################################################
    # s3: enum choice lists (value-ordered)
    ###########################################################################
    op = _prop("terrain::CombineModuleData", "op")
    assert list(op.get("choices", [])) == ["add", "sub", "mul", "min", "max", "mix"], \
        "Combine.op choices wrong: %r" % op.get("choices")
    basis = _prop("terrain::NoiseModuleData", "basis")
    assert list(basis.get("choices", [])) == ["perlin", "simplex", "worleyf1", "voronoi"], \
        "NoiseModule.basis choices wrong: %r" % basis.get("choices")
    print("SCHEMA enums PASS (Combine.op; NoiseModule.basis)", flush=True)

    ###########################################################################
    # s4: display-only (bake-inert) plug markers
    ###########################################################################
    for cls in ("terrain::FbmModuleData", "terrain::NoiseModuleData"):
      ovel = _in(cls, "offset_vel")
      assert ovel.get("display_only") is True, \
          "%s offset_vel not display_only: %r" % (cls, ovel)
    print("SCHEMA display-only PASS (fbm/noise offset_vel)", flush=True)

    ###########################################################################
    # s5: flow_erode.blend — MODULE-PROPERTY range via editor.range.min/max
    ###########################################################################
    blend = _prop("terrain::FlowErodeModuleData", "blend")
    banns = blend.get("annotations", {})
    assert float(banns.get("editor.range.min", -1)) == 0.0 and \
           float(banns.get("editor.range.max", -1)) == 1.0, \
        "flow_erode blend property range missing/wrong: %r" % banns
    print("SCHEMA flow_erode.blend PASS (property editor.range 0..1)", flush=True)

    ###########################################################################
    # s6: the reflection-carried palette == the legacy curated menu
    ###########################################################################
    pal = {}
    for m in mods:
      anns = m["annotations"]
      if m["family"] == "terrain" and anns.get("editor.palette"):
        verb = anns.get("dsl.verb")
        assert verb, "palette class %r missing dsl.verb" % m["name"]
        assert verb not in pal, "palette verb %r curated twice" % verb
        pal[verb] = (m["name"], anns)
    assert sorted(pal) == sorted(LEGACY_ADD_MENU), \
        "palette verbs != legacy menu: got %r" % sorted(pal)
    ordered = sorted(pal, key=lambda v: (int(pal[v][1]["editor.palette.sort"]), v))
    assert tuple(ordered) == LEGACY_ADD_MENU, \
        "palette sort order != legacy menu order: %r" % ordered
    # curation details: sources, recipes, the multi-verb class's curated verb
    assert pal["fbm"][1].get("editor.palette.source") is True, "fbm not marked source"
    assert pal["voronoi"][0] == "terrain::NoiseModuleData" and \
           pal["voronoi"][1].get("editor.palette.source") is True, \
        "voronoi (NoiseModule curated verb) wrong: %r" % (pal["voronoi"],)
    assert pal["flow_erode"][1].get("editor.palette.recipe") == "flow_erode", \
        "flow_erode recipe key missing"
    assert pal["expr"][1].get("editor.palette.recipe") == "expr", "expr recipe key missing"
    assert pal["fill_closed_basins"][1].get("editor.palette.recipe") == "fill_closed_basins", \
        "fill_closed_basins recipe key missing"
    print("SCHEMA palette PASS (13 verbs == legacy menu, order + curation intact)", flush=True)

    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== dflow schema gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
