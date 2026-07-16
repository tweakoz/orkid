#!/usr/bin/env python3
###############################################################################
# Terrain E0 PART 3 — pywriter OPERATOR RE-SUGARING gate (JUL13 Ex.6). The Save
# writer RAISES operator-lowered Remap/Combine chains back to the arithmetic that
# authored them — the verified NOTATION-ONLY inverse of the _node.py lowering: the
# emitted source re-lowers to the identical chain (same tree_paths / classes /
# params, byte-identical bake). Nothing about topology or cook hashes moves.
#
#   a  the canonical chain (fbm*0.5+0.5)*4000*0.03 emits INLINE arithmetic (no
#      `remap_N = T.remap` lines) -> re-import: structural equality + byte-identical
#      bake at dim 256 AND 512.
#   b  GUARDED cases stay explicit / named: a real lo/hi clamp, a user-named node, a
#      bypassed remap, and a multi-consumer intermediate (named variable + reuse).
#   c  PROPERTY test: 25 seeded random operator chains (2-6 ops deep, *k/+k/-k/node*
#      node/T.Min/T.Max) -> lower(raise(doc)) == doc (order-insensitive structural eq).
#   d  a chain referencing a captured document parameter emits the parameter name
#      INLINE (`* amp_m`) and round-trips byte-identically (editor path).
#
# Structural equality is keyed by tree-path (local_name) — order-insensitive — since
# raising legitimately pulls an inlined chain's generator declarations earlier in the
# document without changing the graph (the bake is the load-bearing oracle).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, hashlib, random, functools
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow.terrain.doc import (
    DocNode, tree_paths, find_by_path, param_expr_string)
from ork.hypergraph.dflow.terrain.pywriter import to_python, class_name_from_stem
from ork.hypergraph.dflow.terrain.resolve import load_dsl_class
from ork.editor.terrain_runtime import TerrainRuntime


OUT = "/tmp/tered_resugar"
SRCDIR = "/tmp/tered_resugar_src"
_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"


# ---- fixtures ---------------------------------------------------------------

class Canonical(HeightField):
    """(fbm * 0.5 + 0.5) * 4000.0 * 0.03 -> a pure single-consumer affine chain (Ex.6)."""
    EXTENT_M = 4096.0
    def __init__(self):
        super().__init__()
        h = (T.fbm(frequency=4.0, octaves=1) * 0.5 + 0.5) * 4000.0 * 0.03
        self.capture(h, "height")


class MultiConsumer(HeightField):
    """A shared intermediate (fbm*0.5) fans out to two consumers -> named variable + reuse."""
    EXTENT_M = 4096.0
    def __init__(self):
        super().__init__()
        base = T.fbm(frequency=4.0, octaves=2) * 0.5
        self.capture(T.Max(base + 0.1, base + 0.2), "height")


class ArithParam(HeightField):
    """A captured document parameter drives a scale (Ex.3): (fbm*0.5+0.5) * amp_m."""
    EXTENT_M = 4096.0
    def __init__(self, amp_m=2000.0):
        super().__init__()
        h = (T.fbm(frequency=8.0, octaves=3) * 0.5 + 0.5) * amp_m
        self.capture(h, "height", cache=True)
        self.capture(h, "normal", cache=True)


# ---- helpers ----------------------------------------------------------------

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


def fingerprint(doc):
    """Order-insensitive structural fingerprint keyed by tree-path: class + effective params +
    param-expr strings + connections (producer by local_name) + bypass flag."""
    fp = {}
    for (_pk, key, obj) in tree_paths(doc):
        if isinstance(obj, DocNode):
            eff = {}
            for (k, n, v) in obj.param_actions:
                eff[(k, n)] = v
            params = tuple(sorted((f"{k}:{n}", repr(v)) for (k, n), v in eff.items()))
            pexpr = tuple(sorted((f"{k}:{n}", param_expr_string(e))
                                 for (k, n), e in obj.param_exprs.items()))
            conns = tuple(sorted(
                (ip, getattr(op.node, "local_name", None) or getattr(op.node, "name", "?"),
                 op.plug_name) for (ip, op) in obj.connections))
            fp[key] = (obj.clazz_name, params, pexpr, conns, bool(obj.bypassed))
        else:
            fp[key] = ("__" + type(obj).__name__,)
    return fp


