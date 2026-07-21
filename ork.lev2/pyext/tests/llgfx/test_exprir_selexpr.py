#!/usr/bin/env python3
###############################################################################
# ExprIR SelExpr-capture gate (JUL13 DFLOW E2.5, S6). The hypermesh SelExpr adopts the
# shared ExprIR: exprir_selexpr.capture() lowers a selexpr.SelExpr into a validated
# hypermesh.selexpr ExprIR tree (the reflected SelectData/ExtrudeFacesData `*_tree` fields
# store capture_json()). Proves:
#
#   - CAPTURE + validation over a corpus covering every selexpr construct that reaches an
#     extrude field / select predicate (atoms, math helpers, operators, comparisons, boolean
#     composition, vec3, param(), tag/gid).
#   - JSON round-trip idempotence: encode/decode/encode is BYTE-equal.
#   - RESHAPE idempotence: capturing the SAME SelExpr twice yields byte-identical JSON.
#   - the GLSL EVAL path (emit_block) is UNCHANGED — a captured node still emits shader text
#     (the tag is a pure sibling; S6 must not perturb the shader the op compiles).
#   - fail-loud on an UNTAGGED node (no silent capture hole).
#
# ork.python only (orkengine.core first); no GPU context is created — imports + pure DSL.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys

import orkengine.core  # noqa: F401  (law: core before any hypergraph/lev2 import)
from orkengine.core import vec3

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS = os.path.normpath(os.path.join(
    _HERE, "..", "..", "..", "..", "obt.project", "scripts"))
if _SCRIPTS not in sys.path:
    sys.path.insert(0, _SCRIPTS)

from ork.hypergraph import exprir as X
from ork.hypergraph.dflow.hypermesh import selexpr as SE
from ork.hypergraph.dflow.hypermesh.selexpr import S, sel_normal_dir, sel_area_gt, vexpr, param
from ork.hypergraph.dflow.hypermesh import exprir_selexpr as XSE


_FAILS = []


def _check(cond, msg):
    if not cond:
        _FAILS.append(msg)
        print("  FAIL:", msg)
    else:
        print("  ok  :", msg)


def _corpus():
    p = param("phase", 0.0)
    return [
        ("predicate normal&area", SE.POLY,
         sel_normal_dir(vec3(0, 1, 0), 0.55, 0.05) & (S.area > 0.02)),
        ("extrude dist field",    SE.POLY, S.pow(S.t, 2.0) * 1.5 + 0.5),
        ("extrude inset field",   SE.POLY, S.clamp(S.seg * 0.1, 0.0, 0.5)),
        ("extrude dir vec3",      SE.POLY, vexpr(0.0, -0.3, 0.0) + S.N * 2.0),
        ("param sin field",       SE.POLY, S.sin(S.t * 6.2831 + p)),
        ("tag|gid predicate",     SE.POLY, S.tag(2) | (S.gid > 3.0)),
        ("select parity",         SE.POLY, S.select(S.seg % 2.0, 1.0, 0.25)),
        ("area_gt builder",       SE.POLY, sel_area_gt(0.05)),
    ]


def main():
    print("== S6 SelExpr -> ExprIR capture gate ==")
    for (label, domain, expr) in _corpus():
        # capture + validate (validate is inside capture)
        tree = XSE.capture(expr)
        js = XSE.capture_json(expr)

        # JSON round-trip idempotence (byte-equal)
        rt = X.encode_json(X.decode_json(js))
        _check(rt == js, "%s: JSON round-trip byte-identical" % label)

        # decode reconstructs an equal tree
        _check(X.decode_json(js) == tree, "%s: decode == captured tree" % label)

        # RESHAPE idempotence: re-capture the SAME SelExpr -> identical JSON
        js2 = XSE.capture_json(expr)
        _check(js2 == js, "%s: reshape idempotent (recapture byte-identical)" % label)

        # validate against the registered context explicitly
        XSE.SELEXPR_CTX.validate(tree)

        # GLSL EVAL path intact: a SelExpr still lowers to flat-SSA shader text
        if isinstance(expr, SE.SelExpr):
            stmts, res = expr.emit_block(domain)
            _check(bool(stmts) and bool(res), "%s: GLSL emit_block still produces text" % label)

    # context is registered + discoverable
    _check("hypermesh.selexpr" in X.registered_contexts(),
           "context 'hypermesh.selexpr' registered")

    # fail-loud on an UNTAGGED node (a raw SelExpr with no _meta)
    raw = SE.SelExpr("float", lambda d: "1.0")
    try:
        XSE.capture(raw)
        _check(False, "untagged node -> loud SelExprIRError")
    except XSE.SelExprIRError:
        _check(True, "untagged node -> loud SelExprIRError")

    # a captured tree with an out-of-vocabulary Call fails validation loudly
    try:
        XSE.SELEXPR_CTX.validate(X.Call("bogus_op", X.Const(1.0)))
        _check(False, "unknown Call -> ExprSignatureError")
    except X.ExprSignatureError:
        _check(True, "unknown Call -> ExprSignatureError")

    if _FAILS:
        print("\nFAILED (%d):" % len(_FAILS))
        for f in _FAILS:
            print("  -", f)
        sys.exit(1)
    print("\nALL S6 SelExpr-capture checks PASSED")


if __name__ == "__main__":
    main()
