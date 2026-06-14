###############################################################################
# GPU chunked-terrain vertex source for FWD_SSBO_CUSTOM.
#
# The TERRAIN side of the FWD_SSBO_CUSTOM delegation (see ptex3d/fxv2_template.py +
# project memory project_fwd_ssbo_custom): given the bake grid (dim/extent/height) and a
# chunk size, this is the SINGLE SOURCE OF TRUTH for the GPU geometry contract — it emits
#   * the std430 SSBO layout (CamBlk + indirect args + visible-chunk list + heights),
#   * a helper libblock (terr_pos reads heights[]),
#   * the pull VS body (decode gl_VertexID -> position/normal/binormal/uv0/vtxcolor; the VS
#     IS the gen — no materialized vertex array), and
#   * the reset/cull/finalize compute that fills the visible-chunk list + the draw command.
# All four are spliced into the generated ptex3d PBR material (so the compute shares sif_ptex_vtx's
# merged @0 binding with the VS — no compute-only fallback-binding bug), and the same Python object
# tells the ComputeDrawable consumer the buffer size, the heights upload offset, the cam/args
# offsets, and the per-pass dispatch sizes.
#
# Geometry matches HeightField._build_terrain_mesh_arrays' sampling convention (texel-center world
# coords, height = heights[row*DIM+col] * HEIGHT_M) so collider/camera-follow and the visual align.
###############################################################################


def _f(x):
  """GLSL float literal."""
  return repr(float(x))


