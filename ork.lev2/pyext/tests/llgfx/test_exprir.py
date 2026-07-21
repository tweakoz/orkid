#!/usr/bin/env python3
###############################################################################
# ExprIR gate (JUL13 DFLOW E2.5, S1+S2). PURE-PYTHON — no engine, no gfx: the shared
# expression IR (ork.hypergraph.exprir) is family-neutral and imports only the sibling
# `units` module. Proves:
#
#   S1  node identity + JSON round-trip idempotence (encode/decode/encode byte-equal),
#       canonical pretty-print, context validation (unknown Call / arity / leaf -> loud).
#   S2  whitelisted-ast parse round-trips (text->tree->text fixpoint; tree->text->tree
#       equality) over a synthetic corpus covering every node kind, and rejection of
#       out-of-vocabulary + malicious constructs (__import__, attribute, subscript,
#       comprehension, lambda, wrong context).
#
# Runs standalone (its own lifecycle-free main) AND as a terrain-battery suite (run()
# ignores the ez/ctx it is handed — it needs neither).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
# self-contained sys.path: reach the ork.hypergraph scripts tree from the test location.
_SCRIPTS = os.path.normpath(os.path.join(
    _HERE, "..", "..", "..", "..", "obt.project", "scripts"))
if _SCRIPTS not in sys.path:
    sys.path.insert(0, _SCRIPTS)

from ork.hypergraph import exprir as X
from ork.hypergraph.exprir import (
    Const, ParamRef, Call, ExprContext, LeafSpec, infix, unary, fn,
    PARAM_DOC, PARAM_PLUG, ExprSignatureError, ExprParseError, ExprIRError)


# ---- contexts under test -----------------------------------------------------
# terrain-SHAPED contexts (the S3 retrofit targets), plus a RICH context exercising
# call-syntax functions, the plug leaf kind, and E0 unit-literal Const leaves.

_ARITH = [infix("add", "+"), infix("sub", "-"), infix("mul", "*"),
          infix("div", "/"), unary("neg", "-")]

PARAMS = ExprContext("test.params", functions=_ARITH,
                     leaves=[LeafSpec(PARAM_DOC)])                 # open doc-param leaf
ITER = ExprContext("test.iter", functions=_ARITH,
                   leaves=[LeafSpec("index", spelling="i")])       # fixed loop-index leaf
RICH = ExprContext("test.rich", functions=_ARITH + [
                       fn("sin", 1), fn("clamp", 3), fn("noise", -1)],
                   leaves=[LeafSpec(PARAM_DOC), LeafSpec(PARAM_PLUG, spelling="live")])


# ---- synthetic corpus (every node kind) --------------------------------------
# canonical trees: non-negative Const leaves, negation as an explicit `neg` Call, so
# parse(pretty(t)) == t holds directly.

def corpus():
    return [
        # (context, tree, expected pretty text)
        (PARAMS, Const(2.0), "2"),
        (PARAMS, Const(2.5), "2.5"),
        (PARAMS, ParamRef(PARAM_DOC, "amplitude"), "amplitude"),
        (PARAMS, Call("neg", ParamRef(PARAM_DOC, "sim_time")), "-sim_time"),
        (PARAMS, Call("add", ParamRef(PARAM_DOC, "amp"), Const(2.0)), "(amp + 2)"),
        (PARAMS, Call("mul", ParamRef(PARAM_DOC, "sim_time"), Const(0.5)),
         "(sim_time * 0.5)"),
        (PARAMS, Call("sub", Const(0.8), Call("div", ParamRef(PARAM_DOC, "k"), Const(32.0))),
         "(0.8 - (k / 32))"),
        (ITER, ParamRef("index", None), "i"),
        (ITER, Call("add", Const(2.0), Call("mul", ParamRef("index", None), Const(0.5))),
         "(2 + (i * 0.5))"),
        (ITER, Call("neg", ParamRef("index", None)), "-i"),
        (RICH, Call("sin", Call("mul", ParamRef(PARAM_DOC, "t"), Const(4.0))),
         "sin((t * 4))"),
        (RICH, Call("clamp", ParamRef(PARAM_DOC, "x"), Const(0.0), Const(1.0)),
         "clamp(x, 0, 1)"),
        (RICH, Call("noise", ParamRef(PARAM_DOC, "u"), ParamRef(PARAM_DOC, "v")),
         "noise(u, v)"),
        (RICH, ParamRef(PARAM_PLUG, None), "live"),
        (RICH, Const(2000.0, "meters"), "meters(2000)"),
        (RICH, Call("add", Const(8.0, "cycles"), Const(0.25)), "(cycles(8) + 0.25)"),
    ]


def _raises(fn_, *excs):
    try:
        fn_()
        return False
    except excs:
        return True
    except Exception:
        return False


