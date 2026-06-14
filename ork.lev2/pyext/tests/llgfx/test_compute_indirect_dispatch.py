#!/usr/bin/env python3
###############################################################################
# C.3 gate — dispatchComputeIndirect: GPU-driven group counts, zero readback.
# One dispatch phase: cs_seed writes COUNT=777 GPU-side -> cs_writeargs derives the
# VkDispatchIndirectCommand {ceil(777/64),1,1} from it -> dispatchIndirect(cs_mark)
# launches off those GPU-written args and marks OUT[i]=i+1 for i<COUNT.
# The host NEVER knows the count; if the indirect dispatch is broken (stub/0 groups),
# zero elements get marked. Asserts exactly 777 marked, correct values, clean tail.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, struct
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs    # headless_appinit (the legacy lev2appinit ad-hoc path segfaults)

tokens = core.CrcStringProxy()

SHADER = r"""
fxconfig fxcfg_default {}
storage_interface if_args (descriptor_set 0) { buffer layout(std430) ab { uint gx; uint gy; uint gz; uint a3; }; }
storage_interface if_data (descriptor_set 0) { buffer layout(std430) db { uint COUNT; uint OUT[]; }; }
compute_interface iface { storage { if_args if_data } inputs { layout(local_size_x = 64); } }
compute_shader cs_seed : iface {                     // the GPU-side count (host never sees it)
  if (gl_GlobalInvocationID.x != 0u) { return; }
  COUNT = 777u;
}
compute_shader cs_writeargs : iface {                // derive the dispatch command FROM GPU data
  if (gl_GlobalInvocationID.x != 0u) { return; }
  gx = (COUNT + 63u) / 64u;
  gy = 1u;
  gz = 1u;
  a3 = 0u;
}
compute_shader cs_mark : iface {                     // sized by the indirect dispatch
  uint i = gl_GlobalInvocationID.x;
  if (i >= COUNT) { return; }
  OUT[i] = i + 1u;
}
"""

EXPECT = 777


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  fxi, ci = ctx.FXI, ctx.CI
  ok = False
  try:
    data_size = 4 + 1024 * 4                       # COUNT + OUT[1024] (777 used, tail must stay 0)
    args = fxi.createShaderStorageBufferWithLength(16)
    data = fxi.createShaderStorageBufferWithLength(data_size)

    # zero the data buffer so the tail check is meaningful
    m = fxi.mapStorageBuffer(data, 0, data_size, tokens.WRITE_ONLY)
    m.writeBytes(b"\x00" * data_size)
    fxi.unmapStorageBuffer(m)

    sh = fxi.shaderFromShaderText("test_indirect_dispatch", SHADER)
    cs_seed = fxi.computeShader(sh, "cs_seed")
    cs_args = fxi.computeShader(sh, "cs_writeargs")
    cs_mark = fxi.computeShader(sh, "cs_mark")

    ci.beginDispatchPhase()                        # standalone phase: own command buffer, submit+WAIT
    for cs in (cs_seed, cs_args, cs_mark):
      ci.bindStorageBuffer(cs, 0, args)
      ci.bindStorageBuffer(cs, 1, data)
    ci.dispatch(cs_seed, 1, 1, 1)
    ci.storageBarrier()
    ci.dispatch(cs_args, 1, 1, 1)
    ci.storageBarrier()
    ci.dispatchIndirect(cs_mark, args, 0)          # group counts from the GPU-written command
    ci.endDispatchPhase()

    m = fxi.mapStorageBuffer(data, 0, data_size, tokens.READ_ONLY)
    vals = struct.unpack("%dI" % (data_size // 4), m.data)
    fxi.unmapStorageBuffer(m)
    count, out = vals[0], vals[1:]
    assert count == EXPECT, "COUNT readback %d != %d" % (count, EXPECT)
    marked = [i for i in range(1024) if out[i] != 0]
    assert len(marked) == EXPECT, "marked %d elements, expected %d (indirect dispatch sized wrong)" % (len(marked), EXPECT)
    for i in range(EXPECT):
      assert out[i] == i + 1, "OUT[%d] == %d, expected %d" % (i, out[i], i + 1)
    print("INDIRECT_DISPATCH=PASS (%d elements marked from GPU-written args)" % EXPECT, flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== compute indirect-dispatch gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
