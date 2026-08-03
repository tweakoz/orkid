###############################################################################
# Mesh-stage geometry source for hypermesh meshlets (the VK_EXT_mesh_shader twin of the
# FWD_SSBO_CUSTOM pull VS).
#
# The terrain mesh path (dflow/terrain/gpu_chunk.py mesh_body) generates its geometry ANALYTICALLY —
# a workgroup owns an (n x n) patch of the height grid and computes every corner from a heightfield
# tap. Hypermesh content has no such structure: the geometry is an arbitrary indexed polygon mesh,
# so the meshlets are DATA — a partition computed on the CPU (ork.lev2 hypermesh/meshlet.h) and
# uploaded as three buffers:
#
#   sif_ml_desc : ml_count + per-meshlet {vertexOffset, vertexCount, primOffset, primCount}
#   sif_ml_vtx  : the flat vertex list — GLOBAL mesh vertex ids, sliced per meshlet
#   sif_ml_prim : one uint per triangle, three 8-bit LOCAL indices into that meshlet's slice
#
# so a workgroup is: read one descriptor -> pull this meshlet's vertices from the SAME per-channel
# SSBOs the pull VS reads (sif_ptex_vtx/N/B/uv/clr) -> unpack its local triangles. The vertex decode
# text is handed in BY the pull-VS source, so both paths decode a vertex identically — one contract,
# never two copies of it.
#
# Laws this obeys:
#   * gl_Position is ALWAYS `mvp * position`, exactly as the pull VS computes it. A direct
#     SSBO-sourced clip position mis-rasterizes under MoltenVK; the transform is what makes the
#     two paths the same image.
#   * Nothing tweakable is baked into the text: the meshlet COUNT is a runtime SSBO field, the
#     partition is data. Only the structural limits (workgroup size, the 256/256 taskless-tier
#     caps) are compile-time — they ARE the layout.
#   * Emitted ONLY when the mesh path is enabled (ORKID_HYPERMESH_MESHSHADER), because
#     vulkan_fxi_load creates a shader module for every stage in the program: a machine without
#     VK_EXT_mesh_shader must never be handed a mesh stage.
###############################################################################

import os
import sys

# Largest portable mesh-stage workgroup: VK_EXT_mesh_shader guarantees maxMeshWorkGroupInvocations
# >= 128 and NVIDIA reports exactly 128. Both emission loops stride by it, so a meshlet with more
# vertices or prims than invocations is simply covered in several passes.
MESH_WG_MAX = 128

###############################################################################
# DECLARED OUTPUT CAPS — a MESH-OUTPUT MEMORY BUDGET, not a preference.
#
# The taskless VK_EXT_mesh_shader tier allows 256 vertices / 256 primitives, and Vulkan drivers
# accept that. Metal does not: a mesh threadgroup's entire declared output payload (per-vertex
# outputs + per-primitive outputs) is threadgroup memory, capped at 32KB, and exceeding it fails
# vkCreateGraphicsPipelines with VK_ERROR_INITIALIZATION_FAILED at PIPELINE CREATION — i.e. on the
# first real draw, with nothing about the shader itself to point at.
#
# The ptex3d varying contract is chunky (see _VS_TAIL): wpos vec4, clr vec4, uv0 vec2, tbn MAT3,
# camdist float, camz vec3, opos vec3, onrm vec3, obin vec3, plus gl_Position — ~172 bytes rounded
# to a 176-byte 16-aligned stride. At 256 vertices that is 45KB of vertex payload alone. Terrain's
# mesh path declares an 8x8 meshlet (81 verts / 128 prims, ~14KB) against the SAME varying set,
# which is why it creates fine on the same device.
#
# So the caps are DERIVED: halve verts, then prims, until the payload fits the budget. The safety
# factor is deliberate — the exact Metal-side accounting (attribute padding, fragment interpolant
# reservation) is not published, so the derived point sits ~1.5x terrain's known-good footprint
# rather than at the theoretical line.
###############################################################################

VERTEX_OUTPUT_BYTES = 176    # 16-aligned stride of the ptex3d per-vertex varying set + gl_Position
PRIM_OUTPUT_BYTES   = 16     # 3 local indices (+ alignment); no per-primitive builtins are emitted
MESH_OUTPUT_BUDGET  = 32768  # Metal threadgroup memory available to mesh output

