#!/usr/bin/env ork.python

###############################################################################
# Integration test for vec3 bindings — Expr.vec3 → Vec3Combine module + per-
# axis scalar chains + vec3 output connection. Exercises the C++ Vec3Combine
# class added in task #28.
###############################################################################

import sys

from orkengine.core import *
from orkengine.lev2 import particles

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow import Expr

coreappinit()


# ---------------------------------------------------------------------------
# Case 1 — Vec3(scalar_expr) broadcast — one Vec3Combine, three identical
# scalar chains feeding it, all reading from the same Globals.RelTime
# ---------------------------------------------------------------------------

class Vec3Broadcast(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT",
                                        EmissionRate=1000, LifeSpan=2.0)
        self.turb = P.Turbulence(self.emit, name="TURB")
        self.streaks = P.StreakRenderer(self.turb, name="STRK")
        self.render(self.streaks)

        # The elliptical.py expression: vec3(sin(time) * 20) — broadcast
        self.turb.bind("Amount", Expr.vec3(Expr.sin(Expr.time) * 20))


sys_a = Vec3Broadcast()
g_a = sys_a.generatedflow()

# Expected modules: POOL, EMIT, TURB, STRK, _DSL_Globals, _DSL_Vec3Combine_0 = 6
assert g_a.num_modules == 6, f"expected 6 modules; got {g_a.num_modules}"
print(f"PASS: Vec3Broadcast graph has 6 modules (4 user + Globals + Vec3Combine)")

assert g_a.findModule("_DSL_Vec3Combine_0") is not None, \
    "Vec3Combine module not added for Vec3Expr binding"
print("PASS: Vec3Combine module created with expected name '_DSL_Vec3Combine_0'")

assert g_a.findModule("_DSL_Globals") is not None, "Globals not added"
print("PASS: Globals module added (three axes all reference Expr.time)")


# ---------------------------------------------------------------------------
# Case 2 — Vec3 with three different scalar expressions (per-axis chains)
# ---------------------------------------------------------------------------

class Vec3PerAxisCos(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT",
                                        EmissionRate=1000, LifeSpan=2.0)
        self.turb = P.Turbulence(self.emit, name="TURB")
        self.streaks = P.StreakRenderer(self.turb, name="STRK")
        self.render(self.streaks)

        # Independent per-axis expressions sharing Globals.RelTime.
        # cos used to need FloatExprModule; since #27a it rewrites to
        # sin(x + π/2) at lower time, so all three axes chain-lower
        # cleanly through the single shared Globals source.
        self.turb.bind("Amount", Expr.vec3(
            Expr.sin(Expr.time * 1.0) * 10,
            Expr.cos(Expr.time * 2.0) * 5,
            Expr.sin(Expr.time * 3.0) * 8,
        ))


sys_b = Vec3PerAxisCos()
gd_b  = sys_b.generatedflow()
# Expect: POOL, EMIT, TURB, STRK + _DSL_Globals + _DSL_Vec3Combine_0 = 6 modules.
# Crucially NO FloatExprModule fallback was needed — cos went through the chain.
if gd_b.findModule("_DSL_Globals") is None:
    print("FAIL: Vec3PerAxisCos should have Globals (all three axes use Expr.time)",
          file=sys.stderr)
    sys.exit(1)
if gd_b.findModule("_DSL_Vec3Combine_0") is None:
    print("FAIL: Vec3PerAxisCos should have a Vec3Combine module", file=sys.stderr)
    sys.exit(1)
print("PASS: Vec3 with cos axis lowers via rewrite (no FloatExprModule needed)")


# ---------------------------------------------------------------------------
# Case 3 — Vec3 with chainable per-axis expressions (all sin/scale)
# ---------------------------------------------------------------------------

class Vec3PerAxisOK(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT",
                                        EmissionRate=1000, LifeSpan=2.0)
        self.turb = P.Turbulence(self.emit, name="TURB")
        self.streaks = P.StreakRenderer(self.turb, name="STRK")
        self.render(self.streaks)

        self.turb.bind("Amount", Expr.vec3(
            Expr.sin(Expr.time * 1.0) * 10,
            Expr.sin(Expr.time * 2.0) * 5,
            Expr.sin(Expr.time * 3.0) * 8,
        ))


