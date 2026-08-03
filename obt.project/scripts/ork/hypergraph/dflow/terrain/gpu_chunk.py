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
# coords, height = heights[row*DIM+col] — TRUE METERS) so collider/camera-follow and the visual align.
###############################################################################

import os


def _f(x):
  """GLSL float literal."""
  return repr(float(x))


# Ceiling for a mesh-stage local_size product. VK_EXT_mesh_shader guarantees
# maxMeshWorkGroupInvocations >= 128 and NVIDIA reports exactly 128, so 128 is the largest
# workgroup that is portable without a runtime limits query. Also the per-DIMENSION cap
# glslang enforces, which is why the layout below is flat 1D.
MESH_WG_MAX = 128

# SHIPPED MESHLET DIMENSION — the mesh workgroup's output payload is (n+1)^2 corners / 2n^2 tris.
# BOUND TO terrain_chunk_drawable.h's kTerrainDefaultMeshletDim: the codegen sizes the payload from
# this value and the C++ drawable sizes the dispatch grid from that one, so a split default makes
# the drawable dispatch a grid the shader does not agree with. Move them together or not at all.
# WHY 8 and not the 11 that maxes out the taskless tier (measured jul26, M3 Ultra / MoltenVK, real
# multisampled forward pass): Metal tile memory is shared between the multisampled attachment set
# and the mesh stage's per-workgroup output payload, and mesh cost falls off a CLIFF between
# meshlet 9 and 8. At 8 the real 4x pass BEATS the pull-VS baseline (130.5 vs 120.3 fps); at 11 it
# loses. NVIDIA is payload-flat over the same range (2.646ms at 8 vs 2.638ms at 11, 4x/4K), so the
# smaller payload costs the discrete-GPU platform nothing.
DEFAULT_MESHLET_DIM = 8
# 2n^2 <= 256 max_primitives on the taskless tier -> n <= 11.
MAX_MESHLET_DIM = 11


def _meshmode():
  """ORKID_TERRAIN_MESHSHADER as an INT: 0/absent = baseline pull VS, 1 = direct fixed mesh grid,
  2 = indirect (compacted) mesh dispatch. Mirrors the C++ drawable's atoi read
  (terrain_chunk_drawable.cpp) — the same env decides the codegen and the consumer, so a
  toggle-on run materializes a material the drawable can actually find."""
  v = os.environ.get("ORKID_TERRAIN_MESHSHADER")
  try:
    return int(v) if v else 0
  except ValueError:
    return 0


def _meshlet_dim():
  """ORKID_TERRAIN_MESHLET = meshlet quad dimension n (default DEFAULT_MESHLET_DIM). DIAGNOSTIC
  KNOB for the tile-footprint measurement: n sets the mesh workgroup's output payload ((n+1)^2
  corners, 2n^2 tris) and therefore its tile-memory footprint. Mirrors the C++ drawable's read
  (terrainMeshletDim) — the same env must size the codegen AND the dispatch grid, or the drawable
  dispatches a grid the shader does not agree with. Out-of-range values are REFUSED loudly:
  n>MAX_MESHLET_DIM exceeds the taskless tier's 256 max_primitives."""
  v = os.environ.get("ORKID_TERRAIN_MESHLET")
  if not v:
    return DEFAULT_MESHLET_DIM
  n = int(v)
  if n < 1 or n > MAX_MESHLET_DIM:
    raise ValueError("ORKID_TERRAIN_MESHLET=%d out of range 1..%d (2n^2 <= 256)"
                     % (n, MAX_MESHLET_DIM))
  return n


def _meshcull_granularity():
  """ORKID_TERRAIN_MESHCULL = chunk|meshlet (default meshlet). Selects the frustum-test bound the
  mesh workgroup uses: `chunk` tests the PARENT CHUNK's AABB (all mps^2 workgroups of a chunk agree
  — apples-to-apples with the baseline per-chunk cull granularity), `meshlet` (DEFAULT) tests the
  workgroup's OWN meshlet AABB (finer, strictly less overdraw). STRUCTURAL codegen: the env gates
  the generated mesh body exactly like ORKID_TERRAIN_MESHSHADER gates the mesh stage's existence, so
  a material materialized under one setting carries that granularity (the C++ drawable reads the same
  env only to LOG which was requested). Returns True for chunk-granular."""
  v = os.environ.get("ORKID_TERRAIN_MESHCULL", "meshlet").strip().lower()
  return v == "chunk"