def emit(doc, tag):
    return to_python(doc, class_name=class_name_from_stem(tag), extent_m=4096.0)


def reload_inst(src, tag):
    os.makedirs(SRCDIR, exist_ok=True)
    p = os.path.join(SRCDIR, f"{tag}.py")
    with open(p, "w") as f:
        f.write(src)
    cls = load_dsl_class(p)
    inst = cls()
    inst.generatedflow()
    return inst   # keep alive: anon-name counters are keyed by graph id


def trace(cls):
    inst = cls()
    inst.generatedflow()
    return inst


def load_param_rt(cls, kwargs=None):
    """Editor-path trace (mirrors TerrainRuntime DSL load) so ctor kwargs become document params."""
    rt = TerrainRuntime()
    rt.source_label = cls.__name__
    rt.extent_m = float(cls.EXTENT_M)
    rt._capture_dsl_kwargs(cls, kwargs or {})
    rt.document = rt._trace_param_document(rt._dsl_kwargs)
    return rt


# ---- random operator chains (property test) ---------------------------------

def _random_specs(rng):
    depth = rng.randint(2, 6)
    specs = []
    for _ in range(depth):
        kind = rng.choice(["mulk", "addk", "subk", "nodemul", "nodeadd", "min", "max"])
        if kind == "mulk":
            val = rng.choice([0.5, 2.0, 0.03, -1.0, 4000.0])
        elif kind in ("addk", "subk"):
            val = rng.choice([0.1, 0.5, -0.25, 0.25])
        else:
            val = rng.choice([4.0, 6.0, 8.0])
        specs.append((kind, val))
    return specs


def _apply_specs(h, specs):
    """Apply a pre-chosen op list (module-level, NOT in __init__, so the trace records a flat
    DAG without the raw-loop flatten notice)."""
    def _step(acc, spec):
        kind, val = spec
        if kind == "mulk":
            return acc * val
        if kind == "addk":
            return acc + val
        if kind == "subk":
            return acc - val
        if kind == "nodemul":
            return acc * (T.fbm(frequency=val, octaves=1) * 0.5 + 0.5)
        if kind == "nodeadd":
            return acc + (T.fbm(frequency=val, octaves=1) * 0.5)
        if kind == "min":
            return T.Min(acc, T.fbm(frequency=val, octaves=1) * 0.5)
        return T.Max(acc, T.fbm(frequency=val, octaves=1) * 0.5)
    return functools.reduce(_step, specs, h)


def _make_chain_class(specs):
    class _RandChain(HeightField):
        EXTENT_M = 4096.0
        def __init__(self):
            super().__init__()
            h = T.fbm(frequency=4.0, octaves=1) * 0.5 + 0.5
            self.capture(_apply_specs(h, specs), "height")
    return _RandChain


# ---- main -------------------------------------------------------------------

