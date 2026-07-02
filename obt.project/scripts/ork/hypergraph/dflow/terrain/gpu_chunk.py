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
    uint v_count,v_frustum_count,u_dim,v_total_count;  // visible-chunk header @VIS_OFF (u_dim=runtime grid dim)
    uint v_list[NCHUNK];                               // visible chunk indices @VLIST_OFF
    float heights[DIM*DIM];                            // heightfield @HEIGHTS_OFF (CPU upload)
  """

  def __init__(self, dim, extent_m, height_m, chunk=128, y_bias=0.0, bake_dim=None, relax=False):
    self.dim    = int(dim)
    self.extent = float(extent_m)
    self.hscale = float(height_m)
    self.chunk  = int(chunk)
    # RELAXED-UV mode (the slope-stretch fix). When on, the per-vertex SSBO array is INTERLEAVED
    # (stride 8: height, relaxed_uv.xy, normal.xz, binormal.xyz) instead of mono heights (stride 1),
    # and the VS reads uv0 + the precomputed tangent frame from it (dropping the live finite-diff taps).
    # The dflow RelaxUvModule bakes the frame; the C++ fills vtxdata. relax=False is byte-identical to before.
    self.relax   = bool(relax)
    self._vstride = 8 if self.relax else 1
    self.ybias  = float(y_bias)   # constant world-Y offset on the VISIBLE mesh (physics-vs-render
                                  # alignment). NOT in the normal taps (a constant cancels). Baked.
    # DIM is a RUNTIME uniform (u_dim, in the VIS header) — NOT baked into the shader — so one compiled
    # shader serves BOTH the bake (u_dim = bake_dim) and the render (u_dim = dim) with no recompile when
    # the render dim changes. bake_dim is the MAX grid the shader must serve (render dim <= bake_dim);
    # it sizes the fixed per-chunk array CAP (maxnc) so the std430 offsets stay constant across dims.
    self.bake_dim = int(bake_dim) if bake_dim else self.dim
    assert self.bake_dim >= self.dim, "bake_dim must be >= render dim"
    self.cps    = (self.dim + self.chunk - 1) // self.chunk     # render chunks per side
    self.nchunk = self.cps * self.cps                           # render chunk count (cull dispatch)
    self.vpc    = self.chunk * self.chunk * 6                   # verts per (full) chunk
    self.maxv   = self.nchunk * self.vpc
    self.dimsq  = self.dim * self.dim                           # render heights count
    self.bcps   = (self.bake_dim + self.chunk - 1) // self.chunk  # bake chunks per side
    self.maxnc  = self.bcps * self.bcps                         # MAXNC: fixed per-chunk array cap (bake count)
    # std430 byte offsets. heights[] is the LAST member (RUNTIME-sized: len = u_dim*u_dim of the bound
    # buffer). The fixed-cap per-chunk arrays sit before it so HEIGHTS_OFF is constant regardless of dim.
    # Order chosen for alignment: chunk_y (vec2, needs 8-align) sits FIRST at @192 (8-aligned for any
    # maxnc); v_list (uint) follows; heights (float) last. This is align-clean for any chunk count.
    self.CAM_OFF     = 0
    self.ARGS_OFF    = 160                                      # after CamBlk (64+64+16+16)
    self.VIS_OFF     = 176
    # per-chunk WORLD-Y bounds [minY, maxY] (CPU-computed @materialize) — the TIGHT vertical extent the
    # HZB occlusion test needs (the conservative 0..hscale box never occludes; a valley chunk's real
    # max-Y lets it cull behind a nearer ridge). vec2 stride 8 in std430.
    self.CHUNKY_OFF  = 192                                      # vec2 chunk_y[MAXNC]  (8-aligned @192)
    self.VLIST_OFF   = self.CHUNKY_OFF + self.maxnc * 8         # uint v_list[MAXNC]
    self.HEIGHTS_OFF = self.VLIST_OFF + self.maxnc * 4          # float heights[]/vtxdata[] (runtime, last)
    self.TOTAL       = self.HEIGHTS_OFF + self.dimsq * self._vstride * 4  # render buffer (stride 1 mono / 8 relax)

  # ---- GLSL tokens -----------------------------------------------------------
  # DIM-derived quantities are RUNTIME (read u_dim, the CPU-uploaded grid dim) so the shader is
  # dim-agnostic: _D -> the u_dim field; _CP/_NC -> in-shader locals each function defines from u_dim
  # (see _dim_preamble). chunk-only quantities (_C, _VP) stay compile-time literals — identical for
  # the bake and the render, so baking them costs no recompile flexibility.
  @property
  def _D(self):  return "u_dim"           # runtime grid dimension (VIS-header uniform)
  @property
  def _C(self):  return "%du" % self.chunk
  @property
  def _CP(self): return "cps"             # local: cps = u_dim / CHUNK   (see _dim_preamble)
  @property
  def _NC(self): return "nchunk"          # local: nchunk = cps * cps    (see _dim_preamble)
  @property
  def _VP(self): return "%du" % self.vpc

  def _dim_preamble(self, want_nchunk=True):
    """GLSL locals deriving the runtime DIM quantities from u_dim — prepended to each shader function
    that references _CP/_NC. cps = CEIL(u_dim/CHUNK) (matches Python's (dim+chunk-1)//chunk — the last
    chunk may be partial); nchunk = cps*cps."""
    s = "  uint cps = (u_dim + %du) / %s;\n" % (self.chunk - 1, self._C)
    if want_nchunk:
      s += "  uint nchunk = cps * cps;\n"
    return s

  # ---- shader fragments ------------------------------------------------------
  @property
  def layout(self):
    return (
      "mat4 c_vp; mat4 c_ivp; vec4 c_eye; vec4 c_misc;   // CamBlk @0\n"
      "uint a_vc; uint a_ic; uint a_fv; uint a_fi;        // VkDrawIndirectCommand @%d\n"
      "uint v_count; uint v_frustum_count; uint u_dim; uint v_total_count;  // header @%d (u_dim=runtime grid dim, CPU upload)\n"
      "vec2 chunk_y[%d];                                  // per-chunk world [minY,maxY] @%d (cap MAXNC, CPU upload)\n"
      "uint v_list[%d];                                   // visible chunk indices @%d (cap MAXNC)\n"
      "float heights[];                                   // heightfield @%d (RUNTIME-sized: u_dim*u_dim, CPU upload)"
      % (self.ARGS_OFF, self.VIS_OFF, self.maxnc, self.CHUNKY_OFF,
         self.maxnc, self.VLIST_OFF, self.HEIGHTS_OFF))

  @property
  def lib(self):
    # terr_pos: texel-center world coord + height tap (matches _build_terrain_mesh_arrays).
    # relax mode interleaves the per-vertex array (stride 8) — the height is component 0.
    D = self._D
    hread = ("heights[(cz * %s + cx) * %du]" % (D, self._vstride)) if self.relax \
            else ("heights[cz * %s + cx]" % D)
    return (
      "vec3 terr_pos(uint tx, uint tz) {\n"
      "  uint cx = min(tx, %s - 1u);\n"
      "  uint cz = min(tz, %s - 1u);\n"
      "  float x = ((float(tx) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float z = ((float(tz) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float y = %s * %s + (%s);\n"
      "  return vec3(x, y, z);\n"
      "}" % (D, D, D, _f(self.extent),
             D, _f(self.extent), hread, _f(self.hscale), _f(self.ybias)))

  @property
  def vs_body(self):
    # decode gl_VertexID -> (tx,tz) corner of a cell in a visible chunk.
    head = (
      ("uint cps = (u_dim + %du) / %s;\n" % (self.chunk - 1, self._C)) +  # CEIL(u_dim/CHUNK) runtime cps
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
      % (self._VP, self._CP, self._CP, self._VP, self._C, self._C, self._C, self._C))
    if self.relax:
      # RELAX: precomputed tangent frame + BOTH uv parameterizations from the interleaved per-vertex array
      # (stride 8: [0]=height [1,2]=RELAXED uv [3,4]=normal.x,z [5,6,7]=binormal). normal.y reconstructed
      # +sqrt. uv0 = PLANAR grid uv (the default ctx.uv; channel taps + existing materials, UNCHANGED
      # meaning); ruv = RELAXED uv (the atlas parameterization — the material routes the cap-VS gl_Position
      # + the stored forward frg_uv0 to it). Carrying both lets the material pick per-technique (no overload).
      tail = (
        "vec3 P = terr_pos(tx, tz);\n"
        # CLAMP the frame reads: tx/tz legitimately reach u_dim at the far row/col (the mesh
        # extends half a texel past the last texel center), and unlike terr_pos (which clamps
        # internally) a raw (tz*u_dim+tx) would read the NEXT ROW's texel 0 on the right edge
        # and PAST THE BUFFER on the far row (robustness zeros -> normalize(0) -> NaN TBN).
        # uv0/P/Prelax stay on the unclamped indices (planar uv must reach 1.0 at the edge).
        "uint vx = min(tx, u_dim - 1u);\n"
        "uint vz = min(tz, u_dim - 1u);\n"
        "uint vbase = (vz * u_dim + vx) * 8u;\n"
        "vec2 ruv = vec2(heights[vbase + 1u], heights[vbase + 2u]);   // RELAXED uv (atlas param)\n"
        "vec2 uv0 = vec2((float(tx) + 0.5) / float(%s), (float(tz) + 0.5) / float(%s));   // PLANAR grid uv\n"
        "float _nx = heights[vbase + 3u]; float _nz = heights[vbase + 4u];\n"
        "vec3 normal = vec3(_nx, sqrt(max(0.0, 1.0 - _nx*_nx - _nz*_nz)), _nz);\n"
        "vec3 binormal = vec3(heights[vbase + 5u], heights[vbase + 6u], heights[vbase + 7u]);\n"
        "vec4 position = vec4(P, 1.0);\n"
        # RELAXED world position: worldXZ at the RELAXED uv ((ruv-0.5)*extent), planar height for depth.
        # The cap-VS atlas bake rasterizes this via the ortho mvp (gl_Position = mvp*Prelax) — the SAME
        # projection-matrix path as the forward VS / hypermesh. A DIRECT gl_Position = 2*ruv-1 from an SSBO
        # read is mis-rasterized by MoltenVK below the NDC anti-diagonal; the mvp path is not.
        "vec3 Prelax = vec3((ruv.x - 0.5) * %s, P.y, (ruv.y - 0.5) * %s);\n"
        "vec4 vtxcolor = vec4(1.0);"
        % (self._D, self._D, _f(self.extent), _f(self.extent)))
    else:
      # MONO: live analytic normal/binormal from terr_pos taps + planar uv0 (byte-identical to pre-relax).
      tail = (
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
        % (self._D, self._D, self._D, self._D))
    return head + tail

  @property
  def compute(self):
    # reset -> cull (2D frustum test per chunk, append survivors) -> finalize (vertexCount).
    tw = "(%s / float(u_dim))" % _f(self.extent)   # runtime texel->world scale (was the baked extent/dim)
    return (
      # sif_hzb: the read-only max-depth HZB pyramid (SEPARATE SSBO, bound per-frame by
      # ComputeDrawable::onPreRender). Compute-only (the VS doesn't use it); shares descriptor_set 0,
      # gets a declaration-order binding after sif_ptex_vtx (sif_binding pre-assign).
      "storage_interface sif_hzb (descriptor_set 0) { buffer layout(std430) hzb_blk { float HZB[]; }; }\n"
      "compute_interface cif_terrain : sif_ptex_vtx : sif_hzb { inputs { layout(local_size_x = 64); } }\n"
      "////////////////////////////////////////\n"
      "compute_shader cs_terrain_reset : cif_terrain {\n"
      "%s"  # runtime cps/nchunk preamble (from u_dim)
      "  v_count = 0u; a_vc = 0u; a_ic = 1u; a_fv = 0u; a_fi = 0u;\n"
      "  v_frustum_count = 0u; v_total_count = %s;   // cull funnel counters (u_dim untouched — CPU-uploaded, survives reset)\n"
      "}\n"
      "////////////////////////////////////////\n"
      "compute_shader cs_terrain_cull : cif_terrain {\n"
      "%s"  # runtime cps/nchunk preamble (from u_dim)
      "  uint ci = gl_GlobalInvocationID.x;\n"
      "  if (ci >= %s) { return; }\n"
      "  uint cx = ci %% %s;\n"
      "  uint cz = ci / %s;\n"
      "  float x0 = ((float(cx * %s) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float x1 = ((float((cx + 1u) * %s) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float z0 = ((float(cz * %s) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float z1 = ((float((cz + 1u) * %s) + 0.5) / float(%s) - 0.5) * %s;\n"
      # FRUSTUM box: CONSERVATIVE full height range 0..hscale (a tight per-chunk box wrongly frustum-
      # culls chunks whose real terrain sits below the view planes — e.g. the chunk underfoot, lower
      # than eye level -> a hole). The TIGHT per-chunk box is for the OCCLUSION test only (below).
      "  vec3 mn = vec3(x0, 0.0, z0);\n"
      "  vec3 mx = vec3(x1, %s, z1);\n"
      "  vec4 rx = vec4(c_vp[0].x, c_vp[1].x, c_vp[2].x, c_vp[3].x);\n"
      "  vec4 ry = vec4(c_vp[0].y, c_vp[1].y, c_vp[2].y, c_vp[3].y);\n"
      "  vec4 rz = vec4(c_vp[0].z, c_vp[1].z, c_vp[2].z, c_vp[3].z);\n"
      "  vec4 rw = vec4(c_vp[0].w, c_vp[1].w, c_vp[2].w, c_vp[3].w);\n"
      "  vec4 pl[6];\n"
      "  // CullFrustumScale (c_misc.x, frame-global, from RCFD via ComputeDrawable::onPreRender):\n"
      "  // >1 widens / cull-less, 1.0 exact, <1 narrows. t = 1/scale scales the four SIDE planes\n"
      "  // (same sense as the hypermesh MeshInstCull u_tighten); near/far (pl[4],pl[5]) unchanged.\n"
      "  float t = (c_misc.x > 0.0) ? (1.0 / c_misc.x) : 1.0;\n"
      "  vec4 sx = rx * t; vec4 sy = ry * t;\n"
      "  pl[0] = rw + sx; pl[1] = rw - sx; pl[2] = rw + sy; pl[3] = rw - sy; pl[4] = rz; pl[5] = rw - rz;\n"
      "  bool inside = true;\n"
      "  for (int p = 0; p < 6; p++) {\n"
      "    vec3 pv = vec3(pl[p].x >= 0.0 ? mx.x : mn.x, pl[p].y >= 0.0 ? mx.y : mn.y, pl[p].z >= 0.0 ? mx.z : mn.z);\n"
      "    if ((dot(pl[p].xyz, pv) + pl[p].w) < 0.0) { inside = false; }\n"
      "  }\n"
      "  if (inside) { atomicAdd(v_frustum_count, 1u); }   // passed frustum (pre-occlusion count)\n"
      # HZB occlusion (1-phase): cull the chunk if its NEAREST screen depth is behind the HZB MAX over
      # its screen footprint. c_misc.yzw = HZB base w/h/mips (0 -> HZB unavailable, skip). Same math as
      # the hypermesh MeshInstCull (project the chunk world-AABB mn/mx, y-flip the uv, mip-fit a 2x2
      # fetch, standard-Z compare).
      "  if (inside && c_misc.y > 0.5) {\n"
      "    uint hw = uint(c_misc.y); uint hh = uint(c_misc.z); uint hmips = uint(c_misc.w);\n"
      # TIGHT per-chunk box (chunk_y world min/max) — same XZ as the frustum box but the REAL vertical
      # extent, so a valley chunk can occlude behind a nearer ridge (the conservative 0..hscale box never could).
      "    vec3 omn = vec3(mn.x, chunk_y[ci].x, mn.z);\n"
      "    vec3 omx = vec3(mx.x, chunk_y[ci].y + 0.5, mx.z);\n"
      "    vec3 nmin = vec3(1.0e9); vec3 nmax = vec3(-1.0e9); bool safe = true;\n"
      "    for (int k = 0; k < 8; k++) {\n"
      "      vec3 cor = vec3(((k & 1) != 0) ? omx.x : omn.x, ((k & 2) != 0) ? omx.y : omn.y, ((k & 4) != 0) ? omx.z : omn.z);\n"
      "      vec4 cl = c_vp * vec4(cor, 1.0);\n"
      "      if (cl.w <= 0.0001) { safe = false; }\n"
      "      vec3 nd = cl.xyz / max(cl.w, 0.0001); nmin = min(nmin, nd); nmax = max(nmax, nd);\n"
      "    }\n"
      "    if (safe) {\n"
      "      vec2 uvmn = clamp(vec2(nmin.x * 0.5 + 0.5, 1.0 - (nmax.y * 0.5 + 0.5)), 0.0, 1.0);\n"
      "      vec2 uvmx = clamp(vec2(nmax.x * 0.5 + 0.5, 1.0 - (nmin.y * 0.5 + 0.5)), 0.0, 1.0);\n"
      "      float znr = nmin.z;\n"
      "      float ex = (uvmx.x - uvmn.x) * float(hw); float ey = (uvmx.y - uvmn.y) * float(hh);\n"
      "      int mip = int(ceil(log2(max(max(ex, ey), 1.0)))); mip = clamp(mip, 0, int(hmips) - 1);\n"
      "      uint mw = max(1u, hw >> uint(mip)); uint mh = max(1u, hh >> uint(mip));\n"
      "      uint moff = 0u; uint ow = hw; uint oh = hh;\n"
      "      for (int j = 0; j < mip; j++) { moff += ow * oh; ow = max(1u, ow >> 1u); oh = max(1u, oh >> 1u); }\n"
      "      ivec2 mxd = ivec2(int(mw) - 1, int(mh) - 1);\n"
      "      ivec2 t0 = clamp(ivec2(int(uvmn.x * float(mw)), int(uvmn.y * float(mh))), ivec2(0), mxd);\n"
      "      ivec2 t1 = clamp(ivec2(int(uvmx.x * float(mw)), int(uvmx.y * float(mh))), ivec2(0), mxd);\n"
      "      float occ = HZB[moff + uint(t0.y) * mw + uint(t0.x)];\n"
      "      occ = max(occ, HZB[moff + uint(t0.y) * mw + uint(t1.x)]);\n"
      "      occ = max(occ, HZB[moff + uint(t1.y) * mw + uint(t0.x)]);\n"
      "      occ = max(occ, HZB[moff + uint(t1.y) * mw + uint(t1.x)]);\n"
      # Conservative depth bias: cull only when the chunk's nearest is CLEARLY behind the occluder.
      # Standard-Z is nonlinear (precision collapses near 1.0), so for a far/flat chunk znr and occ are
      # near-equal and float noise flips znr>occ -> swiss-cheese over-cull (visible terrain HOLES). A
      # real occluder (chunk behind a ridge) clears this margin easily; a coplanar/self chunk does not.
      "      if (znr > occ + 0.005) { inside = false; }\n"
      "    }\n"
      "  }\n"
      "  if (inside) { uint slot = atomicAdd(v_count, 1u); v_list[slot] = ci; }\n"
      "}\n"
      "////////////////////////////////////////\n"
      # near-to-far sort for early-Z: draw the closest chunks first so the heavy terrain fragment
      # shader is depth-rejected on the far ones. Single-thread selection sort of the visible list
      # (nchunk is small). Key = chunk-center XZ distance-squared to c_eye; HEIGHT is dropped — it's
      # constant per camera so it can't change the ordering, and dropping it avoids a per-compare
      # heights[] tap (the expensive part in a single-thread O(n^2) loop). In VR c_eye is the head
      # (the VR-aware cull camera), so chunks sort near-to-far from the head.
      "compute_shader cs_terrain_sort : cif_terrain {\n"
      "%s"  # runtime cps preamble (from u_dim)
      "  if (gl_GlobalInvocationID.x != 0u) { return; }\n"   # one thread sorts the whole list
      "  uint n = v_count;\n"
      "  for (uint i = 0u; (i + 1u) < n; i = i + 1u) {\n"
      "    uint bi = i;\n"
      "    uint ca = v_list[i];\n"
      "    float xa = (float((ca %% %s) * %s) + %s) * %s - %s;\n"
      "    float za = (float((ca / %s) * %s) + %s) * %s - %s;\n"
      "    float bd = (xa - c_eye.x) * (xa - c_eye.x) + (za - c_eye.z) * (za - c_eye.z);\n"
      "    for (uint j = i + 1u; j < n; j = j + 1u) {\n"
      "      uint cb = v_list[j];\n"
      "      float xb = (float((cb %% %s) * %s) + %s) * %s - %s;\n"
      "      float zb = (float((cb / %s) * %s) + %s) * %s - %s;\n"
      "      float dj = (xb - c_eye.x) * (xb - c_eye.x) + (zb - c_eye.z) * (zb - c_eye.z);\n"
      "      if (dj < bd) { bd = dj; bi = j; }\n"
      "    }\n"
      "    if (bi != i) { uint t = v_list[i]; v_list[i] = v_list[bi]; v_list[bi] = t; }\n"
      "  }\n"
      "}\n"
      "////////////////////////////////////////\n"
      "compute_shader cs_terrain_finalize : cif_terrain {\n"
      "  a_vc = v_count * %s; a_ic = 1u; a_fv = 0u; a_fi = 0u;\n"
      "}"
      % (self._dim_preamble(),                  # reset preamble (cps -> nchunk)
         self._NC,                              # reset: v_total_count = nchunk
         self._dim_preamble(),                  # cull preamble (cps + nchunk)
         self._NC, self._CP, self._CP,
         self._C, self._D, _f(self.extent), self._C, self._D, _f(self.extent),
         self._C, self._D, _f(self.extent), self._C, self._D, _f(self.extent),
         _f(self.hscale),   # frustum mx.y = conservative full height range
         self._dim_preamble(want_nchunk=False), # sort preamble (cps only)
         # sort args: (CPS, CHUNK, halfchunk, texel->world(runtime tw), halfextent) per coord (xa,za,xb,zb)
         self._CP, self._C, _f(self.chunk / 2.0 + 0.5), tw, _f(0.5 * self.extent),
         self._CP, self._C, _f(self.chunk / 2.0 + 0.5), tw, _f(0.5 * self.extent),
         self._CP, self._C, _f(self.chunk / 2.0 + 0.5), tw, _f(0.5 * self.extent),
         self._CP, self._C, _f(self.chunk / 2.0 + 0.5), tw, _f(0.5 * self.extent),
         self._VP))

  # ---- material delegation ---------------------------------------------------
  def as_material_kwargs(self):
    """The dict spliced into Ptex3d(vertex_source=...) -> materialize_surface_fxv2(ssbo_*)."""
    return dict(ssbo_layout=self.layout,
                ssbo_lib=self.lib,
                ssbo_vs_body=self.vs_body,
                ssbo_compute=self.compute,
                # relax => the vs_body provides a `ruv` local (relaxed uv); the material routes the atlas
                # param (cap-VS gl_Position + stored forward frg_uv0) to it. False => planar uv0 throughout.
                relax_uv=self.relax)

  # ---- ComputeDrawable consumer contract ------------------------------------
  def compute_passes(self):
    """[(shader_name, groups_x, groups_y, groups_z), ...] in run order; storageBarrier between."""
    cull_groups = (self.nchunk + 63) // 64
    return [("cs_terrain_reset",    1,           1, 1),
            ("cs_terrain_cull",     cull_groups, 1, 1),
            ("cs_terrain_sort",     1,           1, 1),   # near-to-far for early-Z
            ("cs_terrain_finalize", 1,           1, 1)]