class TerrainChunkVertexSource:
  """FWD_SSBO_CUSTOM vertex source for square-chunk, 2D-frustum-culled terrain.

  layout (std430, one SSBO shared by the VS + the reset/cull/finalize compute):
    mat4 c_vp; mat4 c_ivp; vec4 c_eye; vec4 c_misc;   // CamBlk @0   (ComputeDrawable.setCameraParams)
    uint a_vc,a_ic,a_fv,a_fi;                          // VkDrawIndirectCommand @ARGS_OFF
    uint v_count,_,_,_;                                // visible-chunk header @VIS_OFF
    uint v_list[NCHUNK];                               // visible chunk indices @VLIST_OFF
    float heights[DIM*DIM];                            // heightfield @HEIGHTS_OFF (CPU upload)
  """

  def __init__(self, dim, extent_m, height_m, chunk=128):
    self.dim    = int(dim)
    self.extent = float(extent_m)
    self.hscale = float(height_m)
    self.chunk  = int(chunk)
    self.cps    = (self.dim + self.chunk - 1) // self.chunk     # chunks per side
    self.nchunk = self.cps * self.cps
    self.vpc    = self.chunk * self.chunk * 6                   # verts per (full) chunk
    self.maxv   = self.nchunk * self.vpc
    self.dimsq  = self.dim * self.dim
    # std430 byte offsets
    self.CAM_OFF     = 0
    self.ARGS_OFF    = 160                                      # after CamBlk (64+64+16+16)
    self.VIS_OFF     = 176
    self.VLIST_OFF   = 192
    self.HEIGHTS_OFF = self.VLIST_OFF + self.nchunk * 4
    self.TOTAL       = self.HEIGHTS_OFF + self.dimsq * 4

  # ---- consts as GLSL tokens -------------------------------------------------
  @property
  def _D(self):  return "%du" % self.dim
  @property
  def _C(self):  return "%du" % self.chunk
  @property
  def _CP(self): return "%du" % self.cps
  @property
  def _NC(self): return "%du" % self.nchunk
  @property
  def _VP(self): return "%du" % self.vpc

  # ---- shader fragments ------------------------------------------------------
  @property
  def layout(self):
    return (
      "mat4 c_vp; mat4 c_ivp; vec4 c_eye; vec4 c_misc;   // CamBlk @0\n"
      "uint a_vc; uint a_ic; uint a_fv; uint a_fi;        // VkDrawIndirectCommand @%d\n"
      "uint v_count; uint v_p0; uint v_p1; uint v_p2;     // visible-chunk header @%d\n"
      "uint v_list[%d];                                   // visible chunk indices\n"
      "float heights[%d];                                 // heightfield (CPU upload @%d)"
      % (self.ARGS_OFF, self.VIS_OFF, self.nchunk, self.dimsq, self.HEIGHTS_OFF))

  @property
  def lib(self):
    # terr_pos: texel-center world coord + height tap (matches _build_terrain_mesh_arrays).
    return (
      "vec3 terr_pos(uint tx, uint tz) {\n"
      "  uint cx = min(tx, %s - 1u);\n"
      "  uint cz = min(tz, %s - 1u);\n"
      "  float x = ((float(tx) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float z = ((float(tz) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float y = heights[cz * %s + cx] * %s;\n"
      "  return vec3(x, y, z);\n"
      "}" % (self._D, self._D, self._D, _f(self.extent),
             self._D, _f(self.extent), self._D, _f(self.hscale)))

  @property
  def vs_body(self):
    # decode gl_VertexID -> (tx,tz) corner of a cell in a visible chunk; analytic normal from taps.
    return (
      "uint gi = uint(gl_VertexID);\n"
      "uint slot = gi / %s;\n"
      "uint chunk = v_list[slot];\n"
      "uint ccx = chunk %% %s;\n"
      "uint ccz = chunk / %s;\n"
      "uint local = gi %% %s;\n"
      "uint cell = local / 6u;\n"
      "uint corner = local %% 6u;\n"
      "uint lx = cell %% %s;\n"
      "uint lz = cell / %s;\n"
      "uint baseC = ccx * %s + lx;\n"
      "uint baseR = ccz * %s + lz;\n"
      "uint dR; uint dC;\n"
      "if      (corner == 0u) { dR = 0u; dC = 0u; }\n"
      "else if (corner == 1u) { dR = 1u; dC = 0u; }\n"
      "else if (corner == 2u) { dR = 0u; dC = 1u; }\n"
      "else if (corner == 3u) { dR = 0u; dC = 1u; }\n"
      "else if (corner == 4u) { dR = 1u; dC = 0u; }\n"
      "else                   { dR = 1u; dC = 1u; }\n"
      "uint tx = baseC + dC;\n"
      "uint tz = baseR + dR;\n"
      "vec3 P   = terr_pos(tx, tz);\n"
      "uint txl = tx > 0u ? tx - 1u : tx;\n"
      "uint txr = (tx + 1u) < %s ? tx + 1u : tx;\n"
      "uint tzl = tz > 0u ? tz - 1u : tz;\n"
      "uint tzr = (tz + 1u) < %s ? tz + 1u : tz;\n"
      "vec3 dx  = terr_pos(txr, tz) - terr_pos(txl, tz);\n"
      "vec3 dz  = terr_pos(tx, tzr) - terr_pos(tx, tzl);\n"
      "vec3 nrm = normalize(cross(dz, dx));\n"
      "if (nrm.y < 0.0) { nrm = -nrm; }\n"
      "vec4 position = vec4(P, 1.0);\n"
      "vec3 normal   = nrm;\n"
      "vec3 binormal = normalize(dx);\n"
      "vec2 uv0      = vec2((float(tx) + 0.5) / float(%s), (float(tz) + 0.5) / float(%s));\n"
      "vec4 vtxcolor = vec4(1.0);"
      % (self._VP, self._CP, self._CP, self._VP, self._C, self._C, self._C, self._C,
         self._D, self._D, self._D, self._D))

  @property
  def compute(self):
    # reset -> cull (2D frustum test per chunk, append survivors) -> finalize (vertexCount).
    return (
      "compute_interface cif_terrain : sif_ptex_vtx { inputs { layout(local_size_x = 64); } }\n"
      "////////////////////////////////////////\n"
      "compute_shader cs_terrain_reset : cif_terrain {\n"
      "  v_count = 0u; a_vc = 0u; a_ic = 1u; a_fv = 0u; a_fi = 0u;\n"
      "}\n"
      "////////////////////////////////////////\n"
      "compute_shader cs_terrain_cull : cif_terrain {\n"
      "  uint ci = gl_GlobalInvocationID.x;\n"
      "  if (ci >= %s) { return; }\n"
      "  uint cx = ci %% %s;\n"
      "  uint cz = ci / %s;\n"
      "  float x0 = ((float(cx * %s) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float x1 = ((float((cx + 1u) * %s) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float z0 = ((float(cz * %s) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float z1 = ((float((cz + 1u) * %s) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  vec3 mn = vec3(x0, 0.0, z0);\n"
      "  vec3 mx = vec3(x1, %s, z1);\n"
      "  vec4 rx = vec4(c_vp[0].x, c_vp[1].x, c_vp[2].x, c_vp[3].x);\n"
      "  vec4 ry = vec4(c_vp[0].y, c_vp[1].y, c_vp[2].y, c_vp[3].y);\n"
      "  vec4 rz = vec4(c_vp[0].z, c_vp[1].z, c_vp[2].z, c_vp[3].z);\n"
      "  vec4 rw = vec4(c_vp[0].w, c_vp[1].w, c_vp[2].w, c_vp[3].w);\n"
      "  vec4 pl[6];\n"
      "  pl[0] = rw + rx; pl[1] = rw - rx; pl[2] = rw + ry; pl[3] = rw - ry; pl[4] = rz; pl[5] = rw - rz;\n"
      "  bool inside = true;\n"
      "  for (int p = 0; p < 6; p++) {\n"
      "    vec3 pv = vec3(pl[p].x >= 0.0 ? mx.x : mn.x, pl[p].y >= 0.0 ? mx.y : mn.y, pl[p].z >= 0.0 ? mx.z : mn.z);\n"
      "    if ((dot(pl[p].xyz, pv) + pl[p].w) < 0.0) { inside = false; }\n"
      "  }\n"
      "  if (inside) { uint slot = atomicAdd(v_count, 1u); v_list[slot] = ci; }\n"
      "}\n"
      "////////////////////////////////////////\n"
      "compute_shader cs_terrain_finalize : cif_terrain {\n"
      "  a_vc = v_count * %s; a_ic = 1u; a_fv = 0u; a_fi = 0u;\n"
      "}"
      % (self._NC, self._CP, self._CP,
         self._C, self._D, _f(self.extent), self._C, self._D, _f(self.extent),
         self._C, self._D, _f(self.extent), self._C, self._D, _f(self.extent),
         _f(self.hscale), self._VP))

  # ---- material delegation ---------------------------------------------------
  def as_material_kwargs(self):
    """The dict spliced into Ptex3d(vertex_source=...) -> materialize_surface_fxv2(ssbo_*)."""
    return dict(ssbo_layout=self.layout,
                ssbo_lib=self.lib,
                ssbo_vs_body=self.vs_body,
                ssbo_compute=self.compute)

  # ---- ComputeDrawable consumer contract ------------------------------------
  def compute_passes(self):
    """[(shader_name, groups_x, groups_y, groups_z), ...] in run order; storageBarrier between."""
    cull_groups = (self.nchunk + 63) // 64
    return [("cs_terrain_reset",    1,           1, 1),
            ("cs_terrain_cull",     cull_groups, 1, 1),
            ("cs_terrain_finalize", 1,           1, 1)]
