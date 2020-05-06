#!/usr/bin/env python3

import math, random
import numpy as np
from scipy import linalg as la
from numba import vectorize
from orkengine.core import *

PI = 3.14159

a = vec2(0,1)
b = vec2(1,0)
c = a+b
d = a*b
print("a: %s"%a)
print("b: %s"%b)
print("c: %s"%c)
print("d: %s"%d)

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

R = mtx4.rotMatrix(vec3(0,1,0),PI)
print("R: %s"%R)
print("inverse(R): %s"%(R.inverse()))
print("R*inverse(R): %s"%(R*R.inverse()))
print("inverse(R)*R: %s"%(R.inverse()*R))
T = mtx4.transMatrix(1,2,3)
print("T: %s"%T)
print("inverse(T): %s"%(T.inverse()))
S = mtx4.scaleMatrix(1,2,3)
print("S: %s"%S)
print("inverse(S): %s"%(S.inverse()))

print("RST: %s"%(R*S*T))
print("TSR: %s"%(T*S*R))

C = mtx4.composed(x,qx,1)

print("C: %s"%(C))

print("R2RST: %s"%(mtx4.deltaMatrix(R,R*S*T)))

print( "%08x"%vec4(1,0,0,1).rgbaU32 )

############################################################
# NumPy integration test
############################################################

as_numpi = np.zeros((8,4,4)) # 8 4x4 matrices
print("/////////////////////////")
print("numpi shape: %s"%str(as_numpi.shape))
a = mtx4()
a.compose(vec3(0,1,0),quat(),2)
as_numpi[0] = a
print("/////////////////////////")
print("a: %s"%a)
print("numpi[0]: %s"%as_numpi[0])
print("linalg determinant of a: %s"%la.det(a))
print("linalg determinant of numpi[0]: %s"%la.det(as_numpi[0]))
eiga, eigv = la.eig(a)
print("linalg eiga<%s>"%eiga)
print("linalg eigv<%s>"%eigv)
print("/////////////////////////")
print(as_numpi[0])
print("/////////////////////////")
print(as_numpi[1])
print("/////////////////////////")
print(as_numpi[7])
print("/////////////////////////")
print(as_numpi[7]*as_numpi[4])

############################################################
