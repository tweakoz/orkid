#!/usr/bin/env ork.python

###############################################################################
# Unit tests for ork.dflow._lower — the Expr → (source, chain stages) lowerer.
# Pure Python; no orkid C++ touched. The integration counterpart (which builds
# a real graphdata and verifies plug connections) is dataflow_bind.py.
###############################################################################

import sys

from ork.hypergraph.dflow import Expr
from ork.hypergraph.dflow._expr import (
    Const, ContextRef, ParamRef,
)
from ork.hypergraph.dflow._lower import lower_to_chain, ChainStage


def check(cond, msg):
    if not cond:
        print(f"FAIL: {msg}", file=sys.stderr)
        sys.exit(1)
    print(f"PASS: {msg}")


def stage_kinds(stages):
    """Convenience — list of just the .kind strings for shape assertions."""
    return [s.kind for s in stages]


# ---------------------------------------------------------------------------
# All-constant expression — emitter writes value directly, no chain
# ---------------------------------------------------------------------------

r = lower_to_chain(Expr.const(5.0))
check(r is not None, "Const(5.0) lowers")
desc, stages = r
check(desc == ("const", 5.0) and stages == [],
      "Const(5.0) lowers to ('const', 5.0) with no stages")

# Folded constants take the same path
r = lower_to_chain(0.6 + Expr.const(0.4))
check(r is not None, "Const+Const lowers (folded to Const)")
desc, _ = r
check(desc == ("const", 1.0), "Const+Const lowers to folded value")


# ---------------------------------------------------------------------------
# Bare TimeRef — connection only, no chain stages
# ---------------------------------------------------------------------------

r = lower_to_chain(Expr.time)
check(r is not None, "bare Expr.time lowers")
desc, stages = r
check(desc == ("ctx", "time") and stages == [],
      "bare Expr.time lowers to ('time', 'RelTime') with no stages")

r = lower_to_chain(Expr.dt)
desc, _ = r
check(desc == ("ctx", "dt"), "Expr.dt lowers to ('time', 'DeltaTime')")


# ---------------------------------------------------------------------------
# Single stages — each chain-targetable primitive in isolation
# ---------------------------------------------------------------------------

r = lower_to_chain(Expr.time * 4)
desc, stages = r
check(desc == ("ctx", "time") and stage_kinds(stages) == ["scale"]
      and stages[0].args["scale"] == 4.0,
      "time * 4 → scale(4)")

r = lower_to_chain(4 * Expr.time)   # commutative — same chain
desc, stages = r
check(stage_kinds(stages) == ["scale"] and stages[0].args["scale"] == 4.0,
      "4 * time (rmul) → scale(4)")

r = lower_to_chain(Expr.time + 0.5)
desc, stages = r
check(stage_kinds(stages) == ["bias"] and stages[0].args["bias"] == 0.5,
      "time + 0.5 → bias(0.5)")

r = lower_to_chain(0.5 + Expr.time)   # commutative
desc, stages = r
check(stage_kinds(stages) == ["bias"] and stages[0].args["bias"] == 0.5,
      "0.5 + time (radd) → bias(0.5)")

r = lower_to_chain(Expr.time - 0.5)
desc, stages = r
check(stage_kinds(stages) == ["bias"] and stages[0].args["bias"] == -0.5,
      "time - 0.5 → bias(-0.5)")

r = lower_to_chain(0.5 - Expr.time)   # k - spine → scale(-1), bias(k)
desc, stages = r
check(stage_kinds(stages) == ["scale", "bias"]
      and stages[0].args["scale"] == -1.0
      and stages[1].args["bias"] == 0.5,
      "0.5 - time → scale(-1), bias(0.5)  [k - spine lowers as two stages]")

r = lower_to_chain(Expr.time / 4)
desc, stages = r
check(stage_kinds(stages) == ["scale"] and stages[0].args["scale"] == 0.25,
      "time / 4 → scale(0.25)")

r = lower_to_chain(4 / Expr.time)
check(r is None, "4 / time has no single-stage chain → None")

r = lower_to_chain(Expr.sin(Expr.time))
desc, stages = r
check(stage_kinds(stages) == ["sine"], "sin(time) → sine")

r = lower_to_chain(Expr.abs(Expr.time))
desc, stages = r
check(stage_kinds(stages) == ["abs"], "abs(time) → abs")

r = lower_to_chain(Expr.pow(Expr.time, 2))
desc, stages = r
check(stage_kinds(stages) == ["power"] and stages[0].args["power"] == 2.0,
      "pow(time, 2) → power(2)")

r = lower_to_chain(Expr.fmod(Expr.time, 3))
desc, stages = r
check(stage_kinds(stages) == ["mod"] and stages[0].args["mod"] == 3.0,
      "fmod(time, 3) → mod(3)")

r = lower_to_chain(Expr.smoothstep(Expr.time, 0.2, 0.8))
desc, stages = r
check(stage_kinds(stages) == ["smoothstep"]
      and stages[0].args == {"edge0": 0.2, "edge1": 0.8},
      "smoothstep(time, 0.2, 0.8) → smoothstep(0.2, 0.8)")

