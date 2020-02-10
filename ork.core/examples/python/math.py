#!/usr/bin/env python3

from orkcore import *
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

C = mtx4()
C.compose(x,qx,1)

print("C: %s"%(C))

print( "%08x"%vec4(1,0,0,1).rgbaU32 )
