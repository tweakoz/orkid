#!/usr/bin/env python3
###############################################################################
# ExprIR particle FORCE-context gate (JUL13 DFLOW E2.5, S8). The "particles.force" ExprContext
# (the adjudication CONTEXT model superseding Q7) — a per-particle FORCE expression; capture
# -> JSON the reflected ExprForce.force_{x,y,z} store; the C++ module evaluates it per particle
# (deterministic). Proves: author -> capture -> JSON round-trip + capture idempotence + the
# family EVALUATOR is deterministic + correct + fail-loud on an out-of-vocabulary symbol.
# (The S9 "particles.color" context has its own gate: test_gradient_expr.py.)
#
# ork.python only (orkengine.core first); no GPU context — pure IR + numeric eval.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys

import orkengine.core  # noqa: F401

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS = os.path.normpath(os.path.join(
    _HERE, "..", "..", "..", "..", "obt.project", "scripts"))
if _SCRIPTS not in sys.path:
    sys.path.insert(0, _SCRIPTS)

from ork.hypergraph import exprir as X
from ork.hypergraph.dflow.particles import exprir_particles as XP
from ork.hypergraph.dflow.particles.exprir_particles import PF

_FAILS = []


def _check(cond, msg):
    (_FAILS.append(msg) or print("  FAIL:", msg)) if not cond else print("  ok  :", msg)


def main():
    print("== S8 particle FORCE ExprIR context gate ==")

    # ---- force context registered (the CONTEXT model, Q7 superseded) ----
    reg = X.registered_contexts()
    _check("particles.force" in reg, "context 'particles.force' registered (S8)")

    # ============================ S8 FORCE ============================
    # a swirl that stiffens with age + damps with speed (honest per-particle symbols)
    fx = PF.vel_z * -2.0
    fz = PF.vel_x * 2.0
    fy = PF.smoothstep(0.0, 0.3, PF.unit_age) * -3.0 - PF.speed * 0.1
    jx, jy, jz = XP.capture_force(fx), XP.capture_force(fy), XP.capture_force(fz)

    # JSON round-trip byte-identical
    for lbl, j in (("fx", jx), ("fy", jy), ("fz", jz)):
        _check(X.encode_json(X.decode_json(j)) == j, "force %s: JSON round-trip byte-identical" % lbl)

    # capture is idempotent (reshape): re-capture -> identical
    _check(XP.capture_force(fy) == jy, "force fy: capture idempotent")

    # the family EVALUATOR is deterministic (two evals of the same tree + syms match)
    syms = {"unit_age": 0.4, "age": 1.2, "random": 0.7,
            "pos_x": 1.0, "pos_y": 2.0, "pos_z": -1.0,
            "vel_x": 0.5, "vel_y": -0.5, "vel_z": 0.25, "speed": 0.75}
    v1 = XP.eval_expr(X.decode_json(jy), syms)
    v2 = XP.eval_expr(X.decode_json(jy), dict(syms))
    _check(v1 == v2, "force fy: evaluator deterministic (%r)" % v1)
    # sanity: smoothstep(0,0.3,0.4)=1 -> -3.0 ; minus speed*0.1 = -0.075 -> -3.075
    _check(abs(v1 - (-3.075)) < 1e-6, "force fy: evaluator value correct (-3.075)")

    # fail-loud on an out-of-vocabulary symbol (not honestly available)
    try:
        _ = PF.moisture  # not a force symbol
        _check(False, "unknown force symbol -> loud AttributeError")
    except AttributeError:
        _check(True, "unknown force symbol -> loud AttributeError")

    if _FAILS:
        print("\nFAILED (%d):" % len(_FAILS))
        for f in _FAILS:
            print("  -", f)
        sys.exit(1)
    print("\nALL S8 particle-FORCE-context checks PASSED")


if __name__ == "__main__":
    main()
