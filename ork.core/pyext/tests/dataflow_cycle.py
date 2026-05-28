#!/usr/bin/env ork.python

###############################################################################
# Negative test for M0 cycle detection (DgSorter two-site fix).
#
# Builds a 3-node cycle (A→B→C→A) and confirms:
#  - DgModuleData::computeMinDepth/MaxDepth (called by DgSorter ctor) terminate
#    instead of infinite-recursing
#  - DgSorter::generateTopology returns nullptr (not an infinite while-loop)
#
# Before M0 step 1 this script would hang forever and the test harness would
# have to kill it via timeout.
###############################################################################

import sys
from orkengine.core import *

coreappinit()

dflow = dataflow

##############################################################################
# Build a cyclic graph: A → B → C → A
##############################################################################

graphdata = dflow.GraphData.createShared()

A = graphdata.create("A", dflow.LambdaModule)
B = graphdata.create("B", dflow.LambdaModule)
C = graphdata.create("C", dflow.LambdaModule)

a_in  = A.createUniformFloatXfInputPlug("in")
a_out = A.createUniformFloatOutputPlug("out")
b_in  = B.createUniformFloatXfInputPlug("in")
b_out = B.createUniformFloatOutputPlug("out")
c_in  = C.createUniformFloatXfInputPlug("in")
c_out = C.createUniformFloatOutputPlug("out")

# wire the cycle
graphdata.connect(b_in, a_out)   # A → B
graphdata.connect(c_in, b_out)   # B → C
graphdata.connect(a_in, c_out)   # C → A   ← closes the loop

print("graph built; attempting sort (should fail fast, not hang)...")

##############################################################################
# Sort — must return None instead of infinite-looping
##############################################################################

ctx = dflow.DgContext.createShared()
ctx.createFloatRegisterBlock("floats", 16)

sorter = dflow.DgSorter.createShared(graphdata, ctx)
topo = sorter.generateTopology()

if topo is None:
    print("PASS: generateTopology() returned None for cyclic graph (expected)")
    coreappexit()
    sys.exit(0)
else:
    print("FAIL: generateTopology() returned a topology for a cyclic graph")
    print("      flattened: %s" % [m._name for m in topo._flattened])
    coreappexit()
    sys.exit(1)
