###############################################################################
# Copyright 1996-2026, Michael T. Mayers. MIT License.
###############################################################################
# GrassFieldSource — the TASK+MESH geometry provider for the procedural grass carpet.
#
# It is a ptex3d `vertex_source` (the duck-typed mesh_source protocol + the task-stage
# extension in fxv2_template._mesh_block), so the blades are a REAL forward-PBR material:
# sun-cascade + cloud shadows, IBL, depth prepass, tonemap and the Wind displace primitive
# all come for free, and the draw side needs no engine change (a ComputeDrawable's
# setMeshDraw counts are task counts once a task stage fronts the pass).
#
#   TASK  (one workgroup per tile) : where the grass IS — a camera-relative tile grid,
#                                    the baked density field, distance falloff and a
#                                    frustum reject, ending in EmitMeshTasksEXT(clusters).
#   MESH  (one workgroup per cluster, one invocation per blade) : what a blade LOOKS like —
#                                    root placement, ground conform, clumping, the curved
#                                    tapered ribbon, wind, and the standard ptex3d varyings.
#
# STRUCTURAL vs PARAMETRIC (owner law A8): the only baked numbers are the ones that SIZE the
# stages — segments per blade, blades per workgroup, and the vertex/primitive caps those two
# imply. Every number a person would ever tune (tile size, grid extent, LOD radii, blade
# height/width/taper, clump statistics, palette, dryness, wind) rides in ublk_ptex_params via
# displace_params(), so retuning the carpet is a param rebind, never a recompile.
#
# NO PULL-VS GRASS PATH: the blades exist only in the task+mesh stages. See _PULL_VS_BODY.
#
# GrassSurface (bottom of this file) is the FRAGMENT half of the same contract: it reads the
# per-blade varyings this source writes and the same ublk_ptex_params members, so the two
# halves are edited together or the carpet shades against numbers that mean something else.
###############################################################################

from ..hypermesh import Wind
from ...ptex3d import Ptex3d, P

# ---- STRUCTURAL constants (baked, salted into the material digest) ---------------------------
# A blade is a SEGMENTS-segment ribbon: SEGMENTS+1 rings of 2 corners = 2*(SEGMENTS+1) vertices
# and 2*SEGMENTS triangles. 3 segments is the cheapest curve that reads as a bent blade rather
# than a straight card at eye level (1 segment cannot bend at all, 2 kinks visibly).
SEGMENTS        = 3
VERTS_PER_BLADE = 2 * (SEGMENTS + 1)          # 8
PRIMS_PER_BLADE = 2 * SEGMENTS                # 6
# One mesh workgroup = one CLUSTER of blades, one invocation each. 8 is set by MoltenVK's NATIVE
# mesh route, not by the invocation limit: Metal accounts a mesh threadgroup's whole DECLARED
# output as threadgroup memory, and above ~16KB MoltenVK silently switches to an emulation that
# restarts the Metal render pass a thousand times per draw (green build, no diagnostic, collapsed
# frame time — see test_hypermesh_meshshader_codegen's native-route guard). At the ptex3d varying
# stride that ceiling lands between 8 blades (12032B) and 16 (24064B). Small clusters also make
# the fractional-density cut finer: a thinning tile drops single blades.
BLADES_PER_WG   = 8
MAX_VERTICES    = BLADES_PER_WG * VERTS_PER_BLADE     # 64
MAX_PRIMITIVES  = BLADES_PER_WG * PRIMS_PER_BLADE     # 48
# The task stage is ONE tile per workgroup: its whole job is a workgroup-uniform decision, so a
# wider workgroup would only have every lane recompute the same numbers.
TASK_WG         = 1
# SUPERTILE — the edge, in tiles, of the tile block ONE task workgroup owns and LOOPS, so a
# 128x128 tile grid dispatches (128/S)^2 task workgroups and each emits only the tiles that
# survived radius + frustum + HZB: the launch count follows the VISIBLE set instead of the grid.
#
# It is the BAKED payload capacity (SUPERTILE^2 survivor slots), not the live value — the drawable
# binds the live edge in GrassCull.x and the shader clamps to this ceiling, so a smaller authored
# supertile needs no re-materialize and a larger one cannot overrun the payload.
#
# 4, MEASURED (scn_forestg, 128x128 tiles, offscreen, this platform): the payload is the whole
# trade. It is threadgroup memory on Metal, so its SIZE is object-stage occupancy, and it grows as
# S^2 while the launches it saves fall as S^2. 8 (1040B) measured 69 FPS, 4 (272B) and 2 (80B) both
# measured 71.7 — the curve is flat below 272B and falls above it, so 4 is the largest block that
# costs nothing to carry.
SUPERTILE       = 4
SUPERTILE_SLOTS = SUPERTILE * SUPERTILE            # payload survivor slots

FIELD_BLOCK_NAME = "sif_grass_field"
HZB_BLOCK_NAME   = "sif_hzb"
STATS_BLOCK_NAME = "sif_grass_stats"

# The rasterstate the carpet's surface DSL must declare (GrassSurface, Phase 3):
#   self.surface(..., **GRASS_RASTER)
# A2C rather than alpha blend so the blades are order-independent and VR-safe, and no cull
# because a blade is a two-sided ribbon. It is NOT returned from as_material_kwargs(): the
# ptex3d DSL always emits its own raster dict from surface()/unlit() (dsl.py:962-964) and the
# two would collide as duplicate keyword arguments at materialize time.
GRASS_RASTER = dict(alpha_to_coverage=True, cull="off")

# ---- FIELD SSBO ------------------------------------------------------------------------------
# One std430 buffer, CamBlk at offset 0 exactly like the terrain drawable's (ComputeDrawable
# .setCameraParams(ssbo, 0) fills it), then the field header and a single trailing array — the
# resampled terrain capture channels the C++ bootstrap packs from the baked channel EXRs.
_FIELD_BLOCK = (
  "storage_interface %s (descriptor_set 0) {\n"
  "  buffer layout(std430) grass_field_data {\n"
  "    mat4 c_vp; mat4 c_ivp; vec4 c_eye; vec4 c_misc;   // CamBlk @0 (setCameraParams)\n"
  "    vec4 g_meta;    // x = field dim (texels/side)  y = extent (m)  z = y min  w = y range\n"
  "    vec4 g_meta2;   // reserved (field revision / debug)\n"
  "    // FIELD TEXEL (the C++ buffer lane packs exactly this):\n"
  "    //   x = height (m)   y = density01\n"
  "    //   z = ground normal xz as packHalf2x16, bit-reinterpreted as float\n"
  "    //   w = dryness01\n"
  "    vec4 g_field[];\n"
  "  };\n"
  "}\n" % FIELD_BLOCK_NAME)

