#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2026, Michael T. Mayers. MIT License.
################################################################################
# TASK + MESH shader asteroid belt — amplification at an obscene count, on screen.
#
# A 1152 x 16 sector grid over an annulus. ONE TASK workgroup runs per sector and
# decides, from that sector's bounding sphere, how many MESH workgroups it deserves —
# one mesh workgroup per asteroid:
#
#   sector off screen / past cull -> 0  workgroups  (nothing dispatched at all)
#   inside lod0 radius            -> 24 workgroups  @ 12x5 rock  (120 tris each)
#   inside lod1 radius            -> 24 workgroups  @  8x4 rock  ( 64 tris each)
#   inside lod2 radius            -> 12 workgroups  @  5x3 rock  ( 30 tris each)
#   beyond, still inside cull     ->  6 workgroups  @  5x3 rock
#
# The ceiling is 18432 task workgroups -> 442368 asteroids -> 53.1M triangles. Nothing
# on the CPU side knows or cares which of those actually run: the whole decision lives
# in the task stage, and the emitted grid is DATA-DEPENDENT twice over — by distance
# (LOD + density) and by FRUSTUM (a sector behind you emits nothing). That second one
# is the canonical amplification win and it is the reason the count is affordable.
#
# There is no vertex buffer, no index buffer, no instance buffer and no compute pass
# anywhere in this file. Every asteroid's orbit, size, spin, lumpy silhouette and
# shading is a pure function of (sector, rock index) evaluated on the GPU. The only
# per-frame CPU work is five vec4 writes.
#
# WHY THE BELT SHEARS BUT NEVER LEAKS: orbital rate is Keplerian-ish (~r^-1.5) and is
# evaluated ONCE PER RADIAL BAND, not per rock. A band therefore turns rigidly, so a
# rock can never drift out of the sector whose bounding sphere the task stage culled
# against — while band-against-band still shears visibly across the belt, which is the
# effect worth having. A per-rock rate would look the same for about ten seconds and
# then start deleting asteroids that wandered outside their own bound.
#
# NOT SILENTLY DEGRADABLE: a device without taskShader, or an engine that dropped the
# task stage and ran the pass taskless, is a hard named failure here — never a quieter
# picture. The two guards are ctx.supports_task_shader at init and the context's
# task-stage draw counter after the first frames. Both were seen to pass on this
# checkout's MoltenVK (supports_task_shader=True, task_shader_draws>0); a device that
# answers otherwise gets the named refusal, not a fallback.
#
# Uses ComponentizedApplication + StandardSceneGraphComponent (the SGVP path), same as
# grass_taskmesh.py — the drawable's render hook only fires on that path.
#
# KEYS:  T  toggle LOD tint (paint each asteroid by the LOD its sector was assigned)
#        [  tighten the LOD/cull radii    ]  loosen them
################################################################################

import math
import sys
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

################################################################################
# The dispatch shape. These three are the ONLY numbers that decide how obscene this
# gets — everything downstream of them is the task stage's business.
################################################################################

# Total asteroids = SECTORS_ANGULAR * SECTORS_RADIAL * ROCKS_PER_SECTOR. To scale the
# count, reach for SECTORS_ANGULAR first: it multiplies the population AND narrows each
# wedge, so the task stage's bounding spheres get tighter and the frustum cull gets more
# precise. Raising ROCKS_PER_SECTOR instead buys the same population against unchanged
# (coarser) bounds — more asteroids per cull decision, not more cull decisions.
SECTORS_ANGULAR = 250   # task workgroups in x — wedges around the belt
SECTORS_RADIAL  = 128     # task workgroups in y — bands across the belt
ROCKS_PER_SECTOR = 512    # the LOD ceiling: most mesh workgroups any one sector may ask for

# Belt geometry, in world units.
BELT_INNER   = 32.0
BAND_WIDTH   = 9.0
BELT_THICK   = 45.0       # half-thickness; the belt is a disc, not a slab
BELT_OUTER   = BELT_INNER + SECTORS_RADIAL * BAND_WIDTH

# Orbit rate scale. Angular velocity is BELT_SPIN * r^-1.5, so this sets the whole belt's
# tempo while leaving the Keplerian falloff (and therefore the band-against-band shear)
# intact. At the mid-belt radius one revolution takes about
#   2*pi / (BELT_SPIN * r^-1.5)  seconds — ~200s here. Lower is slower.
BELT_SPIN    = 30.0

