#!/usr/bin/env python3

from orkcore import *
import math
PI = 3.14159

coreappinit()
x = vec3(1,0,0)
y = vec3(0,1,0)
print("x: %s" % x)
print("y: %s" % y)
print("x.cross(y): %s" % x.cross(y))

qx = quat(x,PI)
print("qx: %s"%qx)
mx = mtx3(qx)
print("mx: %s"%mx)