# NATIVE-ROUTE THRESHOLD — the cliff that has no error message.
#
# MoltenVK compiles a mesh pipeline down one of two routes. The NATIVE taskless route is taken only
# when the stage declares <=128 vertices, <=128 primitives, spills no varyings, uses NO WORKGROUP
# VARIABLES, and its estimated output stays under 16KB (MVKPipeline.mm, kMVKNativeTasklessMesh*).
# Miss ANY of those and it silently falls back to an EMULATION whose indirect/batched schedule is
# sized from a fixed 1<<22 workgroup ceiling — on our payload that is on the order of a thousand
# Metal render-pass restarts per draw. The build stays green, no validation layer complains, and the
# frame time collapses.
#
# So this is a tripwire, not a preference, and the margin is thin: the hypermesh stage sits at
# 13312B, but terrain's modelled footprint is 16304B — EIGHTY BYTES under. One added varying on a
# mesh stage craters terrain with nothing anywhere to point at. The codegen gate asserts against
# this constant so that addition fails loudly at generation time instead of silently at runtime.
#
# These ceilings describe MOLTENVK, not VK_EXT_mesh_shader: a driver with a native mesh stage has no
# second route to fall off, so on such a platform they constrain nothing — and meshlet_caps() there
# deliberately keeps the 256/256 tier maximum, which EXCEEDS them. Read them through
# native_route_limits(), never raw, so the ceiling carries the same platform scope as the budget it
# belongs to.
MESH_NATIVE_ROUTE_BUDGET = 16384
MESH_NATIVE_ROUTE_MAX_VERTS = 128
MESH_NATIVE_ROUTE_MAX_PRIMS = 128
MESH_BUDGET_SAFETY  = 0.75   # margin against unpublished driver-side accounting


def metal_mesh_backend(platform=None):
  """True where the Vulkan mesh stage is TRANSLATED to Metal by MoltenVK.

  ONE predicate for the two platform-scoped facts here — the 32KB threadgroup output budget and the
  native-vs-emulated route split — because they are the same translation layer seen twice. Scoped
  separately they drift apart, and a ceiling applied where the route does not exist asserts a
  constraint with no meaning."""
  return (platform or sys.platform) == "darwin"


def native_route_limits(platform=None):
  """(byte budget, max_vertices, max_primitives) of MoltenVK's NATIVE taskless mesh route, or None
  on a platform whose driver has no such route (nothing to fall off, nothing to assert).

  Same platform split as meshlet_caps' budget=None default, taken from the same predicate."""
  if not metal_mesh_backend(platform):
    return None
  return (MESH_NATIVE_ROUTE_BUDGET, MESH_NATIVE_ROUTE_MAX_VERTS, MESH_NATIVE_ROUTE_MAX_PRIMS)


def meshlet_caps(budget=None, per_vertex=VERTEX_OUTPUT_BYTES, per_prim=PRIM_OUTPUT_BYTES):
  """(max_vertices, max_primitives) that fit the platform's mesh-output budget.

  budget=None picks the platform default (Metal's threadgroup limit on darwin, unlimited
  elsewhere); budget=0 means "no budget" and yields the taskless-tier maximum. Halving keeps both
  caps powers of two, which keeps the local-index packing and the builder's bucket arithmetic
  exactly as they are. Primitives are held at 2x vertices (the Euler bound for a triangle mesh —
  a bucket of V unique vertices cannot reach more than ~2V triangles), so budget spent on
  primitive capacity is capacity the builder can actually reach."""
  if budget is None:
    budget = MESH_OUTPUT_BUDGET if metal_mesh_backend() else 0
  verts = 256
  prims = min(256, 2 * verts)
  if budget <= 0:
    return (verts, prims)
  limit = int(budget * MESH_BUDGET_SAFETY)
  while verts * per_vertex + prims * per_prim > limit and verts > 32:
    verts //= 2
    prims = min(256, 2 * verts)
  return (verts, prims)


# These MUST equal kMeshletMaxVerts / kMeshletMaxPrims in
# ork.lev2/inc/ork/lev2/gfx/hypermesh/meshlet.h — the builder buckets to them and the mesh stage
# declares them as its output limits; a split pair means the layout cannot hold what the CPU built
# (the codegen gate pins them against the live C++ values).
MESHLET_MAX_VERTS, MESHLET_MAX_PRIMS = meshlet_caps()

# The three meshlet buffers, in binding-declaration order. The C++ drawable binds by BLOCK NAME,
# but shadlang assigns binding ids in SPIR-V declaration order, so this text is the order of record.
MESHLET_BLOCKS = (
  "storage_interface sif_ml_desc (descriptor_set 0) { buffer layout(std430) hm_mld {\n"
  "  uint ml_count; uint ml_pad0; uint ml_pad1; uint ml_pad2;   // live meshlet count (pooled buffer may be larger)\n"
  "  uint MLDesc[]; }; }                                        // 4 uints/meshlet: vtxOff,vtxCnt,primOff,primCnt\n"
  "storage_interface sif_ml_vtx  (descriptor_set 0) { buffer layout(std430) hm_mlv { uint MLVerts[]; }; }  // global vert ids\n"
  "storage_interface sif_ml_prim (descriptor_set 0) { buffer layout(std430) hm_mlp { uint MLPrims[]; }; }  // 3x8-bit local\n")