ROCK_SCALE_MIN = 0.06
ROCK_SCALE_MAX = 0.90
ROCK_FIELD_MAX = 1.63    # max of the 3-lobe radius field (1 + .340 + .187 + .103);
                         #  the task stage's sector bound must include a rock this big
                         #  sitting exactly on the sector boundary, or edge rocks pop.
ROCK_SPIN      = 2.9

# LOD/cull radii, in world units from the eye to the sector center. CULL_DIST must clear
# the far arc (eye radius + BELT_OUTER) or the belt visibly ends in mid-air; the [ and ]
# keys scale all four at runtime, which is the fastest way to SEE the task stage working.
LOD0_DIST = 40.0
LOD1_DIST = 110.0
LOD2_DIST = 260.0
CULL_DIST = 900.0

# Structural — these SIZE the mesh stage's output layout and must match the LOD0 row
# of lodLon/lodLat in the shader. Everything else a user might tune rides in the
# uniform block and is never baked into the shader text.
LOD0_LON, LOD0_LAT = 12, 5
MAX_VERTS = (LOD0_LAT + 1) * LOD0_LON      # 72
MAX_PRIMS = LOD0_LAT * LOD0_LON * 2        # 120
MESH_LOCAL_SIZE = 32                       # invocations per mesh workgroup; the emit
                                           #  loops stride by it, so it is free to sit
                                           #  below MAX_VERTS / MAX_PRIMS.

SUN_DIR = vec3(0.42, 0.55, -0.72)

