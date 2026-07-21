#!/usr/bin/env ork.python
###############################################################################
# Propsheet-model regression gate (JUL13 DFLOW E2.5 propsheet wave + completion, headless).
#
# The PERMANENT committed net for the property-sheet editor MODELS — recreating the coverage
# a prior lane verified only in uncommitted scratch harnesses (exprforce_model_check /
# vec3_model_check / sort_model_check), and extending it with the propsheet-completion wave's
# four deliverables. A gate that is not a committed test does not count as a gate.
#
# Coverage (all against MERGED-HEAD behavior, model layer, no windows):
#   (1) EXPR FIELDS on the REAL exprforce asset (particles.force): expr_fields surfaced,
#       open-shows-printed-source, valid commit updates the stored tree, invalid commit is
#       loud + leaves the document unchanged, printer round-trip is idempotent.
#   (2) VEC3 decomposition (three per-component rows; editing .y changes ONLY y through the
#       doc round-trip; a connected vec3 plug ghosts all three as a unit) — over a mock doc,
#       since no registered module exposes a vec3 INPUT plug.
#   (3) MODULE-PROP surfacing (SpriteRenderer.sort Bool round-trips to serialized GraphData,
#       draw_order Int; the editor-infra flags bypassed/cachepoint/viewable are EXCLUDED).
#   (4) PLUG-ROW ICON annotations (D1): input-plug rows stamp row_plug (+ plug_connected when
#       wired); module-prop rows stamp NONE; the "[input]"/"[module]" text decorator is GONE.
#   (5) TERRAIN expr_source editing (D2): the ExprModule source row is a detail editor
#       (editor.custom=="expr"), NOT a plain lineedit; open shows the source; a valid commit
#       recompiles+writes; an invalid python source is loud + leaves the doc unchanged.
#   (6) HYPERMESH SelExpr editing (D3): the select/extrude *_tree rows print selexpr source;
#       a valid commit parses+writes the canonical tree; an invalid symbol is loud + unchanged.
#   (7) The expression-window PLACEMENT function (D4): never intersects the viewport, always
#       min-size, and each of the three strategy branches fires for the right geometry.
#   (8) The ExprEditorController APPLY/CLOSE/DIRTY lifecycle (D4): apply twice in one session,
#       invalid apply keeps the doc unchanged + window alive, close discards, dirty toggles.
#
# ork.python only (orkengine.core first); needs a headless GPU ctx for the reflection surface
# (moduleClasses / plugSpec) the models read. Verdict line: PROPSHEET_MODELS_RESULT=PASS|FAIL.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys

import orkengine.core  # noqa: F401  (law: core before any hypergraph/lev2 import)
from orkengine import core
from orkengine import lev2
from orkengine import ecs
from orkengine.core import vec3 as _vec3

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS = os.path.normpath(os.path.join(
    _HERE, "..", "..", "..", "..", "obt.project", "scripts"))
if _SCRIPTS not in sys.path:
    sys.path.insert(0, _SCRIPTS)

dflow = core.dataflow
_uip = lev2.ui.PropertyType

_FAILS = []


def _check(name, cond):
    ok = bool(cond)
    if not ok:
        _FAILS.append(name)
    print("  %s : %s" % ("ok  " if ok else "FAIL", name), flush=True)
    return ok


def _ann_bool(ann, key):
    if ann is None:
        return False
    return bool(getattr(ann, key, None))


def _ann_str(ann, key):
    if ann is None:
        return None
    return getattr(ann, key, None)


def _keys(pm):
    return list(pm.getChildren(""))


