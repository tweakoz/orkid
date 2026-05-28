#!/usr/bin/env ork.python

###############################################################################
# Unit tests for ork.dflow.Expr — the symbolic builder used by HyperSyn DSL
# bindings. Pure Python; no orkid C++ runtime touched, so no coreappinit().
###############################################################################

import math
import sys

from ork.hypergraph.dflow import Expr
from ork.hypergraph.dflow._expr import (
    Const, ContextRef, ParamRef, BinOp, UnaryFn,
    PowExpr, ClampExpr, MinMaxExpr, LerpExpr, Vec3Expr,
    FmodExpr, SmoothstepExpr, QuantizeExpr, CurveExpr,
)


def check(cond, msg):
    if not cond:
        print(f"FAIL: {msg}", file=sys.stderr)
        sys.exit(1)
    print(f"PASS: {msg}")


# ---------------------------------------------------------------------------
# Leaves
# ---------------------------------------------------------------------------

t = Expr.time
check(isinstance(t, ContextRef) and t.dsl_name == "time", "Expr.time -> ContextRef('time')")

dt = Expr.dt
check(isinstance(dt, ContextRef) and dt.dsl_name == "dt", "Expr.dt -> ContextRef('dt')")

p = Expr.param("Intensity")
check(isinstance(p, ParamRef) and p.name == "Intensity", "Expr.param('Intensity') -> ParamRef('Intensity')")

c = Expr.const(3.5)
check(isinstance(c, Const) and c.value == 3.5, "Expr.const(3.5) -> Const(3.5)")


# ---------------------------------------------------------------------------
# Operator overloads — non-foldable cases produce BinOp
# ---------------------------------------------------------------------------

e = Expr.time + 1
check(isinstance(e, BinOp) and e.op == "+", "Expr.time + 1 -> BinOp('+')")
check(isinstance(e.lhs, ContextRef) and isinstance(e.rhs, Const) and e.rhs.value == 1.0,
      "Expr.time + 1 numeric autowrap")

e = 1 + Expr.time   # __radd__
check(isinstance(e, BinOp) and isinstance(e.lhs, Const) and e.lhs.value == 1.0,
      "1 + Expr.time -> BinOp('+', Const(1), ContextRef) via __radd__")

e = Expr.time - 2
check(isinstance(e, BinOp) and e.op == "-", "Expr.time - 2 -> BinOp('-')")

e = 2 - Expr.time   # __rsub__
check(isinstance(e, BinOp) and e.op == "-" and isinstance(e.lhs, Const),
      "2 - Expr.time -> BinOp('-', Const, ContextRef) via __rsub__")

e = Expr.time * 4
check(isinstance(e, BinOp) and e.op == "*", "Expr.time * 4 -> BinOp('*')")

e = Expr.time / 2
check(isinstance(e, BinOp) and e.op == "/", "Expr.time / 2 -> BinOp('/')")

e = -Expr.time      # __neg__
check(isinstance(e, BinOp) and e.op == "*"
      and isinstance(e.lhs, Const) and e.lhs.value == -1.0
      and isinstance(e.rhs, ContextRef),
      "-Expr.time -> BinOp('*', Const(-1), ContextRef)")


# ---------------------------------------------------------------------------
# Constant folding — Const op Const collapses to Const
# ---------------------------------------------------------------------------

e = Expr.const(0.6) + 0.4
check(isinstance(e, Const) and abs(e.value - 1.0) < 1e-12,
      "Const(0.6) + 0.4 folds to Const(1.0)")

e = Expr.const(5) * Expr.const(2)
check(isinstance(e, Const) and e.value == 10.0, "Const(5)*Const(2) folds to Const(10)")

e = Expr.const(7) - Expr.const(4)
check(isinstance(e, Const) and e.value == 3.0, "Const(7)-Const(4) folds to Const(3)")

e = Expr.const(10) / Expr.const(4)
check(isinstance(e, Const) and e.value == 2.5, "Const(10)/Const(4) folds to Const(2.5)")

# Const-Const inside a larger non-const tree
e = Expr.time * (Expr.const(2) * Expr.const(3))
check(isinstance(e, BinOp) and isinstance(e.rhs, Const) and e.rhs.value == 6.0,
      "nested Const*Const folds before composing with ContextRef")


# ---------------------------------------------------------------------------
# Unary fns — fold for foldable args; preserve for non-Const args
# ---------------------------------------------------------------------------

e = Expr.sin(0)
check(isinstance(e, Const) and e.value == 0.0, "sin(0) folds to Const(0)")

e = Expr.cos(0)
check(isinstance(e, Const) and abs(e.value - 1.0) < 1e-12, "cos(0) folds to Const(1)")