SHADER = """
fxconfig fxcfg_default {}
////////////////////////////////////////
uniform_block ublock_rocks (descriptor_set 0) {
  mat4 mvp;
  mat4 ivmtx;      // inverse view -> eye position, which is what the task stage LODs against
  vec4 belt;       // x=inner radius  y=band width  z=half thickness  w=time
  vec4 grid;       // x=angular sectors  y=radial bands  z=rocks/sector  w=belt spin scale
  vec4 rock;       // x=min scale  y=max scale  z=field max  w=spin rate
  vec4 lodr;       // x=lod0 dist  y=lod1 dist  z=lod2 dist  w=cull dist
  vec4 sun;        // xyz = sun direction (unit, world)  w = lod tint mix [0..1]
}
////////////////////////////////////////
libblock lib_rocks {

  // cheap deterministic hash -> [0,1). Every asteroid in the belt is addressed by
  // (sector, rock index) through this and nothing else — no buffer, no upload.
  float hash11(uint n) {
    n = (n ^ 61u) ^ (n >> 16u);
    n = n + (n << 3u);
    n = n ^ (n >> 4u);
    n = n * 668265261u;
    n = n ^ (n >> 15u);
    return float(n & 65535u) * 0.0000152587890625;
  }
  vec3 hash31(uint n) {
    return vec3(hash11(n), hash11(n ^ 2166136261u), hash11(n ^ 374761393u));
  }
  // a hash direction that can never be the zero vector (normalize(0) is NaN, and one
  // NaN axis turns a whole asteroid into a screen-wide garbage triangle).
  vec3 hashDir(uint n) {
    return normalize(hash31(n) * 2.0 - 1.0 + vec3(1.0e-3, 2.0e-3, 3.0e-3));
  }

  //////////////////////////////////////
  // ORBIT — evaluated PER BAND, so a band turns rigidly and no rock ever leaves the
  // sector the task stage bounded. Rate falls off ~r^-1.5 (Keplerian), which is what
  // makes the belt shear band-against-band instead of spinning like a solid plate.
  //////////////////////////////////////
  float bandMidRadius(uint band, float r0, float bw) {
    return r0 + (float(band) + 0.5) * bw;
  }
  float bandSpin(uint band, float r0, float bw, float t, float scale) {
    float r = max(bandMidRadius(band, r0, bw), 1.0);
    return t * scale * pow(r, -1.5);
  }

  //////////////////////////////////////
  // PLACEMENT — rock k of sector (sa, band). Returns xyz = world position, w = scale.
  //////////////////////////////////////
  vec4 rockPose(uint seed, uint sa, uint band,
                float sa_count, float r0, float bw, float thick, float spin,
                float smin, float smax) {
    vec3 h = hash31(seed);
    float ang = (float(sa) + h.x) / sa_count * 6.2831853 + spin;
    float rad = r0 + (float(band) + h.y) * bw;
    float yy  = hash11(seed + 22801763u) * 2.0 - 1.0;
    // cube-biased size: mostly gravel, a few boulders. A uniform distribution reads as
    // a texture of identical pebbles; this reads as a belt.
    float sc  = smin + h.z * h.z * h.z * (smax - smin);
    return vec4(cos(ang) * rad, yy * thick, sin(ang) * rad, sc);
  }
  float rockSpinAngle(uint seed, float t, float rate) {
    return hash11(seed + 49979687u) * 6.2831853 + t * rate * (0.25 + hash11(seed + 86028121u));
  }
  mat3 axisAngle(vec3 a, float th) {
    float c = cos(th); float s = sin(th); float ic = 1.0 - c;
    return mat3(c + a.x * a.x * ic,      a.x * a.y * ic + a.z * s, a.x * a.z * ic - a.y * s,
                a.y * a.x * ic - a.z * s, c + a.y * a.y * ic,      a.y * a.z * ic + a.x * s,
                a.z * a.x * ic + a.y * s, a.z * a.y * ic - a.x * s, c + a.z * a.z * ic);
  }

  //////////////////////////////////////
  // SHAPE — three directional sine lobes over the unit sphere. CONTINUOUS in the
  // direction, so LOD0 and LOD2 sample the SAME silhouette and an asteroid does not
  // change shape when the task stage demotes it. The lobe axes/phases are derived once
  // per rock by the caller and passed in, so the per-vertex cost is 3 dots + 3 sines.
  //////////////////////////////////////
  float rockField(vec3 dir, vec3 a0, vec3 a1, vec3 a2, vec3 ph) {
    return 1.0
         + 0.340 * sin(dot(dir, a0) *  2.1 + ph.x)
         + 0.187 * sin(dot(dir, a1) *  5.0 + ph.y)
         + 0.103 * sin(dot(dir, a2) * 12.0 + ph.z);
  }
  vec3 sphereDir(float th, float phi) {
    return vec3(sin(th) * cos(phi), cos(th), sin(th) * sin(phi));
  }
  vec3 rockPoint(float th, float phi, vec3 a0, vec3 a1, vec3 a2, vec3 ph) {
    vec3 d = sphereDir(th, phi);
    return d * rockField(d, a0, a1, a2, ph);
  }

  //////////////////////////////////////
  // LOD table. Two lookups instead of one uvec2 so the mesh stage can hoist whichever
  // it needs; both are workgroup-uniform (the level comes from the payload).
  //////////////////////////////////////
  uint lodLon(uint level) { return (level == 0u) ? 12u : ((level == 1u) ? 8u : 5u); }
  uint lodLat(uint level) { return (level == 0u) ?  5u : ((level == 1u) ? 4u : 3u); }

  //////////////////////////////////////
  // FRUSTUM — Gribb-Hartmann planes pulled straight out of the mvp.
  // ONLY the four SIDE planes are tested: near/far depend on the clip-space depth
  // convention (GL -1..1 vs Vulkan 0..1) and picking the wrong one silently deletes
  // the entire belt. The far cut is done in WORLD distance instead, which has no
  // convention at all, and the near cut is not worth having for sector-sized spheres.
  //////////////////////////////////////
  bool planeCullsSphere(vec4 p, vec3 c, float r) {
    float L = length(p.xyz);
    return (L > 1.0e-9) && ((dot(p.xyz, c) + p.w) < (-r * L));
  }
  bool sphereOutsideSides(mat4 m, vec3 c, float r) {
    vec4 rx = vec4(m[0][0], m[1][0], m[2][0], m[3][0]);
    vec4 ry = vec4(m[0][1], m[1][1], m[2][1], m[3][1]);
    vec4 rw = vec4(m[0][3], m[1][3], m[2][3], m[3][3]);
    if (planeCullsSphere(rw + rx, c, r)) { return true; }
    if (planeCullsSphere(rw - rx, c, r)) { return true; }
    if (planeCullsSphere(rw + ry, c, r)) { return true; }
    if (planeCullsSphere(rw - ry, c, r)) { return true; }
    return false;
  }
}
////////////////////////////////////////
// the task->mesh payload: declared ONCE, inherited by BOTH stages. The task stage
// writes it, the mesh stage reads it. 48 bytes, far inside the 16KB portable cap.
// It carries the DECISION, not the geometry — the mesh stage re-derives every
// asteroid from (sector, rock index) with the same hashes the task stage used.
task_payload pld_rocks {
  vec4 sector;    // x = angular index, y = radial band, z = rocks emitted, w = band spin angle
  vec4 lodinfo;   // x = lod level, y = sector distance, z = unused, w = unused
  vec4 tint;      // lod tint, so the amplification decision is legible on screen
}
////////////////////////////////////////
task_interface iface_task : ublock_rocks {
  inputs { layout(local_size_x = 1, local_size_y = 1, local_size_z = 1); }
}
// the mesh stage stands in for the vertex stage, so it carries the vertex interface
// (which is where its workgroup size and output-count layout live).
vertex_interface iface_mesh : ublock_rocks {
  inputs  { layout(local_size_x = 32, local_size_y = 1, local_size_z = 1); }
  outputs {
    layout(triangles, max_vertices = 72, max_primitives = 120);
    vec4 frg_clr;
  }
}
fragment_interface iface_frg {
  inputs  { vec4 frg_clr; }
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
// TASK STAGE — one workgroup per belt sector. Decides the sector's mesh workgroup count.
task_shader ts_rocks : extension(GL_EXT_mesh_shader) : iface_task : pld_rocks : lib_rocks {
  uint sa   = gl_WorkGroupID.x;
  uint band = gl_WorkGroupID.y;

  float sa_count = grid.x;
  float r0       = belt.x;
  float bw       = belt.y;
  float thick    = belt.z;
  float t        = belt.w;

  float spin = bandSpin(band, r0, bw, t, grid.w);

  // sector bounding sphere. The wedge fits inside a box of (outer chord) x (2*thickness)
  // x (band width) centered on the sector midpoint — the arc bulge is already inside the
  // chord once the chord is taken at the OUTER radius — plus the biggest rock that can
  // sit on the boundary. Conservative on purpose: a bound that is too tight pops
  // asteroids at the screen edge, and neighbouring bounds overlapping costs nothing.
  float a_mid = (float(sa) + 0.5) / sa_count * 6.2831853 + spin;
  float r_mid = r0 + (float(band) + 0.5) * bw;
  vec3  center = vec3(cos(a_mid) * r_mid, 0.0, sin(a_mid) * r_mid);

  float r_out = r0 + float(band + 1u) * bw;
  float dphi  = 6.2831853 / sa_count;
  float chord = 2.0 * r_out * sin(dphi * 0.5);
  float rmax  = rock.y * rock.z;
  float bound = 0.5 * length(vec3(chord, 2.0 * thick, bw)) + rmax;

  vec3  eye  = (ivmtx * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
  float dist = length(center - eye);

  uint  rocks = 0u;
  float level = 3.0;
  vec4  tint  = vec4(0.0, 0.0, 0.0, 1.0);

  // THE decision. Two independent reasons to emit nothing — out of range, or off
  // screen — and the frustum one is what makes 147456 asteroids affordable at all.
  bool alive = ((dist - bound) < lodr.w) && (!sphereOutsideSides(mvp, center, bound));
  if (alive) {
    float ceilf = grid.z;
    if (dist < lodr.x) {
      rocks = uint(ceilf);
      level = 0.0;
      tint  = vec4(1.00, 0.36, 0.22, 1.0);
    } else if (dist < lodr.y) {
      rocks = uint(ceilf);
      level = 1.0;
      tint  = vec4(0.98, 0.78, 0.24, 1.0);
    } else if (dist < lodr.z) {
      rocks = max(1u, uint(ceilf * 0.5));
      level = 2.0;
      tint  = vec4(0.30, 0.82, 0.55, 1.0);
    } else {
      rocks = max(1u, uint(ceilf * 0.25));
      level = 2.0;
      tint  = vec4(0.34, 0.52, 0.95, 1.0);
    }
  }

  pld_rocks.sector  = vec4(float(sa), float(band), float(rocks), spin);
  pld_rocks.lodinfo = vec4(level, dist, 0.0, 0.0);
  pld_rocks.tint    = tint;

  // the dispatch verb. rocks==0 cancels the sector outright — no mesh workgroup runs.
  EmitMeshTasksEXT(rocks, 1u, 1u);
}
////////////////////////////////////////
// MESH STAGE — one workgroup per ASTEROID. Builds a lumpy lat/long shell whose ring
// counts come from the LOD the task stage picked. Every number it needs about WHICH
// asteroid it is comes from the payload plus its own gl_WorkGroupID.x.
mesh_shader ms_rocks : extension(GL_EXT_mesh_shader) : iface_mesh : pld_rocks : lib_rocks {
  uint k   = gl_WorkGroupID.x;              // which rock of this sector
  uint tid = gl_LocalInvocationID.x;

  uint  sa    = uint(pld_rocks.sector.x);
  uint  band  = uint(pld_rocks.sector.y);
  float spin  = pld_rocks.sector.w;
  uint  level = uint(pld_rocks.lodinfo.x);

  // workgroup-uniform: the payload is identical for every invocation here, so this is
  // uniform control flow and SetMeshOutputsEXT sees the same counts on every lane.
  uint LON = lodLon(level);
  uint LAT = lodLat(level);
  uint nv  = (LAT + 1u) * LON;
  uint np  = LAT * LON * 2u;
  SetMeshOutputsEXT(nv, np);

  uint seed = ((band * 4096u + sa) * 1621u + k) + 1u;

  float t = belt.w;
  vec4  pose  = rockPose(seed, sa, band, grid.x, belt.x, belt.y, belt.z, spin, rock.x, rock.y);
  vec3  org   = pose.xyz;
  float scale = pose.w;
  mat3  rot   = axisAngle(hashDir(seed + 32452843u), rockSpinAngle(seed, t, rock.w));

  // the three lobe axes + phases: derived ONCE per rock, then reused by every vertex
  // and every finite-difference probe below.
  vec3 a0 = hashDir(seed + 7919u);
  vec3 a1 = hashDir(seed + 104729u);
  vec3 a2 = hashDir(seed + 1299709u);
  vec3 ph = hash31(seed + 15485863u) * 6.2831853;

  vec3 albedo = mix(vec3(0.26, 0.235, 0.215), vec3(0.47, 0.40, 0.32), hash11(seed + 999983u));
  albedo = mix(albedo, pld_rocks.tint.rgb, sun.w);

  float dth = 3.14159265 / float(LAT);
  float dph = 6.2831853 / float(LON);
  // finite-difference step for the normal, a fraction of a cell so the probe stays on
  // the same lump it is shading.
  float eps = 0.25 * min(dth, dph);

  //////////////////////////////////////
  // VERTICES — a (LAT+1) x LON lat/long grid. Rows 0 and LAT are the poles: their LON
  // vertices are coincident, which costs LON-1 duplicates and buys the pole caps for
  // free (the degenerate half of each pole quad rasterizes to nothing).
  //////////////////////////////////////
  for (uint i = tid; i < nv; i += 32u) {
    uint r = i / LON;
    uint c = i - r * LON;
    float th  = dth * float(r);
    float phi = dph * float(c);

    vec3 dir = sphereDir(th, phi);
    vec3 lp  = dir * rockField(dir, a0, a1, a2, ph);

    // normal from the tangent plane of the SAME lumpy field, not of the sphere — a
    // sphere normal on a rock this dented reads as a smooth ball with a jagged edge.
    // theta is clamped off the poles: there the phi tangent collapses and the cross
    // product would be NaN for every vertex of the cap.
    float thc = clamp(th, eps, 3.14159265 - eps);
    vec3 dT = rockPoint(thc + eps, phi, a0, a1, a2, ph) - rockPoint(thc - eps, phi, a0, a1, a2, ph);
    vec3 dP = rockPoint(thc, phi + eps, a0, a1, a2, ph) - rockPoint(thc, phi - eps, a0, a1, a2, ph);
    vec3 nl = cross(dT, dP);
    float nlen = length(nl);
    nl = (nlen > 1.0e-9) ? (nl / nlen) : dir;
    // orient outward without caring which way the cross came out — cheaper and more
    // robust than reasoning about the parameterization's handedness.
    nl = (dot(nl, dir) < 0.0) ? -nl : nl;

    vec3 wp = org + rot * (lp * scale);
    vec3 wn = rot * nl;

    float lam = max(dot(wn, sun.xyz), 0.0);
    // hemisphere ambient standing in for the skybox: warm from above, cool from below.
    vec3  amb = mix(vec3(0.055, 0.06, 0.085), vec3(0.20, 0.185, 0.155), 0.5 + 0.5 * wn.y);
    vec3  clr = albedo * (amb + vec3(1.05, 0.98, 0.88) * lam);

    gl_MeshVerticesEXT[i].gl_Position = mvp * vec4(wp, 1.0);
    frg_clr[i] = vec4(clr, 1.0);
  }

  //////////////////////////////////////
  // PRIMITIVES — two triangles per (row, column) quad, columns wrapping at LON so the
  // shell closes with no seam vertices.
  //////////////////////////////////////
  for (uint i = tid; i < np; i += 32u) {
    uint q = i >> 1u;
    uint r = q / LON;
    uint c = q - r * LON;
    uint c1 = (c + 1u == LON) ? 0u : (c + 1u);
    uint v00 = r * LON + c;
    uint v01 = r * LON + c1;
    uint v10 = (r + 1u) * LON + c;
    uint v11 = (r + 1u) * LON + c1;
    gl_PrimitiveTriangleIndicesEXT[i] = ((i & 1u) == 0u) ? uvec3(v00, v10, v11)
                                                        : uvec3(v00, v11, v01);
  }
}
////////////////////////////////////////
fragment_shader ps_rocks : iface_frg {
  out_clr = frg_clr;
}
state_block sb_rocks : default {
  // CullTest OFF: the shell winding is authored in the mesh stage and the viewport is
  // Y-flipped, so which face ends up front is a convention question this example does
  // not need to answer — the shells are closed and depth-tested, so double-siding is
  // free of artifacts. (Turning it ON is the obvious next perf win once the winding is
  // confirmed on the target driver.)
  CullTest  = PASS_BACK;
  DepthTest = LEQUALS;
  BlendMode = OFF;
  DepthMask = ON;
}
////////////////////////////////////////
technique tek_rocks {
  fxconfig = fxcfg_default;
  pass p0 {
    task_shader = ts_rocks;
    mesh_shader = ms_rocks;
    fragment_shader = ps_rocks;
    state_block = sb_rocks;
  }
}
"""