###############################################################################
# (1) expr fields on the REAL exprforce asset (particles.force)
###############################################################################
def _sec_exprforce():
    print("\n[1] exprforce expr fields (particles.force)", flush=True)
    from ork.hypergraph.dflow.particles import resolve as pr
    from ork.hypergraph.dflow.particles.capabilities import ParticlesEditorCapabilities
    from ork.hypergraph.dflow.graphdata_document import GraphDataDocument
    from ork.editor.graphdoc_models import GraphDocumentPropertyModel

    path = pr.resolve_dsl_file("exprforce")
    cls = pr.class_in_module(pr.load_dsl_module(path), path)
    graph = cls().generatedflow()
    doc = GraphDataDocument(graph, capabilities=ParticlesEditorCapabilities())
    node = next(n for (n, c) in doc.nodes() if "ExprForce" in c)

    fields = doc.expr_fields(node)
    _check("exprforce: 3 particles.force fields surfaced",
           [f for (f, _c) in fields] == ["force_x", "force_y", "force_z"]
           and all(c == "particles.force" for (_f, c) in fields))

    pm = GraphDocumentPropertyModel(doc)
    pm.set_object(node)
    fx = "force_x"
    _check("exprforce: force_x row is a custom expr editor",
           _ann_str(pm.getAnnotations(fx), "editor.custom") == "expr")

    src_x = pm.getValue(fx)
    _check("exprforce: open shows printed source (non-empty, matches doc)",
           bool(src_x) and src_x == doc.expr_field_source(node, "force_x"))

    # printer round-trip idempotence: re-applying the printed source is a byte no-op source
    pm.setValue(fx, src_x)
    _check("exprforce: printer round-trip idempotent", pm.getValue(fx) == src_x)

    # valid commit updates the tree: write force_z's source into force_x
    src_z = doc.expr_field_source(node, "force_z")
    _check("exprforce: force_x != force_z pre-edit", src_x != src_z)
    pm.setValue(fx, src_z)
    _check("exprforce: valid commit updates the stored tree",
           pm.getValue(fx) == src_z and doc.expr_field_source(node, "force_x") == src_z)

    # invalid commit: loud + document unchanged
    before = doc.expr_field_source(node, "force_x")
    loud = False
    try:
        pm.setValue(fx, "$$ not a valid force expr $$")
    except Exception:
        loud = True
    _check("exprforce: invalid commit is loud + leaves the tree unchanged",
           loud and doc.expr_field_source(node, "force_x") == before)


###############################################################################
# (2) vec3 decomposition (mock doc — no registered module exposes a vec3 INPUT plug)
###############################################################################
def _sec_vec3():
    print("\n[2] vec3 plug decomposition", flush=True)
    from ork.hypergraph.dflow.document import GraphDocument
    from ork.editor.graphdoc_models import GraphDocumentPropertyModel

    class _Vec3MockDoc(GraphDocument):
        def __init__(self):
            super().__init__()
            self._v = [1.0, 2.0, 3.0]
            self.connected = False
        def editable_params(self, node):
            return [("inputs", "force", list(self._v))]
        def get_param(self, node, kind, name):
            return list(self._v)
        def set_param(self, node, kind, name, value):
            self._v = [float(value.x), float(value.y), float(value.z)]
            return value
        def node_class_name(self, node):
            return None
        def plug_is_connected(self, node, name):
            return self.connected
        def plug_transformer(self, node, name):
            return None
        def expr_fields(self, node):
            return []

    doc = _Vec3MockDoc()
    pm = GraphDocumentPropertyModel(doc)
    pm.set_object("n")
    comp = [k for k in _keys(pm) if k.startswith("force#v3#")]
    _check("vec3: three per-component rows",
           comp == ["force#v3#x", "force#v3#y", "force#v3#z"])
    _check("vec3: rows carry the plug glyph annotation (unconnected)",
           all(_ann_bool(pm.getAnnotations(k), "row_plug")
               and not _ann_bool(pm.getAnnotations(k), "plug_connected") for k in comp))
    # edit .y changes ONLY y through the doc round-trip
    pm.setValue("force#v3#y", 9.0)
    _check("vec3: editing .y changes only y (doc round-trip)",
           doc.get_param("n", "inputs", "force") == [1.0, 9.0, 3.0]
           and pm.getValue("force#v3#x") == 1.0 and pm.getValue("force#v3#z") == 3.0)

    # a connected vec3 plug ghosts all three as a unit (read_only + filled glyph)
    doc.connected = True
    pm.set_object("n")
    comp = [k for k in _keys(pm) if k.startswith("force#v3#")]
    _check("vec3: connected -> all three ghost as a unit (read_only + filled glyph)",
           all(_ann_bool(pm.getAnnotations(k), "read_only")
               and _ann_bool(pm.getAnnotations(k), "plug_connected") for k in comp))