# ---- S3 doc-JSON byte-identity gate (engine path only) -----------------------
# GOLDEN doc-JSON SHA256 captured from the PRISTINE (pre-retrofit) terrain doc.py at
# 844281985, for a param-expr-bearing doc (ArithParam: scale=amp_m; step_m=sim_time*0.5) and
# an iter-expr-bearing doc (IterLoop: sharpness=2.0 + L.i*0.5). The retrofit routes these
# through the shared ExprIR yet must serialize BYTE-IDENTICALLY (the wire format is frozen
# until the coordinated salt bump). If a golden mismatches, the ExprIR wire encoders drifted.
_GOLDEN_PARAM_SHA = "0c68760be6572724fe8aaa934e15c9a912c2e926c175d6e418512a1a8ad8f42c"
_GOLDEN_ITER_SHA = "ace562859a13c2e044b28373983544f378327ba39f6d39d5bb013a94301a4456"


def _s3_byte_identity(results):
    import json, hashlib
    from orkengine import lev2  # noqa: F401  (engine already initialized by the battery)
    from ork.hypergraph.dflow.terrain import HeightField
    from ork.hypergraph.dflow import terrain as T
    from ork.hypergraph.dflow.terrain.doc import to_json, from_json
    from ork.editor.terrain_runtime import TerrainRuntime

    class ArithParam(HeightField):
        EXTENT_M = 4096.0

        def __init__(self, sim_time=250.0, amp_m=2000.0):
            super().__init__()
            h = (T.Fbm(frequency=8.0, octaves=3) * 0.5 + 0.5) * amp_m
            h = T.terrace(h, step_m=sim_time * 0.5)
            self.capture(h, "height", cache=True)
            self.capture(h, "normal", cache=True)

    class IterLoop(HeightField):
        EXTENT_M = 4096.0

        def __init__(self):
            super().__init__()
            h = (T.Fbm(frequency=6.0, octaves=4) * 0.5 + 0.5) * 2000.0
            with T.loop(3, h=h) as L:
                L.h = T.terrace(L.h, step_m=1.0 / 6.0, sharpness=2.0 + L.i * 0.5)
            self.capture(L.h, "height", cache=True)
            self.capture(L.h, "normal", cache=True)

    def _sha(doc):
        return hashlib.sha256(json.dumps(to_json(doc), sort_keys=True).encode()).hexdigest()

    def _stable(doc):
        j = json.dumps(to_json(doc), sort_keys=True)
        return json.dumps(to_json(from_json(json.loads(j))), sort_keys=True) == j

    rt = TerrainRuntime()
    rt.source_label = "ArithParam"
    rt.extent_m = 4096.0
    rt._capture_dsl_kwargs(ArithParam, {})
    rt.document = rt._trace_param_document(rt._dsl_kwargs)
    param_sha = _sha(rt.document)
    param_stable = _stable(rt.document)

    inst = IterLoop()
    inst.close_trace()
    idoc = inst.document()
    iter_sha = _sha(idoc)
    iter_stable = _stable(idoc)

    param_ok = param_sha == _GOLDEN_PARAM_SHA and param_stable
    iter_ok = iter_sha == _GOLDEN_ITER_SHA and iter_stable
    print(f"[S3.param] sha={param_sha[:12]} golden={_GOLDEN_PARAM_SHA[:12]} "
          f"stable={param_stable} -> {param_ok}", flush=True)
    print(f"[S3.iter]  sha={iter_sha[:12]} golden={_GOLDEN_ITER_SHA[:12]} "
          f"stable={iter_stable} -> {iter_ok}", flush=True)
    results["s3_param_byte_identity"] = param_ok
    results["s3_iter_byte_identity"] = iter_ok


# ---- checks ------------------------------------------------------------------

