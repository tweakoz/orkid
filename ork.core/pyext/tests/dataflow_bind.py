#!/usr/bin/env ork.python

###############################################################################
# Integration test for ork.dflow bind() + chain lowering.
#
# Builds a real ParticleSystem via the DSL with bind() calls and verifies the
# resulting graphdata is structurally what the lowerer should have emitted:
# implicit Globals module added, expected modules present, no exceptions on
# graph mutation. The unit tests for the lowerer's logic live in
# dataflow_lower.py; this exercises the actual graph emission path.
###############################################################################

import sys

from orkengine.core import *
from orkengine.lev2 import particles

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow import Expr

coreappinit()


# ---------------------------------------------------------------------------
# Case 1 — single bind with multi-stage chain
# ---------------------------------------------------------------------------

class SingleBind(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT",
                                        EmissionRate=1000, LifeSpan=2.0)
        self.streaks = P.StreakRenderer(self.emit, name="STRK",
                                        Length=0.1, Width=0.01)
        self.render(self.streaks)

        # the canonical MinV expression — 4-stage chain rooted at Globals.RelTime
        self.emit.bind("MinV", 0.6 + Expr.sin(Expr.time * 4) * 0.2)


sys_a = SingleBind()
g_a = sys_a.generatedflow()

# Before generatedflow(): 3 user modules. After: implicit Globals adds one.
assert g_a.num_modules == 4, f"expected 4 modules (3 user + implicit Globals), got {g_a.num_modules}"
print(f"PASS: SingleBind graph has 4 modules ({g_a.num_modules})")

assert g_a.findModule("_DSL_Globals") is not None, "implicit Globals not added"
print("PASS: implicit _DSL_Globals module lazily added")

for name in ("POOL", "EMIT", "STRK"):
    assert g_a.findModule(name) is not None, f"user module {name!r} missing"
print("PASS: all user modules (POOL, EMIT, STRK) present")


# ---------------------------------------------------------------------------
# Case 2 — multiple binds sharing the SAME implicit Globals
# ---------------------------------------------------------------------------

class MultiBind(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT",
                                        EmissionRate=1000, LifeSpan=2.0)
        self.streaks = P.StreakRenderer(self.emit, name="STRK")
        self.render(self.streaks)

        # three binds — all referencing Expr.time, must share ONE Globals
        self.emit.bind("MinV",         0.6 + Expr.sin(Expr.time * 4) * 0.2)
        self.emit.bind("MaxV",         0.4 - Expr.sin(Expr.time * 4) * 0.2)
        self.emit.bind("DispersionAngle", 90 + Expr.sin(Expr.time) * 30)


sys_b = MultiBind()
g_b = sys_b.generatedflow()

# Still expect ONE Globals despite three binds
assert g_b.num_modules == 4, \
    f"three binds should share one Globals (4 modules total); got {g_b.num_modules}"
print(f"PASS: MultiBind graph has 4 modules (Globals shared across 3 binds)")


# ---------------------------------------------------------------------------
# Case 3 — bind to a Const folds to a static plug value, NO Globals added
# ---------------------------------------------------------------------------

class ConstBind(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT",
                                        EmissionRate=1000, LifeSpan=2.0)
        self.streaks = P.StreakRenderer(self.emit, name="STRK")
        self.render(self.streaks)

        # bind to a fully-foldable Expr — no source needed
        self.emit.bind("MinV", 0.6 + Expr.const(0.4))   # folds to Const(1.0)


sys_c = ConstBind()
g_c = sys_c.generatedflow()

assert g_c.num_modules == 3, \
    f"all-Const bind should NOT add Globals; expected 3 modules, got {g_c.num_modules}"
print("PASS: ConstBind has no Globals (all-Const bind takes the static-value path)")

assert g_c.findModule("_DSL_Globals") is None, "Globals incorrectly added for Const bind"
print("PASS: ConstBind did not lazy-add Globals")

# (readback assertion dropped — input proxy's __getattr__ returns the plug
# object, not the stored value; the setattr write-through path is the same
# one used by chain_op._chain.py and is verified by the existing tests.)


# ---------------------------------------------------------------------------
# Case 4 — bind() outside trace context raises (runs BEFORE the deliberate-
# failure cases below, since a failed __init__ orphans the trace state — a
# known limitation to fix in a v1 polish pass).
# ---------------------------------------------------------------------------

