#!/usr/bin/env ork.python
"""
Headless validation of the instance-cull compute shader (the GPU frustum cull that feeds
DrawInstancedIndexedPrimitiveIndirectEML). Modeled on test_compute_basic.py.

The cull reads per-instance world matrices from an input SSBO, frustum-tests each instance's
bounding sphere (local center+radius transformed by the instance matrix, radius scaled by the
instance's max-axis scale) against a VP matrix, and STREAM-COMPACTS survivors into an output SSBO
via atomicAdd on a VkDrawIndexedIndirectCommand.instanceCount.

Validation uses an IDENTITY VP, which makes the frustum the clip box x,y in [-1,1], z in [0,1]
(Vulkan depth). We place instances at known translations and assert: (a) instanceCount equals the
number inside, and (b) the compacted survivor translations are exactly the inside set.
"""

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)
import struct
import numpy as np
from orkengine import core
from orkengine import lev2
from orkengine import ecs

tokens = core.CrcStringProxy()

MAXI = 65536                       # must match InstancedDrawable::k_max_instances
SZ_MTX = 64 * MAXI                 # mat4 region
SZ_COL = 16 * MAXI                 # vec4 region
IN_OUT_SIZE = SZ_MTX + SZ_COL      # input/output SSBO size (matrices + colors)
PAR_SIZE = 96                      # mat4 vp + vec4 bound + uint count + 3 pad
ARG_SIZE = 32                      # VkDrawIndexedIndirectCommand (20B) padded

# cull compute shader (shadlang). Kept in lockstep with the C++ drawable's embedded copy.
CULL_SHADER = """
fxconfig fxcfg_default {}
storage_interface sif_in (descriptor_set 0) {
  buffer layout(std430) bin { mat4 in_mtx[65536]; vec4 in_col[65536]; };
}
storage_interface sif_out (descriptor_set 0) {
  buffer layout(std430) bout { mat4 out_mtx[65536]; vec4 out_col[65536]; };
}
storage_interface sif_par (descriptor_set 0) {
  buffer layout(std430) bpar { mat4 u_vp; vec4 u_bound; uint u_count; uint u_p0; uint u_p1; uint u_p2; };
}
storage_interface sif_arg (descriptor_set 0) {
  buffer layout(std430) barg { uint a_indexCount; uint a_instanceCount; uint a_firstIndex; uint a_vertexOffset; uint a_firstInstance; };
}
compute_interface iface_cull {
  storage { sif_in sif_out sif_par sif_arg }
  inputs { layout(local_size_x = 64, local_size_y = 1, local_size_z = 1); }
}
compute_shader cs_cull : iface_cull {
  uint i = gl_GlobalInvocationID.x;
  if (i >= u_count) { return; }
  mat4 M = in_mtx[i];
  vec3 c = (M * vec4(u_bound.xyz, 1.0)).xyz;
  float r = u_bound.w * max(length(M[0].xyz), max(length(M[1].xyz), length(M[2].xyz)));
  vec4 rx = vec4(u_vp[0].x, u_vp[1].x, u_vp[2].x, u_vp[3].x);
  vec4 ry = vec4(u_vp[0].y, u_vp[1].y, u_vp[2].y, u_vp[3].y);
  vec4 rz = vec4(u_vp[0].z, u_vp[1].z, u_vp[2].z, u_vp[3].z);
  vec4 rw = vec4(u_vp[0].w, u_vp[1].w, u_vp[2].w, u_vp[3].w);
  vec4 pl0 = rw + rx; vec4 pl1 = rw - rx;
  vec4 pl2 = rw + ry; vec4 pl3 = rw - ry;
  vec4 pl4 = rz;      vec4 pl5 = rw - rz;
  bool inside = true;
  if ((dot(pl0.xyz, c) + pl0.w) < (-r * length(pl0.xyz))) { inside = false; }
  if ((dot(pl1.xyz, c) + pl1.w) < (-r * length(pl1.xyz))) { inside = false; }
  if ((dot(pl2.xyz, c) + pl2.w) < (-r * length(pl2.xyz))) { inside = false; }
  if ((dot(pl3.xyz, c) + pl3.w) < (-r * length(pl3.xyz))) { inside = false; }
  if ((dot(pl4.xyz, c) + pl4.w) < (-r * length(pl4.xyz))) { inside = false; }
  if ((dot(pl5.xyz, c) + pl5.w) < (-r * length(pl5.xyz))) { inside = false; }
  if (inside) {
    uint slot = atomicAdd(a_instanceCount, 1u);
    out_mtx[slot] = M;
    out_col[slot] = in_col[i];
  }
}
"""