e = Expr.abs(-3)
check(isinstance(e, Const) and e.value == 3.0, "abs(-3) folds to Const(3)")

e = Expr.sqrt(9)
check(isinstance(e, Const) and e.value == 3.0, "sqrt(9) folds to Const(3)")

e = Expr.sin(Expr.time)
check(isinstance(e, UnaryFn) and e.fn == "sin" and isinstance(e.arg, ContextRef),
      "sin(time) preserved as UnaryFn")


# ---------------------------------------------------------------------------
# Multi-arg fns — fold all-Const, preserve otherwise
# ---------------------------------------------------------------------------

e = Expr.pow(2, 3)
check(isinstance(e, Const) and e.value == 8.0, "pow(2,3) folds to Const(8)")

e = Expr.pow(Expr.time, 2)
check(isinstance(e, PowExpr) and isinstance(e.x, ContextRef) and isinstance(e.k, Const),
      "pow(time, 2) preserved as PowExpr (chain-lowerable)")

e = Expr.clamp(5, 0, 1)
check(isinstance(e, Const) and e.value == 1.0, "clamp(5, 0, 1) folds to Const(1)")

e = Expr.clamp(Expr.time, 0, 1)
check(isinstance(e, ClampExpr), "clamp(time, 0, 1) preserved as ClampExpr")

e = Expr.min(3, 5)
check(isinstance(e, Const) and e.value == 3.0, "min(3, 5) folds to Const(3)")

e = Expr.max(3, 5)
check(isinstance(e, Const) and e.value == 5.0, "max(3, 5) folds to Const(5)")

e = Expr.lerp(0, 10, 0.25)
check(isinstance(e, Const) and abs(e.value - 2.5) < 1e-12,
      "lerp(0, 10, 0.25) folds to Const(2.5)")

e = Expr.lerp(0, Expr.time, 0.5)
check(isinstance(e, LerpExpr), "lerp(0, time, 0.5) preserved as LerpExpr")


# ---------------------------------------------------------------------------
# Vec3 — broadcast and per-axis forms
# ---------------------------------------------------------------------------

v = Expr.vec3(Expr.time)
check(isinstance(v, Vec3Expr) and v.x is v.y is v.z,
      "Vec3(time) broadcasts — x, y, z all the SAME node")

v = Expr.vec3(1, 2, 3)
check(isinstance(v, Vec3Expr)
      and isinstance(v.x, Const) and v.x.value == 1.0
      and isinstance(v.y, Const) and v.y.value == 2.0
      and isinstance(v.z, Const) and v.z.value == 3.0,
      "Vec3(1,2,3) wraps each axis as distinct Const")

v = Expr.vec3(Expr.sin(Expr.time) * 20)
check(isinstance(v, Vec3Expr) and isinstance(v.x, BinOp) and v.x is v.y is v.z,
      "Vec3(sin(time)*20) broadcasts a composite expr")


# ---------------------------------------------------------------------------
# Type discipline — non-numeric raises at construction
# ---------------------------------------------------------------------------

try:
    Expr.time + "hello"   # type: ignore
    check(False, "string in arithmetic should raise TypeError")
except TypeError:
    check(True, "string in arithmetic raises TypeError at construction")


# ---------------------------------------------------------------------------
# Full elliptical.py expressions — structural shape
# ---------------------------------------------------------------------------

# MinV = 0.6 + sin(time*4) * 0.2
e = 0.6 + Expr.sin(Expr.time * 4) * 0.2
check(isinstance(e, BinOp) and e.op == "+"
      and isinstance(e.lhs, Const) and e.lhs.value == 0.6
      and isinstance(e.rhs, BinOp) and e.rhs.op == "*"
      and isinstance(e.rhs.lhs, UnaryFn) and e.rhs.lhs.fn == "sin"
      and isinstance(e.rhs.rhs, Const) and e.rhs.rhs.value == 0.2,
      "MinV expression: 0.6 + sin(time*4) * 0.2 has expected tree shape")

# MaxV = 0.4 - sin(time*4) * 0.2
e = 0.4 - Expr.sin(Expr.time * 4) * 0.2
check(isinstance(e, BinOp) and e.op == "-"
      and isinstance(e.lhs, Const) and e.lhs.value == 0.4,
      "MaxV expression: 0.4 - sin(time*4) * 0.2 root is BinOp('-')")

# Amount = vec3(sin(time) * 20)
v = Expr.vec3(Expr.sin(Expr.time) * 20)
check(isinstance(v, Vec3Expr) and isinstance(v.x, BinOp) and v.x.op == "*"
      and isinstance(v.x.lhs, UnaryFn) and v.x.lhs.fn == "sin",
      "Amount expression: vec3(sin(time) * 20) has expected tree shape")

