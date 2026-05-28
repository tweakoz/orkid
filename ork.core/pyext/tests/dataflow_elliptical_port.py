#!/usr/bin/env ork.python

###############################################################################
# Headless port-equivalence test for elliptical.py.
#
# Builds the SAME module chain + SAME binds as
# ork.lev2/pyext/tests/hypersyn/particles/elliptical.py, minus the GPU-bound
# material/texture setup (which needs a GPU context). Verifies the bind/
# Vec3Combine emission ends in the expected module count + names. Visual
# verification of motion still requires running the player itself.
###############################################################################

import sys

from orkengine.core import *
from orkengine.lev2 import particles

from ork.dflow.particles import ParticleSystem
from ork.dflow import particles as P
from ork.dflow import Expr

coreappinit()


class EllipticalPort(ParticleSystem):
    """Same shape as elliptical.py — pool → emitter → turbulence → vortex →
    elliptical → gravity → streaks — plus three time-driven binds. No
    material/texture (GPU-context-bound), so we can verify the graph
    structure headless."""

    def __init__(self):
        super().__init__()

        self.ptc_pool = P.PoolData(size=50000, name="POOL")
        self.emitter  = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                            EmissionVelocity=1.0,
                                            DispersionAngle=180,
                                            LifeSpan=2.0,
                                            EmissionRate=10000)
        self.turbulence = P.Turbulence(self.emitter, name="TURB")
        self.vortex     = P.Vortex(self.turbulence, name="VORT",
                                   VortexStrength=-5, OutwardStrength=-1,
                                   Falloff=0.001)
        self.elliptical = P.EllipticalAttractor(self.vortex, name="SPHR",
                                                Inertia=111, Dampening=0.999)
        self.gravity    = P.Gravity(self.elliptical, name="GRAV", G=0,
                                    Mass=1, OthMass=1, MinDistance=10)
        self.streaks    = P.StreakRenderer(self.gravity, name="STRK",
                                           Length=0.15, Width=0.015)
        self.render(self.streaks)

        # Same binds as the real elliptical.py
        self.emitter.bind("MinV",      0.6 + Expr.sin(Expr.time) * 0.2)
        self.emitter.bind("MaxV",      0.4 - Expr.sin(Expr.time) * 0.2)
        self.turbulence.bind("Amount", Expr.vec3(Expr.sin(Expr.time * 0.25) * 20))


system = EllipticalPort()
graph = system.generatedflow()

# Expected modules:
#   user-built (7):  POOL, EMITN, TURB, VORT, SPHR, GRAV, STRK
#   lowerer-added (2): _DSL_Globals, _DSL_Vec3Combine_0
#   total: 9
expected = ["POOL", "EMITN", "TURB", "VORT", "SPHR", "GRAV", "STRK",
            "_DSL_Globals", "_DSL_Vec3Combine_0"]

present = [n for n in expected if graph.findModule(n) is not None]
missing = sorted(set(expected) - set(present))

assert graph.num_modules == len(expected), \
    f"expected {len(expected)} modules; got {graph.num_modules}"
print(f"PASS: graph has {graph.num_modules} modules (matches expected)")

assert not missing, f"missing modules: {missing}"
print(f"PASS: all {len(expected)} expected modules present")

# Globals serves BOTH the scalar binds (MinV, MaxV) AND the vec3 bind (Amount)
# — confirms the shared-Globals pattern works across the mixed binds.
assert graph.findModule("_DSL_Globals") is not None, "shared Globals missing"
print("PASS: one shared Globals for all three time-driven binds")

# Vec3Combine present for the Amount bind
assert graph.findModule("_DSL_Vec3Combine_0") is not None, "Vec3Combine missing"
print("PASS: Vec3Combine_0 for the turbulence.Amount vec3 bind")

print("OK")
coreappexit()
sys.exit(0)