MESHLET_BLOCK_NAMES = ("sif_ml_desc", "sif_ml_vtx", "sif_ml_prim")

# PER-CLUSTER BOUNDS — the reject contract. 2 vec4 per meshlet: (sphere xyz + radius) then
# (cone axis xyz + cutoff), both OBJECT SPACE, produced on the GPU by cs_meshlet_bounds
# (ork.lev2 hmdflow_render.cpp) from the LIVE positions. Emitted only for materials whose mesh
# stage does NOT displace vertices: a bound derived from the stored positions cannot bound a
# displacement the stage applies afterwards, and a bound that is too small culls a VISIBLE
# cluster. Absent block -> the C++ side logs "cluster reject OFF" and draws every cluster.
MESHLET_BOUNDS_BLOCK = (
  "storage_interface sif_ml_bounds (descriptor_set 0) { buffer layout(std430) hm_mlb {\n"
  "  vec4 MLBounds[]; }; }   // 2/meshlet: sphere(xyz,r), cone(axis,cutoff) — object space\n")

MESHLET_BOUNDS_BLOCK_NAME = "sif_ml_bounds"


def meshmode():
  """ORKID_HYPERMESH_MESHSHADER as an INT: 0/absent = the pull-VS baseline, 1 = the mesh path
  (direct-sized dispatch — one workgroup per published meshlet). Mirrors the C++ drawable's atoi
  read, so ONE env decides both the codegen and the consumer: a toggle-on run materializes a
  material that actually CARRIES FWD_SSBO_CUSTOM_MESH and the drawable then finds it (and with the
  toggle off the drawable's loud refusal stays honest)."""
  v = os.environ.get("ORKID_HYPERMESH_MESHSHADER")
  try:
    return int(v) if v else 0
  except ValueError:
    return 0


def meshcullstats():
  """ORKID_HYPERMESH_MESHCULL_STATS — emit the per-cluster reject COUNTERS into the mesh stage.

  Off by default, and deliberately a CODEGEN-time switch rather than a runtime one: with it off the
  generated text is byte-identical to the counter-free shader, so the measured path and the shipped
  path are the same program plus nothing. Turning it on changes the source, which changes the
  content-addressed material digest, so a stats run can never collide with a cached silent build.

  Mirrors the C++ reader (hm_meshlet.cpp meshCullStatsEnabled) — ONE env decides both sides."""
  v = os.environ.get("ORKID_HYPERMESH_MESHCULL_STATS")
  return bool(v) and v != "0"


# REJECT COUNTERS — 2 monotonic uints: [0] clusters TESTED, [1] clusters REJECTED. Written only by
# the GPU (one atomic pair per workgroup, from a single invocation), never by the host: a host write
# to a host-visible buffer shadows subsequent GPU writes under MoltenVK. They are never reset either,
# so the host reads raw values and reports the DELTA between frames — unsigned wraparound makes that
# subtraction correct no matter what the buffer started as, which is what lets it stay uninitialized.
#
# `readwrite` is load-bearing: a storage interface with no declared access is READONLY in any
# non-compute stage (the shadlang default), and an atomicAdd on a readonly buffer has no l-value —
# the stage fails to compile. This is the ONLY meshlet block a graphics stage writes; the rest stay
# undeclared, hence readonly.
MESHLET_CULLSTAT_BLOCK = (
  "storage_interface sif_ml_cull (descriptor_set 0) { buffer layout(std430, readwrite) hm_mlc {\n"
  "  uint MLCull[]; }; }   // [0] clusters tested, [1] clusters rejected (monotonic, GPU-only)\n")

MESHLET_CULLSTAT_BLOCK_NAME = "sif_ml_cull"