def run(ez=None, ctx=None):
    results = {}

    # S1.a — JSON round-trip idempotence: encode/decode/encode is byte-equal for every node.
    ok = True
    for (_c, t, _txt) in corpus():
        j1 = X.encode_json(t)
        j2 = X.encode_json(X.decode_json(j1))
        if j1 != j2:
            ok = False
            print(f"[S1.a] NON-IDEMPOTENT: {j1!r} != {j2!r}", flush=True)
        if X.decode_json(j1) != t:
            ok = False
            print(f"[S1.a] decode != original for {t!r}", flush=True)
    print(f"[S1.a] json idempotence + decode-eq -> {ok}", flush=True)
    results["s1_json_idempotent"] = ok

    # S1.b — sorted/stable keys (byte-identical across dict-iteration order).
    sorted_ok = X.encode_json(Call("add", ParamRef(PARAM_DOC, "z"), Const(1.0))) \
        == '{"args":[{"k":"ref","kind":"doc","name":"z"},{"k":"const","v":1.0}],' \
           '"k":"call","name":"add"}'
    print(f"[S1.b] canonical sorted json -> {sorted_ok}", flush=True)
    results["s1_json_sorted"] = sorted_ok

    # S1.c — canonical pretty-print matches expectation for every node kind.
    pp_ok = True
    for (c, t, txt) in corpus():
        got = X.pretty_print(t, c)
        if got != txt:
            pp_ok = False
            print(f"[S1.c] pretty {t!r} -> {got!r} expected {txt!r}", flush=True)
    print(f"[S1.c] pretty-print -> {pp_ok}", flush=True)
    results["s1_pretty"] = pp_ok

    # S1.d — validation is loud: unknown Call, wrong arity, undeclared leaf, wrong context.
    val_ok = (
        _raises(lambda: PARAMS.validate(Call("sin", Const(1.0))), ExprSignatureError)
        and _raises(lambda: PARAMS.validate(Call("add", Const(1.0))), ExprSignatureError)
        and _raises(lambda: PARAMS.validate(ParamRef("index", None)), ExprSignatureError)
        and _raises(lambda: ITER.validate(ParamRef(PARAM_DOC, "x")), ExprSignatureError)
        and _raises(lambda: PARAMS.validate(Const(1.0, "furlongs")), ExprIRError))
    print(f"[S1.d] validation loud on bad trees -> {val_ok}", flush=True)
    results["s1_validation"] = val_ok

    # S1.e — context registry: register + lookup + loud on dup/unknown.
    reg = ExprContext("test.reg.unique", functions=_ARITH, leaves=[LeafSpec(PARAM_DOC)])
    X.register_context(reg)
    reg_ok = (X.get_context("test.reg.unique") is reg
              and _raises(lambda: X.register_context(reg), ExprIRError)
              and _raises(lambda: X.get_context("test.reg.absent"), ExprIRError))
    print(f"[S1.e] context registry -> {reg_ok}", flush=True)
    results["s1_registry"] = reg_ok

    # S2.a — parse fixpoint: text->tree->text == text (over the pretty text of the corpus).
    txt_fix = True
    for (c, t, txt) in corpus():
        tree = X.parse(txt, c)
        back = X.pretty_print(tree, c)
        if back != txt:
            txt_fix = False
            print(f"[S2.a] text fixpoint {txt!r} -> {back!r}", flush=True)
    print(f"[S2.a] text->tree->text fixpoint -> {txt_fix}", flush=True)
    results["s2_text_fixpoint"] = txt_fix

    # S2.b — tree round-trip: parse(pretty(tree)) == tree, for every node kind.
    tree_eq = True
    for (c, t, txt) in corpus():
        back = X.parse(X.pretty_print(t, c), c)
        if back != t:
            tree_eq = False
            print(f"[S2.b] tree round-trip {t!r} -> {back!r}", flush=True)
    print(f"[S2.b] tree->text->tree equality -> {tree_eq}", flush=True)
    results["s2_tree_roundtrip"] = tree_eq

    # S2.c — rejection: out-of-vocabulary + malicious constructs, all loud + positioned.
    rej_ok = (
        _raises(lambda: X.parse("sin(x)", PARAMS), ExprParseError)          # fn not in ctx
        and _raises(lambda: X.parse("amp", ITER), ExprParseError)          # leaf wrong ctx
        and _raises(lambda: X.parse("__import__('os')", RICH), ExprParseError)
        and _raises(lambda: X.parse("os.system('x')", RICH), ExprParseError)  # attribute
        and _raises(lambda: X.parse("data[0]", RICH), ExprParseError)         # subscript
        and _raises(lambda: X.parse("[x for x in y]", RICH), ExprParseError)  # comprehension
        and _raises(lambda: X.parse("lambda: 0", RICH), ExprParseError)       # lambda
        and _raises(lambda: X.parse("sin(a, b)", RICH), ExprParseError)       # arity
        and _raises(lambda: X.parse("clamp(x)", RICH), ExprParseError)        # arity
        and _raises(lambda: X.parse("f(x=1)", RICH), ExprParseError)          # kwargs
        and _raises(lambda: X.parse("a and b", RICH), ExprParseError)         # bool-op
        and _raises(lambda: X.parse("a )( b", RICH), ExprParseError))         # syntax error
    print(f"[S2.c] malicious/out-of-vocab rejection -> {rej_ok}", flush=True)
    results["s2_rejection"] = rej_ok

    # S2.d — E0 unit-literal Const parses in any context; negative-const normalization.
    unit_ok = (X.parse("meters(2000)", RICH) == Const(2000.0, "meters")
               and X.parse("cycles(-4)", RICH) == Const(-4.0, "cycles")
               and _raises(lambda: X.parse("furlongs(3)", RICH), ExprParseError)
               # a negative numeric literal normalizes to neg(Const) (documented behavior)
               and X.parse("-2", PARAMS) == Call("neg", Const(2.0)))
    print(f"[S2.d] unit-literal + neg normalization -> {unit_ok}", flush=True)
    results["s2_unit_literals"] = unit_ok

    # S3 — terrain doc-JSON byte-identity (engine path only; skipped standalone/pure-python).
    if ez is not None and ctx is not None:
        _s3_byte_identity(results)

    return results


def main():
    results = run()
    ok = all(results.values())
    print("\n" + "=" * 60, flush=True)
    for k, v in results.items():
        print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    print("=" * 60, flush=True)
    print(f"=== exprir S1+S2 {'PASSED' if ok else 'FAILED'} ===", flush=True)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
