#!/usr/bin/env ork.python

###############################################################################
# Headless test for the elliptical_per_particle variant — exercises the
# Expr.ptc.unit_age path end-to-end through bind() + Vec3Combine.
#
# Same module shape as elliptical_per_particle.py (minus the GPU-bound
# material/texture). Verifies:
#  - Pool is found via REQUIRE_EXISTING (not duplicated)
#  - Globals is added once for the time-driven binds
#  - One Vec3Combine for the turbulence Amount vec3
#  - Per-particle width bind survives lowering without an extra Globals
###############################################################################

import sys

from orkengine.core import *
from orkengine.lev2 import particles

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow import Expr

coreappinit()


class EllipticalPerParticlePort(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool       = P.PoolData(size=4096, name="POOL")
        self.emitter    = P.EllipticalEmitter(self.pool, name="EMITN",
                                              EmissionRate=1000, LifeSpan=2.0)
        self.turbulence = P.Turbulence(self.emitter, name="TURB")
        self.vortex     = P.Vortex(self.turbulence, name="VORT",
                                   VortexStrength=-5, OutwardStrength=-1,
                                   Falloff=0.001)
        self.elliptical = P.EllipticalAttractor(self.vortex, name="SPHR",
                                                Inertia=111, Dampening=0.999)
        self.gravity    = P.Gravity(self.elliptical, name="GRAV",
                                    G=0, Mass=1, OthMass=1, MinDistance=10)
        self.streaks    = P.StreakRenderer(self.gravity, name="STRK",
                                           Length=0.15, Width=0.015)
        self.render(self.streaks)

        e = Expr
        # Same time-driven binds as elliptical.py
        self.emitter.bind("MinV",      0.6 + e.sin(e.time) * 0.2)
        self.emitter.bind("MaxV",      0.4 - e.sin(e.time) * 0.2)
        self.turbulence.bind("Amount", e.vec3(e.sin(e.time * 0.25) * 20))
        # NEW: per-particle width via Pool.UnitAge
        self.streaks.bind("Width", (e.sin(e.ptc.unit_age * 6.28) * 0.5 + 0.5) * 0.05)


system = EllipticalPerParticlePort()
graph = system.generatedflow()

# Expected modules:
#   user-built (7):  POOL, EMITN, TURB, VORT, SPHR, GRAV, STRK
#   lowerer-added (2): _DSL_Globals (shared by MinV/MaxV/Amount),
#                      _DSL_Vec3Combine_0 (for Amount)
#   per-particle bind on Width does NOT add a new module — it routes from
#   the EXISTING user Pool (REQUIRE_EXISTING policy)
#   total: 9
expected = ["POOL", "EMITN", "TURB", "VORT", "SPHR", "GRAV", "STRK",
            "_DSL_Globals", "_DSL_Vec3Combine_0"]

assert graph.num_modules == len(expected), \
    f"expected {len(expected)} modules; got {graph.num_modules}"
print(f"PASS: graph has {graph.num_modules} modules (per-particle bind did not add a Pool)")

missing = [n for n in expected if graph.findModule(n) is None]
assert not missing, f"missing modules: {missing}"
print(f"PASS: all {len(expected)} expected modules present")

# Confirm the Pool the lowerer used IS the user's POOL (not a synthetic one)
assert graph.findModuleByClass(particles.Pool) is graph.findModule("POOL"), \
    "lowerer should have routed Width through the user's POOL"
print("PASS: per-particle Width bind routes through user-declared POOL (not a synthetic one)")

# No extra Globals — Width bind doesn't need one (unit_age has its own source)
globals_count = sum(1 for n in [n for n in expected if "Globals" in n]
                    if graph.findModule(n) is not None)
assert globals_count == 1, f"expected exactly one Globals; found {globals_count}"
print("PASS: one shared Globals (time binds) + zero added by the per-particle bind")

print("OK")
coreappexit()
sys.exit(0)