def run(ez, ctx):
    os.makedirs(OUT, exist_ok=True)
    results = {}

    # ---- a: the canonical chain inlines + round-trips (structeq + bake 256/512) ---
    c = trace(Canonical)
    src = emit(c.document(), "canonical")
    inline_present = "(fbm_0 * 0.5 + 0.5) * 4000.0 * 0.03" in src
    no_remap_lines = "= T.remap(" not in src
    cb = reload_inst(src, "canonical")
    structeq = fingerprint(cb.document()) == fingerprint(c.document())
    sha256_a = bake_height_sha(c.document(), 4096.0, 256, "canon256_a", ctx)
    sha256_b = bake_height_sha(cb.document(), 4096.0, 256, "canon256_b", ctx)
    sha512_a = bake_height_sha(c.document(), 4096.0, 512, "canon512_a", ctx)
    sha512_b = bake_height_sha(cb.document(), 4096.0, 512, "canon512_b", ctx)
    bake256 = sha256_a == sha256_b
    bake512 = sha512_a == sha512_b
    a = inline_present and no_remap_lines and structeq and bake256 and bake512
    print(f"[a] inline_present={inline_present} no_remap_lines={no_remap_lines} "
          f"structeq={structeq} bake256={bake256} bake512={bake512} -> {a}", flush=True)
    if not a:
        print(f"[a] emitted:\n{src}", flush=True)
    results["a_canonical_inline"] = a

    # ---- b1: a real lo/hi clamp stays explicit T.remap -------------------------
    class ClampDoc(HeightField):
        EXTENT_M = 4096.0
        def __init__(self):
            super().__init__()
            h = T.fbm(frequency=4.0, octaves=1) * 0.5 + 0.5
            h = T.remap(h, scale=2.0, lo=-1.0, hi=1.0)          # real clamp -> explicit
            self.capture(h, "height")
    cd = trace(ClampDoc)
    src_b1 = emit(cd.document(), "clamp")
    b1 = ("T.remap(" in src_b1
          and fingerprint(reload_inst(src_b1, "clamp").document()) == fingerprint(cd.document()))
    print(f"[b1] clamp stays explicit={('T.remap(' in src_b1)} structeq -> {b1}", flush=True)

    # ---- b2: a user-named affine node stays explicit (no sugar) -----------------
    class NamedDoc(HeightField):
        EXTENT_M = 4096.0
        def __init__(self):
            super().__init__()
            h = T.fbm(frequency=4.0, octaves=1) * 0.5      # sentinel scale remap...
            self.capture(h + 0.5, "height")
    nd = trace(NamedDoc)
    scale_node = next(o for (_pk, _k, o) in tree_paths(nd.document())
                      if isinstance(o, DocNode) and o.clazz_name == "RemapModule"
                      and o.local_name == "remap_0")
    scale_node.local_name = "user_scale"                    # simulate a user name=
    src_b2 = emit(nd.document(), "named")
    b2 = ("T.remap(" in src_b2) and ("user_scale * 0.5" not in src_b2) \
        and ("= T.remap(user_scale" not in src_b2)
    print(f"[b2] user-named stays explicit -> {b2}", flush=True)
    if not b2:
        print(f"[b2] emitted:\n{src_b2}", flush=True)

    # ---- b3: a bypassed affine node stays explicit T.remap + T.bypass -----------
    class BypassDoc(HeightField):
        EXTENT_M = 4096.0
        def __init__(self):
            super().__init__()
            h = T.fbm(frequency=4.0, octaves=1) * 0.5 + 0.5
            self.capture(h, "height")
    bd = trace(BypassDoc)
    byp = next(o for (_pk, _k, o) in tree_paths(bd.document())
               if isinstance(o, DocNode) and o.clazz_name == "RemapModule"
               and o.local_name == "remap_0")
    byp.set_bypassed(True)
    src_b3 = emit(bd.document(), "bypass")
    b3 = ("T.remap(" in src_b3) and ("T.bypass(" in src_b3) \
        and (fingerprint(reload_inst(src_b3, "bypass").document()) == fingerprint(bd.document()))
    print(f"[b3] bypass stays explicit + T.bypass + structeq -> {b3}", flush=True)

    # ---- b4: a multi-consumer intermediate -> named variable + reuse ------------
    m = trace(MultiConsumer)
    src_b4 = emit(m.document(), "multicons")
    named_var = "remap_0 = fbm_0 * 0.5" in src_b4
    reuse = ("remap_0 + 0.1" in src_b4) and ("remap_0 + 0.2" in src_b4)
    b4 = (named_var and reuse
          and fingerprint(reload_inst(src_b4, "multicons").document()) == fingerprint(m.document()))
    print(f"[b4] named_var={named_var} reuse={reuse} structeq -> {b4}", flush=True)
    if not b4:
        print(f"[b4] emitted:\n{src_b4}", flush=True)

    results["b_guarded_cases"] = b1 and b2 and b3 and b4

    # ---- c: property test — 25 seeded random operator chains -------------------
    # keep = liveness anchor (anon-name counters are keyed by graph id, so a source doc must
    # outlive its re-import). fix = naturally-traced fixpoint candidates (NamedDoc is excluded:
    # its local_name was hand-mutated to simulate a user name= that the writer has never emitted,
    # so it is not a round-trippable document — a guard test, not a fixpoint candidate).
    rng = random.Random(0xE06C0DE)
    keep = [c, m, cd, nd, bd]
    fix = [c, m, cd, bd]
    nprop = 25
    prop_ok = 0
    for i in range(nprop):
        inst = trace(_make_chain_class(_random_specs(rng)))
        keep.append(inst)
        fix.append(inst)
        srcp = emit(inst.document(), f"prop{i}")
        instb = reload_inst(srcp, f"prop{i}")
        keep.append(instb)
        if fingerprint(instb.document()) == fingerprint(inst.document()):
            prop_ok += 1
        else:
            print(f"[c] prop{i} MISMATCH\n{srcp}", flush=True)
    prop = prop_ok == nprop
    print(f"[c] property {prop_ok}/{nprop} chains: lower(raise(doc))==doc -> {prop}", flush=True)
    results["c_property_random_chains"] = prop

    # ---- c2: FIXPOINT — save -> re-import -> save is byte-identical text --------
    fix_ok = 0
    for inst in fix:
        s1 = emit(inst.document(), "fixA")
        s2 = emit(reload_inst(s1, "fixtmp").document(), "fixA")
        if s1 == s2:
            fix_ok += 1
        else:
            print(f"[c2] fixpoint DIFF\n--s1--\n{s1}\n--s2--\n{s2}", flush=True)
    fixpoint = fix_ok == len(fix)
    print(f"[c2] fixpoint {fix_ok}/{len(fix)} idempotent -> {fixpoint}", flush=True)
    results["c2_fixpoint_idempotent"] = fixpoint

    # ---- d: a captured document parameter raises to an inline reference ---------
    rt = load_param_rt(ArithParam)
    src_d = to_python(rt.document, class_name=class_name_from_stem("arithparam"),
                      extent_m=float(ArithParam.EXTENT_M))
    param_inline = "* amp_m" in src_d
    default_ok = "amp_m=2000.0" in src_d            # plain literal default in the ctor signature
    srcdir = f"{OUT}/src"
    os.makedirs(srcdir, exist_ok=True)
    p = f"{srcdir}/arithparam.py"
    with open(p, "w") as fh:
        fh.write(src_d)
    cls_r = load_dsl_class(p)
    rtr = load_param_rt(cls_r)
    param_reimport = rtr.document.params.get("amp_m") == 2000.0
    sha_src = bake_height_sha(rt.document, float(ArithParam.EXTENT_M), 256, "d_src", ctx)
    sha_re = bake_height_sha(rtr.document, float(cls_r.EXTENT_M), 256, "d_re", ctx)
    byte_id = sha_src == sha_re
    d = param_inline and default_ok and param_reimport and byte_id
    print(f"[d] param_inline={param_inline} default_ok={default_ok} "
          f"param_reimport={param_reimport} byte_identical={byte_id} -> {d}", flush=True)
    if not d:
        print(f"[d] emitted:\n{src_d}", flush=True)
    results["d_param_compose"] = d

    return results


def main():
    ez = ecs.headless_appinit(use_subsystems=["opq", "core", "gpu", "lev2"])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    results = run(ez, ctx)
    ez.mainThreadEnd()
    ok = all(results.values())
    print(f"\n=== terrain resugar {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
        print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
