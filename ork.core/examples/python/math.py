#!/usr/bin/env python3

from orkcore import *
import math
PI = 3.14159

#coreappinit()
x = vec3(1,0,0)
y = vec3(0,1,0)
print("x: %s" % x)
print("y: %s" % y)
print("x.cross(y): %s" % x.cross(y))

qx = quat(x,PI)
print("qx: %s"%qx)
mx3 = mtx3(qx)
print("mx3: %s"%mx3)
mx4 = mtx4(qx)
print("mx4: %s"%mx4)
print("mx4*mx4: %s"%(mx4*mx4))
print("mx4*mx4*mx4: %s"%(mx4*mx4*mx4))

V = mtx4.lookAt(vec3(0,1,-1),vec3(0,0,0),vec3(0,1,0))
P = mtx4.perspective(45,1,.01,10.0)
print("V: %s"%V)
print("P: %s"%P)
print("V*P: %s"%(V*P))

S = mtx4.scaleMatrix(1,2,3)
print("S: %s"%S)
print("V*S: %s"%(V*S))

print( "%08x"%vec4(1,0,0,1).rgbaU32 )
