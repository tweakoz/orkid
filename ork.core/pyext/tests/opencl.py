#!/usr/bin/env ork.python

from orkengine.core import vec2, vec3, vec4, mtx3, mtx4
from orkengine.core import coreappinit, CrcStringProxy, DataBlock
from orkengine.core import opencl as cl

coreappinit()

KERNEL_SOURCE = """
#pragma OPENCL EXTENSION all : enable
__kernel void add_one(__global float* data)
{
    // Compute the global index of this work-item
    int i = get_global_id(0);

    // Add 1.0 to the current element
    data[i] += 1.0f;
}
"""

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

krn = ctx.createKernelFromString("kern",KERNEL_SOURCE)
print(f"krn: {krn}")

buf0 = ctx.createBuffer(tokens.READ_WRITE, 65536)

dblock = DataBlock.createWithSize(65536)
dblock_bytes = dblock.mutable_bytes
dblock_bytes[5] = 12
buf1 = ctx.createBufferWithDataBlock( datablock=dblock,
                                      access=tokens.READ_WRITE, 
                                      memconfig=tokens.MAP_TO_HOST_PTR,
                                      )

print(f"buf0: {buf0}")
print(f"buf1: {buf1}")