################################################################################

class AsteroidApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.SGC = self.addComponent(
        "std_scenegraph",
        StandardSceneGraphComponent,
        eye=vec3(0, 16, 200),         # inside the annulus, looking across the hole at
        tgt=vec3(0, 0, 0),            #  the far arc: near rocks at LOD0, far arc at LOD2
        up=vec3(0, 1, 0),
        near=1.0,
        far=35000.0,
        explicit_near_far = True,
        ssaa=2,                       # thousands of sub-pixel rocks; MSAA earns its cost here
        grid_variant=None,
        sg_params={"SkyboxTexPathStr": "<ork_envmaps2>/pillars4k.xir"})
    self.time      = 0.0
    self.tint_mix  = 0.0    # 0 = rock albedo, 1 = LOD tint (T toggles)
    self.lod_scale = 1.0    # [ / ] scale the LOD + cull radii
    self.checked   = False
    self.frames    = 0
    self.createEzApp()

  def _onGpuInit(self, ctx):
    self.scene     = self.SGC.scenegraph
    self.layer_fwd = self.SGC.layer_fwd
    self.ctx       = ctx

    # HARD GATE, by name. No taskless fallback: the entire point is the task stage.
    if not ctx.supports_task_shader:
      raise RuntimeError(
          "TASKMESH-ASTEROIDS-UNAVAILABLE: this device does not advertise the VK_EXT_mesh_shader "
          "taskShader feature (ctx.supports_task_shader is False). The belt is built by a "
          "task+mesh pass and has no taskless form — refusing to draw a lesser picture.")

    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx, "taskmesh_asteroids", SHADER)
    #mtl.rasterstate.setBlendingMacro(tokens.ADDITIVE)
    #mtl.rasterstate.culltest  = tokens.PASS_FRONT
    #mtl.rasterstate.depthtest = tokens.LEQUALS

    permu = lev2.FxPipelinePermutation(rendermodel="ForwardPBR")
    permu.technique = mtl.shader.technique("tek_rocks")
    assert permu.technique, "technique tek_rocks not found"
    pipe = mtl.fxcache.findPipeline(permu)
    pipe.bindParam(mtl.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
    pipe.bindParam(mtl.param("ivmtx"), tokens.RCFD_Camera_IV_Mono)
    pipe.sharedMaterial = mtl
    self.pipe   = pipe
    self.mtl    = mtl
    self.p_belt = mtl.param("belt")
    self.p_grid = mtl.param("grid")
    self.p_rock = mtl.param("rock")
    self.p_lodr = mtl.param("lodr")
    self.p_sun  = mtl.param("sun")
    self._writeUniforms()

    gdd = lev2.ComputeDrawableData()
    gdd.pipeline = pipe
    # the sector grid: one workgroup per belt sector. With a task stage in the pass
    # these are TASK workgroup counts, and each one decides its own mesh workgroup count.
    gdd.setMeshDraw(permu.technique, SECTORS_ANGULAR, SECTORS_RADIAL, 1)
    self.node = self.layer_fwd.createDrawableNodeFromData("asteroids", gdd)

    self.scene.lightingmanager.gpuInit(ctx)

    tasks = SECTORS_ANGULAR * SECTORS_RADIAL
    rocks = tasks * ROCKS_PER_SECTOR
    print("taskmesh_asteroids: %d sectors (%dx%d task workgroups)" %
          (tasks, SECTORS_ANGULAR, SECTORS_RADIAL), flush=True)
    print("taskmesh_asteroids: ceiling %d asteroids / %.1fM triangles, "
          "all of it the task stage's call" % (rocks, rocks * MAX_PRIMS / 1.0e6), flush=True)
    print("taskmesh_asteroids: belt r=%.0f..%.0f  keys: T=lod tint  [ ]=lod radii" %
          (BELT_INNER, BELT_OUTER), flush=True)

  def _writeUniforms(self):
    s = self.lod_scale
    d = SUN_DIR.normalized
    self.pipe.bindParam(self.p_belt, vec4(BELT_INNER, BAND_WIDTH, BELT_THICK, self.time))
    self.pipe.bindParam(self.p_grid, vec4(float(SECTORS_ANGULAR), float(SECTORS_RADIAL),
                                          float(ROCKS_PER_SECTOR), BELT_SPIN))
    self.pipe.bindParam(self.p_rock, vec4(ROCK_SCALE_MIN, ROCK_SCALE_MAX,
                                          ROCK_FIELD_MAX, ROCK_SPIN))
    self.pipe.bindParam(self.p_lodr, vec4(LOD0_DIST * s, LOD1_DIST * s,
                                          LOD2_DIST * s, CULL_DIST * s))
    self.pipe.bindParam(self.p_sun, vec4(d.x, d.y, d.z, self.tint_mix))

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
            "TASKMESH-ASTEROIDS-NOT-ENGAGED: %d frames drawn but ctx.task_shader_draws is 0 — the "
            "pipeline ran WITHOUT a task stage. The picture may look plausible; it is not the "
            "thing under test." % self.frames)
      print("taskmesh_asteroids: task stage ENGAGED — %d task+mesh draws in %d frames" %
            (n, self.frames), flush=True)

  def _onUiEvent(self, uievent):
    if uievent.code == tokens.KEY_DOWN.hashed:
      k = uievent.keycode
      if k == ord('T'):
        self.tint_mix = 0.0 if self.tint_mix > 0.5 else 1.0
        print("taskmesh_asteroids: lod tint %s" %
              ("ON" if self.tint_mix > 0.5 else "OFF"), flush=True)
        return lev2.ui.HandlerResult()
      if k == ord('['):
        self.lod_scale = max(0.05, self.lod_scale * 0.8)
        print("taskmesh_asteroids: lod scale %.3f" % self.lod_scale, flush=True)
        return lev2.ui.HandlerResult()
      if k == ord(']'):
        self.lod_scale = min(8.0, self.lod_scale * 1.25)
        print("taskmesh_asteroids: lod scale %.3f" % self.lod_scale, flush=True)
        return lev2.ui.HandlerResult()
    self.SGC._onCameraUiEvent(uievent)
    return lev2.ui.HandlerResult()

################################################################################

if __name__ == "__main__":
  app = AsteroidApp()
  app.ezapp.mainThreadLoop()