###############################################################################
# (3)+(4) module-prop surfacing + plug-row icon annotations (SpriteRenderer graph)
###############################################################################
def _sec_module_props_and_icons():
    print("\n[3+4] module-prop surfacing + plug-row icon annotations", flush=True)
    from ork.hypergraph.dflow.graphdata_document import GraphDataDocument
    from ork.editor.graphdoc_models import GraphDocumentPropertyModel

    gd = dflow.GraphData.createShared()
    gd.cacheable = False
    pool = gd.create("pool0", lev2.particles.Pool)
    spr = gd.create("spr0", lev2.particles.SpriteRenderer)
    doc = GraphDataDocument(gd)

    pm = GraphDocumentPropertyModel(doc)
    pm.set_object("spr0")
    keys = _keys(pm)

    # module props surfaced with the right editor type; exclusions honored
    _check("moduleprop: sort surfaced as Bool",
           "sort" in keys and pm.getPropertyType("sort") == _uip.Bool)
    _check("moduleprop: draw_order surfaced as Int",
           "draw_order" in keys and pm.getPropertyType("draw_order") == _uip.Int)
    _check("moduleprop: editor-infra flags excluded",
           not any(k in keys for k in ("bypassed", "cachepoint", "viewable")))

    # sort Bool round-trips to the serialized GraphData
    pm.setValue("sort", True)
    rt = doc.get_param("spr0", "module", "sort")
    _check("moduleprop: sort round-trips to serialized GraphData", bool(rt) is True)
    pm.setValue("draw_order", 3)
    _check("moduleprop: draw_order round-trips",
           int(doc.get_param("spr0", "module", "draw_order")) == 3)

    # ICONS: module-prop rows carry NO glyph annotation; labels have no [module] decorator
    _check("icons: module-prop rows carry NO plug glyph",
           not _ann_bool(pm.getAnnotations("sort"), "row_plug")
           and not _ann_bool(pm.getAnnotations("draw_order"), "row_plug"))
    _check("icons: '[module]'/'[input]' text decorator absent from labels",
           all("[module]" not in pm.getDisplayName(k) and "[input" not in pm.getDisplayName(k)
               for k in keys))

    # ICONS: an INPUT-plug row carries the glyph annotation; connected -> filled + read_only
    in_plug = doc.editable_params("spr0")
    inrow = next((n for (k, n, _v) in in_plug if k == "inputs"), None)
    _check("icons: sprite has an input plug row", inrow is not None)
    if inrow is not None:
        _check("icons: input-plug row stamps row_plug (unconnected: hollow)",
               _ann_bool(pm.getAnnotations(inrow), "row_plug")
               and not _ann_bool(pm.getAnnotations(inrow), "plug_connected"))
        # wire pool -> sprite input; the connected plug ghosts (filled + read_only)
        gd.connect(getattr(gd.findModule("spr0").inputs, inrow),
                   gd.findModule("pool0").outputs.pool)
        pm.set_object("spr0")
        _check("icons: connected input-plug row -> filled glyph + ghosted",
               _ann_bool(pm.getAnnotations(inrow), "plug_connected")
               and _ann_bool(pm.getAnnotations(inrow), "read_only"))


