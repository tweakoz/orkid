#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2026, Michael T. Mayers. MIT License.
################################################################################
# TASK + MESH shader grass field — the amplification pair, end to end, on screen.
#
# A 16x16 grid of ground tiles. ONE TASK workgroup runs per tile and decides, from
# that tile's distance to the eye, how many MESH workgroups the tile deserves:
#
#   inside lod0 radius -> 6 clusters   (48 blades)
#   inside lod1 radius -> 3 clusters   (24 blades)
#   inside cull radius -> 1 cluster    ( 8 blades)
#   beyond cull radius -> 0 clusters   (nothing dispatched at all)
#
# That per-tile count is the whole point: the emitted grid is DATA-DEPENDENT, so the
# task stage is doing real amplification work rather than forwarding a constant. Fly
# the camera and the density follows you; the field is a disc, not a square, because
# the corner tiles cancel themselves with a zero emit.
#
# The task stage hands its decision to the mesh stage through the shared payload
# (tile origin, cluster count, lod tint). The mesh stage builds blade geometry
# procedurally from it — there is no vertex buffer, no index buffer and no compute
# pass anywhere in this file. Wind comes from a time uniform, so it moves.
#
# Each blade yaw-billboards: it rotates about its own vertical axis so its face turns
# toward the eye, while staying rooted and upright. Wind bends the blade inside that
# billboarded frame.
#
# NOT SILENTLY DEGRADABLE: a device without taskShader, or an engine that dropped
# the task stage and ran the pass taskless, is a hard named failure here — never a
# quieter picture. The two guards are ctx.supports_task_shader at init and the
# context's task-stage draw counter after the first frames.
#
# Uses ComponentizedApplication + StandardSceneGraphComponent (the SGVP path), same
# as compute_drawable.py — the drawable's render hook only fires on that path.
################################################################################

import math
import sys
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

GRID     = 1024    # tiles per side -> GRID*GRID task workgroups, one per tile
TILE     = 0.25  # tile size in world units
BLADES   = 8     # blades per mesh workgroup == mesh local_size_x (structural: sizes max_vertices)
CLUSTERS = 6     # most mesh workgroups any one tile may ask for (structural: the LOD ceiling)

################################################################################
# Everything a user might tune rides in the uniform block, never baked into the
# shader text: field extents, the LOD radii, the cluster ceiling, wind. Only the
# two counts that SIZE the mesh stage's output (BLADES, and the 4 verts / 2 prims
# per blade) are structural, and those are loop/layout bounds.
################################################################################

