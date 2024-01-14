#!/usr/bin/env python3

from orkengine.core import *

coreappinit()

dflow = dataflow

#####################################################
# create graph
#####################################################

graphdata = dflow.GraphData.createShared()
a = graphdata.create("A",dflow.LambdaModule)
b = graphdata.create("B",dflow.LambdaModule)

print("graphdata.uuid:", graphdata.uuid)
print("graphdata.properties:", graphdata.properties.dict)