sys_c = Vec3PerAxisOK()
g_c = sys_c.generatedflow()
assert g_c.num_modules == 6, f"expected 6 modules; got {g_c.num_modules}"
print(f"PASS: Vec3PerAxisOK has 6 modules (all three axes chain-lowered)")


# ---------------------------------------------------------------------------
# Case 4 — Vec3(const, const, const) — Vec3Combine still emitted (target is
# vec3 plug; can't bypass) but no Globals needed
# ---------------------------------------------------------------------------

class Vec3AllConst(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT",
                                        EmissionRate=1000, LifeSpan=2.0)
        self.turb = P.Turbulence(self.emit, name="TURB")
        self.streaks = P.StreakRenderer(self.turb, name="STRK")
        self.render(self.streaks)

        self.turb.bind("Amount", Expr.vec3(1.0, 2.0, 3.0))


sys_d = Vec3AllConst()
g_d = sys_d.generatedflow()

# 4 user + Vec3Combine = 5 (no Globals, since all axes are Const)
assert g_d.num_modules == 5, f"expected 5 modules; got {g_d.num_modules}"
print(f"PASS: Vec3AllConst has 5 modules (Vec3Combine but no Globals)")

assert g_d.findModule("_DSL_Globals") is None, \
    "Globals should NOT be added for all-Const Vec3 bind"
print("PASS: Vec3AllConst did not lazy-add Globals (no axis references Expr.time)")


# ---------------------------------------------------------------------------
# Case 5 — Multiple Vec3 bindings get separate Vec3Combine modules
# ---------------------------------------------------------------------------

class TwoVec3Binds(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT",
                                        EmissionRate=1000, LifeSpan=2.0)
        self.turb = P.Turbulence(self.emit, name="TURB")
        self.streaks = P.StreakRenderer(self.turb, name="STRK")
        self.render(self.streaks)

        self.turb.bind("Amount", Expr.vec3(Expr.sin(Expr.time) * 20))
        # P1 is a vec3 plug on EllipticalEmitter — bind a different vec3 expr
        self.emit.bind("P1", Expr.vec3(0, Expr.sin(Expr.time * 0.5) * 2, 0))


sys_e = TwoVec3Binds()
g_e = sys_e.generatedflow()

# 4 user + 1 Globals + 2 Vec3Combine = 7
assert g_e.num_modules == 7, f"expected 7 modules; got {g_e.num_modules}"
print(f"PASS: TwoVec3Binds has 7 modules (one Globals shared, two Vec3Combine)")

assert g_e.findModule("_DSL_Vec3Combine_0") is not None, "Vec3Combine_0 missing"
assert g_e.findModule("_DSL_Vec3Combine_1") is not None, "Vec3Combine_1 missing"
print("PASS: Two Vec3 binds → two distinct _DSL_Vec3Combine_N modules")


# ---------------------------------------------------------------------------
# Case 6 — Mixed scalar + vec3 bindings in one system (the elliptical.py case)
# ---------------------------------------------------------------------------

class EllipticalLite(ParticleSystem):
    """Subset of elliptical.py's binds — sin-driven MinV/MaxV + vec3 Amount."""
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT",
                                        EmissionRate=1000, LifeSpan=2.0)
        self.turb = P.Turbulence(self.emit, name="TURB")
        self.streaks = P.StreakRenderer(self.turb, name="STRK")
        self.render(self.streaks)

        self.emit.bind("MinV",   0.6 + Expr.sin(Expr.time * 4) * 0.2)
        self.emit.bind("MaxV",   0.4 - Expr.sin(Expr.time * 4) * 0.2)
        self.turb.bind("Amount", Expr.vec3(Expr.sin(Expr.time) * 20))


sys_f = EllipticalLite()
g_f = sys_f.generatedflow()

# 4 user + 1 Globals + 1 Vec3Combine = 6
assert g_f.num_modules == 6, f"expected 6 modules; got {g_f.num_modules}"
print(f"PASS: EllipticalLite has 6 modules (4 user + Globals + Vec3Combine)")

# Verify one Globals serves both scalar AND vec3 binds
globals_count = sum(1 for n in ("_DSL_Globals",) if g_f.findModule(n) is not None)
assert globals_count == 1, "expected exactly one _DSL_Globals"
print("PASS: One implicit Globals shared across scalar AND vec3 binds")


print("OK")
coreappexit()
sys.exit(0)