class MeshletMeshSource:
  """The mesh-stage half of GpuMeshRenderSource. Constructed BY that source (it owns the vertex
  decode + any vertex displacement), and handed to the ptex3d template as `mesh_source` — the
  template then asks it for the interface + body around ITS varying contract.

  vertex_decode : the pull-VS decode text with the vertex index taken from an INDEX EXPRESSION
                  (the meshlet's vertex-list entry) instead of gl_VertexID. Leaves the same locals
                  (position/normal/binormal/uv0/uv0z/vtxcolor) the varying writes consume.
  displace_calls: the VertexDisplace lines appended after the decode (identical to the pull VS).
  wants_inst_data: the displace reads a per-instance vec4; the mesh path is never instanced, so it
                  gets the same vec4(0) the un-instanced pull VS gets."""

  def __init__(self, vertex_decode, displace_calls="", wants_inst_data=False):
    self._decode     = vertex_decode
    self._displace   = displace_calls
    self._wants_inst = bool(wants_inst_data)
    # CLUSTER REJECT is available exactly when the stage emits the stored positions unmodified.
    # A displacing stage moves vertices AFTER any precomputed bound was derived, so the bound no
    # longer contains what gets emitted — the reject is disabled rather than made approximate.
    self._cull = not bool(displace_calls and str(displace_calls).strip())
    self._cullstats = self._cull and meshcullstats()

  # ---- what the mesh stage needs BEYOND the pull-VS inherit list ----
  # ONLY the meshlet buffers: the channel blocks (sif_N/B/uv/clr) and any displace param block
  # already reach the mesh stage through the template's own inherit list (the same
  # ssbo_vs_inherits the pull VS carries), and inheriting a block twice is not a thing to risk.
  def inherits(self):
    return (MESHLET_BLOCK_NAMES
            + ((MESHLET_BOUNDS_BLOCK_NAME,) if self._cull else ())
            + ((MESHLET_CULLSTAT_BLOCK_NAME,) if self._cullstats else ()))

  def blocks(self):
    """The storage-block TEXT this source needs, in declaration order (shadlang assigns binding ids
    in SPIR-V declaration order, so this is the order of record)."""
    return (MESHLET_BLOCKS
            + (MESHLET_BOUNDS_BLOCK if self._cull else "")
            + (MESHLET_CULLSTAT_BLOCK if self._cullstats else ""))

  def mesh_interface(self, name="vif_ptex_mesh", storage="sif_ptex_vtx", outputs="", inherits=()):
    """The mesh stage's interface: a flat 1D workgroup (the body indexes purely by
    gl_LocalInvocationIndex and both emission loops stride by it, so the invocation count is free
    to sit below the vertex/prim caps) plus the EXT topology/limits line and the per-vertex
    varyings the template supplies — the SAME varyings in the SAME order as the pull-VS interface,
    or the two paths' locations desynchronize."""
    names = list(inherits) if inherits else [storage]
    for n in self.inherits():
      if n not in names:
        names.append(n)
    inh  = "".join(" : %s" % n for n in names)
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
      "}" % (name, inh, MESH_WG_MAX, MESHLET_MAX_VERTS, MESHLET_MAX_PRIMS, outs))

  def mesh_body(self, mvp="mvp", varying_writes=""):
    """One workgroup = one meshlet: descriptor -> vertices -> local triangles.

    `mvp` names the clip-space matrix (the material's `mvp` uniform); `varying_writes` is the
    template's per-vertex varying text with $V standing for the emitted vertex index, running with
    the SAME locals the pull VS leaves behind.

    The ml_count guard is the self-defence: the dispatch is CPU-sized from the published partition,
    so an over-dispatch (a partition that shrank between sizing and draw) emits nothing instead of
    reading a stale descriptor slot."""
    vw = ""
    for line in varying_writes.strip().splitlines():
      vw += "    %s\n" % line.strip().replace("$V", "vi")
    dec = ""
    for line in self._decode.strip().splitlines():
      dec += "    %s\n" % line.strip()
    if self._wants_inst:
      dec = "    vec4 inst_data = vec4(0.0);   // the mesh path is never instanced\n" + dec
    dsp = ""
    for line in self._displace.strip().splitlines():
      if line.strip():
        dsp += "    %s\n" % line.strip()
    # CLUSTER REJECT — whole-meshlet frustum test, before ANY decode work. The planes are pulled
    # from the SAME matrix this body uses to place vertices, so the test can never disagree with
    # what would have been rasterized: in a shadow cascade it is the cascade's matrix, in the eye
    # pass the eye's. Deriving them from a separately-bound view/proj could diverge per pass, and a
    # diverged plane set over-culls — which is a dropped visible cluster, not a missed saving.
    #
    # Object space throughout: the stored bound is object space and the Gribb-Hartmann rows of an
    # object->clip matrix ARE object-space planes, so no bound needs transforming and no scale
    # factor has to be recovered from the model matrix.
    #
    # NO workgroup/shared variables anywhere in this stage: MoltenVK drops a mesh pipeline that uses
    # them off its NATIVE taskless path onto an emulation that restarts the Metal render pass over a
    # thousand times per draw. Every lane redundantly computing six planes is far cheaper than that.
    #
    # The normal cone is stored by the bounds pass but NOT tested here: the test needs the eye in
    # object space, and the only per-pass-authoritative matrix this stage has is the combined one,
    # from which the eye cannot be recovered. Backface rejection waits for an eye source proven to
    # track the same pass, rather than risking an over-cull for it.
    cull = ""
    if self._cull:
      cull = (
        "vec4 _b0 = MLBounds[ml * 2u + 0u];\n"
        "vec4 _r0 = vec4(%(M)s[0][0], %(M)s[1][0], %(M)s[2][0], %(M)s[3][0]);\n"
        "vec4 _r1 = vec4(%(M)s[0][1], %(M)s[1][1], %(M)s[2][1], %(M)s[3][1]);\n"
        "vec4 _r2 = vec4(%(M)s[0][2], %(M)s[1][2], %(M)s[2][2], %(M)s[3][2]);\n"
        "vec4 _r3 = vec4(%(M)s[0][3], %(M)s[1][3], %(M)s[2][3], %(M)s[3][3]);\n"
        "bool _out = false;\n"
        # Vulkan clip volume: z in [0,1] -> the near plane is row2 alone (not row3+row2).
        "for (int _i = 0; _i < 6; _i++) {\n"
        "    vec4 _pl = (_i == 0) ? (_r3 + _r0) : (_i == 1) ? (_r3 - _r0)\n"
        "             : (_i == 2) ? (_r3 + _r1) : (_i == 3) ? (_r3 - _r1)\n"
        "             : (_i == 4) ? _r2         : (_r3 - _r2);\n"
        "    float _ln = length(_pl.xyz);\n"
        "    if (_ln > 1.0e-12 && (dot(_pl.xyz, _b0.xyz) + _pl.w) / _ln < -_b0.w) { _out = true; }\n"
        "}\n"
        + ((
        # ONE increment per workgroup, from lane 0 only: every lane derived _out from the same
        # workgroup-uniform data, so counting per-lane would multiply every figure by the local size.
        # No barrier and no shared variable — an SSBO atomic is Buffer storage, which keeps the stage
        # on MoltenVK's native taskless route (see the payload guard).
        "if (gl_LocalInvocationIndex == 0u) {\n"
        "    atomicAdd(MLCull[0], 1u);\n"
        "    if (_out) { atomicAdd(MLCull[1], 1u); }\n"
        "}\n") if self._cullstats else "")
        + "if (_out) { SetMeshOutputsEXT(0u, 0u); return; }   // cluster outside the frustum\n"
      ) % dict(M=mvp)
    return (
      "uint ml = gl_WorkGroupID.x;\n"
      "if (ml >= ml_count) { SetMeshOutputsEXT(0u, 0u); return; }   // over-dispatch -> emit nothing\n"
      + cull +
      "uint d0    = ml * 4u;\n"
      "uint v_off = MLDesc[d0 + 0u];\n"
      "uint v_cnt = MLDesc[d0 + 1u];\n"
      "uint p_off = MLDesc[d0 + 2u];\n"
      "uint p_cnt = MLDesc[d0 + 3u];\n"
      # workgroup-UNIFORM counts (everything above derives from the workgroup id + the descriptor).
      "SetMeshOutputsEXT(v_cnt, p_cnt);\n"
      "if (p_cnt == 0u) { return; }\n"
      # each UNIQUE meshlet vertex is decoded ONCE (the pull VS re-decodes every shared corner per
      # triangle). The index is the partition's global vertex id — the same id gl_VertexIndex would
      # have carried through the index buffer, so every channel tap is identical.
      "for (uint vi = gl_LocalInvocationIndex; vi < v_cnt; vi += %(WG)du) {\n"
      "%(DEC)s"
      "%(DSP)s"
      "    gl_MeshVerticesEXT[vi].gl_Position = %(MVP)s * position;\n"
      "%(VARY)s"
      "}\n"
      # LOCAL indices, three 8-bit fields per triangle — the builder's pack (meshlet.h primIndices).
      "for (uint pi = gl_LocalInvocationIndex; pi < p_cnt; pi += %(WG)du) {\n"
      "    uint packed = MLPrims[p_off + pi];\n"
      "    gl_PrimitiveTriangleIndicesEXT[pi] = uvec3(packed & 255u, (packed >> 8u) & 255u, (packed >> 16u) & 255u);\n"
      "}"
      % dict(WG=MESH_WG_MAX, DEC=dec, DSP=dsp, VARY=vw, MVP=mvp))