# ---- HZB SSBO (read-only) --------------------------------------------------------------------
# The engine's 1-phase max-depth pyramid, the SAME buffer + the SAME packing the terrain chunk cull
# reads (ComputeDrawable binds it per frame and stamps its base w/h/mips into CamBlk c_misc.yzw).
# Declared AFTER the field block so the program-level declaration order — which is what assigns the
# descriptor bindings — stays sif_ptex_vtx, sif_grass_field, sif_hzb on every stage that inherits it.
_HZB_BLOCK = (
  "storage_interface %s (descriptor_set 0) {\n"
  "  buffer layout(std430) hzb_blk { float HZB[]; };\n"
  "}\n" % HZB_BLOCK_NAME)

# ---- STATS SSBO (write) ------------------------------------------------------------------------
# 16 bytes of its own, because it is the one thing the carpet WRITES: a storage interface with no
# declared access is readonly in every graphics stage (shadlang_backend_spirv's stage-derived
# default), and promoting the 4MB field block to readwrite to carry one counter would cost the
# read-only guarantee on every one of the millions of field taps the mesh stage makes.
# GPU-OWNED: the drawable zeroes it at creation and never writes it again — a host write in the
# same frame shadows the shader's writes on readback (MoltenVK), so a per-frame reset would make
# the counter read back as zero forever. The reader takes differences instead.
_STATS_BLOCK = (
  "storage_interface %s (descriptor_set 0) {\n"
  "  buffer layout(std430, readwrite) grass_stats_blk {\n"
  "    uint g_emitted;                              // cumulative mesh workgroups emitted\n"
  "    uint g_stats_r1; uint g_stats_r2; uint g_stats_r3;   // reserved\n"
  "  };\n"
  "}\n" % STATS_BLOCK_NAME)

# ---- the pull-VS body ------------------------------------------------------------------------
# There is NO taskless grass. The generated FWD_SSBO_CUSTOM family is gated on a non-empty pull
# body (fxv2_template._ssbo_block:456) and the mesh/task techniques are emitted inside it, so
# the material cannot carry the amplified path without carrying a vertex path too. This one is
# DEGENERATE by construction — it collapses every vertex to a single point, so a consumer that
# binds FWD_SSBO_CUSTOM draws nothing at all rather than a lesser grass field. The loud half of
# the contract lives in the drawable (Phase 2): no task stage in the bound pass, or a
# taskShaderDrawCount() of zero after real frames, is a named refusal.
_PULL_VS_BODY = (
  "// DEGENERATE: grass geometry exists only in the task+mesh stages (see the module header).\n"
  "// FWD_SSBO_CUSTOM_MESH, fronted by its task stage, is the ONLY legitimate draw route for this\n"
  "// material — the drawable dispatches no other, and a pass that reaches here draws nothing.\n"
  "vec4 position = vec4(0.0, 0.0, 0.0, 1.0);\n"
  "vec3 normal   = vec3(0.0, 1.0, 0.0);\n"
  "vec3 binormal = vec3(1.0, 0.0, 0.0);\n"
  "vec2 uv0      = vec2(0.0);\n"
  "float uv0z    = 0.0;\n"
  "vec4 vtxcolor = vec4(0.0);")

# The bilinear tap addressing, written ONCE and spliced into both field readers: the scalar tap
# and the normal tap must never resolve to different texels for the same point. Not a shader
# macro — nothing in the shadlang tree uses the preprocessor, and this is not the slice to find
# out whether it parses.
_TAPS = (
  "  // TEXEL-CENTRE convention (* dim - 0.5), NOT node-aligned (* (dim-1)): texel i covers the\n"
  "  // world span whose centre is ((i+0.5)/dim - 0.5)*extent — that is what Image::resampledOf\n"
  "  // produced when the bake was downsampled and what the terrain render mesh puts on screen.\n"
  "  // Node alignment shifts the read by -wxz/dim metres (0 at the origin, half a texel at the\n"
  "  // extent edge), which lifts or buries the blades against the surface they stand on.\n"
  "  vec2 uv  = (wxz / max(extent, 1.0e-4) + vec2(0.5)) * dim - vec2(0.5);\n"
  "  vec2 i0  = clamp(floor(uv), vec2(0.0), vec2(dim - 1.0));\n"
  "  vec2 i1  = clamp(i0 + vec2(1.0), vec2(0.0), vec2(dim - 1.0));\n"
  "  vec2 f   = clamp(uv - i0, vec2(0.0), vec2(1.0));\n"
  "  uint d   = uint(dim);\n"
  "  uint x0  = uint(i0.x); uint x1 = uint(i1.x);\n"
  "  uint y0  = uint(i0.y) * d; uint y1 = uint(i1.y) * d;\n"
  "  vec4 t00 = g_field[y0 + x0]; vec4 t10 = g_field[y0 + x1];\n"
  "  vec4 t01 = g_field[y1 + x0]; vec4 t11 = g_field[y1 + x1];\n")

