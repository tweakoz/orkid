#!/usr/bin/env python3

from orkengine.core import *

coreappinit()

dflow = dataflow

a = dflow.DgModuleData.createShared()
b = dflow.DgModuleData.createShared()
print(a,b)
aout = a.createUniformFloatOutputPlug("outputX")
binp = b.createUniformFloatXfInputPlug("inputX")
print(aout)
print(binp)
graphdata = dflow.GraphData.createShared()
print(graphdata)

graphdata.addModule(a,"A")
graphdata.addModule(b,"B")
graphdata.safeConnect(binp,aout)

print( "a.mindepth: %d" % a.mindepth )
print( "a.maxdepth: %d" % a.maxdepth )
print( "b.mindepth: %d" % b.mindepth )
print( "b.maxdepth: %d" % b.maxdepth )

ctx = dflow.DgContext.createShared()

ctx.createFloatRegisterBlock("floats",16)
ctx.createVec3RegisterBlock("fvec3s",16)

sorter = dflow.DgSorter.createShared(graphdata,ctx)
topo = sorter.generateTopology()

graphinst = dflow.GraphInst.createShared(graphdata)
graphinst.updateTopology(topo)

updata = UpdateData()
updata.absolutetime = 0
updata.deltatime = 0.1

for i in range(100):
  graphinst.compute(updata)
  updata.absolutetime = updata.absolutetime + updata.deltatime
