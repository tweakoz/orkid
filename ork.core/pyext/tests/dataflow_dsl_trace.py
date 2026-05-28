#!/usr/bin/env ork.python

###############################################################################
# Smoke test for M1 step 1: trace infrastructure + ONE DSL op (pool_data).
#
# Verifies the DSL-traced graph is structurally equivalent to the
# imperative-form graph that existing ptc_*.py scripts build today. Equivalent
# = same module name, same module class, same plug-value settings.
#
# This is the proof-of-pattern for M1. Once green, additional ops
# (emitter / force / renderer) follow the same pattern — each is a small
# .py file under obt.project/scripts/ork/dflow/particles/ops/.
###############################################################################

import sys
from orkengine.core import *
from orkengine.lev2 import particles

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P

coreappinit()

dflow = dataflow

##############################################################################
# Build via DSL
##############################################################################

class TinyPool(ParticleSystem):
    def __init__(self):
        super().__init__()
        self.pool = P.PoolData(size=4096, name="POOL")

dsl_sys = TinyPool()
dsl_graph = dsl_sys.generatedflow()

assert dsl_graph.num_modules == 1, f"DSL graph has {dsl_graph.num_modules} modules; expected 1"
print("PASS: DSL graph has 1 module")

dsl_pool = dsl_sys.pool
assert dsl_pool.module is not None
# graphdata.findModule(name) returns the registered module by name, or None.
# Used in lieu of a bound .name attribute on DgModuleData (intentionally not
# exposed today — round-trip via the graph is the canonical lookup path).
dsl_found = dsl_graph.findModule("POOL")
assert dsl_found is not None, "DSL graph has no module named POOL"
print("PASS: DSL module registered as POOL")

assert dsl_pool.module.pool_size == 4096
print("PASS: DSL pool_size == 4096")

##############################################################################
# Build the imperative-form equivalent
##############################################################################

imp_graph = dflow.GraphData.createShared()
imp_pool = imp_graph.create("POOL", particles.Pool)
imp_pool.pool_size = 4096

assert imp_graph.num_modules == 1
assert imp_graph.findModule("POOL") is not None
assert imp_pool.pool_size == 4096
print("PASS: imperative-form equivalent built")

##############################################################################
# Equivalence checks (structural — same shape)
##############################################################################

assert dsl_graph.num_modules == imp_graph.num_modules
print("PASS: same module count")

# Both pool modules are the same C++ class
dsl_class = dsl_pool.module.clazz.name
imp_class = imp_pool.clazz.name
assert dsl_class == imp_class, f"class mismatch: DSL={dsl_class!r} imperative={imp_class!r}"
print(f"PASS: same module class ({dsl_class})")

# Both have identical pool_size
assert dsl_pool.module.pool_size == imp_pool.pool_size
print("PASS: same pool_size")

print("OK")
coreappexit()
sys.exit(0)