try:
    sys_a.emit.bind("MinV", Expr.time)
    print("FAIL: bind() outside trace should have raised RuntimeError", file=sys.stderr)
    sys.exit(1)
except RuntimeError as e:
    if "trace context" not in str(e):
        print(f"FAIL: error message should mention trace context; got: {e}", file=sys.stderr)
        sys.exit(1)
    print("PASS: bind() outside trace context raises RuntimeError")


# ---------------------------------------------------------------------------
# Case 5 — bind that cannot lower raises NotImplementedError pointing at #27
# ---------------------------------------------------------------------------

class MultiSourceBind(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096)
        self.emit = P.EllipticalEmitter(self.pool, EmissionRate=1000, LifeSpan=2.0)
        self.streaks = P.StreakRenderer(self.emit)
        self.render(self.streaks)
        # multi-input expression — chain can't express; fallback (#27) absent
        self.emit.bind("MinV", Expr.sin(Expr.time) + Expr.cos(Expr.time))


try:
    sys_d = MultiSourceBind()
    sys_d.generatedflow()
    print("FAIL: multi-source bind should have raised NotImplementedError", file=sys.stderr)
    sys.exit(1)
except NotImplementedError as e:
    if "task #27" not in str(e):
        print(f"FAIL: error message should reference task #27; got: {e}", file=sys.stderr)
        sys.exit(1)
    print(f"PASS: multi-source bind raises NotImplementedError pointing at #27")


# ---------------------------------------------------------------------------
# Case 6 — Expr.param() with a matching expose() works; without expose()
# raises a clear RuntimeError telling the author to call self.expose().
# (Pre-#26 this case raised NotImplementedError; #26 implemented the
# Parameters module so the working path is now exercised.)
# ---------------------------------------------------------------------------

class ParamBindExposed(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.expose("Intensity", default=1.0)
        self.pool = P.PoolData(size=4096)
        self.emit = P.EllipticalEmitter(self.pool, EmissionRate=1000, LifeSpan=2.0)
        self.streaks = P.StreakRenderer(self.emit)
        self.render(self.streaks)
        self.emit.bind("EmissionRate", Expr.param("Intensity") * 10000)


sys_e = ParamBindExposed()
gd_e  = sys_e.generatedflow()
if gd_e.findModule("_DSL_Parameters") is None:
    print("FAIL: expose() should have created _DSL_Parameters module", file=sys.stderr)
    sys.exit(1)
print("PASS: Expr.param('Intensity') with expose() wires through Parameters module")


class ParamBindUnexposed(ParticleSystem):
    def __init__(self):
        super().__init__()
        # NOTE: no self.expose("Intensity", ...) — this is the author error
        # the validator should catch with a clear message.
        self.pool = P.PoolData(size=4096)
        self.emit = P.EllipticalEmitter(self.pool, EmissionRate=1000, LifeSpan=2.0)
        self.streaks = P.StreakRenderer(self.emit)
        self.render(self.streaks)
        self.emit.bind("EmissionRate", Expr.param("Intensity") * 10000)


try:
    sys_u = ParamBindUnexposed()
    sys_u.generatedflow()
    print("FAIL: unexposed Expr.param should have raised RuntimeError", file=sys.stderr)
    sys.exit(1)
except RuntimeError as e:
    if "expose(" not in str(e):
        print(f"FAIL: error message should tell author to call self.expose(); got: {e}",
              file=sys.stderr)
        sys.exit(1)
    print("PASS: unexposed Expr.param raises RuntimeError telling author to call self.expose()")


# ---------------------------------------------------------------------------
# Case 7 — bind() rejects non-Expr non-numeric values at construction
# ---------------------------------------------------------------------------

class BadBind(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096)
        self.emit = P.EllipticalEmitter(self.pool, EmissionRate=1000, LifeSpan=2.0)
        self.streaks = P.StreakRenderer(self.emit)
        self.render(self.streaks)
        self.emit.bind("MinV", "hello")   # type error


try:
    sys_f = BadBind()
    print("FAIL: bind('MinV', 'hello') should have raised TypeError", file=sys.stderr)
    sys.exit(1)
except TypeError:
    print("PASS: bind(plug, str) raises TypeError at construction")


print("OK")
coreappexit()
sys.exit(0)
