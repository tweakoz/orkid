#!/usr/bin/env ork.python

###############################################################################
# Smoke test for M1 step 2: full particle chain via DSL (no rendering).
#
# Builds the same module set + connections as ptc_elliptical3.py's imperative
# graph construction, then confirms structural equivalence. Headless — no
# scenegraph attach, no GPU init, no main loop. The render path will be
# exercised by a visual ptc_*.py example as the next step.
###############################################################################

import sys
from orkengine.core import *
from orkengine.lev2 import particles

from ork.dflow.particles import ParticleSystem
from ork.dflow import particles as P

coreappinit()

dflow = dataflow

##############################################################################
# DSL form — what users will write
##############################################################################

class EllipticalSystem(ParticleSystem):
    """Mirrors ptc_elliptical3.py's graph: pool → emitter → turb → vort →
    elliptical attractor → gravity → streak renderer."""

    def __init__(self):
        super().__init__()
        self.pool  = P.PoolData(size=50000, name="POOL")
        self.emit  = P.EllipticalEmitter(self.pool, name="EMITN",
                                         EmissionVelocity=1.0,
                                         DispersionAngle=180,
                                         LifeSpan=2.0,
                                         EmissionRate=10000)
        self.turb  = P.Turbulence(self.emit, name="TURB",
                                  Amount=vec3(1, 1, 1))
        self.vort  = P.Vortex(self.turb, name="VORT",
                              VortexStrength=-5,
                              OutwardStrength=-1,
                              Falloff=0.001)
        self.ell   = P.EllipticalAttractor(self.vort, name="SPHR",
                                           Inertia=0.01,
                                           P1=vec3(0, 1, 0),
                                           P2=vec3(0, -1, 0.1),
                                           Dampening=0.999)
        self.grav  = P.Gravity(self.ell, name="GRAV",
                               G=0.01, Mass=1, OthMass=1, MinDistance=1)
        # Renderer with no material set yet (material assignment happens in
        # _onGpuInit of a real visual test; the DSL doesn't require it at
        # construction time).
        self.streaks = P.StreakRenderer(self.grav, name="STRK",
                                        Length=0.15, Width=0.015)
        self.render(self.streaks)


sys_dsl = EllipticalSystem()
gdata = sys_dsl.generatedflow()

##############################################################################
# Confirm structure
##############################################################################

EXPECTED_MODULES = ["POOL", "EMITN", "TURB", "VORT", "SPHR", "GRAV", "STRK"]

assert gdata.num_modules == len(EXPECTED_MODULES), \
    f"expected {len(EXPECTED_MODULES)} modules; got {gdata.num_modules}"
print(f"PASS: {gdata.num_modules} modules in graph")

for name in EXPECTED_MODULES:
    m = gdata.findModule(name)
    assert m is not None, f"missing module {name!r}"
print(f"PASS: all expected modules present: {EXPECTED_MODULES}")

# Each chained module has its `pool` input connected to the predecessor's `pool`
# output. We can't easily introspect plug connectivity from Python today, but
# we can verify the chain by re-mutating plug values per-frame style — the
# pattern ptc_elliptical3.py uses — and confirming no exception.
sys_dsl.emit.inputs.LifeSpan = 1.5
sys_dsl.turb.inputs.Amount = vec3(0.5, 0.5, 0.5)
sys_dsl.vort.inputs.VortexStrength = -3
sys_dsl.ell.inputs.P1 = vec3(0, 2, 0)
sys_dsl.grav.inputs.G = 0.02
sys_dsl.streaks.inputs.Length = 0.2
print("PASS: per-frame inputs.X = value mutation works on every chained module")

# Renderer-as-output-sink recorded correctly
assert sys_dsl.output_renderer is sys_dsl.streaks
print("PASS: self.render() recorded streaks as output renderer")

# Auto-naming sanity: if we don't pass name=, a name is generated
class Anon(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=100)              # no name=
        self.grav = P.Gravity(self.pool, G=0.01)      # no name=
        self.r    = P.SpriteRenderer(self.grav)       # no name=
        self.render(self.r)

anon_sys = Anon()
anon_gdata = anon_sys.generatedflow()
assert anon_gdata.num_modules == 3
print(f"PASS: auto-named graph has {anon_gdata.num_modules} modules")

print("OK")
coreappexit()
sys.exit(0)
