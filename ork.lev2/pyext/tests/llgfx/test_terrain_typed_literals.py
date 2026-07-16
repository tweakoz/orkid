#!/usr/bin/env python3
###############################################################################
# Terrain E0 part-2 TYPED-LITERALS gate (jul13). Proves the unit-constructor
# vocabulary (meters/cycles/…) rides ctor-kwarg DEFAULTS as typed literals: the
# tag promotes into the document params table, serializes as the ratified
# {"<unit>": v} node, displays/round-trips through the pywriter as meters(2000),
# and — because the tag is DOC-LAYER metadata (values fold to plain floats) — the
# elaborated graph is unchanged, so unchanged values bake BYTE-IDENTICALLY. Plain
# literals stay legal everywhere (no flag-day).
#
#   u  the units module folds numerically like a float + carries its tag.
#   a  tagged ctor default -> params-table tag -> doc-JSON tagged-node round-trip.
#   b  edit COERCES to the tag (string->float) and REJECTS uncoercible loudly.
#   c  pywriter emits `meters(2000)` + the units import, and re-imports (editor
#      path) rebaking BYTE-IDENTICAL.
#   d  plain literals unchanged: the `new` asset stays plain (no tags, plain
#      doc-JSON, editable) — additive, no flag-day.
#   e  redundant unit-suffix -> advisory comment, NEVER an automatic rename.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, hashlib
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph import units as U
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow.terrain.doc import (
    DocNode, TerrainDocParamError, tree_paths, to_json, from_json)
from ork.hypergraph.dflow.terrain.pywriter import to_python, class_name_from_stem
from ork.hypergraph.dflow.terrain.resolve import load_dsl_class
from ork.editor.terrain_runtime import TerrainRuntime


# ---- fixtures ---------------------------------------------------------------

class SuffixExemplar(HeightField):
    """A tagged kwarg whose NAME still carries the unit suffix (amplitude_m tagged meters)
    — the writer notes the suffix is redundant but NEVER renames it (author's choice)."""
    EXTENT_M = 4096.0

    def __init__(self, amplitude_m=U.meters(1500)):
        super().__init__()
        h = T.voronoi(frequency=8.0, octaves=1, amplitude=amplitude_m)
        self.capture(h, "height", cache=True)


# ---- helpers (mirror test_terrain_doc_params) -------------------------------

OUT = "/tmp/tered_typed_literals"
_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"


def masked_sha(path):
    with open(path, "rb") as f:
        b = bytearray(f.read())
    i = b.find(_CAPDATE)
    if i >= 0:
        b[i + len(_CAPDATE):i + len(_CAPDATE) + 19] = b"\x00" * 19
    return hashlib.sha256(bytes(b)).hexdigest()


def bake_height_sha(doc, extent, dim, tag, ctx):
    g, _cap = doc.elaborate()
    for cap in lev2.terrain.capture_modules(g):
        chans = [c.strip() for c in (cap.channel or "height").split(",") if c.strip()]
        # path EVERY capture (not just height): an unpathed capture flushes its
        # ".cookhash" currency sidecar into the CWD (repo-root dropping, JUL13 filed).
        cap.path = f"{OUT}/{tag}.{chans[0]}.exr"
    lev2.terrain.bake_heightfield(g, ctx, dim, extent_m=extent)
    return masked_sha(f"{OUT}/{tag}.height.exr")


def first_op(doc):
    for (_pk, _k, obj) in tree_paths(doc):
        if isinstance(obj, DocNode) and obj.clazz_name != "CaptureModule":
            return obj
    return None


def effective(node, kind, name):
    v = None
    for (k, n, val) in node.param_actions:
        if k == kind and n == name:
            v = val
    return v


def load_fixture(cls, kwargs=None):
    rt = TerrainRuntime()
    rt.source_label = cls.__name__
    rt.extent_m = float(cls.EXTENT_M)
    rt._capture_dsl_kwargs(cls, kwargs or {})
    rt.document = rt._trace_param_document(rt._dsl_kwargs)
    return rt


def raises(fn, exc):
    try:
        fn()
        return False
    except exc:
        return True


# ---- main -------------------------------------------------------------------