# EmissionRate = param('Intensity') * 10000
e = Expr.param("Intensity") * 10000
check(isinstance(e, BinOp) and e.op == "*"
      and isinstance(e.lhs, ParamRef) and e.lhs.name == "Intensity"
      and isinstance(e.rhs, Const) and e.rhs.value == 10000.0,
      "EmissionRate expression: param('Intensity') * 10000")


# ---------------------------------------------------------------------------
# Chain-targetable primitives (fmod / smoothstep / quantize / curve / remap)
# ---------------------------------------------------------------------------

# fmod — math.fmod semantics (preserves sign)
e = Expr.fmod(7, 3)
check(isinstance(e, Const) and e.value == 1.0, "fmod(7, 3) folds to 1.0")

e = Expr.fmod(-1, 3)
check(isinstance(e, Const) and abs(e.value - math.fmod(-1, 3)) < 1e-12,
      "fmod(-1, 3) preserves sign per math.fmod")

e = Expr.fmod(Expr.time, 3)
check(isinstance(e, FmodExpr) and isinstance(e.x, ContextRef) and isinstance(e.k, Const),
      "fmod(time, 3) preserves as FmodExpr (chain-lowerable)")

# smoothstep — clamped linear remap, NOT cubic
e = Expr.smoothstep(0.5, 0, 1)
check(isinstance(e, Const) and abs(e.value - 0.5) < 1e-12,
      "smoothstep(0.5, 0, 1) folds to 0.5 (linear)")

e = Expr.smoothstep(2, 0, 1)
check(isinstance(e, Const) and e.value == 1.0,
      "smoothstep(2, 0, 1) folds to 1.0 (clamped high)")

e = Expr.smoothstep(-1, 0, 1)
check(isinstance(e, Const) and e.value == 0.0,
      "smoothstep(-1, 0, 1) folds to 0.0 (clamped low)")

e = Expr.smoothstep(0.5, 0.2, 0.8)
check(isinstance(e, Const) and abs(e.value - 0.5) < 1e-12,
      "smoothstep(0.5, 0.2, 0.8) folds to 0.5")

e = Expr.smoothstep(Expr.time, 0, 1)
check(isinstance(e, SmoothstepExpr), "smoothstep(time, 0, 1) preserves")

try:
    Expr.smoothstep(0.5, 1, 1)
    check(False, "smoothstep with edge0==edge1 should raise ValueError")
except ValueError:
    check(True, "smoothstep with edge0==edge1 raises at construction")

# quantize — matches C++ floatxfquantizedata exactly
# step=0.25 → inumsteps = int(4.0) + 1 = 5;  int(0.73 * 5) = 3;  3/5 = 0.6
e = Expr.quantize(0.73, 0.25)
check(isinstance(e, Const) and abs(e.value - 0.6) < 1e-12,
      "quantize(0.73, 0.25) folds to 0.6 (matches C++ semantics)")

e = Expr.quantize(0.5, 0)
check(isinstance(e, Const) and e.value == 0.5,
      "quantize(x, 0) passes x through (matches C++ pass-through)")

e = Expr.quantize(Expr.time, 0.1)
check(isinstance(e, QuantizeExpr), "quantize(time, 0.1) preserves")

# curve — opaque hold, no fold
cp = [(0.0, 0.0), (1.0, 1.0)]
e = Expr.curve(Expr.time, cp)
check(isinstance(e, CurveExpr) and isinstance(e.x, ContextRef) and e.curve is cp,
      "curve(time, [(0,0),(1,1)]) preserves with curve data held opaquely")

e = Expr.curve(Expr.const(0.5), cp)
check(isinstance(e, CurveExpr),
      "curve(Const, …) preserves — folding would require runtime sampling")

# remap — sugar; desugars to BinOp tree (or fully folds if all-Const)
e = Expr.remap(0.5, 0, 1, 100, 200)
check(isinstance(e, Const) and abs(e.value - 150.0) < 1e-12,
      "remap(0.5, 0, 1, 100, 200) all-Const folds to 150.0")

e = Expr.remap(Expr.time, 0, 1, -1, 1)
check(isinstance(e, BinOp) and e.op == "+",
      "remap(time, 0, 1, -1, 1) desugars to BinOp tree (chain-lowerable as bias→scale→scale→bias)")


# ---------------------------------------------------------------------------
# Repr sanity (debugging aid — non-empty, no crash)
# ---------------------------------------------------------------------------

e = 0.6 + Expr.sin(Expr.time * 4) * 0.2
r = repr(e)
check(isinstance(r, str) and len(r) > 0 and "Const" in r and "ContextRef" in r and "sin" in r,
      "repr of full expression contains expected node names")


print("OK")
sys.exit(0)