SHADER = """
fxconfig fxcfg_default {}
////////////////////////////////////////
uniform_block ublock_grass (descriptor_set 0) {
  mat4 mvp;
  mat4 ivmtx;      // inverse view -> eye position, which is what the task stage LODs against
  vec4 field;      // x=tile size  y=grid dim   z=cull radius  w=time
  vec4 lod;        // x=lod0 dist  y=lod1 dist  z=cluster ceiling  w=wind amplitude
}
////////////////////////////////////////
libblock lib_grass {
  // cheap deterministic hash -> [0,1); the blade scatter must agree between LOD
  // levels, so blade N of a tile lands in the same spot however many clusters ran.
  float hash11(uint n) {
    n = (n ^ 61u) ^ (n >> 16u);
    n = n + (n << 3u);
    n = n ^ (n >> 4u);
    n = n * 668265261u;
    n = n ^ (n >> 15u);
    return float(n & 65535u) * 0.0000152587890625;
  }
  vec2 tileOrigin(uint tx, uint tz, float tile_size, float grid_dim) {
    float half_span = 0.5 * grid_dim * tile_size;
    return vec2(float(tx) * tile_size - half_span, float(tz) * tile_size - half_span);
  }
}
////////////////////////////////////////
// the task->mesh payload: declared ONCE, inherited by BOTH stages. The task stage
// writes it, the mesh stage reads it. 48 bytes, far inside the 16KB portable cap.
task_payload pld_grass {
  vec4 tile_origin;   // xy = tile origin in world XZ, z = tile size, w = wind phase
  vec4 tile_params;   // x = lod level, y = clusters emitted, z = tile id, w = unused
  vec4 tile_tint;     // lod tint, so the amplification decision is legible on screen
}
////////////////////////////////////////
task_interface iface_task : ublock_grass {
  inputs { layout(local_size_x = 1, local_size_y = 1, local_size_z = 1); }
}
// the mesh stage stands in for the vertex stage, so it carries the vertex interface
// (which is where its workgroup size and output-count layout live).
vertex_interface iface_mesh : ublock_grass {
  inputs  { layout(local_size_x = 8, local_size_y = 1, local_size_z = 1); }
  outputs {
    layout(triangles, max_vertices = 32, max_primitives = 16);
    vec4 frg_clr;
  }
}
fragment_interface iface_frg {
  inputs  { vec4 frg_clr; }
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
// TASK STAGE — one workgroup per tile. Decides the tile's mesh workgroup count.
task_shader ts_grass : extension(GL_EXT_mesh_shader) : iface_task : pld_grass : lib_grass {
  uint tx = gl_WorkGroupID.x;
  uint tz = gl_WorkGroupID.y;
  float tile_size = field.x;
  float grid_dim  = field.y;
  vec2 org = tileOrigin(tx, tz, tile_size, grid_dim);
  vec3 center = vec3(org.x + tile_size * 0.5, 0.0, org.y + tile_size * 0.5);
  vec3 eye = (ivmtx * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
  float dist = length(center - eye);

  uint clusters = 0u;
  float level = 3.0;
  vec4 tint = vec4(0.0, 0.0, 0.0, 1.0);
  
  if (dist < lod.x) {
    clusters = uint(lod.z);
    level = 0.0;
    tint = vec4(0.55, 1.00, 0.40, 1.0);
  } else if (dist < lod.y) {
    clusters = uint(lod.z) / 2u;
    level = 1.0;
    tint = vec4(0.40, 0.80, 0.35, 1.0);
  } else if (dist < field.z) {
    clusters = 1u;
    level = 2.0;
    tint = vec4(0.26, 0.52, 0.28, 1.0);
  }

  uint tile_id = tz * uint(grid_dim) + tx;
  pld_grass.tile_origin = vec4(org.x, org.y, tile_size, hash11(tile_id) * 6.2831853);
  pld_grass.tile_params = vec4(level, float(clusters), float(tile_id), 0.0);
  pld_grass.tile_tint   = tint;

  // the dispatch verb. clusters==0 cancels the tile outright — no mesh workgroup runs.
  EmitMeshTasksEXT(clusters, 1u, 1u);
}
////////////////////////////////////////
// MESH STAGE — one workgroup per cluster, one invocation per blade, 4 verts / 2 tris
// each. Every number it needs about WHERE it is comes from the payload.
mesh_shader ms_grass : extension(GL_EXT_mesh_shader) : iface_mesh : pld_grass : lib_grass {
  SetMeshOutputsEXT(32u, 16u);

  uint cluster = gl_WorkGroupID.x;
  uint b       = gl_LocalInvocationID.x;
  uint blade   = cluster * 8u + b;

  float tile_size = pld_grass.tile_origin.z;
  uint tile_id    = uint(pld_grass.tile_params.z);
  uint seed       = tile_id * 977u + blade;

  float fx = hash11(seed);
  float fz = hash11(seed + 7919u);
  float bx = pld_grass.tile_origin.x + fx * tile_size;
  float bz = pld_grass.tile_origin.y + fz * tile_size;

  float height = 0.18 + hash11(seed + 104729u) * 0.24;
  float wide   = 0.016 + hash11(seed + 15485863u) * 0.012;
  float lean   = hash11(seed + 32452843u) * 6.2831853;

  vec3 base = vec3(bx, 0.0, bz);

  // BILLBOARD, yaw only: the blade spins about its OWN vertical axis to face the eye.
  // World up stays the blade's up axis — a spherical billboard would tip the blades
  // over as the camera climbs, and grass that lies down is wrong at any altitude.
  // The width axis is cross(world_up, flattened toEye), which is already unit length
  // when the flattened vector is.
  vec3 eye = (ivmtx * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
  vec2 to_eye = vec2(eye.x - bx, eye.z - bz);
  float to_eye_len = length(to_eye);
  // camera directly overhead: the flattened vector collapses and normalize() would
  // hand back NaN for every blade under it. Fall back to this blade's own hash angle,
  // which is fixed frame to frame, so the field holds still instead of vanishing.
  vec2 facing = (to_eye_len > 1.0e-5) ? (to_eye / to_eye_len) : vec2(cos(lean), sin(lean));
  vec3 side_dir = vec3(facing.y, 0.0, -facing.x);

  // wind: the tip swings, the base does not. The gust direction stays in WORLD space so
  // neighbouring blades agree, but the tip is displaced inside the billboarded frame:
  // only the gust's in-plane component slides the tip along the width axis, and the
  // out-of-plane component becomes tip drop rather than skewing the ribbon out of the
  // frame it was just built in. Drop uses the full bend, so it does not change as the
  // camera orbits — a height that breathed with view angle would shimmer.
  float phase = field.w * 0.17 + pld_grass.tile_origin.w + bx * 0.6 + bz * 0.4;
  float bend  = sin(phase) * lod.w + 0.06;
  vec2 gust   = vec2(cos(lean), sin(lean));
  float in_plane = gust.x * side_dir.x + gust.y * side_dir.z;
  float drop     = bend * bend * 0.5 / height;

  vec3 tip  = base + vec3(0.0, height - drop, 0.0) + side_dir * (bend * in_plane);
  vec3 side = side_dir * wide;

  uint v = b * 4u;
  gl_MeshVerticesEXT[v + 0u].gl_Position = mvp * vec4(base - side, 1.0);
  gl_MeshVerticesEXT[v + 1u].gl_Position = mvp * vec4(base + side, 1.0);
  gl_MeshVerticesEXT[v + 2u].gl_Position = mvp * vec4(tip - side * 0.25, 1.0);
  gl_MeshVerticesEXT[v + 3u].gl_Position = mvp * vec4(tip + side * 0.25, 1.0);

  vec4 root_clr = pld_grass.tile_tint * 0.35;
  vec4 tip_clr  = pld_grass.tile_tint;
  frg_clr[v + 0u] = root_clr;
  frg_clr[v + 1u] = root_clr;
  frg_clr[v + 2u] = tip_clr;
  frg_clr[v + 3u] = tip_clr;

  uint p = b * 2u;
  gl_PrimitiveTriangleIndicesEXT[p + 0u] = uvec3(v + 0u, v + 1u, v + 2u);
  gl_PrimitiveTriangleIndicesEXT[p + 1u] = uvec3(v + 2u, v + 1u, v + 3u);
}
////////////////////////////////////////
fragment_shader ps_grass : iface_frg {
  out_clr = frg_clr;
}
state_block sb_grass : default {
  CullTest  = OFF;
  DepthTest = LEQUALS;
  BlendMode = OFF;
  DepthMask = ON;
}
////////////////////////////////////////
technique tek_grass {
  fxconfig = fxcfg_default;
  pass p0 {
    task_shader = ts_grass;
    mesh_shader = ms_grass;
    fragment_shader = ps_grass;
    state_block = sb_grass;
  }
}
"""

