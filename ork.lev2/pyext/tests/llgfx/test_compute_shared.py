#!/usr/bin/env ork.python
"""
Test for shared memory and barrier() in Vulkan compute shaders.
Verifies cross-thread communication via shared memory after barrier synchronization.
"""

import sys
import struct
from orkengine import core
from orkengine import lev2

tokens = core.CrcStringProxy()

COMPUTE_SHADER_TEXT = """
storage_interface sif_output (descriptor_set 0) {
  buffer layout(std430) output_data {
    uint values[256];
  };
}
compute_interface iface_compute {
  storage { sif_output }
  inputs {
    layout(local_size_x = 256, local_size_y = 1, local_size_z = 1);
  }
}
compute_shader cs_shared_test : iface_compute {
  shared uint shared_data[256];
  
  uint lID = gl_LocalInvocationID.x;
  shared_data[lID] = lID;
  barrier();
  
  // Read neighbor's value (with wrap)
  uint neighbor = shared_data[(lID + 1u) % 256u];
  values[lID] = neighbor;
}
"""

def main():
    print("="*60, flush=True)
    print("Starting compute shader basic test", flush=True)
    print("="*60, flush=True)

    # Initialize lev2 app with offscreen graphics context
    print("Initializing lev2 app...", flush=True)
    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()

    print(f"graphics context: {ctx}")
    fxi = ctx.FXI
    ci = ctx.CI

    print("Starting main thread...")
    ezapp.mainThreadBegin()

    # Create SSBO for compute output (256 floats = 1024 bytes)
    num_values = 256
    ssbo_size = num_values * 4  # 4 bytes per float
    print(f"Creating SSBO with size {ssbo_size} bytes...")
    ssbo = fxi.createShaderStorageBufferWithLength(ssbo_size)
    print(f"Created SSBO: {ssbo}")

    # Load the compute shader
    print("Loading compute shader...")
    shader = fxi.shaderFromShaderText("test_compute", COMPUTE_SHADER_TEXT)
    print(f"Loaded shader: {shader}")

    # Get the compute shader object
    compute_shader = fxi.computeShader(shader, "cs_shared_test")
    print(f"Got compute shader: {compute_shader}")

    # Begin frame (required for command buffer)
    print("Beginning frame...")
    ctx.beginFrame()

    # Bind the SSBO and dispatch compute shader
    print("Dispatching compute shader with 1 work group...")
    ci.beginDispatchPhase()
    ci.bindStorageBuffer(compute_shader, 0, ssbo)
    ci.dispatch(compute_shader, 1, 1, 1)
    ci.endDispatchPhase()

    # End frame to submit command buffer
    print("Ending frame...")
    ctx.endFrame()

    # Process to ensure GPU work completes
    print("Processing frame...")
    ezapp.mainThreadIter()

    # Map SSBO and read back results
    print("Reading back SSBO data...", flush=True)
    mapping = fxi.mapStorageBuffer(ssbo, 0, ssbo_size, tokens.READ_ONLY)
    print(f"  mapping: {mapping}", flush=True)
    print(f"  mapping.length: {mapping.length}", flush=True)
    print(f"  mapping.data length: {len(mapping.data)}", flush=True)

    # Read the uint values
    results = []
    for i in range(num_values):
        offset = i * 4
        value = struct.unpack('I', mapping.data[offset:offset+4])[0]
        results.append(value)

    print(f"  First 10 values: {results[:10]}", flush=True)

    fxi.unmapStorageBuffer(mapping)

    # Verify results: each value should be neighbor's ID = (i+1) % 256
    print("Verifying results...")
    errors = 0
    for i in range(num_values):
        expected = (i + 1) % 256
        actual = results[i]
        if actual != expected:
            print(f"  ERROR at index {i}: expected {expected}, got {actual}")
            errors += 1
        elif i < 5 or i >= num_values - 5:
            # Print first and last few values for verification
            print(f"  [OK] index {i}: {actual} (expected {expected})")

    if errors == 0:
        print(f"All {num_values} values verified correctly!")
    else:
        print(f"FAILED: {errors} values were incorrect")

    print("Ending main thread...")
    ezapp.mainThreadEnd()

    print("="*60)
    if errors == 0:
        print("Compute shader test PASSED!")
    else:
        print("Compute shader test FAILED!")
    print("="*60)

    return 0 if errors == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
