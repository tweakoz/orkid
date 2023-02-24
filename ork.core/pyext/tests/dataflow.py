#!/usr/bin/env python3

from orkengine.core import *

coreappinit()

dflow = dataflow

#####################################################
# create graph
#####################################################

graphdata = dflow.GraphData.createShared()

#####################################################
# create/define modules
#####################################################

a = dflow.LambdaModule.createShared()
b = dflow.LambdaModule.createShared()

def onACompute(m,gi,updata):
  print("compute A: module<%s> gi<%s> upd<%s> impl<%s>" % (m,gi,updata,gi.impl))
def onBCompute(m,gi,updata):
  print("compute B: module<%s> gi<%s> upd<%s> impl<%s>" % (m,gi,updata,gi.impl))

a.onCompute(onACompute)
b.onCompute(onBCompute)
a.onLink(lambda m,gi : print("link A: module<%s> gi<%s> impl<%s>" % (m,gi,gi.impl)) )
b.onLink(lambda m,gi : print("link B: module<%s> gi<%s> impl<%s>" % (m,gi,gi.impl)) )

#####################################################
# add modules to graph
#####################################################

graphdata.addModule(a,"A")
graphdata.addModule(b,"B")

#####################################################
# make connections between modules
#####################################################

aout = a.createUniformFloatOutputPlug("outputX")
binp = b.createUniformFloatXfInputPlug("inputX")
graphdata.safeConnect(binp,aout)

#####################################################
# generate execution topology
#####################################################

ctx = dflow.DgContext.createShared()
ctx.createFloatRegisterBlock("floats",16)
ctx.createVec3RegisterBlock("fvec3s",16)
sorter = dflow.DgSorter.createShared(graphdata,ctx)
topo = sorter.generateTopology()

#####################################################
# instantiate graph, bind topology
#####################################################

graphinst = graphdata.createGraphInst()
graphinst.impl = 10.0 # graphinst user defined data, should be able to set to any type
graphinst.bindTopology(topo)

#####################################################
# execute graph
#####################################################

updata = UpdateData()
updata.absolutetime = 0
updata.deltatime = 0.1

for i in range(10):
  graphinst.compute(updata)
  updata.absolutetime = updata.absolutetime + updata.deltatime
