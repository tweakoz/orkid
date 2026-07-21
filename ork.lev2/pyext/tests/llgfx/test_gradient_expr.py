#!/usr/bin/env python3
###############################################################################
# ExprIR particle COLOR-context gate (JUL13 DFLOW E2.5, S9). The "particles.color" ExprContext
# (the adjudication CONTEXT model superseding Q7) — a renderer color-ramp expression of
# unit_age, BAKED to a 256-entry LUT (gradient_expr.bake_gradient / lut_signature). Proves:
#
#   - context registered; unit_age is the ONLY honest bake-time symbol (per-particle symbols
#     like velocity/random are rejected loud — the 1-D ramp is a function of unit_age only),
#   - DETERMINISM: the same color exprs twice -> byte-identical LUT,
#   - AUTHORED CHANGE: a red->blue ramp vs a different (green-pulse) ramp -> DIFFERENT LUT (the
#     expression provably changes the ramp as authored), and the endpoints read as authored.
#
# The determinism + authored-change here is the CPU LUT-bake proof; the render pixel-proof
# (sampling the baked GradientMap on screen) is a gate-runner sim/render duty.
#
# ork.python only (orkengine.core first); no GPU context — pure IR + numeric LUT bake.
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
from ork.hypergraph.dflow.particles import gradient_expr as XG
from ork.hypergraph.dflow.particles.gradient_expr import PC

_FAILS = []
def _check(c, m):
    (_FAILS.append(m) or print("  FAIL:", m)) if not c else print("  ok  :", m)


def main():
    print("== S9 particle COLOR ExprIR context gate ==")

    _check("particles.color" in X.registered_contexts(), "context 'particles.color' registered")

    # authored red -> blue over life (r fades out, b fades in)
    red2blue = XG.lut_signature(r=1.0 - PC.unit_age, g=0.0, b=PC.unit_age, a=0.0)
    red2blue_2 = XG.lut_signature(r=1.0 - PC.unit_age, g=0.0, b=PC.unit_age, a=0.0)
    _check(red2blue == red2blue_2, "same expr twice -> byte-identical LUT (determinism)")
    _check(len(red2blue) == 256, "LUT has 256 entries")
    _check(red2blue[0] == (1.0, 0.0, 0.0, 0.0), "LUT[0] == red as authored")
    _check(red2blue[-1] == (0.0, 0.0, 1.0, 0.0), "LUT[255] == blue as authored")

    green_pulse = XG.lut_signature(r=0.0, g=PC.smoothstep(0.0, 0.5, PC.unit_age), b=0.0, a=0.0)
    _check(green_pulse != red2blue, "different expr -> different LUT (ramp changed as authored)")

    # unit_age is the ONLY honest bake-time symbol (per-particle symbols rejected)
    for bad in ("speed", "vel_x", "random"):
        try:
            getattr(PC, bad)
            _check(False, "per-particle symbol %r -> loud AttributeError" % bad)
        except AttributeError:
            _check(True, "per-particle symbol %r -> loud AttributeError" % bad)

    if _FAILS:
        print("\nFAILED (%d):" % len(_FAILS))
        for f in _FAILS: print("  -", f)
        sys.exit(1)
    print("\nALL S9 particle-COLOR-context checks PASSED")


if __name__ == "__main__":
    main()
