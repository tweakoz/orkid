#!/usr/bin/env python3
###############################################################################
# ExprForce engine gate (JUL13 DFLOW E2.5, S8). Authors a particle graph that uses the DSL
# op P.ExprForce (context "particles.force") and proves the ExprIR force reaches the engine:
#
#   1. the ExprForce module is present in the authored dflow graph, its reflected
#      force_{x,y,z} carry the CAPTURED particles.force ExprIR JSON (the DSL wiring),
#   2. the reflected force trees round-trip a set/get through the C++ module (pyext reflection),
#   3. a graph WITHOUT ExprForce is unaffected (the new module is additive; no ExprForce type).
#
# The ExprIR JSON round-trip identity is proven at the IR level in test_exprir_particles.py; the
# per-particle deterministic sim advance (two runs byte/pixel-identical) is a gate-runner
# sim-advance duty on the exprforce demo asset — the C++ ExprForce evaluates the tree per
# particle on the CPU with a fixed dt, so it is deterministic by construction.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys

from orkengine import core   # core before lev2
from orkengine import lev2   # noqa: F401  (loads + touches the particle module reflection)

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS = os.path.normpath(os.path.join(
    _HERE, "..", "..", "..", "..", "obt.project", "scripts"))
if _SCRIPTS not in sys.path:
    sys.path.insert(0, _SCRIPTS)

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow.particles.exprir_particles import PF, capture_force

_FAILS = []
def _check(c, m):
    (_FAILS.append(m) or print("  FAIL:", m)) if not c else print("  ok  :", m)


class _ExprForceGraph(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT", LifeSpan=2.0, EmissionRate=1000)
        self.efrc = P.ExprForce(self.emit, name="EFRC",
                                strength=1.0,
                                fx=PF.vel_z * -2.0,
                                fz=PF.vel_x * 2.0,
                                fy=PF.smoothstep(0.0, 0.3, PF.unit_age) * -3.0)
        self.mtl = lev2.particles.GradientMaterial.createShared()
        self.strk = P.StreakRenderer(self.efrc, name="STRK", material=self.mtl,
                                     Length=0.2, Width=0.02)
        self.render(self.strk)


class _PlainGraph(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT", LifeSpan=2.0, EmissionRate=1000)
        self.grav = P.Gravity(self.emit, name="GRAV")
        self.mtl = lev2.particles.GradientMaterial.createShared()
        self.strk = P.StreakRenderer(self.grav, name="STRK", material=self.mtl,
                                     Length=0.2, Width=0.02)
        self.render(self.strk)


def main():
    print("== S8 ExprForce engine gate ==")

    g = _ExprForceGraph()
    m = g.efrc.module

    # 1. reflected force trees carry the captured JSON (the DSL wiring: PF author -> capture ->
    #    the ExprForce.force_{x,y,z} reflected fields)
    exp_fy = capture_force(PF.smoothstep(0.0, 0.3, PF.unit_age) * -3.0)
    _check(m.force_y == exp_fy, "force_y reflected == captured particles.force JSON (DSL wiring)")
    _check(m.force_x != "" and m.force_z != "", "force_x/force_z populated")
    _check("unit_age" in m.force_y and "smoothstep" in m.force_y, "force_y carries the ExprIR JSON")

    # 2. the reflected force trees round-trip a set/get through the C++ module (pyext reflection)
    probe = '{"k":"call","name":"mul","args":[{"k":"ref","kind":"ptc","name":"speed"},{"k":"const","v":0.5}]}'
    m.force_x = probe
    _check(m.force_x == probe, "force_x reflected property round-trips a JSON string via C++")

    # 3. a graph WITHOUT ExprForce is unaffected (additive — the module type simply isn't present)
    p = _PlainGraph()
    _check(p.grav.module is not None, "plain graph (Gravity, no ExprForce) authors fine")
    _check(type(g.efrc.module).__name__ != type(p.grav.module).__name__,
           "ExprForce is a distinct module type from the existing forces")

    if _FAILS:
        print("\nFAILED (%d):" % len(_FAILS))
        for f in _FAILS: print("  -", f)
        sys.exit(1)
    print("\nALL S8 ExprForce engine checks PASSED")


if __name__ == "__main__":
    main()
