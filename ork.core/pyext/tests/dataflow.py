#!/usr/bin/env python3

from orkengine.core import *

coreappinit()

dflow = dataflow

a = dflow.LambdaModule.createShared()
b = dflow.LambdaModule.createShared()
print(a,b)

def onACompute(m,gi,updata):
  print("compute A: module<%s> gi<%s> upd<%s> impl<%s>" % (m,gi,updata,gi.impl))
def onBCompute(m,gi,updata):
  print("compute B: module<%s> gi<%s> upd<%s> impl<%s>" % (m,gi,updata,gi.impl))

a.setComputeLambda(onACompute)
b.setComputeLambda(onBCompute)

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

graphinst = graphdata.createGraphInst()
graphinst.updateTopology(topo)
graphinst.impl = 10.0

updata = UpdateData()
updata.absolutetime = 0
updata.deltatime = 0.1

for i in range(100):
  graphinst.compute(updata)
  updata.absolutetime = updata.absolutetime + updata.deltatime