# ---- shared library ---------------------------------------------------------------------------
# Inherited by every stage (task, mesh, pull VS) as lib_ptex_vtx. Uniform-free except for the
# field storage it reads — values arrive as arguments, so the text is value-INDEPENDENT.
_LIB = (
  "// cheap deterministic hash -> [0,1). Blade N of a tile must land in the same spot however\n"
  "// many clusters the task stage launched, so every placement decision is a hash of stable ids.\n"
  "float grass_hash11(uint n) {\n"
  "  n = (n ^ 61u) ^ (n >> 16u);\n"
  "  n = n + (n << 3u);\n"
  "  n = n ^ (n >> 4u);\n"
  "  n = n * 668265261u;\n"
  "  n = n ^ (n >> 15u);\n"
  "  return float(n & 65535u) * 0.0000152587890625;\n"
  "}\n"
  "// a WORLD-LATTICE cell id: the tile grid slides with the camera, so a tile's identity must\n"
  "// come from where it is, never from its workgroup index (that would re-seed the whole field\n"
  "// every time the camera crossed a tile boundary).\n"
  "uint grass_cell_id(vec2 p, float cell) {\n"
  "  ivec2 c = ivec2(floor(p / max(cell, 1.0e-4)));\n"
  "  return uint((c.x * 73856093) ^ (c.y * 19349663));\n"
  "}\n"
  # The field is a dim x dim grid spanning `extent` metres centred on the origin (the terrain
  # atlas parameterization, downsampled); outside it the clamp holds the edge value and the
  # caller zeroes density instead. The four-tap address arithmetic is written ONCE here and
  # spliced into both readers, so the scalar tap and the normal tap can never sample different
  # texels for the same point.
  + "// SCALAR channels -> (height_m, density01, dryness01, 0). Bilinear, straightforwardly.\n"
  "vec4 grass_field(vec2 wxz, float dim, float extent) {\n"
  + _TAPS +
  "  vec4 b = mix(mix(t00, t10, f.x), mix(t01, t11, f.x), f.y);\n"
  "  return vec4(b.x, b.y, b.w, 0.0);\n"
  "}\n"
  "// GROUND NORMAL. The texel packs nrm.xz as packHalf2x16 bit-reinterpreted into a float, so it\n"
  "// is UNPACKED FIRST and interpolated in vector space — mixing two bit patterns would produce a\n"
  "// direction related to neither, and nearest-sampling instead would step the blade orientation\n"
  "// at every texel edge (a visible seam grid across the carpet). nrm.y is reconstructed: on a\n"
  "// heightfield the normal is always the up-facing root of the unit vector.\n"
  "vec3 grass_ground_normal(vec2 wxz, float dim, float extent) {\n"
  + _TAPS +
  "  vec2 n00 = unpackHalf2x16(floatBitsToUint(t00.z));\n"
  "  vec2 n10 = unpackHalf2x16(floatBitsToUint(t10.z));\n"
  "  vec2 n01 = unpackHalf2x16(floatBitsToUint(t01.z));\n"
  "  vec2 n11 = unpackHalf2x16(floatBitsToUint(t11.z));\n"
  "  vec2 nxz = mix(mix(n00, n10, f.x), mix(n01, n11, f.x), f.y);\n"
  "  float y2 = max(1.0 - dot(nxz, nxz), 1.0e-4);\n"
  "  return normalize(vec3(nxz.x, sqrt(y2), nxz.y));\n"
  "}\n"
  "// BAKED dryness biased by the live GrassDry.x fraction (0.5 = the field as baked), so the\n"
  "// carpet's dry mix stays a runtime knob over baked data. One function, so the tile-level and\n"
  "// per-blade readings can never drift apart.\n"
  "float grass_dryness(float baked, float fraction) {\n"
  "  return clamp(baked + (fraction - 0.5) * 2.0, 0.0, 1.0);\n"
  "}\n"
  "// FRUSTUM — Gribb-Hartmann side planes pulled straight out of the clip matrix that will\n"
  "// rasterize THIS view, so the reject can never disagree with what would have been drawn.\n"
  "// Side planes only: near/far depend on the clip-space depth convention and the far cut is\n"
  "// already done in world distance against the cull radius (no convention at all).\n"
  "bool grass_plane_culls(vec4 p, vec3 c, float r) {\n"
  "  float L = length(p.xyz);\n"
  "  return (L > 1.0e-9) && ((dot(p.xyz, c) + p.w) < (-r * L));\n"
  "}\n"
  "bool grass_outside_sides(mat4 m, vec3 c, float r) {\n"
  "  vec4 rx = vec4(m[0][0], m[1][0], m[2][0], m[3][0]);\n"
  "  vec4 ry = vec4(m[0][1], m[1][1], m[2][1], m[3][1]);\n"
  "  vec4 rw = vec4(m[0][3], m[1][3], m[2][3], m[3][3]);\n"
  "  if (grass_plane_culls(rw + rx, c, r)) { return true; }\n"
  "  if (grass_plane_culls(rw - rx, c, r)) { return true; }\n"
  "  if (grass_plane_culls(rw + ry, c, r)) { return true; }\n"
  "  if (grass_plane_culls(rw - ry, c, r)) { return true; }\n"
  "  return false;\n"
  "}\n"
  # HZB OCCLUSION — the terrain chunk cull's test, unchanged in its math (gpu_chunk.py's
  # cs_terrain_cull): project the box, mip-fit a 2x2 fetch of the MAX-depth pyramid, compare the
  # box's NEAREST standard-Z against it. It reads the same buffer through the same c_misc.yzw
  # header, so a tile and a terrain chunk can never disagree about what is behind what.
  "// FAIL-SAFE by construction: no pyramid this frame (misc.y/w == 0), or a box corner at/behind\n"
  "// the eye plane (w <= 0, where the NDC projection is meaningless), answers NOT OCCLUDED. An\n"
  "// absent or zero-dimension HZB must never be able to subtract grass.\n"
  "bool grass_hzb_occluded(mat4 vp, vec4 misc, vec3 mn, vec3 mx, float bias) {\n"
  "  if (misc.y < 0.5 || misc.w < 0.5) { return false; }\n"
  "  uint hw = uint(misc.y); uint hh = uint(misc.z); uint hmips = uint(misc.w);\n"
  "  vec3 nmin = vec3(1.0e9); vec3 nmax = vec3(-1.0e9);\n"
  "  for (int k = 0; k < 8; k++) {\n"
  "    vec3 cor = vec3(((k & 1) != 0) ? mx.x : mn.x,\n"
  "                    ((k & 2) != 0) ? mx.y : mn.y,\n"
  "                    ((k & 4) != 0) ? mx.z : mn.z);\n"
  "    vec4 cl = vp * vec4(cor, 1.0);\n"
  "    if (cl.w <= 0.0001) { return false; }\n"
  "    vec3 nd = cl.xyz / max(cl.w, 0.0001);\n"
  "    nmin = min(nmin, nd); nmax = max(nmax, nd);\n"
  "  }\n"
  "  vec2 uvmn = clamp(vec2(nmin.x * 0.5 + 0.5, 1.0 - (nmax.y * 0.5 + 0.5)), 0.0, 1.0);\n"
  "  vec2 uvmx = clamp(vec2(nmax.x * 0.5 + 0.5, 1.0 - (nmin.y * 0.5 + 0.5)), 0.0, 1.0);\n"
  "  float ex = (uvmx.x - uvmn.x) * float(hw); float ey = (uvmx.y - uvmn.y) * float(hh);\n"
  "  int mip = int(ceil(log2(max(max(ex, ey), 1.0)))); mip = clamp(mip, 0, int(hmips) - 1);\n"
  "  uint mw = max(1u, hw >> uint(mip)); uint mh = max(1u, hh >> uint(mip));\n"
  "  uint moff = 0u; uint ow = hw; uint oh = hh;\n"
  "  for (int j = 0; j < mip; j++) { moff += ow * oh; ow = max(1u, ow >> 1u); oh = max(1u, oh >> 1u); }\n"
  "  ivec2 mxd = ivec2(int(mw) - 1, int(mh) - 1);\n"
  "  ivec2 t0 = clamp(ivec2(int(uvmn.x * float(mw)), int(uvmn.y * float(mh))), ivec2(0), mxd);\n"
  "  ivec2 t1 = clamp(ivec2(int(uvmx.x * float(mw)), int(uvmx.y * float(mh))), ivec2(0), mxd);\n"
  "  float occ = HZB[moff + uint(t0.y) * mw + uint(t0.x)];\n"
  "  occ = max(occ, HZB[moff + uint(t0.y) * mw + uint(t1.x)]);\n"
  "  occ = max(occ, HZB[moff + uint(t1.y) * mw + uint(t0.x)]);\n"
  "  occ = max(occ, HZB[moff + uint(t1.y) * mw + uint(t1.x)]);\n"
  "  // standard-Z is nonlinear, so a far/coplanar box's nearest and the occluder's max sit within\n"
  "  // float noise of each other; the bias is the margin that keeps a tile from culling itself.\n"
  "  return (nmin.z > occ + bias);\n"
  "}\n"
  # THE wind function, taken verbatim from the Wind VertexDisplace primitive rather than
  # re-typed: the carpet then sways on the same curve and the same clock as the trees that
  # share the scene's wind dict, and a change to one can never leave the other behind.
  + Wind().glsl_func() + "\n")