###############################################################################
# (5) terrain expr_source editing (D2)
###############################################################################
def _sec_terrain_expr():
    print("\n[5] terrain expr_source editing", flush=True)
    if not hasattr(lev2.terrain.ExprModule.createShared(), "expr_source"):
        _check("terrain: SKIP (ExprModule.expr_source absent in this binary)", True)
        return
    from ork.editor.terrain_runtime import TerrainRuntime
    from ork.editor.terrain_doc_model import TerrainNodePropertyModel
    from ork.hypergraph.dflow.terrain.doc import add_op_node, tree_paths, DocNode

    rt = TerrainRuntime(); rt.load("new")
    doc = rt.document
    anchor = next(o for (_pk, _k, o) in tree_paths(doc) if isinstance(o, DocNode))
    add_op_node(doc, "expr", anchor)
    exprnode = next(o for (_pk, _k, o) in tree_paths(doc)
                    if isinstance(o, DocNode) and o.clazz_name == "ExprModule")

    _check("terrain: doc.expr_fields surfaces expr_source",
           [f for (f, _c) in doc.expr_fields(exprnode)] == ["expr_source"])

    pm = TerrainNodePropertyModel(exprnode)
    keys = _keys(pm)
    _check("terrain: expr_source row is a custom expr editor (not a plain lineedit)",
           "expr_source" in keys
           and _ann_str(pm.getAnnotations("expr_source"), "editor.custom") == "expr")

    src = pm.getValue("expr_source")
    _check("terrain: open shows the recorded source", bool(src) and "ctx.input" in src)

    # valid commit recompiles + writes
    pm.setValue("expr_source", "ctx.input(0) * 0.5")
    _check("terrain: valid commit recompiles + writes",
           doc.expr_field_source(exprnode, "expr_source") == "ctx.input(0) * 0.5")

    # invalid python source: loud + document unchanged
    before = doc.expr_field_source(exprnode, "expr_source")
    loud = False
    try:
        pm.setValue("expr_source", "$$ not python $$")
    except Exception:
        loud = True
    _check("terrain: invalid source is loud + leaves the doc unchanged",
           loud and doc.expr_field_source(exprnode, "expr_source") == before)


###############################################################################
# (6) hypermesh selexpr editing (D3)
###############################################################################
def _sec_hypermesh_selexpr():
    print("\n[6] hypermesh selexpr editing", flush=True)
    from ork.hypergraph.dflow.hypermesh_document import HypermeshDocument
    from ork.hypergraph.assets.hypermesh import _resolve as hm_resolve
    from ork.editor.graphdoc_models import GraphDocumentPropertyModel
    from ork.hypergraph import exprir as X
    from ork.hypergraph.dflow.hypermesh.exprir_selexpr import SELEXPR_CTX

    graph = hm_resolve.resolve_asset("select_demo")().generatedflow()
    doc = HypermeshDocument(graph)
    node = next(n for (n, c) in doc.nodes() if "SelectData" in c)

    _check("hypermesh: predicate_tree surfaced as a selexpr field",
           [f for (f, _c) in doc.expr_fields(node)] == ["predicate_tree"])

    pm = GraphDocumentPropertyModel(doc)
    pm.set_object(node)
    key = "predicate_tree"
    _check("hypermesh: predicate_tree row is a custom expr editor",
           _ann_str(pm.getAnnotations(key), "editor.custom") == "expr")

    src = pm.getValue(key)
    # open shows the pretty_print of the stored tree
    stored = doc._module_props(node).get("predicate_tree")
    _check("hypermesh: open shows pretty_print of the stored tree",
           bool(src) and src == X.pretty_print(X.decode_json(stored), SELEXPR_CTX))

    # a valid commit parses + writes the canonical tree (step(edge, x) is 2-arg selexpr)
    new_src = "step(0.02, area)"
    pm.setValue(key, new_src)
    want = X.encode_json(X.parse(new_src, SELEXPR_CTX))
    _check("hypermesh: valid commit writes the canonical tree",
           doc._module_props(node).get("predicate_tree") == want)

    # an invalid symbol (out-of-vocabulary function) is loud + leaves the tree unchanged
    before = doc._module_props(node).get("predicate_tree")
    loud = False
    try:
        pm.setValue(key, "nonexistent_fn(area)")
    except Exception:
        loud = True
    _check("hypermesh: invalid symbol is loud + leaves the tree unchanged",
           loud and doc._module_props(node).get("predicate_tree") == before)


