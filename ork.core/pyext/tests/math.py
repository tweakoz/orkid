#!/usr/bin/env python3

from orkengine.core import *

a = Transform()
a.directMatrix = mtx4.lookAt(vec3(1,1,1),vec3(0,0,0),vec3(0,1,0))

print("a: ", a)
print("a.composed: " , a.composed )
