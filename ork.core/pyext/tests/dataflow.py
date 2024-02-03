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

# "A" via python subclass of c++ class trampoline (PyLambdaModule)
# TODO trampoline handling of module_instance->self 
# https://github.com/pybind/pybind11/issues/1145
# https://github.com/pybind/pybind11/issues/1333

class ModuleA(dflow.PyLambdaModule):
  def onLink(module_instance,gi):
    print("Linking A! <%s>" % module_instance)
  def onCompute(module_instance,gi,updata):
    print("compute A: module<%s> gi<%s> upd<%s> impl<%s>" % (module_instance,gi,updata,gi.impl))

a = graphdata.create("A",ModuleA)

#####################################################

# "B" via raw methods

b = graphdata.create("B",dflow.LambdaModule)

def onBCompute(m,gi,updata):
  print("compute B: module<%s> gi<%s> upd<%s> impl<%s>" % (m,gi,updata,gi.impl))

b.onCompute(onBCompute)
b.onLink(lambda m,gi : print("link B: module<%s> gi<%s> impl<%s>" % (m,gi,gi.impl)) )

#####################################################
# make connections between modules
#####################################################

aout = a.createUniformFloatOutputPlug("outputX")
binp = b.createUniformFloatXfInputPlug("inputX")
graphdata.connect(binp,aout)


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