################################################################################

class GrassApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.SGC = self.addComponent(
        "std_scenegraph",
        StandardSceneGraphComponent,
        eye=vec3(0, 2.2, 7),
        tgt=vec3(0, 0.3, 0),
        up=vec3(0, 1, 0),
        msaa=2,
        grid_variant=None)
    self.time = 0.0
    self.checked = False
    self.frames = 0
    self.createEzApp()

  def _onGpuInit(self, ctx):
    self.scene     = self.SGC.scenegraph
    self.layer_fwd = self.SGC.layer_fwd
    self.ctx       = ctx

    # HARD GATE, by name. No taskless fallback: the whole example is the task stage.
    if not ctx.supports_task_shader:
      raise RuntimeError(
          "GRASS-TASKMESH-UNAVAILABLE: this device does not advertise the VK_EXT_mesh_shader "
          "taskShader feature (ctx.supports_task_shader is False). The grass field is built by a "
          "task+mesh pass and has no taskless form — refusing to draw a lesser picture.")

    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx, "grass_taskmesh", SHADER)
    mtl.rasterstate.culltest  = tokens.OFF   # blades are two-sided ribbons
    mtl.rasterstate.depthtest = tokens.LEQUALS

    permu = lev2.FxPipelinePermutation(rendermodel="ForwardPBR")
    permu.technique = mtl.shader.technique("tek_grass")
    assert permu.technique, "technique tek_grass not found"
    pipe = mtl.fxcache.findPipeline(permu)
    pipe.bindParam(mtl.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
    pipe.bindParam(mtl.param("ivmtx"), tokens.RCFD_Camera_IV_Mono)
    pipe.sharedMaterial = mtl
    self.pipe  = pipe
    self.mtl   = mtl
    self.p_fld = mtl.param("field")
    self.p_lod = mtl.param("lod")
    self._writeUniforms()

    gdd = lev2.ComputeDrawableData()
    gdd.pipeline = pipe
    # the task grid: one workgroup per tile. With a task stage in the pass these are
    # TASK workgroup counts, and each one decides its own mesh workgroup count.
    gdd.setMeshDraw(permu.technique, GRID, GRID, 1)
    self.node = self.layer_fwd.createDrawableNodeFromData("grass", gdd)

    self.scene.lightingmanager.gpuInit(ctx)
    print("grass_taskmesh: %dx%d tiles -> task stage -> up to %d mesh workgroups each" %
          (GRID, GRID, CLUSTERS), flush=True)

  def _writeUniforms(self):
    cull = 0.62 * GRID * TILE
    self.pipe.bindParam(self.p_fld, vec4(TILE, float(GRID), cull, self.time))
    self.pipe.bindParam(self.p_lod, vec4(0.30 * cull, 0.60 * cull, float(CLUSTERS), 0.11))

  def _onUpdate(self, updinfo):
    self.time = updinfo.absolutetime
    self.SGC.scenegraph.updateScene(self.SGC.cameralut)

  def _onGpuUpdate(self, ctx):
    self._writeUniforms()
    # EVIDENCE, not vibes: the context counts mesh draws that were issued with a task
    # stage actually bound to the pipeline. Zero after real frames means the pass ran
    # taskless behind our back — say so by name and stop.
    self.frames += 1
    if (not self.checked) and self.frames >= 8:
      self.checked = True
      n = ctx.task_shader_draws
      if n == 0:
        raise RuntimeError(
            "GRASS-TASKMESH-NOT-ENGAGED: %d frames drawn but ctx.task_shader_draws is 0 — the "
            "pipeline ran WITHOUT a task stage. The picture may look plausible; it is not the "
            "thing under test." % self.frames)
      print("grass_taskmesh: task stage ENGAGED — %d task+mesh draws in %d frames" %
            (n, self.frames), flush=True)

  def _onUiEvent(self, uievent):
    self.SGC._onCameraUiEvent(uievent)
    return lev2.ui.HandlerResult()

################################################################################

if __name__ == "__main__":
  app = GrassApp()
  app.ezapp.mainThreadLoop()