class GrassFieldSource:
  """ptex3d vertex_source for the task+mesh grass carpet.

  Constructed by the scene beside the GrassDrawableData that feeds it (the drawable owns the
  field SSBO and the dispatch grid; this owns the shader text). The constructor arguments are
  DEFAULTS for the bindable params — none of them is baked into the generated source, so two
  carpets with different knobs share one compiled material.

  tile        : world size of one task tile (metres) — one task workgroup covers this square
  grid        : task workgroups per side (the drawable's setMeshDraw grid must match)
  cull_r      : beyond this distance from the eye a tile emits nothing
  lod0/lod1   : full-density radius / mid radius (metres)
  clusters    : the LOD CEILING — most mesh workgroups any one tile may ask for. A full tile
                therefore holds clusters * BLADES_PER_WG blades, so the default is stated
                against that product: halving the workgroup doubles this and the carpet is
                unchanged. The param's meaning never changes — it is always workgroups.
  """

  wants_task_stage = True

  def __init__(self, *, tile=1.0, grid=96, cull_r=64.0, lod0=12.0, lod1=32.0, fade_pow=1.5,
               clusters=12, density_scale=1.0, height=0.32, height_var=0.35, width=0.022,
               taper=0.85, clump_radius=0.9, clump_lean=0.55, clump_phase_var=1.0,
               clump_hue_var=0.18, clump_height_var=0.0,
               color_a=(0.16, 0.30, 0.09, 1.0), color_b=(0.30, 0.46, 0.14, 1.0),
               dry_color=(0.45, 0.40, 0.18, 1.0), dry_fraction=0.25, dry_fade_start=0.35,
               dry_fade_end=0.9, backlit=0.6, stereo_widen=0.35,
               wind_amp=0.015, wind_freq=0.3, wind_dir=(1.0, 0.0, 0.0)):
    self.tile          = float(tile)
    self.grid          = int(grid)
    self.cull_r        = float(cull_r)
    self.lod0          = float(lod0)
    self.lod1          = float(lod1)
    self.fade_pow      = float(fade_pow)
    self.clusters      = int(clusters)
    self.density_scale = float(density_scale)
    self.height        = float(height)
    self.height_var    = float(height_var)
    self.width         = float(width)
    self.taper         = float(taper)
    self.clump         = (float(clump_radius), float(clump_lean),
                          float(clump_phase_var), float(clump_hue_var))
    # GrassClump2 — the second clump pack. x = per-clump height amplitude; yzw are held for the
    # rest of the tussock statistics, and MUST stay zero until they mean something.
    self.clump2        = (float(clump_height_var), 0.0, 0.0, 0.0)
    self.color_a       = tuple(float(c) for c in color_a)
    self.color_b       = tuple(float(c) for c in color_b)
    self.dry_color     = tuple(float(c) for c in dry_color)
    self.dry           = (float(dry_fraction), float(dry_fade_start),
                          float(dry_fade_end), float(backlit))
    self.stereo_widen  = float(stereo_widen)
    self.wind          = Wind(amp=wind_amp, freq=wind_freq, dir=wind_dir)

  # ---- ComputeDrawable / scene consumer contract --------------------------------------------
  def task_groups(self):
    """The TASK workgroup grid the drawable dispatches (setMeshDraw counts are task counts once
    a task stage fronts the pass). Fixed: the field follows the camera inside the shader, so no
    host code resizes or repositions anything as the walker moves."""
    return (self.grid, self.grid, 1)

  # ---- A8: every tweakable is a UBO member --------------------------------------------------
  def displace_params(self):
    """[(name, gtype, default), ...] -> merged into ublk_ptex_params (ptex3d/dsl.py:1458), which
    every generated stage inherits. Nothing here is ever inlined into the shader text: the whole
    look of the carpet is rebindable at runtime, and Phase 5's iteration is param writes rather
    than re-materialized materials. Time's default is the RCFD_TIME provider token, so the engine
    feeds the scene clock with no per-frame host code — the same clock the trees sway on."""
    return [
      ("GrassField",  "vec4", (self.tile, float(self.grid), self.cull_r, self.density_scale)),
      ("GrassLod",    "vec4", (self.lod0, self.lod1, self.fade_pow, float(self.clusters))),
      ("GrassBlade",  "vec4", (self.height, self.height_var, self.width, self.taper)),
      ("GrassClump",  "vec4", self.clump),
      ("GrassClump2", "vec4", self.clump2),
      ("GrassColorA", "vec4", self.color_a),
      ("GrassColorB", "vec4", self.color_b),
      ("GrassDryColor", "vec4", self.dry_color),
      ("GrassDry",    "vec4", self.dry),
      # stereo cull widening (design decision 1): the _ST task twin culls ONCE, from a centre
      # camera, and widens the tile bound to cover what either eye can see.
      ("GrassStereo", "vec4", (self.stereo_widen, 0.0, 0.0, 0.0)),
      # the COMPACTION/OCCLUSION pack. x = the live supertile edge in tiles (clamped in-shader to
      # SUPERTILE, the baked payload capacity); y = the HZB standard-Z bias; z = HZB test on/off;
      # w = arms the emitted-workgroup counter in the field SSBO's GPU-owned header tail.
      ("GrassCull",   "vec4", (float(SUPERTILE), 0.005, 1.0, 0.0)),
    ] + self.wind.params()

  # ---- TASK STAGE -----------------------------------------------------------------------------
  def task_payload(self, name="pld_ptex_mesh"):
    """The SURVIVING tiles of one supertile: ONE vec4 each plus a header (272 bytes at S=4).

    SIZE IS THE POINT. Metal accounts an object shader's payload as threadgroup memory, so payload
    bytes are object-stage occupancy: every field that can be re-derived on the mesh side costs
    less as arithmetic there than as bytes here (measured — a fatter payload carrying the tile size,
    the tile id and the eye distance cost several ms/frame at this dispatch). So this carries the
    four numbers that cannot be recomputed, and the mesh stage re-derives the rest: the tile size is
    a bound param it already reads, and the tile id is grass_cell_id of the tile centre — the same
    call the task stage made, on the same lattice, so the blades hash identically either way.

    w is the CLUSTER BASE — the running sum of the clusters the earlier survivors asked for. The
    bases are monotone in slot order, so the mesh stage turns its global workgroup id back into
    (tile, cluster) with a short binary search and no atomics anywhere in the pipeline."""
    return (
      "task_payload %s {\n"
      "  vec4 tile_data[%d];   // xy = tile origin (object XZ)  z = density01  w = cluster base\n"
      "  vec4 tile_meta;       // x = surviving tiles  y = clusters emitted  zw = reserved\n"
      "}\n" % (name, SUPERTILE_SLOTS))

  def task_interface(self, name="tif_ptex_mesh", inherits=()):
    """One tile per workgroup — the decision is workgroup-uniform, so extra lanes would only
    recompute it. Blocks are inherited by the task SHADER (the template's own inherit list), not
    here, so this interface carries nothing but the workgroup shape."""
    inh = "".join(" : %s" % n for n in inherits)
    return ("task_interface %s%s {\n"
            "  inputs { layout(local_size_x = %d, local_size_y = 1, local_size_z = 1); }\n"
            "}\n" % (name, inh, TASK_WG))

  def task_body(self, mvp="mvp"):
    """The amplification decision for one SUPERTILE, ending in EmitMeshTasksEXT.

    LOOP COMPACTION: the workgroup is still one thread (the decision is workgroup-uniform), but it
    now owns an S x S block of tiles and walks them, appending the survivors of radius + frustum +
    HZB to the payload and emitting the SUM of their cluster counts as ONE mesh grid. The dispatch
    is therefore (grid/S)^2 workgroups rather than grid^2, and the mesh workgroups launched are the
    visible set instead of the grid. No shared memory, no barriers and no subgroup ops: the object
    stage's translation is not a place to find out how well those lower.

    NEAR-TO-FAR: payload order is mesh workgroup order is primitive order, so each axis is walked
    from the edge nearest the eye. The carpet draws with no depth prepass (the scene keeps it out
    of that layer, which is also the shadow-caster set), so front-to-back early-z is the only thing
    standing between overlapping blades and full shading of every one of them.

    `mvp` is the clip matrix of the view being rasterized. MONO reads it (and the CamBlk eye)
    directly. STEREO (the template hands the per-view expression) cannot: a task stage has no
    view index, and culling both eyes against the LEFT eye's frustum drops grass out of the
    right one. Design decision 1 (owner, 2026-08-04): the _ST twin culls ONCE against a CENTRE
    camera — the mean of the two per-view matrices, the midpoint of the two eye positions — with
    the tile bound widened by GrassStereo.x to cover the eye separation. One decision, one
    payload, both views.

    Object space throughout: tiles are placed from the camera position and rasterized by
    mvp = vp * m, so the carpet's model matrix is expected to be identity (the terrain
    drawable's convention).
    """
    stereo = "spvr_vp" in mvp
    if stereo:
      clip  = "((spvr_vp[0] + spvr_vp[1]) * 0.5 * m)"
      eye   = "(0.5 * (spvr_eyepos[0].xyz + spvr_eyepos[1].xyz))"
      widen = " + GrassStereo.x"
    else:
      clip  = mvp
      eye   = "c_eye.xyz"
      widen = ""
    return (
      "float tile   = GrassField.x;\n"
      "float grid   = GrassField.y;\n"
      "float cull_r = GrassField.z;\n"
      "vec3  eye    = %(EYE)s;\n"
      "float dim    = g_meta.x;\n"
      "float extent = g_meta.y;\n"
      "// the LIVE supertile edge, clamped to the baked payload capacity: the drawable owns the\n"
      "// number and the shader owns the ceiling, so the two can never disagree into an overrun.\n"
      "uint  S      = clamp(uint(GrassCull.x + 0.5), 1u, %(SUP)su);\n"
      "// CAMERA-RELATIVE GRID: the dispatch is a fixed (grid/S) x (grid/S), and the tile lattice\n"
      "// SNAPS to the eye — the field follows the walker with zero host updates, and snapping is\n"
      "// what keeps a tile's blades still until the camera crosses a whole tile (an unsnapped\n"
      "// origin re-seeds every blade every frame: a boiling carpet).\n"
      "vec2 snapped = floor(eye.xz / tile) * tile;\n"
      "vec2 sbase   = vec2(gl_WorkGroupID.xy * S);\n"
      "vec2 sctr    = snapped + (sbase + vec2(0.5 * float(S)) - vec2(0.5 * grid)) * tile;\n"
      "// walk each axis from the edge nearest the eye (see the docstring): front-to-back.\n"
      "bool revx    = (eye.x > sctr.x);\n"
      "bool revy    = (eye.z > sctr.y);\n"
      "uint nsurv   = 0u;\n"
      "uint total   = 0u;\n"
      "for (uint ry = 0u; ry < S; ry++) {\n"
      "  for (uint rx = 0u; rx < S; rx++) {\n"
      "    uint lx  = revx ? (S - 1u - rx) : rx;\n"
      "    uint ly  = revy ? (S - 1u - ry) : ry;\n"
      "    vec2 gid = sbase + vec2(float(lx), float(ly));\n"
      "    // a supertile straddling the grid edge owns fewer than S*S tiles.\n"
      "    if (gid.x >= grid || gid.y >= grid) { continue; }\n"
      "    vec2 org = snapped + (gid - vec2(0.5 * grid)) * tile;\n"
      "    vec2 ctr = org + vec2(0.5 * tile);\n"
      "    vec4 fs  = grass_field(ctr, dim, extent);\n"
      "    vec3 centre = vec3(ctr.x, fs.x + GrassBlade.x * 0.5, ctr.y);\n"
      "    float dist  = length(centre - eye);\n"
      "    // DENSITY: the baked field, scaled, then a SMOOTH distance falloff — the carpet thins\n"
      "    // continuously toward the cull radius instead of ending at a visible circle. Written\n"
      "    // as 1 - smoothstep(near, far) because GLSL leaves smoothstep undefined for e0 >= e1.\n"
      "    float density = clamp(fs.y * GrassField.w, 0.0, 1.0);\n"
      "    density *= pow(1.0 - smoothstep(GrassLod.x, cull_r, dist), max(GrassLod.z, 1.0e-3));\n"
      "    // off the baked field there is no data to place grass from -> emit nothing.\n"
      "    if (max(abs(ctr.x), abs(ctr.y)) > 0.5 * extent) { continue; }\n"
      "    if (dist > cull_r) { continue; }\n"
      "    // the tile bound: half-diagonal plus the tallest blade the height variance allows —\n"
      "    // the per-blade variance AND the per-clump one, or a tall tussock is culled on screen.\n"
      "    float tall   = GrassBlade.x * (1.0 + GrassBlade.y) * (1.0 + max(GrassClump2.x, 0.0));\n"
      "    float radius = 0.70711 * tile + tall%(WIDEN)s;\n"
      "    if (grass_outside_sides(%(CLIP)s, centre, radius)) { continue; }\n"
      "    uint clusters = uint(ceil(density * GrassLod.w));\n"
      "    clusters = min(clusters, uint(GrassLod.w));\n"
      "    if (clusters == 0u) { continue; }\n"
      "    // OCCLUSION, last (it is the only test that reads a buffer): the tile's world box is\n"
      "    // its XZ footprint, the tallest blade above the centre height, and the tile's own\n"
      "    // half-diagonal below it as the slope allowance — only the CENTRE of the field is\n"
      "    // tapped here, so the floor has to cover whatever the tile falls away to.\n"
      "    vec3 bmn = vec3(org.x,        fs.x - 0.70711 * tile, org.y);\n"
      "    vec3 bmx = vec3(org.x + tile, fs.x + tall,           org.y + tile);\n"
      "    if (GrassCull.z > 0.5 && grass_hzb_occluded(%(CLIP)s, c_misc, bmn, bmx, GrassCull.y))\n"
      "      { continue; }\n"
      "    pld_ptex_mesh.tile_data[nsurv] = vec4(org.x, org.y, density, float(total));\n"
      "    total += clusters;\n"
      "    nsurv += 1u;\n"
      "  }\n"
      "}\n"
      "pld_ptex_mesh.tile_meta = vec4(float(nsurv), float(total), 0.0, 0.0);\n"
      "// emitted-workgroup telemetry, armed by the drawable (GrassCull.w; on by default, it feeds\n"
      "// the player's perf HUD). One atomic per supertile, none when disarmed — workgroup-uniform.\n"
      "if (GrassCull.w > 0.5) { atomicAdd(g_emitted, total); }\n"
      "// the dispatch verb. total == 0 cancels the whole supertile — no mesh workgroup runs.\n"
      "EmitMeshTasksEXT(total, 1u, 1u);"
      % dict(EYE=eye, CLIP=clip, WIDEN=widen, SUP=SUPERTILE))

  # ---- MESH STAGE ------------------------------------------------------------------------------
  def mesh_interface(self, name="vif_ptex_mesh", storage=FIELD_BLOCK_NAME, outputs="", inherits=()):
    """The mesh stage stands in for the vertex stage and rides a vertex_interface: `inputs` is the
    workgroup shape (one invocation per blade), `outputs` the EXT topology/limits line plus the
    per-vertex varyings the template supplies — the SAME varyings in the SAME order as the pull-VS
    interface, or the two paths' locations desynchronize.

    The task payload is NOT inherited here: a task_payload inheritance resolves only on a mesh or
    task SHADER node (shadlang_ast_sema.cpp:1021-1045) and is silently dropped on a pipeline
    interface. The template puts it on the stages."""
    inh  = "".join(" : %s" % n for n in (inherits if inherits else (storage,)))
    outs = ""
    for line in outputs.strip().splitlines():
      outs += "    %s\n" % line.strip()
    return (
      "vertex_interface %s%s {\n"
      "  inputs { layout(local_size_x = %d, local_size_y = 1, local_size_z = 1); }\n"
      "  outputs {\n"
      "    layout(triangles, max_vertices = %d, max_primitives = %d);\n"
      "%s"
      "  }\n"
      "}" % (name, inh, BLADES_PER_WG, MAX_VERTICES, MAX_PRIMITIVES, outs))

  def mesh_body(self, mvp="mvp", varying_writes=""):
    """One workgroup = one cluster of BLADES_PER_WG blades, one invocation each.

    `mvp` names the clip matrix expression; `varying_writes` is the template's per-vertex varying
    text with $V standing for the emitted vertex index, running with the SAME locals the pull VS
    leaves behind (position/normal/binormal/uv0/uv0z/vtxcolor). The COLOR stage and the DEPTH
    PREPASS stage are this same body under different varying writes, so their positions agree
    bit for bit and the prepass cannot z-fight its own colour pass."""
    vw = ""
    for line in varying_writes.strip().splitlines():
      vw += "    %s\n" % line.strip().replace("$V", "vi")
    return (
      "uint b       = gl_LocalInvocationID.x;\n"
      "// PAYLOAD DEMUX: the task stage emitted ONE mesh grid for a whole supertile, so this\n"
      "// workgroup's id is a global cluster index over the survivors it packed. tile_data.w holds\n"
      "// each survivor's running cluster BASE, monotone in slot order, so the owning tile is the\n"
      "// last slot whose base has not passed us — found by BINARY search (the array is sorted by\n"
      "// construction, and this runs in every one of tens of thousands of mesh workgroups, where\n"
      "// a linear walk's average half-array costs an order of magnitude more payload reads).\n"
      "uint lo = 0u;\n"
      "uint hi = uint(pld_ptex_mesh.tile_meta.x);\n"
      "while ((hi - lo) > 1u) {\n"
      "  uint mid = (lo + hi) >> 1u;\n"
      "  if (uint(pld_ptex_mesh.tile_data[mid].w) <= gl_WorkGroupID.x) { lo = mid; } else { hi = mid; }\n"
      "}\n"
      "vec4  tdat   = pld_ptex_mesh.tile_data[lo];\n"
      "uint cluster = gl_WorkGroupID.x - uint(tdat.w);\n"
      "// the tile SIZE and the tile ID are re-derived rather than carried: the size is a bound\n"
      "// param this stage already reads, and the id is grass_cell_id of the tile centre — the same\n"
      "// call on the same world lattice the task stage made, so the blades hash identically.\n"
      "float tile   = GrassField.x;\n"
      "float density = tdat.z;\n"
      "uint  tile_id = grass_cell_id(tdat.xy + vec2(0.5 * tile), tile);\n"
      "// FRACTIONAL DENSITY, workgroup-uniform: blade index N is the same blade however many\n"
      "// clusters the task launched, so a thinning tile drops its LAST blades ONE AT A TIME.\n"
      "// Cutting whole clusters instead is what makes LOD grass pop.\n"
      "float want = density * GrassLod.w * %(BPW)s.0;\n"
      "uint  kept = uint(clamp(want - float(cluster * %(BPW)su), 0.0, %(BPW)s.0));\n"
      "SetMeshOutputsEXT(kept * %(VPB)su, kept * %(PPB)su);\n"
      "if (b >= kept) { return; }\n"
      "uint blade = cluster * %(BPW)su + b;\n"
      "uint seed  = tile_id * 977u + blade * 31u;\n"
      "// ROOT: scattered in the tile, then conformed to the ground the field describes.\n"
      "vec2 root_xz = tdat.xy\n"
      "             + vec2(grass_hash11(seed), grass_hash11(seed + 7919u)) * tile;\n"
      "vec4 fs   = grass_field(root_xz, g_meta.x, g_meta.y);\n"
      "vec3 root = vec3(root_xz.x, fs.x, root_xz.y);\n"
      "vec3 up   = grass_ground_normal(root_xz, g_meta.x, g_meta.y);\n"
      "// PER-BLADE dryness from the same tap that placed the root — finer than the payload's\n"
      "// tile-level figure, and free (the tap is already here). Same bias function, so a blade\n"
      "// can never disagree with the tile it grew in about what the knob means.\n"
      "float dryness = grass_dryness(fs.z, GrassDry.x);\n"
      "// CLUMP: neighbours inside one clump radius share a lean, a wind phase and a hue, which is\n"
      "// what makes a field read as tussocks rather than as uniform noise.\n"
      "uint  clump    = grass_cell_id(root_xz, GrassClump.x);\n"
      "float lean_dir = grass_hash11(clump) * 6.2831853\n"
      "               + (grass_hash11(seed + 32452843u) - 0.5) * GrassClump.y;\n"
      "float phase    = grass_hash11(clump + 15485863u) * GrassClump.z;\n"
      "float hue      = (grass_hash11(clump + 104729u) - 0.5) * GrassClump.w\n"
      "               + (grass_hash11(seed + 104729u) - 0.5) * GrassClump.w * 0.5;\n"
      "// CLUMP HEIGHT: one tussock scale shared by the whole clump. GrassBlade.y is hashed per\n"
      "// BLADE, so raising it only makes isolated spikes; a field reads as tussocks when whole\n"
      "// clumps stand tall or low together, which needs a factor hashed on the SAME clump cell\n"
      "// as the lean/phase/hue above. GrassClump2.x is the amplitude (0 = every clump the same\n"
      "// height, i.e. exactly the per-blade-only carpet); the factor is floored at zero so an\n"
      "// amplitude past 1 flattens a clump instead of inverting the blades through the ground.\n"
      "float clump_h01 = grass_hash11(clump + 27644437u);\n"
      "float clump_amp = max(GrassClump2.x, 0.0);\n"
      "float clump_h   = max(mix(1.0 - clump_amp, 1.0 + clump_amp, clump_h01), 0.0);\n"
      "float h        = GrassBlade.x * (1.0 + (grass_hash11(seed + 15485863u) - 0.5) * 2.0 * GrassBlade.y)\n"
      "               * clump_h;\n"
      "// the blade frame: up is the GROUND normal (grass stands off the slope it grows on, which\n"
      "// is the whole reason the field carries a normal), bend is the lean direction projected\n"
      "// into that ground plane, side is the ribbon's width axis.\n"
      "vec3 lean3 = vec3(cos(lean_dir), 0.0, sin(lean_dir));\n"
      "vec3 bend  = lean3 - up * dot(lean3, up);\n"
      "bend       = (length(bend) > 1.0e-4) ? normalize(bend) : vec3(1.0, 0.0, 0.0);\n"
      "vec3 side  = normalize(cross(bend, up));\n"
      "float bend_amt = GrassClump.y * h;\n"
      "for (uint k = 0u; k < %(VPB)su; k++) {\n"
      "    uint  ring = k >> 1u;\n"
      "    float sgn  = ((k & 1u) == 0u) ? -1.0 : 1.0;\n"
      "    float t    = float(ring) / %(SEG)s.0;\n"
      "    // QUADRATIC BEND: the base stays planted and the tip carries the whole lean.\n"
      "    vec3 spine = root + up * (h * t) + bend * (bend_amt * t * t);\n"
      "    // wind on the SAME curve and the SAME clock as the trees (the Wind primitive's own\n"
      "    // function): phased per clump, weighted by height up the blade so the root holds.\n"
      "    spine += hm_wind(root, h * t, WindDir.xyz, WindParams.x, WindParams.y, Time.x,\n"
      "                     vec3(phase, 0.0, 0.0));\n"
      "    float halfw = GrassBlade.z * 0.5 * (1.0 - GrassBlade.w * t);\n"
      "    vec4 position = vec4(spine + side * (halfw * sgn), 1.0);\n"
      "    // the emitted normal is the ribbon's own face normal (d(spine)/dt x side), so the\n"
      "    // blades are lit as curved surfaces instead of as flat cards.\n"
      "    vec3 tangent  = normalize(up * h + bend * (2.0 * bend_amt * t));\n"
      "    vec3 normal   = normalize(cross(tangent, side));\n"
      "    vec3 binormal = side;\n"
      "    vec2 uv0      = vec2(sgn * 0.5 + 0.5, t);\n"
      "    float uv0z    = 0.0;\n"
      "    vec4 vtxcolor = vec4(hue, dryness, t, 1.0);\n"
      "    uint vi = b * %(VPB)su + k;\n"
      "    gl_MeshVerticesEXT[vi].gl_Position = %(MVP)s * position;\n"
      "%(VARY)s"
      "}\n"
      "// two triangles per segment. Winding is irrelevant: a blade is a two-sided ribbon and the\n"
      "// carpet's rasterstate is cull=off (GRASS_RASTER).\n"
      "for (uint s = 0u; s < %(SEG)su; s++) {\n"
      "    uint v0 = b * %(VPB)su + s * 2u;\n"
      "    uint p0 = b * %(PPB)su + s * 2u;\n"
      "    gl_PrimitiveTriangleIndicesEXT[p0 + 0u] = uvec3(v0 + 0u, v0 + 1u, v0 + 2u);\n"
      "    gl_PrimitiveTriangleIndicesEXT[p0 + 1u] = uvec3(v0 + 2u, v0 + 1u, v0 + 3u);\n"
      "}"
      % dict(BPW=BLADES_PER_WG, VPB=VERTS_PER_BLADE, PPB=PRIMS_PER_BLADE, SEG=SEGMENTS,
             MVP=mvp, VARY=vw))

  # ---- material delegation ----------------------------------------------------------------------
  def as_material_kwargs(self):
    """The dict spliced into Ptex3d(vertex_source=...) -> materialize_surface_fxv2(ssbo_*).

    `mesh_source=self` is the opt-in that makes the template ask for the mesh interface/body, and
    wants_task_stage then adds the task payload + interface + the two task shaders in front of
    both mesh techniques. The raster state (GRASS_RASTER) is NOT here — it belongs to the surface
    DSL, whose own raster dict would collide with it."""
    return dict(
      ssbo_layout="",                       # sif_ptex_vtx is unused: the field lives in its own block
      ssbo_extra_blocks=_FIELD_BLOCK + _HZB_BLOCK + _STATS_BLOCK,
      ssbo_lib=_LIB,
      ssbo_vs_inherits=(FIELD_BLOCK_NAME, HZB_BLOCK_NAME, STATS_BLOCK_NAME, "ublk_ptex_params"),
      ssbo_vs_body=_PULL_VS_BODY,
      ssbo_compute="",
      mesh_source=self)


