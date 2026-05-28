#!/usr/bin/env ork.python

###############################################################################
# Integration test for the context-variable registry + DSL namespace bridge.
#
# Exercises the headline new capability: Expr.ptc.unit_age — a per-particle
# context source registered by the C++ side via a static initializer in
# modules_pool.cpp, mirrored on the Python side by ork.dflow.particles
# registering particles.Pool. Lowerer resolves via the REQUIRE_EXISTING
# policy: finds the user's pool module by class identity; raises a clear
# error if absent (the DSL never invents a Pool).
###############################################################################

import sys

from orkengine.core import *
from orkengine.lev2 import particles

from ork.dflow.particles import ParticleSystem
from ork.dflow import particles as P
from ork.dflow import Expr

coreappinit()


# ---------------------------------------------------------------------------
# Case 1 — registry exposes the expected names + specs
# ---------------------------------------------------------------------------

names = dataflow.context_variables.all_names()
assert "time" in names, "registry missing 'time'"
assert "ptc.unit_age" in names, "registry missing 'ptc.unit_age'"
print(f"PASS: registry has expected names: {sorted(names)}")

spec = dataflow.context_variables.lookup("ptc.unit_age")
assert spec is not None
assert spec.output_plug_name == "UnitAge"
assert spec.policy == "require_existing"
print(f"PASS: ptc.unit_age spec — plug={spec.output_plug_name} policy={spec.policy}")

spec = dataflow.context_variables.lookup("time")
assert spec.output_plug_name == "RelTime"
assert spec.policy == "singleton"
print(f"PASS: time spec — plug={spec.output_plug_name} policy={spec.policy}")


# ---------------------------------------------------------------------------
# Case 2 — Expr.ptc.unit_age namespace access produces ContextRef
# ---------------------------------------------------------------------------

from ork.dflow._expr import ContextRef
ref = Expr.ptc.unit_age
assert isinstance(ref, ContextRef) and ref.dsl_name == "ptc.unit_age", \
    f"Expr.ptc.unit_age should be ContextRef('ptc.unit_age'); got {ref!r}"
print("PASS: Expr.ptc.unit_age -> ContextRef('ptc.unit_age')")


# ---------------------------------------------------------------------------
# Case 3 — bind Expr.ptc.unit_age to a renderer plug; pool exists → works
# ---------------------------------------------------------------------------

class PerParticleSize(ParticleSystem):
    """Sprite size driven by per-particle age via UnitAge → scale chain.
    Demonstrates the REQUIRE_EXISTING policy: the Pool is the source the
    lowerer finds by class identity."""
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT",
                                        EmissionRate=1000, LifeSpan=2.0)
        self.spr  = P.SpriteRenderer(self.emit, name="SPR")
        self.render(self.spr)
        # bind: Size = unit_age * 0.5
        self.spr.bind("Size", Expr.ptc.unit_age * 0.5)


sys_a = PerParticleSize()
g_a = sys_a.generatedflow()

# Expected: 3 user modules; the lowerer adds NO Globals (no Expr.time) and NO
# Vec3Combine (no vec3 bind), and FINDS the user's Pool rather than creating
# a new one (REQUIRE_EXISTING). So total = 3.
assert g_a.num_modules == 3, f"expected 3 modules; got {g_a.num_modules}"
print(f"PASS: PerParticleSize has 3 modules (Pool was found, not duplicated)")

assert g_a.findModule("_DSL_Pool") is None, \
    "REQUIRE_EXISTING policy should NOT create a _DSL_-named pool"
print("PASS: lowerer did not create a duplicate Pool module")

assert g_a.findModule("POOL") is not None, "user's POOL must be present"
print("PASS: user's POOL still present (the Pool the lowerer resolved against)")


# ---------------------------------------------------------------------------
# Case 4 — bind Expr.ptc.unit_age when NO Pool exists → clear error
# ---------------------------------------------------------------------------

class NoPool(ParticleSystem):
    """Author forgot to declare a Pool — REQUIRE_EXISTING should raise at
    generatedflow() with a clear message pointing at the missing module."""
    def __init__(self):
        super().__init__()
        # No P.PoolData call! Bind references ptc.unit_age anyway.
        # We still need SOMETHING to bind on — use a sprite renderer
        # constructed standalone (won't make a working system, but the
        # lowering error fires before any pool/emit ordering matters).
        self.spr = particles.SpriteRendererData.createShared()  # raw C++ — bypasses chain
        # We can't easily fabricate a binding without an upstream pool, so
        # skip this case and instead validate via a manually-built minimal graph.


# Test #4 takes a different shape — build the failure case by hand:

# Construct a minimal graph that uses the bind path but lacks a Pool.
# Easiest: a ParticleSystem subclass that declares only an emitter pointing
# at a Pool, then DROPS the pool reference. But the chain_op pattern needs
# the pool's output to wire upstream of the emitter, so removing the pool
# breaks the chain. Skip the malformed-graph fail case — the failure path
# is exercised by the C++ raise in _resolve_context_var when the lowerer
# can't findModuleByClass, which we'd need a more elaborate setup to
# demonstrate cleanly.

# Instead: verify the failure mode programmatically using a hand-built graph.
g_bare = dataflow.GraphData.createShared()
# add a sprite renderer module so there's somewhere to attach a binding
spr_mod = g_bare.create("SPR", particles.SpriteRenderer)
# build a minimal ContextRef + Binding by hand and try to emit it
from ork.dflow._bindings import Binding, emit_bindings
binding = Binding(spr_mod, "Size", Expr.ptc.unit_age * 0.5)
try:
    emit_bindings(g_bare, [binding])
    print("FAIL: emitting ptc.unit_age binding without a Pool should have raised",
          file=sys.stderr)
    sys.exit(1)
except RuntimeError as e:
    msg = str(e)
    assert "ptc.unit_age" in msg, f"error should mention ptc.unit_age; got: {msg}"
    assert "ParticlePoolData" in msg or "Pool" in msg, \
        f"error should mention Pool; got: {msg}"
    print("PASS: REQUIRE_EXISTING with no Pool raises RuntimeError pointing at Pool")


# ---------------------------------------------------------------------------
# Case 5 — Globals singleton uses the new reserved name format
# ---------------------------------------------------------------------------

class TimeBound(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")
        self.emit = P.EllipticalEmitter(self.pool, name="EMIT",
                                        EmissionRate=1000, LifeSpan=2.0)
        self.spr  = P.SpriteRenderer(self.emit, name="SPR")
        self.render(self.spr)
        self.spr.bind("Size", Expr.sin(Expr.time) * 0.3 + 0.5)


sys_b = TimeBound()
g_b = sys_b.generatedflow()

assert g_b.findModule("_DSL_Globals") is not None, \
    "Globals singleton should use _DSL_Globals (keyed by py_class.__name__)"
print("PASS: Globals singleton uses _DSL_Globals reserved name")


print("OK")
coreappexit()
sys.exit(0)