class TerrainChunkVertexSource:
  """FWD_SSBO_CUSTOM vertex source for square-chunk, 2D-frustum-culled terrain.

  layout (std430, one SSBO shared by the VS + the reset/cull/finalize compute):
    mat4 c_vp; mat4 c_ivp; vec4 c_eye; vec4 c_misc;   // CamBlk @0   (ComputeDrawable.setCameraParams)
    uint a_vc,a_ic,a_fv,a_fi;                          // VkDrawIndirectCommand @ARGS_OFF
                                                       //   ...or VkDrawMeshTasksIndirectCommandEXT
                                                       //   {x=a_vc,y=a_ic,z=a_fv} in mesh_indirect
                                                       //   mode: ONE args slot holding whichever
                                                       //   command THIS drawable's single draw path
                                                       //   needs (the paths are mutually exclusive;
                                                       //   each writes the slot from its own cull
                                                       //   pass before its own draw). Kept a union
                                                       //   so the emitted layout text — and thus
                                                       //   every existing material's hash — is
                                                       //   byte-identical across all three modes.
    uint v_count,v_frustum_count,u_dim,v_total_count;  // visible-chunk header @VIS_OFF (u_dim=runtime grid dim)
    vec2 u_ybounds;                                    // world height [min,max] METERS @YB_OFF (CPU upload)
    uint v_list[NCHUNK];                               // visible chunk indices @VLIST_OFF
    float heights[DIM*DIM];                            // heightfield @HEIGHTS_OFF (TRUE METERS, CPU upload)
  """

  def __init__(self, dim, extent_m, chunk=128, y_bias=0.0, bake_dim=None, relax=False, meshlet=None,
               mesh=None, mesh_indirect=None, mesh_cull_chunk=None):
    self.dim    = int(dim)
    self.extent = float(extent_m)
    self.chunk  = int(chunk)
    # MESH-SHADER variant sizing (opt-in; the baseline pull-VS path never reads these). A MESHLET is
    # an n x n quad patch inside a chunk, emitted by ONE mesh workgroup: (n+1)^2 unique corners and
    # 2n^2 triangles. n=MAX_MESHLET_DIM (11) is the largest patch that fits the taskless
    # VK_EXT_mesh_shader tier's 256 max_vertices / 256 max_primitives (144 corners, 242 tris); the
    # SHIPPED default is DEFAULT_MESHLET_DIM (8 -> 81 corners, 128 tris) — see its comment for why.
    # meshlet=None -> ORKID_TERRAIN_MESHLET (default DEFAULT_MESHLET_DIM); an explicit argument WINS
    # over the env so a gate that owns its own shape cannot be perturbed by an ambient setting.
    self.meshlet      = int(meshlet) if meshlet is not None else _meshlet_dim()
    self.mps          = (self.chunk + self.meshlet - 1) // self.meshlet  # meshlets per chunk side
    self.mesh_maxvtx  = (self.meshlet + 1) ** 2                          # unique corners == max_vertices
    self.mesh_maxprim = 2 * self.meshlet * self.meshlet
    # The workgroup is NOT the corner count. maxMeshWorkGroupInvocations is 128 both on the
    # Vulkan-guaranteed baseline and on every NVIDIA part; a 144-invocation mesh workgroup is
    # not diagnosed as an over-limit shader — it SEGFAULTS the driver's SPIR-V compiler inside
    # vkCreateGraphicsPipelines (libnvidia-glvkspirv). Both emission loops stride by MESH_WG_MAX,
    # so a meshlet with more corners than invocations is simply covered in several passes.
    self.mesh_wg      = min(self.mesh_maxvtx, MESH_WG_MAX)
    assert self.mesh_maxvtx <= 256, "meshlet corners (%d) exceed max_vertices 256" % self.mesh_maxvtx
    assert self.mesh_maxprim <= 256, "meshlet triangles (%d) exceed max_primitives 256" % self.mesh_maxprim
    # MESH TECHNIQUE OPT-IN — mirrors the C++ drawable's ORKID_TERRAIN_MESHSHADER toggle
    # (terrain_chunk_drawable.cpp): the SAME env decides both, so a toggle-on run materializes a
    # material that CARRIES FWD_SSBO_CUSTOM_MESH and the drawable then finds it (toggle-off keeps
    # the drawable's loud fallback honest). OFF is byte-identical to the pre-mesh generator AND
    # emits no mesh SPIR-V — vulkan_fxi_load.cpp creates a shader module for EVERY stage in the DB
    # at load time, so a machine without VK_EXT_mesh_shader must never be handed one.
    # mesh=True/False forces (the gates); None = env-derived (mode >= 1).
    _mode = _meshmode()
    self.mesh = (_mode >= 1) if mesh is None else bool(mesh)
    # MODE 2 — INDIRECT mesh dispatch. The mesh grid's y dimension stops being the dense chunk
    # grid and becomes a COMPACTED visible-chunk list: one small compute pass (cs_terrain_meshcull,
    # the chunk-level half of cs_terrain_cull) writes v_list + a VkDrawMeshTasksIndirectCommandEXT,
    # and the mesh stage looks its chunk up through v_list. An all-culled view then dispatches ZERO
    # workgroups instead of mps^2 * nchunk empty ones (the MoltenVK away-view regression). The
    # per-meshlet self-cull is RETAINED — compaction is chunk-granular, self-cull is meshlet-granular.
    # Env-derived ONLY when `mesh` is (explicit construction stays fully explicit, so a gate that
    # asks for mesh=True gets the mode-1 text no matter what the ambient env says).
    self.mesh_indirect = ((_mode == 2) if mesh is None else False) if mesh_indirect is None \
                         else bool(mesh_indirect)
    if self.mesh_indirect and not self.mesh:
      raise ValueError("mesh_indirect requires mesh (there is no mesh stage to dispatch)")
    # MODE 1 DIRECT-SIZED — the IMPROVED mode 1. Carries the SAME compacted v_list + compaction pass
    # (cs_terrain_meshcull) as mode 2, but the C++ drawable consumes it with a DIRECT DrawMeshTasksEML
    # whose grid it sizes per-frame from a lag-1 CPU visible-chunk count (+ margin) — no per-frame
    # fence, no DrawMeshTasksIndirectEML (its MoltenVK emulation is the ~32ms trap). Env-derived ONLY
    # (explicit construction stays fully explicit, so the A/B gate's forced flags are untouched). The
    # old FIXED-GRID mode 1 (dense decode over the whole chunk grid) is removed from the env-derived
    # path — it survives only for the A/B gate's explicit three-way (mesh=True built by hand).
    self.mesh_direct_sized = (mesh is None and mesh_indirect is None and _mode == 1)
    # COMPACTED codegen (the compaction compute + the v_list chunk decode) is emitted for EITHER
    # compacted consumer — the indirect draw (mode 2) or the direct-sized draw (mode 1 improved).
    self.mesh_compacted = self.mesh_indirect or self.mesh_direct_sized
    # CULL GRANULARITY knob (ORKID_TERRAIN_MESHCULL) — see _meshcull_granularity. Baked structurally
    # into the mesh body. Env-derived unless forced. INDEPENDENT of the sizing above: sizing decides
    # WHICH workgroups launch, this decides what each launched workgroup frustum-tests.
    self.mesh_cull_chunk = (_meshcull_granularity() if mesh_cull_chunk is None else bool(mesh_cull_chunk))
    # RELAXED-UV mode (the slope-stretch fix). When on, the per-vertex relaxed uv + tangent frame
    # live in a SEPARATE stride-5 UINT runtime SSBO (sif_terra_frame — WS4 fp16 packing:
    # [0,1]=uv fp32-bitcast, [2]=half2(nrm.xz), [3]=half2(bn.xy), [4]=half2(bn.z,0);
    # the old slot-0 height was never read and is dropped) consumed ONLY by the color/cap VS.
    # heights[] stays DENSE stride-1 so terr_pos and the DEPTH-PREPASS keep mono's bandwidth
    # (8 verts per 32B cache line — an interleaved heights[] made the dpp touch one line PER vertex,
    # a regression that grew with render_dimension^2). The dflow RelaxUvModule bakes the frame;
    # the C++ fills BOTH buffers. relax=False is byte-identical to before.
    self.relax   = bool(relax)
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
    # GLOBAL world-height bounds [min,max] METERS (CPU-computed @materialize from the actual field)
    # — the frustum cull's conservative vertical box (heights are true meters; there is no scale
    # constant to build a 0..hscale box from anymore). vec2, 8-aligned @192.
    self.YB_OFF      = 192
    # per-chunk WORLD-Y bounds [minY, maxY] (CPU-computed @materialize) — the TIGHT vertical extent the
    # HZB occlusion test needs (the conservative global box never occludes; a valley chunk's real
    # max-Y lets it cull behind a nearer ridge). vec2 stride 8 in std430.
    self.CHUNKY_OFF  = 200                                      # vec2 chunk_y[MAXNC]  (8-aligned @200)
    self.VLIST_OFF   = self.CHUNKY_OFF + self.maxnc * 8         # uint v_list[MAXNC]
    self.HEIGHTS_OFF = self.VLIST_OFF + self.maxnc * 4          # float heights[] (runtime, last; DENSE stride 1)
    self.TOTAL       = self.HEIGHTS_OFF + self.dimsq * 4        # render buffer (heights always dense)
    # relax: the frame SSBO (separate buffer bound to sif_terra_frame) — stride 5 uints/vertex
    # (WS4 fp16 packing; was 8 floats = 32B, now 20B)
    self.FRAME_TOTAL = (self.dimsq * 5 * 4) if self.relax else 0

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
      "vec2 u_ybounds;                                    // world height [min,max] METERS @%d (CPU upload)\n"
      "vec2 chunk_y[%d];                                  // per-chunk world [minY,maxY] @%d (cap MAXNC, CPU upload)\n"
      "uint v_list[%d];                                   // visible chunk indices @%d (cap MAXNC)\n"
      "float heights[];                                   // heightfield @%d (RUNTIME-sized: u_dim*u_dim, TRUE METERS, CPU upload)"
      % (self.ARGS_OFF, self.VIS_OFF, self.YB_OFF, self.maxnc, self.CHUNKY_OFF,
         self.maxnc, self.VLIST_OFF, self.HEIGHTS_OFF))

  @property
  def frame_block(self):
    """The relax frame storage block (SEPARATE buffer; runtime-sized stride-5 UINT array —
    WS4 fp16 packing: [0,1]=RELAXED uv as fp32 bitcasts (atlas-param precision: fp16 would
    quantize ~2 atlas texels near 1.0), [2]=packHalf2x16(nrm.x, nrm.z),
    [3]=packHalf2x16(bn.x, bn.y), [4]=packHalf2x16(bn.z, 0). The old slot-0 height was never
    read (terr_pos reads the dense heights[]) and is dropped: 32B -> 20B per texel. MUST
    mirror the C++ packer (terrain_chunk_drawable.cpp pack_frame5). Declared as an extra
    vertex storage block so EVERY VS variant (forward/cap/dpp/instanced) inherits it
    uniformly; only the color/cap tail actually reads it (dpp's dead reads DCE)."""
    return ("storage_interface sif_terra_frame (descriptor_set 0) {\n"
            "  buffer layout(std430) terra_frame_blk { uint tframe[]; };\n"
            "}")

  @property
  def lib(self):
    # terr_pos: texel-center world coord + height tap (matches _build_terrain_mesh_arrays).
    # heights[] is DENSE stride-1 in both modes (the relax frame lives in sif_terra_frame).
    D = self._D
    hread = "heights[cz * %s + cx]" % D
    return (
      "vec3 terr_pos(uint tx, uint tz) {\n"
      "  uint cx = min(tx, %s - 1u);\n"
      "  uint cz = min(tz, %s - 1u);\n"
      "  float x = ((float(tx) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float z = ((float(tz) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float y = %s + (%s);\n"
      "  return vec3(x, y, z);\n"
      "}" % (D, D, D, _f(self.extent),
             D, _f(self.extent), hread, _f(self.ybias)))

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
      # PARTIAL CHUNKS (u_dim %% chunk != 0 -> the last chunk row/col is partial, e.g. 64 spare
      # cells at u_dim=1600 chunk=128): cells whose corner-0 lies fully beyond the grid collapse
      # to a point (zero-area -> rasterizer culls). Without this their clamped-height taps
      # stretch a wide flat apron past the +X/+Z terrain edges. Divisible dims have no such
      # cells (baseC max == u_dim-1), so this line is inert there.
      "if (baseC >= u_dim || baseR >= u_dim) { tx = 0u; tz = 0u; }\n"
      % (self._VP, self._CP, self._CP, self._VP, self._C, self._C, self._C, self._C))
    return head + self._vertex_decode

  @property
  def _vertex_decode(self):
    """(tx,tz) -> position/normal/binormal/uv0/vtxcolor (+ ruv/Prelax when relaxed). THE per-vertex
    decode, shared verbatim by the pull VS (vs_body) and the mesh stage (mesh_body) so a divergence
    is impossible by construction."""
    return self._relax_vertex if self.relax else self._mono_vertex

  @property
  def _relax_vertex(self):
    """RELAX per-vertex decode: precomputed tangent frame + BOTH uv parameterizations. The frame
    comes from the SEPARATE sif_terra_frame SSBO (stride-5 uints — WS4 fp16: [0,1]=uv fp32-bitcast,
    [2]=half2(normal.x,z), [3]=half2(binormal.xy), [4]=half2(binormal.z, 0)) — heights[] stays
    dense for terr_pos/dpp. normal.y reconstructed +sqrt. uv0 = PLANAR grid uv (the default ctx.uv;
    channel taps + existing materials, UNCHANGED meaning); ruv = RELAXED uv (the atlas
    parameterization — the material routes the cap-VS gl_Position + the stored forward frg_uv0 to
    it). Both carried: material picks per-technique."""
    return (
      "vec3 P = terr_pos(tx, tz);\n"
      # CLAMP the frame reads: tx/tz legitimately reach u_dim at the far row/col (the mesh
      # extends half a texel past the last texel center), and unlike terr_pos (which clamps
      # internally) a raw (tz*u_dim+tx) would read the NEXT ROW's texel 0 on the right edge
      # and PAST THE BUFFER on the far row (robustness zeros -> normalize(0) -> NaN TBN).
      # uv0/P/Prelax stay on the unclamped indices (planar uv must reach 1.0 at the edge).
      "uint vx = min(tx, u_dim - 1u);\n"
      "uint vz = min(tz, u_dim - 1u);\n"
      "uint vbase = (vz * u_dim + vx) * 5u;\n"
      "vec2 ruv = vec2(uintBitsToFloat(tframe[vbase + 0u]), uintBitsToFloat(tframe[vbase + 1u]));   // RELAXED uv (atlas param, fp32)\n"
      "vec2 uv0 = vec2((float(tx) + 0.5) / float(%s), (float(tz) + 0.5) / float(%s));   // PLANAR grid uv\n"
      "vec2 _nxz = unpackHalf2x16(tframe[vbase + 2u]);\n"
      "float _nx = _nxz.x; float _nz = _nxz.y;\n"
      "vec3 normal = vec3(_nx, sqrt(max(0.0, 1.0 - _nx*_nx - _nz*_nz)), _nz);\n"
      "vec3 binormal = vec3(unpackHalf2x16(tframe[vbase + 3u]), unpackHalf2x16(tframe[vbase + 4u]).x);\n"
      "vec4 position = vec4(P, 1.0);\n"
      # RELAXED world position: worldXZ at the RELAXED uv ((ruv-0.5)*extent), planar height for depth.
      # The cap-VS atlas bake rasterizes this via the ortho mvp (gl_Position = mvp*Prelax) — the SAME
      # projection-matrix path as the forward VS / hypermesh. A DIRECT gl_Position = 2*ruv-1 from an SSBO
      # read is mis-rasterized by MoltenVK below the NDC anti-diagonal; the mvp path is not.
      "vec3 Prelax = vec3((ruv.x - 0.5) * %s, P.y, (ruv.y - 0.5) * %s);\n"
      "vec4 vtxcolor = vec4(1.0);"
      % (self._D, self._D, _f(self.extent), _f(self.extent)))

  @property
  def _mono_vertex(self):
    """MONO per-vertex decode: (tx,tz) -> position/normal/binormal/uv0/vtxcolor. Live analytic
    normal/binormal from terr_pos taps + planar uv0 (byte-identical to pre-relax). Shared by the
    pull VS (vs_body) and the MESH variant (mesh_body) so both stages compute the same vertex from
    the same GLSL text — a divergence here would show up as an A/B pixel mismatch, not a warning."""
    return (
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

  def _chunk_cull_test(self, pad=""):
    """THE per-chunk visibility test: reads `ci` (+ the runtime cps/nchunk locals) and leaves
    `inside`. Frustum first (conservative GLOBAL-Y box), then HZB occlusion (tight per-chunk box).
    ONE copy of the plane math, emitted by cs_terrain_cull (one thread per chunk) AND by
    cs_terrain_meshcull (the indirect mesh path's compaction pass); `pad` re-indents it for the
    loop nesting. Two copies would be free to drift, and a drift here is a chunk one path draws
    and the other does not — a silent A/B pixel mismatch, not a warning."""
    body = (
      "  uint cx = ci %% %s;\n"
      "  uint cz = ci / %s;\n"
      "  float x0 = ((float(cx * %s) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float x1 = ((float((cx + 1u) * %s) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float z0 = ((float(cz * %s) + 0.5) / float(%s) - 0.5) * %s;\n"
      "  float z1 = ((float((cz + 1u) * %s) + 0.5) / float(%s) - 0.5) * %s;\n"
      # FRUSTUM box: CONSERVATIVE global height range u_ybounds (a tight per-chunk box wrongly frustum-
      # culls chunks whose real terrain sits below the view planes — e.g. the chunk underfoot, lower
      # than eye level -> a hole). The TIGHT per-chunk box is for the OCCLUSION test only (below).
      # Heights are TRUE METERS; the CPU uploads the field's actual [min,max] (no scale constant).
      "  vec3 mn = vec3(x0, u_ybounds.x, z0);\n"
      "  vec3 mx = vec3(x1, u_ybounds.y, z1);\n"
      "  bool inside = true;\n"
      "  // c_misc.x < 0 is the ORKID_DISABLE_FRUSTUM_CULL sentinel (host-stamped): skip the frustum reject\n"
      "  // so all chunks are treated visible (occlusion below, if enabled, still applies).\n"
      "  if (c_misc.x >= 0.0) {\n"
      "    vec4 rx = vec4(c_vp[0].x, c_vp[1].x, c_vp[2].x, c_vp[3].x);\n"
      "    vec4 ry = vec4(c_vp[0].y, c_vp[1].y, c_vp[2].y, c_vp[3].y);\n"
      "    vec4 rz = vec4(c_vp[0].z, c_vp[1].z, c_vp[2].z, c_vp[3].z);\n"
      "    vec4 rw = vec4(c_vp[0].w, c_vp[1].w, c_vp[2].w, c_vp[3].w);\n"
      "    vec4 pl[6];\n"
      "    // CullFrustumScale (c_misc.x, frame-global, from RCFD via ComputeDrawable::onPreRender):\n"
      "    // >1 widens / cull-less, 1.0 exact, <1 narrows. t = 1/scale scales the four SIDE planes\n"
      "    // (same sense as the hypermesh MeshInstCull u_tighten); near/far (pl[4],pl[5]) unchanged.\n"
      "    float t = (c_misc.x > 0.0) ? (1.0 / c_misc.x) : 1.0;\n"
      "    vec4 sx = rx * t; vec4 sy = ry * t;\n"
      "    pl[0] = rw + sx; pl[1] = rw - sx; pl[2] = rw + sy; pl[3] = rw - sy; pl[4] = rz; pl[5] = rw - rz;\n"
      "    for (int p = 0; p < 6; p++) {\n"
      "      vec3 pv = vec3(pl[p].x >= 0.0 ? mx.x : mn.x, pl[p].y >= 0.0 ? mx.y : mn.y, pl[p].z >= 0.0 ? mx.z : mn.z);\n"
      "      if ((dot(pl[p].xyz, pv) + pl[p].w) < 0.0) { inside = false; }\n"
      "    }\n"
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
      % (self._CP, self._CP,
         self._C, self._D, _f(self.extent), self._C, self._D, _f(self.extent),
         self._C, self._D, _f(self.extent), self._C, self._D, _f(self.extent)))
    if not pad:
      return body
    return "".join(pad + ln + "\n" for ln in body.split("\n")[:-1])

  @property
  def compute(self):
    # reset -> cull (2D frustum test per chunk, append survivors) -> sort -> finalize (vertexCount).
    # RELAX: the compute must ALSO inherit sif_terra_frame (it never reads it) so its binding set stays
    # DENSE and consistent with the program-level ids. The compute stage gets per-shader FALLBACK bindings
    # (sequential, inheritance order) while the pipeline LAYOUT uses the program-level declaration-order
    # ids — a graphics-only block between sif_ptex_vtx and sif_hzb desynchronizes them (layout {0,2} vs
    # stage {0,1}) and vkCreateComputePipelines fails (-3, MoltenVK). Inherit order MUST match the
    # program declaration order: sif_ptex_vtx, sif_terra_frame, sif_hzb.
    frame_inh = " : sif_terra_frame" if self.relax else ""
    # sif_hzb: the read-only max-depth HZB pyramid (SEPARATE SSBO, bound per-frame by
    # ComputeDrawable::onPreRender). Compute-only (the VS doesn't use it); shares descriptor_set 0,
    # gets a declaration-order binding after sif_ptex_vtx (sif_binding pre-assign).
    header = (
      "storage_interface sif_hzb (descriptor_set 0) { buffer layout(std430) hzb_blk { float HZB[]; }; }\n"
      "compute_interface cif_terrain : sif_ptex_vtx%s : sif_hzb { inputs { layout(local_size_x = 64); } }\n"
      % frame_inh)
    # INDIRECT mesh mode appends the compaction pass (below). The reset/cull/sort/finalize four
    # stay emitted in EVERY mode: they are what the drawable's loud fallback (a device with no
    # mesh extension at all) runs.
    return header + (
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
      "%s"  # THE per-chunk visibility test (shared with cs_terrain_meshcull) -> `inside`
      "  if (inside) { uint slot = atomicAdd(v_count, 1u); v_list[slot] = ci; }\n"
      "}\n"
      "////////////////////////////////////////\n"
      "compute_shader cs_terrain_sort : cif_terrain {\n"
      "%s"  # runtime cps preamble (from u_dim)
      "  if (gl_GlobalInvocationID.x != 0u) { return; }\n"   # one thread sorts the whole list
      "%s"  # THE near-to-far reorder (shared with cs_terrain_meshcull)
      "}\n"
      "////////////////////////////////////////\n"
      "compute_shader cs_terrain_finalize : cif_terrain {\n"
      "  a_vc = v_count * %s; a_ic = 1u; a_fv = 0u; a_fi = 0u;\n"
      "}"
      % (self._dim_preamble(),                  # reset preamble (cps -> nchunk)
         self._NC,                              # reset: v_total_count = nchunk
         self._dim_preamble(),                  # cull preamble (cps + nchunk)
         self._NC,                              # cull: ci bound
         self._chunk_cull_test(),               # cull: THE per-chunk visibility test
         self._dim_preamble(want_nchunk=False), # sort preamble (cps only)
         self._sort_body(pad="  "),             # sort: THE near-to-far reorder
         self._VP)) + (self._meshcull if self.mesh_compacted else "")

  def _sort_body(self, pad=""):
    """THE near-to-far reorder for early-Z: draw the closest chunks first so the heavy terrain
    fragment shader is depth-rejected on the far ones. Reads v_count and permutes v_list in place;
    v_count is NEVER written (sorting decides which chunk sits in which slot, never how many there
    are — the drawable's lag-1 sizing reads the same count either way). Single-thread selection
    sort (nchunk is small, and after compaction the visible count is smaller still); the CALLER
    must have already gated to one invocation and made the appended v_list visible to it.

    Key = chunk-center XZ distance-squared to c_eye; HEIGHT is dropped — it's constant per camera
    so it can't change the ordering, and dropping it avoids a per-compare heights[] tap (the
    expensive part in a single-thread O(n^2) loop). In VR c_eye is the head (the VR-aware cull
    camera), so chunks sort near-to-far from the head.

    ONE copy, emitted by cs_terrain_sort (pull-VS path, its own dispatch) AND inlined into
    cs_terrain_meshcull's thread 0 (the indirect mesh path has no separate sort dispatch); `pad`
    re-indents it for the nesting. Two copies would be free to drift, and a drift here is a
    different draw ORDER per path — depth ties resolving differently, i.e. an A/B pixel mismatch.
    Requires the runtime cps local in scope (_dim_preamble)."""
    tw = "(%s / float(u_dim))" % _f(self.extent)   # runtime texel->world scale
    body = (
      "uint n = v_count;\n"
      "for (uint i = 0u; (i + 1u) < n; i = i + 1u) {\n"
      "  uint bi = i;\n"
      "  uint ca = v_list[i];\n"
      "  float xa = (float((ca %% %s) * %s) + %s) * %s - %s;\n"
      "  float za = (float((ca / %s) * %s) + %s) * %s - %s;\n"
      "  float bd = (xa - c_eye.x) * (xa - c_eye.x) + (za - c_eye.z) * (za - c_eye.z);\n"
      "  for (uint j = i + 1u; j < n; j = j + 1u) {\n"
      "    uint cb = v_list[j];\n"
      "    float xb = (float((cb %% %s) * %s) + %s) * %s - %s;\n"
      "    float zb = (float((cb / %s) * %s) + %s) * %s - %s;\n"
      "    float dj = (xb - c_eye.x) * (xb - c_eye.x) + (zb - c_eye.z) * (zb - c_eye.z);\n"
      "    if (dj < bd) { bd = dj; bi = j; }\n"
      "  }\n"
      "  if (bi != i) { uint t = v_list[i]; v_list[i] = v_list[bi]; v_list[bi] = t; }\n"
      "}\n"
      # (CPS, CHUNK, halfchunk, texel->world(runtime tw), halfextent) per coord (xa,za,xb,zb)
      % (self._CP, self._C, _f(self.chunk / 2.0 + 0.5), tw, _f(0.5 * self.extent),
         self._CP, self._C, _f(self.chunk / 2.0 + 0.5), tw, _f(0.5 * self.extent),
         self._CP, self._C, _f(self.chunk / 2.0 + 0.5), tw, _f(0.5 * self.extent),
         self._CP, self._C, _f(self.chunk / 2.0 + 0.5), tw, _f(0.5 * self.extent)))
    if not pad:
      return body
    return "".join(pad + ln + "\n" for ln in body.split("\n")[:-1])

  @property
  def _meshcull(self):
    """cs_terrain_meshcull — the INDIRECT mesh path's ENTIRE pre-draw cost: ONE dispatch of ONE
    workgroup that compacts the visible chunks into v_list and publishes the mesh draw's
    VkDrawMeshTasksIndirectCommandEXT{x=mps^2, y=visible chunks, z=1}. All-culled => y=0 => the
    draw dispatches nothing, which is the whole point (the fixed grid pays mps^2*nchunk workgroups
    to cull themselves).

    ONE workgroup, not one-thread-per-chunk over many: the reset -> append -> publish ordering is
    then a workgroup barrier instead of three dispatches with storage barriers between them, and
    the dispatch phase is a submit+WAIT whose fixed cost dwarfs the arithmetic. The 64u stride is
    cif_terrain's local_size_x; nchunk is at most a few hundred, so each thread does a handful of
    chunk tests. The per-MESHLET self-cull in the mesh stage is untouched — this is the chunk-
    granular pre-pass in front of it, and a chunk AABB contains every one of its meshlet AABBs, so
    compaction can never reject a meshlet the fixed grid would have emitted.

    Thread 0's tail ALSO runs the near-to-far reorder (_sort_body) before publishing the args: the
    mesh draw's y axis walks v_list in slot order, so permuting the list IS the chunk draw order,
    and that ordering is what feeds the depth prepass / early-Z. No separate sort dispatch — the
    append barrier already gave thread 0 the whole visible list, and re-splitting this into two
    dispatches would pay back the submit+WAIT that compaction exists to avoid."""
    return (
      "\n"
      "////////////////////////////////////////\n"
      "compute_shader cs_terrain_meshcull : cif_terrain {\n"
      "%(PRE)s"
      "  if (gl_LocalInvocationIndex == 0u) {\n"
      "    v_count = 0u; v_frustum_count = 0u; v_total_count = %(NC)s;\n"
      "  }\n"
      "  memoryBarrierBuffer();\n"
      "  barrier();\n"
      "  for (uint ci = gl_LocalInvocationIndex; ci < %(NC)s; ci = ci + 64u) {\n"
      "%(TEST)s"
      "    if (inside) { uint slot = atomicAdd(v_count, 1u); v_list[slot] = ci; }\n"
      "  }\n"
      "  memoryBarrierBuffer();\n"
      "  barrier();\n"
      "  if (gl_LocalInvocationIndex == 0u) {\n"
      "%(SORT)s"   # near-to-far reorder of the just-compacted list (v_count untouched)
      # the SHARED args slot, carrying the mesh command shape this frame (see the class layout
      # docstring): x = meshlets per chunk, y = compacted chunk count, z = 1.
      "    a_vc = %(MPS2)du; a_ic = v_count; a_fv = 1u; a_fi = 0u;   // VkDrawMeshTasksIndirectCommandEXT{x,y,z}\n"
      "  }\n"
      "}"
      % dict(PRE=self._dim_preamble(), NC=self._NC,
             TEST=self._chunk_cull_test(pad="  "), SORT=self._sort_body(pad="    "),
             MPS2=self.mps * self.mps))

  # ---- MESH-SHADER variant (taskless VK_EXT_mesh_shader; prototype) -----------
  # Same SSBO layout, same heights[], same triangulation as the pull VS — a DIFFERENT way to get
  # the triangles to the rasterizer:
  #   pull VS : compute reset/cull/sort/finalize -> v_list + VkDrawIndirectCommand -> one
  #             non-indexed vertex per triangle corner (chunk^2*6), so every interior corner is
  #             decoded (and its heights[] tapped) ~6x.
  #   mesh    : NO compute, NO indirect args. A FIXED workgroup grid (mesh_groups(), known at
  #             drawable build time) covers chunks x meshlets; each workgroup frustum-tests its own
  #             meshlet AABB and either emits nothing or emits its (n+1)^2 corners ONCE plus
  #             local-index triangles.
  #   mesh
  #   indirect: ONE compaction dispatch (cs_terrain_meshcull) + a GPU-written mesh draw command;
  #             the grid's y axis walks the COMPACTED visible-chunk list instead of the dense chunk
  #             grid, so culled chunks cost no workgroups at all. Per-meshlet self-cull retained,
  #             and the same list is sorted near-to-far in that one dispatch, so early-Z holds.
  # The per-meshlet cull is ~mps^2 finer than the whole-chunk compute cull. Only the FIXED-GRID
  # `mesh` variant above loses the near-to-far chunk ordering (_sort_body) that gives the pull path
  # its early-Z — it has no visible list to permute, its grid order IS the dense chunk index. Every
  # COMPACTED consumer (mode 2 indirect, mode 1 direct-sized) walks the sorted list.

  def mesh_interface(self, name="vif_terrain_mesh", storage="sif_ptex_vtx", outputs="", inherits=()):
    """The mesh stage's interface. The mesh stage stands in for the vertex stage and rides a
    vertex_interface: `inputs` carries the workgroup size (flat 1D, capped at the device-portable
    128 — the emission loops stride when corners exceed invocations), `outputs` carries the EXT
    topology/limits layout line plus the per-vertex varyings (the backend emits mesh outputs as
    arrays). `outputs` MUST declare the same varyings in the same order as the pull-VS interface
    the fragment stage inherits, or the two paths' locations desynchronize. `inherits` overrides
    the single-storage default when the consumer needs the pull-VS interface's WHOLE inherit list
    (the generated ptex3d material: ub_std_vtx first)."""
    inh = "".join(" : %s" % n for n in (inherits if inherits else (storage,)))
    outs = ""
    for line in outputs.strip().splitlines():
      outs += "    %s\n" % line.strip()
    # flat 1D workgroup: the body indexes purely by gl_LocalInvocationIndex, and the emission
    # loops stride by it, so the invocation count is free to sit below max_vertices.
    return (
      "vertex_interface %s%s {\n"
      "  inputs { layout(local_size_x = %d, local_size_y = 1, local_size_z = 1); }\n"
      "  outputs {\n"
      "    layout(triangles, max_vertices = %d, max_primitives = %d);\n"
      "%s"
      "  }\n"
      "}" % (name, inh, self.mesh_wg,
             self.mesh_maxvtx, self.mesh_maxprim, outs))

  def mesh_body(self, mvp="mvp", varying_writes="", compacted=None, guard_capacity=None,
               chunk_cull=None):
    """The mesh_shader body: (chunk, meshlet) from gl_WorkGroupID -> frustum self-cull -> emit.
    `mvp` names the clip-space matrix expression (the CamBlk's c_vp for a raw SSBO consumer, the
    material's `mvp` uniform inside a generated material). `varying_writes` is the caller's
    per-vertex varying assignment text with $V standing for the emitted vertex index (it runs with
    the SAME locals the pull VS body leaves: position/normal/binormal/uv0/vtxcolor).

    `compacted` picks the CHUNK DECODE and defaults to self.mesh_compacted, so the material template
    needs no mode plumbing: False reads the dense (y,z) grid, True reads slot y of the compacted
    v_list cs_terrain_meshcull wrote. Everything after the decode — the partial-meshlet clamp, the
    per-meshlet frustum self-cull, the emission — is IDENTICAL in both, which is why the modes are
    pixel-for-pixel the same image. The A/B gate passes it explicitly to put both decodes in one
    program.

    `guard_capacity` (defaults self.mesh_direct_sized, compacted only) adds the OVER-DISPATCH guard
    the mode-1 direct-sized path needs: the C++ sizes the grid's y from a LAG-1 count, so a workgroup
    whose gl_WorkGroupID.y >= the CURRENT v_count would read a STALE v_list slot — the guard makes it
    emit nothing instead (SetMeshOutputsEXT(0,0)), so over-dispatch is merely cheap, never wrong. In
    the indirect path (mode 2) the grid y == v_count exactly, so the guard is never emitted there.

    `chunk_cull` (defaults self.mesh_cull_chunk) selects the FRUSTUM-TEST bound: False (meshlet) tests
    this workgroup's own meshlet AABB, True (chunk) tests the parent chunk's AABB (all mps^2 workgroups
    agree). Either way the meshlet's own geometry is what gets emitted — only the reject decision's
    granularity changes."""
    if compacted is None:
      compacted = self.mesh_compacted
    if guard_capacity is None:
      guard_capacity = self.mesh_direct_sized
    if chunk_cull is None:
      chunk_cull = self.mesh_cull_chunk
    vw = ""
    for line in varying_writes.strip().splitlines():
      vw += "  %s\n" % line.strip().replace("$V", "vi")
    vtx = ""
    for line in self._vertex_decode.splitlines():
      vtx += "  %s\n" % line
    # the CHUNK the workgroup owns. Dense: the two outer grid axes. Compacted: one indirection
    # through the visible list, whose LENGTH is the dispatched y count (so slot < count always).
    # OVER-DISPATCH guard (mode-1 direct-sized only): the grid y is CPU-sized from a lag-1 count, so
    # a slot at/above the CURRENT v_count is stale — emit nothing rather than draw a stale chunk.
    guard = ("if (gl_WorkGroupID.y >= v_count) { SetMeshOutputsEXT(0u, 0u); return; }\n"
             ) if (compacted and guard_capacity) else ""
    decode = (guard +
              "uint chunk = v_list[gl_WorkGroupID.y];   // compacted visible list (cs_terrain_meshcull)\n"
              "uint ccx = chunk % cps;\n"   # substituted AFTER the %-format below: no %% escape here
              "uint ccz = chunk / cps;\n") if compacted else (
              "uint ccx = gl_WorkGroupID.y;\n"
              "uint ccz = gl_WorkGroupID.z;\n")
    # FRUSTUM-TEST bound source (see chunk_cull): meshlet uses this workgroup's clamped meshlet span
    # (baseC0/nx ...); chunk uses the parent chunk's full clamped span. nx/nz (the EMITTED meshlet)
    # are untouched — only the fbc/fnx/fbr/fnz the AABB is built from change. self._C ("128u") is
    # inlined HERE — this string is a SUBSTITUTED value in the outer format, never re-scanned.
    _c = self._C
    cullbound = ("  uint fbc = ccx * %s; uint fnx = min(%s, u_dim - ccx * %s);\n"
                 "  uint fbr = ccz * %s; uint fnz = min(%s, u_dim - ccz * %s);\n"
                 % (_c, _c, _c, _c, _c, _c)) if chunk_cull else (
                 "  uint fbc = baseC0; uint fnx = nx; uint fbr = baseR0; uint fnz = nz;\n")
    return (
      "uint cps = (u_dim + %(CM1)du) / %(C)s;\n"
      "%(DECODE)s"
      "uint mlx = gl_WorkGroupID.x %% %(MPS)du;\n"
      "uint mlz = gl_WorkGroupID.x / %(MPS)du;\n"
      "uint baseC0 = ccx * %(C)s + mlx * %(N)du;\n"
      "uint baseR0 = ccz * %(C)s + mlz * %(N)du;\n"
      "uint nx = 0u;\n"
      "uint nz = 0u;\n"
      "if (ccx < cps && ccz < cps && baseC0 < u_dim && baseR0 < u_dim) {\n"
      # PARTIAL meshlets: clamp to the chunk remainder (chunk % n != 0) AND the grid remainder
      # (u_dim % chunk != 0). The pull VS collapses those cells to a point instead; here they are
      # simply never emitted, which leaves the VALID cells' shared corners intact.
      "  nx = min(%(N)du, %(C)s - mlx * %(N)du);\n"
      "  nz = min(%(N)du, %(C)s - mlz * %(N)du);\n"
      "  nx = min(nx, u_dim - baseC0);\n"
      "  nz = min(nz, u_dim - baseR0);\n"
      "}\n"
      # per-MESHLET frustum self-cull — the cs_terrain_cull test at mps^2-finer granularity, on
      # the CONSERVATIVE global u_ybounds vertical box (chunk_y[] is per CHUNK, not per meshlet).
      # c_misc.x < 0 is the same host-stamped ORKID_DISABLE_FRUSTUM_CULL sentinel the compute cull
      # honors; c_misc.x is CullFrustumScale otherwise. NO HZB test: sif_hzb is bound to the
      # compute passes only, so this prototype's mesh path is FRUSTUM-ONLY.
      "if (nx > 0u && nz > 0u && c_misc.x >= 0.0) {\n"
      "%(CULLBOUND)s"
      "  float x0 = ((float(fbc) + 0.5) / float(u_dim) - 0.5) * %(EXT)s;\n"
      "  float x1 = ((float(fbc + fnx) + 0.5) / float(u_dim) - 0.5) * %(EXT)s;\n"
      "  float z0 = ((float(fbr) + 0.5) / float(u_dim) - 0.5) * %(EXT)s;\n"
      "  float z1 = ((float(fbr + fnz) + 0.5) / float(u_dim) - 0.5) * %(EXT)s;\n"
      "  vec3 bmn = vec3(x0, u_ybounds.x, z0);\n"
      "  vec3 bmx = vec3(x1, u_ybounds.y, z1);\n"
      "  vec4 rx = vec4(c_vp[0].x, c_vp[1].x, c_vp[2].x, c_vp[3].x);\n"
      "  vec4 ry = vec4(c_vp[0].y, c_vp[1].y, c_vp[2].y, c_vp[3].y);\n"
      "  vec4 rz = vec4(c_vp[0].z, c_vp[1].z, c_vp[2].z, c_vp[3].z);\n"
      "  vec4 rw = vec4(c_vp[0].w, c_vp[1].w, c_vp[2].w, c_vp[3].w);\n"
      "  float t = (c_misc.x > 0.0) ? (1.0 / c_misc.x) : 1.0;\n"
      "  vec4 sx = rx * t; vec4 sy = ry * t;\n"
      "  vec4 pl[6];\n"
      "  pl[0] = rw + sx; pl[1] = rw - sx; pl[2] = rw + sy; pl[3] = rw - sy; pl[4] = rz; pl[5] = rw - rz;\n"
      "  for (int p = 0; p < 6; p++) {\n"
      "    vec3 pv = vec3(pl[p].x >= 0.0 ? bmx.x : bmn.x, pl[p].y >= 0.0 ? bmx.y : bmn.y, pl[p].z >= 0.0 ? bmx.z : bmn.z);\n"
      "    if ((dot(pl[p].xyz, pv) + pl[p].w) < 0.0) { nx = 0u; nz = 0u; }\n"
      "  }\n"
      "}\n"
      # SetMeshOutputsEXT takes workgroup-UNIFORM counts (everything above derives from the
      # workgroup id + SSBO uniforms) and is the culled meshlet's whole cost: (0,0) -> no
      # rasterization work at all.
      "uint nv = (nx + 1u) * (nz + 1u);\n"
      "uint np = 2u * nx * nz;\n"
      "if (nx == 0u || nz == 0u) { nv = 0u; np = 0u; }\n"
      "SetMeshOutputsEXT(nv, np);\n"
      "if (np == 0u) { return; }\n"
      # ONE emission per unique corner: the heights[] tap the pull VS pays ~6x per interior
      # corner is paid ONCE here (plus the 4 neighbor taps of the analytic normal). Strided
      # because the workgroup is capped at MESH_WG_MAX invocations, which may be fewer than
      # the meshlet's corners.
      "for (uint vi = gl_LocalInvocationIndex; vi < nv; vi += %(WG)du) {\n"
      "  uint tx = baseC0 + (vi %% (nx + 1u));\n"
      "  uint tz = baseR0 + (vi / (nx + 1u));\n"
      "%(VTX)s"
      "  gl_MeshVerticesEXT[vi].gl_Position = %(MVP)s * position;\n"
      "%(VARY)s"
      "}\n"
      # local-index triangles: SAME diagonal + SAME winding as the pull VS's 6-corner cell
      # (corners 0,1,2 = v00,v01,v10 and 3,4,5 = v10,v01,v11), so both paths rasterize the
      # same triangles in the same orientation.
      "for (uint p = gl_LocalInvocationIndex; p < np; p += %(WG)du) {\n"
      "  uint q   = p >> 1u;\n"
      "  uint qi  = q %% nx;\n"
      "  uint qj  = q / nx;\n"
      "  uint i00 = qj * (nx + 1u) + qi;\n"
      "  uint i10 = i00 + 1u;\n"
      "  uint i01 = i00 + nx + 1u;\n"
      "  uint i11 = i01 + 1u;\n"
      "  gl_PrimitiveTriangleIndicesEXT[p] = ((p & 1u) == 0u) ? uvec3(i00, i01, i10) : uvec3(i10, i01, i11);\n"
      "}"
      % dict(C=self._C, CM1=self.chunk - 1, N=self.meshlet, MPS=self.mps, DECODE=decode,
             CULLBOUND=cullbound, WG=self.mesh_wg, EXT=_f(self.extent), MVP=mvp, VTX=vtx, VARY=vw))

  def mesh_groups(self):
    """The FIXED mesh-workgroup dispatch grid (x=meshlets per chunk, y/z=chunks per side) — the
    CPU-known upper bound the mesh path draws EVERY frame; culled meshlets cost one workgroup that
    calls SetMeshOutputsEXT(0,0). Workgroups whose (ccx,ccz) exceeds the RUNTIME cps self-reject,
    so this may be dispatched at the layout cap while u_dim is smaller.

    UNUSED by the INDIRECT path — there the whole command (x=mps^2, y=compacted count, z=1) is
    GPU-written into the args slot by cs_terrain_meshcull, which is the entire point: no CPU-side
    upper bound survives into the dispatch."""
    return (self.mps * self.mps, self.cps, self.cps)

  def mesh_compute_passes(self):
    """The INDIRECT mesh path's compute contract — the ONE compaction dispatch that replaces the
    pull path's four (reset/cull/sort/finalize). Same shape as compute_passes(): one workgroup, so
    the whole pass is a single dispatch phase."""
    return [("cs_terrain_meshcull", 1, 1, 1)]

  # ---- material delegation ---------------------------------------------------
  def as_material_kwargs(self):
    """The dict spliced into Ptex3d(vertex_source=...) -> materialize_surface_fxv2(ssbo_*)."""
    kw = dict(ssbo_layout=self.layout,
              ssbo_lib=self.lib,
              ssbo_vs_body=self.vs_body,
              ssbo_compute=self.compute,
              # relax => the vs_body provides a `ruv` local (relaxed uv); the material routes the atlas
              # param (cap-VS gl_Position + stored forward frg_uv0) to it. False => planar uv0 throughout.
              relax_uv=self.relax)
    if self.relax:
      # the frame SSBO: declared as an extra vertex storage block + inherited by every VS variant
      # (the SoA-channel mechanism). The C++ drawable binds the buffer to sif_terra_frame.
      kw["ssbo_extra_blocks"] = self.frame_block
      kw["ssbo_vs_inherits"]  = ("sif_terra_frame",)
    if self.mesh:
      # MESH opt-in: hand the template THIS object so it can ask for the meshlet interface + body
      # around ITS varying contract (_VS_TAIL). Absent => no FWD_SSBO_CUSTOM_MESH technique and the
      # generated text is byte-identical to the pre-mesh generator.
      kw["mesh_source"] = self
    return kw

  # ---- ComputeDrawable consumer contract ------------------------------------
  def compute_passes(self):
    """[(shader_name, groups_x, groups_y, groups_z), ...] in run order; storageBarrier between."""
    cull_groups = (self.nchunk + 63) // 64
    return [("cs_terrain_reset",    1,           1, 1),
            ("cs_terrain_cull",     cull_groups, 1, 1),
            ("cs_terrain_sort",     1,           1, 1),   # near-to-far for early-Z
            ("cs_terrain_finalize", 1,           1, 1)]

  # ---- runtime-dim (u_dim) upload — THE shared Python seam --------------------
  # DIM is a RUNTIME uniform read from the VIS header (u_dim @VIS_OFF+8), NOT a baked literal
  # (res-decoupling, commit e82564dcd): the cull compute derives nchunk = ceil(u_dim/CHUNK)^2
  # from it and `if (ci >= nchunk) return;` culls EVERY chunk when it reads 0 -> a zero-vertex
  # indirect draw -> INVISIBLE terrain. The C++ ECS drawable uploads it
  # (terrain_chunk_drawable.cpp); EVERY Python parity setup that builds the SSBO + uploads
  # heights itself MUST also call upload_dim(), or the terrain never draws.

  def upload_dim(self, fxi, ssbo):
    """Upload the RENDER grid dim (u_dim) into the VIS header slot 2 (@VIS_OFF+8). One uint32
    (the int path of copyDataIntoShaderStorageBuffer writes a 4-byte int32 — bit-identical to
    the uint the shader reads; a numpy array would float-cast). The per-frame reset never
    touches this slot (survives reset). Mirrors terrain_chunk_drawable.cpp's write. CALL IT ON
    EVERY ground (re)build / heights re-upload with the CURRENT render dim — the editor rebakes
    at multiple dims (preview vs full-res), so a once-at-init upload is not enough."""
    fxi.copyDataIntoShaderStorageBuffer(int(self.dim), ssbo, self.VIS_OFF + 8)

  def read_dim(self, fxi, ssbo):
    """Read back the uploaded u_dim (VIS header slot 2 @VIS_OFF+8) as an int — the upload's
    landing-offset check for headless gates."""
    import struct
    from orkengine.core import CrcStringProxy
    m = fxi.mapStorageBuffer(ssbo, self.VIS_OFF + 8, 4, CrcStringProxy().READ_ONLY)
    b = bytes(m.data)
    fxi.unmapStorageBuffer(m)
    return struct.unpack("<i", b[:4])[0]