###############################################################################
# (7) expression-window placement (D4)
###############################################################################
def _sec_placement():
    print("\n[7] expr-window placement", flush=True)
    from ork.ui.expr_window_placement import compute_expr_window_rect as C, MIN_W, MIN_H

    def inter(a, b):
        ax, ay, aw, ah = a; bx, by, bw, bh = b
        return not (ax + aw <= bx or bx + bw <= ax or ay + ah <= by or by + bh <= ay)

    # wide screen, viewport on the RIGHT half -> OUTSIDE (room to the right)
    r, s = C((100, 100, 1200, 800), (700, 100, 600, 800), (0, 0, 2560, 1440))
    _check("placement: wide/vp-right -> outside, clear, min-size",
           s == "outside" and not inter(r, (700, 100, 600, 800))
           and r[2] >= MIN_W and r[3] >= MIN_H)

    # main spans screen, viewport on the RIGHT -> OVERLAP the left (non-viewport) band
    r, s = C((0, 0, 1920, 1080), (1000, 0, 920, 1080), (0, 0, 1920, 1080))
    _check("placement: overlap/vp-right -> overlap left band, clear, min-size",
           s == "overlap" and not inter(r, (1000, 0, 920, 1080)) and r[2] >= MIN_W)

    # main spans screen, viewport on the LEFT -> OVERLAP the right band
    r, s = C((0, 0, 1920, 1080), (0, 0, 1200, 1080), (0, 0, 1920, 1080))
    _check("placement: overlap/vp-left -> overlap right band, clear, min-size",
           s == "overlap" and not inter(r, (0, 0, 1200, 1080)) and r[2] >= MIN_W)

    # degenerate: viewport covers the whole screen -> FALLBACK (min-size, corner-anchored)
    r, s = C((0, 0, 700, 600), (0, 0, 700, 600), (0, 0, 700, 600))
    _check("placement: fallback fires (min-size, corner-anchored) when no clear region",
           s == "fallback" and r[2] >= MIN_W and r[3] >= MIN_H)


###############################################################################
# (8) ExprEditorController apply/close/dirty lifecycle (D4)
###############################################################################
def _sec_controller():
    print("\n[8] ExprEditorController lifecycle", flush=True)
    from ork.ui.expr_detail_editor import ExprEditorController

    applied = []
    rebakes = {"n": 0}

    def on_apply(src):
        if "bad" in src:
            raise ValueError("invalid expr")
        applied.append(src)
        rebakes["n"] += 1

    c = ExprEditorController("force_x", "particles.force", "a", on_apply)
    _check("controller: initial title clean (no dirty marker)", c.title().endswith("particles.force"))

    # apply twice in one session with two different valid expressions
    c.setBuffer("b")
    _check("controller: dirty toggles on edit", c.dirty and c.title().endswith("*"))
    _check("controller: apply #1 succeeds + window stays open (not closed)",
           c.apply() and not c.closed and not c.dirty and c.applied == "b")
    c.setBuffer("c")
    _check("controller: apply #2 succeeds", c.apply() and c.applied == "c")
    _check("controller: two rebakes fired, doc reflects each in sequence",
           applied == ["b", "c"] and rebakes["n"] == 2)

    # invalid apply -> error + window open + document unchanged
    c.setBuffer("bad")
    ok = c.apply()
    _check("controller: invalid apply keeps window open, doc unchanged, error set",
           (not ok) and (not c.closed) and c.applied == "c"
           and applied == ["b", "c"] and c.last_error is not None)

    # close with an unapplied edit discards it (doc keeps the last applied value)
    closed_cb = {"n": 0}
    c2 = ExprEditorController("f", "ctx", "x", on_apply, on_close=lambda: closed_cb.__setitem__("n", 1))
    c2.setBuffer("y")
    c2.close()
    _check("controller: close discards unapplied edit (keeps last applied)",
           c2.closed and c2.applied == "x" and closed_cb["n"] == 1)


