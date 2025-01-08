#!/usr/bin/env ork.python

from orkengine.core import vec2, vec3, vec4, mtx3, mtx4
from orkengine.core import CrcStringProxy, coreappinit
from orkengine.core import opencl as cl

coreappinit()
tokens = CrcStringProxy()

print("OpenCL TEST")

plat = cl.default_platform()

print(f"OpenCL platform: {plat.name}")

for dev in plat.devices:
  print(f"  OpenCL device: {dev.name}")
  
mydev = plat.devices[0]

props = mydev.properties.dumpToString()

ctx = mydev.context
print(f"dev0 context {ctx}")
print(f"dev0 props: {props}")


buf0 = ctx.createBuffer(tokens.READ_WRITE, 65536)

print(f"buf0: {buf0}")