###############################################################################
# GrassSurface — the FRAGMENT half. Everything it reads is either a varying the mesh stage
# above writes (vtxcolor = hue offset / dryness / height-along-blade) or a ublk_ptex_params
# member the drawable binds, so the whole look is a param rebind and the generated text
# carries no authored value (A8).
###############################################################################

class GrassSurface(Ptex3d):
  """Forward-PBR shading for a task+mesh grass blade.

  The colour params are the SAME UBO members GrassFieldSource declares (dedup is by name, and
  the surface's spec wins), so a carpet is authored by passing one dict of values to both —
  never by tuning two copies. GRASS_RASTER is splatted here because the ptex3d DSL emits its
  own rasterstate from surface(): the source cannot also contribute one.

  vtxcolor (per blade, from the mesh stage): x = clump hue offset, y = dryness01,
  z = 0 at the root .. 1 at the tip, w = 1.
  """

  def __init__(self, ctx, *,
               color_a=(0.16, 0.30, 0.09, 1.0),
               color_b=(0.30, 0.46, 0.14, 1.0),
               dry_color=(0.45, 0.40, 0.18, 1.0),
               dry=(0.25, 0.35, 0.9, 0.6),
               ground_color=(0.22, 0.21, 0.13, 1.0),
               fade=(32.0, 64.0, 0.85, 0.0),
               surf=(0.8, 0.45, 4.0, 0.35),
               shadow_filter=1.0,
               light_up_blend=0.5,
               amb_shadow=0.4):
    ca   = ctx.param("GrassColorA", color_a)      # lush hue A
    cb   = ctx.param("GrassColorB", color_b)      # lush hue B (the clump hue offset mixes A<->B)
    cdry = ctx.param("GrassDryColor", dry_color)
    dp   = ctx.param("GrassDry", dry)             # x fraction (task/mesh side) y,z dry mix window w backlit
    gnd  = ctx.param("GrassGround", ground_color) # v1 params-only ground palette (design decision 3)
    fd   = ctx.param("GrassFade", fade)           # x fade start (m) y fade end (m) z max ground mix
    sf   = ctx.param("GrassSurf", surf)           # x roughness y tip gain z backlit exponent w root AO
    # REDUCED SUN-SHADOW FILTER (1 = on, 0 = the full PCSS evaluator) — a live param,
    # so the trade is an A/B at runtime and not a rebuild. The carpet is the one surface
    # in a scene that both covers most of the lower frame AND carries detail an order of
    # magnitude finer than a shadow texel: the blocker search and the tent lattice spend
    # up to 45 fetches per fragment computing a penumbra that a blade's own silhouette
    # noise hides. It still receives the same cascades.
    shf  = ctx.param("GrassShadowFilter", shadow_filter)
    # LIGHTING NORMAL UP-BLEND (0 = raw blade normal, 1 = world up) — a live param.
    # Blade face normals sit near edge-on to a high sun, which starves the direct
    # term (the only shadow-carrying term) to invisibility; carpets light like the
    # ground they stand on. 0.5 = owner call 2026-08-07 ("split the difference"
    # after the world-up live probe read a bit dark).
    lub  = ctx.param("GrassLightUpBlend", light_up_blend)
    # CASCADE->AMBIENT weight, the carpet's OWN (live param): ambient dominates a
    # blade's color, so the carpet needs a far deeper ambient bite inside cascade
    # shadow than the scene-wide sun dial, which stays terrain-tuned. 0.4/0.0 split
    # = owner look calls 2026-08-07.
    gam  = ctx.param("GrassAmbShadow", amb_shadow)

    hue   = ctx.Cd.x
    dryv  = ctx.Cd.y
    t     = ctx.Cd.z                              # 0 root .. 1 tip

    # ALONG THE BLADE: tips catch more light and are older/paler than the sheath at the root;
    # one gain drives both the albedo lift and the root's contact darkening.
    tip   = 1.0 - sf.y + sf.y * 2.0 * t
    base  = P.mix(ca.xyz, cb.xyz, P.saturate(hue * 2.0 + 0.5)) * tip
    # DRY MIX: the baked dryness through the same window the field was authored against.
    dryw  = P.smoothstep(dp.y, dp.z, dryv)
    albedo = P.mix(base, cdry.xyz * tip, dryw)

    # DISTANCE: the carpet must DISSOLVE into the ground it stands on before the task stage's
    # cull radius drops it — an abrupt colour edge at the cull ring is the tell of a fake field.
    dist   = P.length(ctx.P - ctx.eye)
    fadew  = P.smoothstep(fd.x, fd.y, dist) * fd.z
    albedo = P.mix(albedo, gnd.xyz, fadew)

    # BACKLIT (v1 approximation, owner design decision 2): a blade seen down-sun glows from the
    # light coming THROUGH it. ctx.sun_dir is the TRAVEL direction, so looking along it is
    # looking at the lit back face; the tip is thinnest, so it transmits most. Gated on
    # ctx.has_sun — the block is unbound in the prepass/capture passes.
    view   = P.normalize(ctx.P - ctx.eye)
    trans  = P.pow(P.saturate(P.dot(view, ctx.sun_dir)), sf.z)
    glow   = albedo * ctx.sun_color * ctx.sun_intensity * ctx.has_sun \
             * (trans * dp.w * (0.35 + 0.65 * t) * 0.25)

    # AO: a blade is buried in its own canopy at the root and open at the tip.
    ao = sf.w + (1.0 - sf.w) * t

    lit_n = P.normalize(P.mix(ctx.N, P.vec3(0.0, 1.0, 0.0), P.saturate(lub.x)))
    self.surface(albedo=albedo, metallic=0.0, roughness=sf.x, ao=ao, emissive=glow,
                 normal=lit_n,
                 shadow_filter=shf,
                 shadow_ambient=gam.x,
                 **GRASS_RASTER)


__all__ = ["GrassFieldSource", "GrassSurface", "GRASS_RASTER", "FIELD_BLOCK_NAME",
           "HZB_BLOCK_NAME", "STATS_BLOCK_NAME", "SEGMENTS", "VERTS_PER_BLADE", "PRIMS_PER_BLADE", "BLADES_PER_WG",
           "MAX_VERTICES", "MAX_PRIMITIVES", "TASK_WG", "SUPERTILE", "SUPERTILE_SLOTS"]