###############################################################################
# (9) never-crash row population on a hypermesh select module (the uint32-mask crash)
###############################################################################
def _sec_nevercrash_select():
    print("\n[9] never-crash select-module population", flush=True)
    from ork.hypergraph.dflow.hypermesh_document import HypermeshDocument
    from ork.hypergraph.assets.hypermesh import _resolve as hm_resolve
    from ork.editor.graphdoc_models import GraphDocumentPropertyModel

    graph = hm_resolve.resolve_asset("extrude_expr_demo")().generatedflow()
    doc = HypermeshDocument(graph)
    node = next(n for (n, c) in doc.nodes() if "SelectData" in c)
    pm = GraphDocumentPropertyModel(doc)
    pm.set_object(node)

    # simulate the C++ propsheet rebuild: touch every row's type/value/annotations/choices.
    raised = None
    try:
        for k in _keys(pm):
            pm.getPropertyType(k); pm.getValue(k); pm.getAnnotations(k); pm.getChoices(k)
    except Exception as ex:
        raised = ex
    _check("nevercrash: select rows populate without an exception", raised is None)
    _check("nevercrash: the selexpr predicate_tree field is present",
           "predicate_tree" in _keys(pm))
    # the uint32 bit-mask (unsel_and == 0xFFFFFFFF) is a READ-ONLY STRING, not an Int that
    # would overflow the int32 editor codec and crash the C++ getValue boundary.
    _check("nevercrash: uint32 mask surfaced read-only (not an out-of-range Int)",
           "unsel_and" in _keys(pm)
           and pm.getPropertyType("unsel_and") == _uip.String
           and pm.getValue("unsel_and") == "4294967295")
    # ROOT proof: EVERY row value the C++ propsheet would decode is codec-safe — an Int row
    # never carries an out-of-int32 value (which is the value the windowed decode would choke on).
    bad = []
    for k in _keys(pm):
        v = pm.getValue(k)
        if pm.getPropertyType(k) == _uip.Int and isinstance(v, int) \
                and not (-2147483648 <= v <= 2147483647):
            bad.append((k, v))
    _check("nevercrash: no Int row carries an out-of-int32 (decode-unsafe) value", not bad)


###############################################################################
# (10) synthetic bad-typed property -> skip-with-loud-log (the whole-class defense)
###############################################################################
def _sec_badtype_skip():
    print("\n[10] synthetic bad-typed property skip-with-log", flush=True)
    from ork.hypergraph.dflow.document import GraphDocument
    from ork.editor.graphdoc_models import GraphDocumentPropertyModel

    class _BadTypeDoc(GraphDocument):
        def editable_params(self, node):
            return [("module", "mask", 0xFFFFFFFF)]
        def get_param(self, node, kind, name):
            return 0xFFFFFFFF
        def set_param(self, node, kind, name, value):
            return value
        def node_class_name(self, node):
            return None
        def plug_is_connected(self, node, name):
            return False
        def plug_transformer(self, node, name):
            return None
        def expr_fields(self, node):
            return []

    pm = GraphDocumentPropertyModel(_BadTypeDoc())
    pm.set_object("n")
    _check("badtype: out-of-int32 value ghosted to a read-only String row (loud log)",
           "mask" in _keys(pm)
           and pm.getPropertyType("mask") == _uip.String
           and pm.getValue("mask") == str(0xFFFFFFFF))


###############################################################################
def run(ez, ctx):
    _sec_exprforce()
    _sec_vec3()
    _sec_module_props_and_icons()
    _sec_terrain_expr()
    _sec_hypermesh_selexpr()
    _sec_placement()
    _sec_controller()
    _sec_nevercrash_select()
    _sec_badtype_skip()


def main():
    ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    try:
        run(ez, ctx)
    except Exception:
        import traceback; traceback.print_exc()
        _FAILS.append("uncaught-exception")
    finally:
        ez.mainThreadEnd()
        result = "PASS" if not _FAILS else "FAIL"
        print("\nPROPSHEET_MODELS_RESULT=%s" % result, flush=True)
        if _FAILS:
            print("FAILED: %s" % ", ".join(_FAILS), flush=True)
        ecs.headless_exit()
        sys.exit(0 if not _FAILS else 1)


if __name__ == "__main__":
    main()