# (translation, scale, expected_inside) — identity-VP frustum box is x,y in [-1,1], z in [0,1].
CASES = [
  ((0.0,  0.0,  0.5), 1.0, True),    # center
  ((0.5, -0.5,  0.2), 1.0, True),    # well inside
  ((-0.9, 0.9,  0.9), 1.0, True),    # near a corner, still inside
  ((0.3,  0.3,  0.4), 2.0, True),    # inside + scaled (exercises max-axis-scale radius path)
  ((2.0,  0.0,  0.5), 1.0, False),   # x > 1
  ((0.0, -2.0,  0.5), 1.0, False),   # y < -1
  ((0.0,  0.0,  1.5), 1.0, False),   # z > 1 (far)
  ((0.0,  0.0, -0.5), 1.0, False),   # z < 0 (near)
]
BOUND_RADIUS = 0.05


def _colmajor(tx, ty, tz, s):
  # column-major mat4 (glm/GLSL) for translation (tx,ty,tz) + uniform scale s.
  return [s,0,0,0,  0,s,0,0,  0,0,s,0,  tx,ty,tz,1]


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  fxi = ctx.FXI
  ci  = ctx.CI

  n = len(CASES)
  expected_inside = [c for c in CASES if c[2]]
  print(f"cull test: {n} instances, {len(expected_inside)} expected inside", flush=True)

  in_ssbo  = fxi.createShaderStorageBufferWithLength(IN_OUT_SIZE)
  out_ssbo = fxi.createShaderStorageBufferWithLength(IN_OUT_SIZE)
  par_ssbo = fxi.createShaderStorageBufferWithLength(PAR_SIZE)
  arg_ssbo = fxi.createShaderStorageBufferWithLength(ARG_SIZE)

  # input matrices (column-major) at offset 0 of the matrices region
  mats = np.array([_colmajor(*t, s) for (t, s, _ok) in CASES], dtype=np.float32).reshape(-1)
  fxi.copyDataIntoShaderStorageBuffer(mats, in_ssbo, 0)

  # cullparams: identity VP (16 floats) @0, bound (0,0,0,radius) @64, count (uint) @80
  vp = np.array([1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1], dtype=np.float32)
  fxi.copyDataIntoShaderStorageBuffer(vp, par_ssbo, 0)
  fxi.copyDataIntoShaderStorageBuffer(np.array([0,0,0,BOUND_RADIUS], dtype=np.float32), par_ssbo, 64)
  fxi.copyDataIntoShaderStorageBuffer(int(n), par_ssbo, 80)

  # args: seed indexCount (dummy) and reset instanceCount to 0 (atomicAdd accumulates)
  fxi.copyDataIntoShaderStorageBuffer(int(36), arg_ssbo, 0)
  fxi.copyDataIntoShaderStorageBuffer(int(0),  arg_ssbo, 4)

  shader = fxi.shaderFromShaderText("instance_cull", CULL_SHADER)
  cs     = fxi.computeShader(shader, "cs_cull")

  groups = (n + 63) // 64
  ci.beginDispatchPhase()
  ci.bindStorageBuffer(cs, 0, in_ssbo)
  ci.bindStorageBuffer(cs, 1, out_ssbo)
  ci.bindStorageBuffer(cs, 2, par_ssbo)
  ci.bindStorageBuffer(cs, 3, arg_ssbo)
  ci.dispatch(cs, groups, 1, 1)
  ci.endDispatchPhase()

  # read back instanceCount
  amap = fxi.mapStorageBuffer(arg_ssbo, 0, ARG_SIZE, tokens.READ_ONLY)
  inst_count = struct.unpack('I', amap.data[4:8])[0]
  fxi.unmapStorageBuffer(amap)

  # read back survivor translations (col3.xyz of each compacted matrix)
  omap = fxi.mapStorageBuffer(out_ssbo, 0, max(inst_count, 1) * 64, tokens.READ_ONLY)
  survivors = []
  for k in range(inst_count):
    m = struct.unpack('16f', omap.data[k*64:(k+1)*64])
    survivors.append((round(m[12], 3), round(m[13], 3), round(m[14], 3)))
  fxi.unmapStorageBuffer(omap)

  ezapp.mainThreadEnd()

  want_count = len(expected_inside)
  want_set   = set((round(t[0], 3), round(t[1], 3), round(t[2], 3)) for (t, s, _ok) in expected_inside)
  got_set    = set(survivors)

  print(f"  instanceCount: got {inst_count}, want {want_count}", flush=True)
  print(f"  survivors: {sorted(got_set)}", flush=True)
  print(f"  expected : {sorted(want_set)}", flush=True)

  ok = (inst_count == want_count) and (got_set == want_set)
  print(f"=== instance-cull compute {'PASSED' if ok else 'FAILED'} ===", flush=True)
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


main()