r = lower_to_chain(Expr.quantize(Expr.time, 0.1))
desc, stages = r
check(stage_kinds(stages) == ["quantize"] and stages[0].args["step"] == 0.1,
      "quantize(time, 0.1) → quantize(0.1)")


# ---------------------------------------------------------------------------
# Multi-stage chains — order matters (source → consumer)
# ---------------------------------------------------------------------------

# elliptical's MinV: 0.6 + sin(time * 4) * 0.2
# expected source-first order: scale(4), sine, scale(0.2), bias(0.6)
e = 0.6 + Expr.sin(Expr.time * 4) * 0.2
r = lower_to_chain(e)
check(r is not None, "MinV expression lowers")
desc, stages = r
check(desc == ("ctx", "time"), "MinV source is Globals.RelTime")
check(stage_kinds(stages) == ["scale", "sine", "scale", "bias"],
      "MinV chain order: scale → sine → scale → bias (source to consumer)")
check(stages[0].args["scale"] == 4.0,  "MinV stage[0] is scale(4)")
check(stages[1].kind == "sine",        "MinV stage[1] is sine()")
check(abs(stages[2].args["scale"] - 0.2) < 1e-12, "MinV stage[2] is scale(0.2)")
check(abs(stages[3].args["bias"] - 0.6) < 1e-12,  "MinV stage[3] is bias(0.6)")

# MaxV: 0.4 - sin(time * 4) * 0.2 — equivalent to bias(-(-0.2*sin) + 0.4)
#       i.e. scale(4), sine, scale(-0.2), bias(0.4) when the * goes commutative
# Actually: 0.4 - (sin(time*4) * 0.2) → BinOp('-', 0.4, BinOp('*', sin(...), 0.2))
# The outer '-' has spine on the RIGHT, so it's "k - spine" which is unhandled.
# This means MaxV in its written form falls back. Author-side fix: write as
# 0.4 + sin(time*4) * -0.2 which IS chain-lowerable.
# MaxV: 0.4 - sin(time*4) * 0.2 — the outer `k - spine` lowers as two stages
# (scale(-1), bias(0.4)), so the full chain becomes:
#   scale(4) → sine → scale(0.2) → scale(-1) → bias(0.4)
# Logically equivalent to scale(-0.2), bias(0.4); orklut iterates more stages
# but each is constant-time, so this is fine.
r = lower_to_chain(0.4 - Expr.sin(Expr.time * 4) * 0.2)
desc, stages = r
check(stage_kinds(stages) == ["scale", "sine", "scale", "scale", "bias"]
      and abs(stages[3].args["scale"] - (-1.0)) < 1e-12
      and abs(stages[4].args["bias"] - 0.4) < 1e-12,
      "MaxV (0.4 - sin(time*4)*0.2) lowers to 5-stage chain via k-spine handler")

# remap desugars to four chain stages
e = Expr.remap(Expr.time, 0, 1, -1, 1)
r = lower_to_chain(e)
desc, stages = r
check(stage_kinds(stages) == ["bias", "scale", "scale", "bias"],
      "remap(time, 0, 1, -1, 1) lowers to four-stage chain (bias, scale, scale, bias)")


# ---------------------------------------------------------------------------
# Multi-source / unsupported — must return None
# ---------------------------------------------------------------------------

r = lower_to_chain(Expr.time + Expr.dt)
check(r is None, "time + dt is multi-source → None (FloatExprModule needed)")

r = lower_to_chain(Expr.sin(Expr.time) * Expr.cos(Expr.time))
check(r is None, "sin(time) * cos(time) has two consumers of time → None")

r = lower_to_chain(Expr.time + Expr.param("X"))
check(r is None, "time + param is multi-source → None")

r = lower_to_chain(Expr.cos(Expr.time))
check(r is None, "cos(time) — no chain stage for cos in v0 → None")

r = lower_to_chain(Expr.clamp(Expr.time, 0, 1))
check(r is None, "clamp(time, 0, 1) — no chain stage for clamp → None")

r = lower_to_chain(Expr.min(Expr.time, 0.5))
check(r is None, "min(time, 0.5) — no chain stage for min → None")

r = lower_to_chain(Expr.lerp(0, 1, Expr.time))
check(r is None, "lerp(0, 1, time) — no chain stage for lerp → None")

r = lower_to_chain(Expr.vec3(Expr.time))
check(r is None, "Vec3(time) — vec3 lowering is a separate path → None at chain layer")


# ---------------------------------------------------------------------------
# ParamRef sources — recognized; emitter handles them separately
# ---------------------------------------------------------------------------

r = lower_to_chain(Expr.param("Intensity") * 10000)
desc, stages = r
check(desc == ("param", "Intensity") and stage_kinds(stages) == ["scale"]
      and stages[0].args["scale"] == 10000.0,
      "param('Intensity') * 10000 → ('param', 'Intensity'), [scale(10000)]")


print("OK")
sys.exit(0)
