#!/usr/bin/env python3
###############################################################################
# Particle topology soft-fail gate (JUL13 DFLOW E2.5, S8.5 — owner law: "changing graph
# topology should not cause a crash (it's ok if it stops / restarts simulation)"; owner
# generalization: "same class of bug for ANY change of topology").
#
# A particle module whose 'pool' input is not connected — however the graph reached that
# state — previously hit OrkAssert(false) in ParticleModuleInst::_onLink during
# ParticlesDrawableData::createDrawable, crashing the process. The soft-fail sits at that
# shared seam (no per-module special case):
#   - _onLink marks the whole topology INVALID instead of asserting,
#   - GraphInst::updateTopology skips stage()/activate() on an invalid topology,
#   - createDrawable returns null -> surfaced to python as None (loud, never an abort).
#
# GATE (headless) — the THREE members of the topology-change class all soft-fail through the
# same seam, and the process exits rc=0 (the assert would have aborted with a nonzero rc):
#   ADD        an unwired-in-chain module (editor state right after adding a node),
#   DELETE     a mid-chain module removed -> its downstream's 'pool' link dangles (the owner's
#              second repro: CurlNoiseForceModuleInst::onLink), then RECONNECT -> rebuilds,
#   DISCONNECT a single edge cut (no node removed) -> the downstream 'pool' link dangles.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys

from orkengine import core, lev2, ecs

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS = os.path.normpath(os.path.join(
    _HERE, "..", "..", "..", "..", "obt.project", "scripts"))
if _SCRIPTS not in sys.path:
    sys.path.insert(0, _SCRIPTS)

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P

_FAILS = []
def _check(c, m):
    (_FAILS.append(m) or print("  FAIL:", m)) if not c else print("  ok  :", m)


class _Chain(ParticleSystem):
    # a valid two-force chain: pool -> emit -> TURB -> CURL -> renderer. The mid-chain node
    # (TURB) and the edge into the downstream force (CURL) are the topology-change targets;
    # CURL is the owner's second-repro downstream (CurlNoiseForceModuleInst::onLink).
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT", LifeSpan=2.0, EmissionRate=1000)
        self.turb = P.Turbulence(self.emit, name="TURB")
        self.curl = P.CurlNoise(self.turb, name="CURL")
        self.mtl = lev2.particles.GradientMaterial.createShared()
        self.strk = P.StreakRenderer(self.curl, name="STRK", material=self.mtl,
                                     Length=0.2, Width=0.02)
        self.render(self.strk)


def _draw(gd):
    dd = lev2.ParticlesDrawableData()
    dd.graphdata = gd
    return dd.createDrawable()


def main():
    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    try:
        print("== S8.5 particle topology soft-fail gate (3-case topology-change class) ==")

        # baseline: a fully-wired chain builds a real drawable (recover-on-restart target)
        _check(_draw(_Chain().graphdata) is not None,
               "baseline: fully-wired chain -> createDrawable builds a drawable")

        # ---- ADD: an unwired-in-chain module (editor state right after adding a node) ----
        s = _Chain(); gd = s.graphdata
        added = gd.create("TURB_UNWIRED", lev2.particles.Turbulence)   # 'pool' input UNWIRED
        gd.connect(s.strk.module.inputs.pool, added.outputs.pool)      # renderer reads it -> in topology
        _check(_draw(gd) is None, "ADD: unwired-in-chain module -> createDrawable None (no crash)")

        # ---- DELETE: remove a MID-CHAIN module -> downstream (CURL) 'pool' link dangles ----
        s = _Chain(); gd = s.graphdata
        gd.removeModule(s.turb.module)      # removeModule reciprocally disconnects CURL's producer link
        _check(_draw(gd) is None, "DELETE: mid-chain node removed -> downstream dangling -> None (no crash)")
        # RECONNECT the severed chain (CURL.pool <- EMIT.pool) -> rebuilds + sim can advance
        gd.connect(s.curl.module.inputs.pool, s.emit.module.outputs.pool)
        _check(_draw(gd) is not None, "DELETE+RECONNECT: severed chain re-wired -> createDrawable rebuilds")

        # ---- DISCONNECT: cut a single edge (no node removed) -> downstream 'pool' dangles ----
        s = _Chain(); gd = s.graphdata
        gd.disconnect(s.curl.module.inputs.pool)   # cut EMIT..TURB->CURL edge at CURL's input
        _check(_draw(gd) is None, "DISCONNECT: edge cut -> downstream dangling -> None (no crash)")
    finally:
        ezapp.mainThreadEnd() if hasattr(ezapp, "mainThreadEnd") else None

    if _FAILS:
        print("\nFAILED (%d):" % len(_FAILS))
        for f in _FAILS: print("  -", f)
        sys.exit(1)
    print("\nALL S8.5 topology soft-fail checks PASSED — ADD / DELETE(+RECONNECT) / DISCONNECT, rc=0")


if __name__ == "__main__":
    main()