def run(ez, ctx):
    os.makedirs(OUT, exist_ok=True)
    results = {}

    # ---- u: the units module folds like a float + carries its tag --------------
    m = U.meters(2000)
    u = (float(m) == 2000.0 and type(float(m)) is float          # folds to PLAIN float
         and U.unit_of(m) == "meters" and U.unit_of(2000.0) is None
         and (U.cycles(8) * 0.5) == 4.0                          # arithmetic folds numerically
         and int(U.meters(3)) == 3
         and U.UNIT_TAGS >= {"meters", "texels", "uv", "cycles", "seconds", "hz",
                             "db", "degrees", "mps", "m2ps", "per_s"}
         and raises(lambda: U.tagged("furlongs", 1), ValueError))
    print(f"[u] fold-plain={type(float(m)) is float} unit={U.unit_of(m)} "
          f"vocab-ok={U.UNIT_TAGS >= {'meters','per_s'}} -> {u}", flush=True)
    results["u_units_fold"] = u

    # ---- a: tagged default -> params tag -> doc-JSON tagged-node round-trip -----
    rt = TerrainRuntime()
    rt.load("voronoi")
    params = rt.document.params
    tags_ok = (params.tag_of("frequency") == "cycles"
               and params.tag_of("amplitude") == "meters"
               and params.tag_of("octaves") is None)          # octaves=1 plain (structural)
    j = to_json(rt.document)
    pj = {name: (enc, struct) for (name, enc, struct) in j["params"]}
    node_form = (pj["frequency"][0] == {"cycles": 8.0}
                 and pj["amplitude"][0] == {"meters": 2000.0}
                 and pj["octaves"][0] == ["i", 1])             # plain stays plain
    doc_r = from_json(j)
    rt_ok = (to_json(doc_r) == j
             and doc_r.params.tag_of("amplitude") == "meters"
             and doc_r.params.get("amplitude") == 2000.0)
    a = tags_ok and node_form and rt_ok
    print(f"[a] tags={tags_ok} node-form={node_form} roundtrip={rt_ok} -> {a}", flush=True)
    results["a_tag_docjson_roundtrip"] = a

    # ---- b: edit COERCES to the tag + REJECTS uncoercible loudly ---------------
    rtb = TerrainRuntime()
    rtb.load("voronoi")
    coerced = rtb.set_dsl_kwarg("amplitude", "2600")           # string -> meters coerces to float
    plug = effective(first_op(rtb.document), "inputs", "amplitude")
    coerce_ok = (coerced == 2600.0 and type(coerced) is float and plug == 2600.0)
    rejected = raises(lambda: rtb.set_dsl_kwarg("amplitude", "not-a-number"),
                      TerrainDocParamError)
    tag_kept = rtb.document.params.tag_of("amplitude") == "meters"
    b = coerce_ok and rejected and tag_kept
    print(f"[b] coerced={coerce_ok} rejected-loud={rejected} tag-kept={tag_kept} -> {b}",
          flush=True)
    results["b_coerce_to_tag"] = b

    # ---- c: pywriter emits meters(2000)/cycles(8) + import, re-imports identical -
    rtc = TerrainRuntime()
    rtc.load("voronoi")
    sha_src = bake_height_sha(rtc.document, rtc.extent_m, 256, "c_src", ctx)
    src = to_python(rtc.document, class_name=class_name_from_stem("c_typed"),
                    extent_m=rtc.extent_m)
    emit_ok = ("amplitude=meters(2000)" in src and "frequency=cycles(8)" in src
               and "from ork.hypergraph.units import" in src
               and "cycles" in src and "meters" in src)
    srcdir = f"{OUT}/src"
    os.makedirs(srcdir, exist_ok=True)
    p = f"{srcdir}/c_typed.py"
    with open(p, "w") as fh:
        fh.write(src)
    cls_r = load_dsl_class(p)
    rtr = load_fixture(cls_r)                                  # EDITOR-path re-import
    reimport_tags = (rtr.document.params.tag_of("amplitude") == "meters"
                     and rtr.document.params.tag_of("frequency") == "cycles")
    sha_reload = bake_height_sha(rtr.document, float(cls_r.EXTENT_M), 256, "c_reload", ctx)
    byte_identical = sha_src == sha_reload
    c = emit_ok and reimport_tags and byte_identical
    print(f"[c] emit={emit_ok} reimport-tags={reimport_tags} byte-identical={byte_identical} "
          f"-> {c}", flush=True)
    results["c_pywriter_roundtrip"] = c

    # ---- d: plain literals unchanged (the `new` asset) — no flag-day ------------
    rtd = TerrainRuntime()
    rtd.load("new")
    plain_no_tags = all(rtd.document.params.tag_of(n) is None
                        for n in rtd.document.params.names())
    jd = to_json(rtd.document)
    plain_form = all(isinstance(enc, list) for (_n, enc, _s) in jd["params"])   # no {unit:} nodes
    editable = rtd.set_dsl_kwarg("frequency", 7.0) == 7.0
    d = plain_no_tags and plain_form and editable
    print(f"[d] no-tags={plain_no_tags} plain-json={plain_form} editable={editable} -> {d}",
          flush=True)
    results["d_plain_unchanged"] = d

    # ---- e: redundant suffix -> advisory comment, NEVER an automatic rename -----
    rte = load_fixture(SuffixExemplar)
    src_e = to_python(rte.document, class_name=class_name_from_stem("e_suffix"),
                      extent_m=rte.extent_m)
    no_rename = "amplitude_m=meters(1500)" in src_e            # name KEPT, tag added
    has_note = ("redundant" in src_e and "amplitude_m" in src_e
                and src_e.count("def __init__") == 1)
    e = no_rename and has_note
    print(f"[e] no-rename={no_rename} advisory-note={has_note} -> {e}", flush=True)
    results["e_redundant_suffix_note"] = e

    return results


def main():
    ez = ecs.headless_appinit(use_subsystems=["opq", "core", "gpu", "lev2"])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    results = run(ez, ctx)
    ez.mainThreadEnd()
    ok = all(results.values())
    print(f"\n=== terrain typed-literals {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
        print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
