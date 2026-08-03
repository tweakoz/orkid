////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// hmdflow_render.cpp — render-time, on-GPU triangulation of the indexed attribute mesh.
//
// The v2 GpuMesh is an INDEXED polygon mesh with MIXED faces (tri/quad/ngon via face_offsets). The
// graphics pipeline wants triangles, so each frame a small raw-FXI compute (the SAME compute API the
// generator modules use — pure on-GPU, no readback) fan-triangulates the faces into a uint32 tri
// vertex-index buffer + writes a VkDrawIndexedIndirectCommand. The drawable then DrawIndexedIndirect's
// off those GPU buffers; the pull VS reads vert attrs by gl_VertexIndex (the index buffer maps each
// drawn index -> a mesh vertex). Shaders compile ONCE; per frame is just bind+dispatch (+ a host write
// of num_faces, and an index-buffer re-pool only when the topology grows past a pow2 boundary).
//
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <ork/lev2/gfx/hypermesh/meshlet.h> // CPU meshlet partition (rebuilt on the re-pool hook)
#include <ork/util/logger.h>                // HYPERMESH-MESHSHADER channel (loud path refusals)
#include <chrono>
#include <set>        // O3 stage 3: dedup unique capture pipes across layers
#include <filesystem> // impostor atlas dump dir (ORKID_IMPOSTOR_DUMP)
#include <ork/file/path.h> // O3 stage 3: <assetcache> section-array cache dir
#include <ork/lev2/gfx/renderer/compute_drawable.h>
#include <ork/lev2/gfx/renderphasestats.h> // perf HUD: hypermesh-gen compute timing
#include <ork/lev2/gfx/renderer/cull_debug.h> // ORKID_DISABLE_FRUSTUM_CULL / _OCCLUSION_CULL debug levers
#include <ork/lev2/gfx/renderer/hzb.h> // HZBBuilder — the per-view occlusion source (read from the RCFD)
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h> // terrain::BakeEnv — the clock mirror for field subgraphs (E.1b)
// impostor bake (A2): offscreen MRT capture of the base mesh from hemi-octahedral angles.
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/rasterstate.h> // section-atlas bake forces CullTest=OFF (winding-agnostic rasterize)
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/image.h>
#include <ork/kernel/datacache.h>

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////

// E.3 — gid render bucketing. The index buffer is built in CONTIGUOUS PER-GID RANGES (A1 tag
// contract: gid = __tags[20:32), the per-face material-class key) and the args buffer is an ARRAY
// of 4096 VkDrawIndexedIndirectCommand slots — one per gid; the drawable issues one indexed-indirect
// draw per BOUND gid (its own material) at argsOffset = gid*20. Gids with no bound material FOLD TO
// SLOT 0 at count/scatter time (the host-written BND bitmask), so an assign_gid without a material
// binding renders with the default material instead of vanishing (ops self-defend). No __tags
// channel -> everything lands in slot 0 = the classic single-draw shape.
static constexpr int kGidSlots = 4096;

///////////////////////////////////////////////////////////////////////////////
// POINT-OF-USE ARBITER for the hypermesh BAKE draws (impostor hemi-oct atlas, section
// atlas). Both are forced-technique captures rasterized through a pushed ortho with an
// identity model, so they are MONO BY CONSTRUCTION even when the frame that triggers
// them is a stereo one — but until this line existed, a census could not distinguish
// "mono because the bake is a bake" from "mono because a stereo peer was missed". The
// live pass's own stereo bit is printed beside the technique so the two are separable.
//
// Once per pipeline. Grep token: SPVR:BAKESEL
///////////////////////////////////////////////////////////////////////////////

static void _announceBakeDraw(
    const RenderContextInstData& RCID, //
    fxpipeline_ptr_t pipe,             //
    const char* bake,                  //
    int gid) {
  if ((nullptr == pipe) or (nullptr == pipe->_technique))
    return;
  static std::set<const void*> s_announced;
  if (not s_announced.insert((const void*)pipe.get()).second)
    return;
  auto RCFD   = RCID.rcfd();
  bool cpd_st = RCFD->hasCPD() ? RCFD->topCPD().isSinglePassStereo() : false;
  printf(
      "[SPVR:BAKESEL] bake DRAW kind<%s> gid<%d> technique<%s> permu_stereo<0> pass_stereo<%d> "
      "(ortho capture — mono by construction)\n",
      bake,
      gid,
      pipe->_technique->_techniqueName.c_str(),
      int(cpd_st));
  fflush(stdout);
}

///////////////////////////////////////////////////////////////////////////////
// MESH-SHADER DRAW (ORKID_HYPERMESH_MESHSHADER=1) — the resolved, per-drawable decision. Resolved
// ONCE in setupMeshRender (capabilities + the material's mesh technique pair + the meshlet storage
// blocks) and consumed by the live hook, which flips the drawable onto the mesh path only while a
// published partition actually matches the live topology.
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_hmml = logger()->configureChannel("HYPERMESH-MESHSHADER", fvec3(0.4, 1, 1), false);

struct MeshletDrawState {
  fxtechnique_constptr_t _tek        = nullptr;
  const FxShaderStorageBlock* _blkDesc = nullptr;
  const FxShaderStorageBlock* _blkVtx  = nullptr;
  const FxShaderStorageBlock* _blkPrim = nullptr;
  // CLUSTER REJECT (per-meshlet FRUSTUM only — the stored normal cone is not tested; see the mesh
  // stage's reject block in gpu_meshlet.py). Null when the material carries no bounds contract —
  // which today's codegen omits for a mesh stage that DISPLACES vertices (a bound computed from the
  // stored positions cannot bound a shader-side displacement), though a material generated before
  // the contract existed presents identically here and only the shader source tells them apart.
  // Null means "reject disabled", NOT "broken": the mesh path still draws, every cluster.
  const FxShaderStorageBlock* _blkBounds = nullptr;
  int _slot       = -1; // index of the first of the meshlet storages in _graphicsStorage
  bool _ready     = false; // everything resolved: the hook may switch the draw
  int _lastReason = -1;    // last per-frame drive decision (0 = driving); logs on TRANSITIONS only
  const void* _boundsPart = nullptr; // partition the cluster bounds were last derived from
  // reject-counter readback state (ORKID_HYPERMESH_MESHCULL_STATS). The counters are monotonic and
  // never reset, so what is reported is the DELTA across one frame; _statHave suppresses the first
  // report, which has no predecessor to difference against.
  const FxShaderStorageBlock* _blkCull = nullptr;
  uint32_t _statPrevTested = 0, _statPrevRejected = 0;
  bool _statHave = false;
};
using meshletdrawstate_ptr_t = std::shared_ptr<MeshletDrawState>;

// Resolve the mesh draw path for THIS drawable. Every refusal is loud and names its cause — a
// silent step-down to the pull-VS path is indistinguishable from the mesh path working.
static meshletdrawstate_ptr_t _resolveMeshletDraw(
    ComputeDrawableData* cdd, Context* ctx, bool instanced, bool wireframe, bool gid_buckets) {
  auto st = std::make_shared<MeshletDrawState>();
  if (meshShaderMode() < 1)
    return st;
  auto pbr   = std::dynamic_pointer_cast<PBRMaterial>(cdd->_material);
  auto fsmtl = pbr ? pbr->_as_freestyle : nullptr;
  if (instanced) {
    logchan_hmml->log("requested, but this drawable is INSTANCED (the mesh stage places no "
                      "per-instance matrix) — using the pull-VS path");
    return st;
  }
  if (gid_buckets) {
    // the partition covers the WHOLE mesh; per-gid bucket draws re-draw slices of that same mesh
    // with their own materials, so running both would double-draw every bucketed triangle.
    logchan_hmml->log("requested, but this drawable has per-gid bucket draws (the partition covers "
                      "the whole mesh) — using the pull-VS path");
    return st;
  }
  if (not ctx->supportsMeshShader()) {
    logchan_hmml->log("requested, but this device has no VK_EXT_mesh_shader — using the pull-VS path");
    return st;
  }
  if (not fsmtl) {
    logchan_hmml->log("requested, but the drawable's material is not freestyle-backed — using the "
                      "pull-VS path");
    return st;
  }
  auto tek = fsmtl->technique("FWD_SSBO_CUSTOM_MESH");
  auto dpp = fsmtl->technique("FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS");
  if (not tek or not dpp) {
    // The shader path is the discriminator, and it is the reason this line prints it: the generated
    // .fxv2 is content-addressed (its digest covers the whole generated source, mesh block
    // included), so a path whose file DOES carry the technique means the compiled artifact behind
    // it is inconsistent with its own source — not that the wrong material was picked.
    logchan_hmml->log("requested, but the material carries no FWD_SSBO_CUSTOM_MESH%s technique — "
                      "using the pull-VS path. shader<%s> (grep that file for the technique: present "
                      "=> stale compiled artifact, absent => the material was generated with the "
                      "toggle off)",
                      tek ? "_DEPTHPREPASS" : "",
                      pbr->_shaderpath.c_str());
    return st;
  }
  st->_blkDesc = fsmtl->storageBlock("sif_ml_desc");
  st->_blkVtx  = fsmtl->storageBlock("sif_ml_vtx");
  st->_blkPrim = fsmtl->storageBlock("sif_ml_prim");
  if (not st->_blkDesc or not st->_blkVtx or not st->_blkPrim) {
    logchan_hmml->log("requested, but the material has a mesh stage without the meshlet storage "
                      "blocks (sif_ml_desc/vtx/prim) — stale material, re-materialize — using the "
                      "pull-VS path");
    return st;
  }
  if (wireframe) // the LINE overlay is a pull-VS indexed draw off the triangulator's line buffer
    logchan_hmml->log("wireframe overlay stays on the pull-VS line draw (the mesh path drives the "
                      "fill only)");
  st->_blkCull   = fsmtl->storageBlock("sif_ml_cull");
  st->_blkBounds = fsmtl->storageBlock("sif_ml_bounds");
  if (not st->_blkBounds)
    // OBSERVED: the block is absent. NOT observed: why. The codegen omits it for a displacing mesh
    // stage, but a material generated before the bounds contract existed is indistinguishable from
    // here — so the line reports what it saw, names the expected cause as expected, and hands over
    // the one grep that separates them (same discriminator idiom as the technique refusal above).
    logchan_hmml->log("cluster reject OFF: this material declares no sif_ml_bounds block, so every "
                      "published cluster draws. Expected cause is a mesh stage that DISPLACES "
                      "vertices (a bound derived from the stored positions cannot bound what such a "
                      "stage emits) — but this side sees only the missing block. shader<%s> (grep "
                      "that file: a displace body present => expected, absent => the material "
                      "predates the bounds contract, re-materialize)",
                      pbr->_shaderpath.c_str());
  st->_tek   = tek;
  st->_ready = true;
  logchan_hmml->log("enabled: direct-sized DrawMeshTasks, one workgroup per published meshlet, "
                    "cluster reject %s. shader<%s>",
                    st->_blkBounds ? "ON (frustum only; the normal cone is computed and stored but "
                                     "NOT tested — no backface rejection is running)"
                                   : "OFF",
                    pbr->_shaderpath.c_str());
  return st;
}

///////////////////////////////////////////////////////////////////////////////
// PER-CLUSTER BOUNDS — the data the mesh stage's cluster reject runs on. One thread per meshlet
// reduces its own bucket (<=64 verts, <=128 prims at the darwin caps) to an OBJECT-SPACE bounding
// sphere + a normal cone; the mesh stage transforms them per view.
//
// Object space, not world space, ON PURPOSE: the bounds are then camera-independent, so ONE
// evaluation serves the color pass, the depth prepass and every sun cascade. Recomputed next to the
// triangulate (same dispatch phase) whenever the graph re-evaluated or a new partition landed, which
// is exactly when positions can have moved.
//
// The cone cutoff is the SINE of the cone half-angle (meshopt's convention), with 2.0 reserved as
// "cone unusable" (normals span more than a hemisphere) — a value the reject test can never satisfy,
// so a degenerate cluster is simply never backface-rejected. Over-culling is a correctness bug;
// under-culling is only a missed saving, so every ambiguity resolves toward drawing.
///////////////////////////////////////////////////////////////////////////////

static std::string _meshlet_bounds_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface sif_mld (descriptor_set 0) { buffer layout(std430) mldb {
  uint ml_count; uint mlpad0; uint mlpad1; uint mlpad2; uint MLDesc[]; }; }
storage_interface sif_mlv (descriptor_set 0) { buffer layout(std430) mlvb { uint MLVerts[]; }; }
storage_interface sif_mlp (descriptor_set 0) { buffer layout(std430) mlpb { uint MLPrims[]; }; }
storage_interface sif_pos (descriptor_set 0) { buffer layout(std430) posb { vec4 Pd[]; }; }
storage_interface sif_bnd (descriptor_set 0) { buffer layout(std430) bndb { vec4 MLBounds[]; }; }
compute_interface iface { storage { sif_mld sif_mlv sif_mlp sif_pos sif_bnd }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_meshlet_bounds : iface {
  uint ml = gl_GlobalInvocationID.x;
  if (ml >= ml_count) { return; }
  uint d0    = ml * 4u;
  uint v_off = MLDesc[d0 + 0u];
  uint v_cnt = MLDesc[d0 + 1u];
  uint p_off = MLDesc[d0 + 2u];
  uint p_cnt = MLDesc[d0 + 3u];
  if (v_cnt == 0u) {          // empty bucket: radius 0 + unusable cone -> frustum rejects it, cone never does
    MLBounds[ml * 2u + 0u] = vec4(0.0, 0.0, 0.0, 0.0);
    MLBounds[ml * 2u + 1u] = vec4(0.0, 1.0, 0.0, 2.0);
    return;
  }
  // sphere: centroid, then the true max radius about it (a centroid sphere is never smaller than the
  // cluster, which is the direction that keeps the reject conservative).
  vec3 ctr = vec3(0.0);
  for (uint i = 0u; i < v_cnt; i++) { ctr += Pd[MLVerts[v_off + i]].xyz; }
  ctr /= float(v_cnt);
  float rad = 0.0;
  for (uint i = 0u; i < v_cnt; i++) { rad = max(rad, length(Pd[MLVerts[v_off + i]].xyz - ctr)); }
  // normal cone: average the triangle normals for the axis, then take the WORST alignment as the
  // half-angle. Degenerate triangles contribute nothing (zero-length normal).
  vec3 nsum = vec3(0.0);
  for (uint t = 0u; t < p_cnt; t++) {
    uint packed = MLPrims[p_off + t];
    vec3 a = Pd[MLVerts[v_off + (packed        & 255u)]].xyz;
    vec3 b = Pd[MLVerts[v_off + ((packed >> 8u) & 255u)]].xyz;
    vec3 c = Pd[MLVerts[v_off + ((packed >> 16u) & 255u)]].xyz;
    vec3 n = cross(b - a, c - a);
    float l = length(n);
    if (l > 1.0e-12) { nsum += n / l; }
  }
  float axlen = length(nsum);
  vec4 cone = vec4(0.0, 1.0, 0.0, 2.0);   // sentinel: unusable cone
  if (axlen > 1.0e-6) {
    vec3 ax = nsum / axlen;
    float mindot = 1.0;
    for (uint t = 0u; t < p_cnt; t++) {
      uint packed = MLPrims[p_off + t];
      vec3 a = Pd[MLVerts[v_off + (packed        & 255u)]].xyz;
      vec3 b = Pd[MLVerts[v_off + ((packed >> 8u) & 255u)]].xyz;
      vec3 c = Pd[MLVerts[v_off + ((packed >> 16u) & 255u)]].xyz;
      vec3 n = cross(b - a, c - a);
      float l = length(n);
      if (l > 1.0e-12) { mindot = min(mindot, dot(ax, n / l)); }
    }
    // mindot <= 0 => the normals span at least a hemisphere => no direction can see the whole
    // cluster's back, so the cone stays unusable rather than pretending to a bound it does not have.
    if (mindot > 0.0) { cone = vec4(ax, sqrt(max(0.0, 1.0 - mindot * mindot))); }
  }
  MLBounds[ml * 2u + 0u] = vec4(ctr, rad);
  MLBounds[ml * 2u + 1u] = cone;
}
)S";
}

// Driver for the bounds pass. Owns only the shader; the buffers belong to MeshletHost (desc/vtx/prim/
// bounds) and to the mesh (positions), so there is nothing here to keep in sync with a rebuild.
struct MeshletBounds {
  const FxComputeShader* _cs = nullptr;
  void build(Context* ctx) {
    auto fxi = ctx->FXI();
    auto sh  = fxi->shaderFromShaderText("hypermesh_meshlet_bounds", _meshlet_bounds_text());
    _cs      = fxi->computeShader(sh, "cs_meshlet_bounds");
  }
  // IN-dispatch-phase. `nml` sizes the dispatch; the shader re-checks ml_count from the buffer, so an
  // over-dispatch against a partition that shrank writes nothing.
  void dispatch(Context* ctx, MeshletHost* host, gpumesh_ptr_t mesh, uint32_t nml) {
    auto pch = mesh->channel(MeshChannel::POSITION);
    if (not _cs or not pch or 0 == nml)
      return;
    auto ci = ctx->CI();
    ci->bindStorageBuffer(_cs, 0, host->_descSSBO);
    ci->bindStorageBuffer(_cs, 1, host->_vtxSSBO);
    ci->bindStorageBuffer(_cs, 2, host->_primSSBO);
    ci->bindStorageBuffer(_cs, 3, pch->_ssbo);
    ci->bindStorageBuffer(_cs, 4, host->_boundsSSBO);
    ci->dispatchCompute(_cs, (nml + 63) / 64, 1, 1);
    ci->storageBarrier(); // bounds write -> mesh-stage read
  }
};

static std::string _triangulate_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface sif_par (descriptor_set 0) { buffer layout(std430) parb {
  uint num_faces; uint inst_count; uint has_tags; uint pad2; }; } // inst_count -> indirect instanceCount (1 = non-instanced)
storage_interface sif_fo  (descriptor_set 0) { buffer layout(std430) fob { uint FOd[]; }; }  // CSR offsets
storage_interface sif_vi  (descriptor_set 0) { buffer layout(std430) vib { uint VId[]; }; }  // corner->vert
storage_interface sif_idx (descriptor_set 0) { buffer layout(std430) idb { uint IDX[]; }; }  // out tri vert-indices
storage_interface sif_arg (descriptor_set 0) { buffer layout(std430) arb { uint ARG[]; }; }  // 4096 x VkDrawIndexedIndirectCommand (5 uints each)
storage_interface sif_tf  (descriptor_set 0) { buffer layout(std430) tfb { uint TFd[]; }; }   // out-tri -> source FACE id
storage_interface sif_lidx (descriptor_set 0) { buffer layout(std430) lib { uint LIDX[]; }; } // out LINE vert-indices (edges)
storage_interface sif_larg (descriptor_set 0) { buffer layout(std430) lab {                   // VkDrawIndexedIndirectCommand (lines)
  uint l_indexCount; uint l_instanceCount; uint l_firstIndex; uint l_vertexOffset; uint l_firstInstance; }; }
storage_interface sif_hist (descriptor_set 0) { buffer layout(std430) hsb { uint HIST[]; }; } // per-gid tri-index counts -> write cursors
storage_interface sif_bnd  (descriptor_set 0) { buffer layout(std430) bnb { uint BND[]; }; }  // 128 uints: bound-gid bitmask (bit 0 always)
storage_interface sif_tags (descriptor_set 0) { buffer layout(std430) tgb { uint TAGS[]; }; } // face __tags (dummy when has_tags=0)
storage_interface sif_P    (descriptor_set 0) { buffer layout(std430) ppb { vec4 Pd[]; }; }   // positions (E.5 ear clip)
// ONE shared interface for all shaders so each storage resolves to a SINGLE global binding id
// (sif_par=0 sif_fo=1 sif_vi=2 sif_idx=3 sif_arg=4 sif_tf=5 sif_lidx=6 sif_larg=7 sif_hist=8
//  sif_bnd=9 sif_tags=10 sif_P=11); every shader binds all slots consistently.
compute_interface iface { storage { sif_par sif_fo sif_vi sif_idx sif_arg sif_tf sif_lidx sif_larg sif_hist sif_bnd sif_tags sif_P }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
// reset: one thread per gid slot zeroes its histogram bin + draw command; thread 0 also resets lines.
compute_shader cs_reset : iface {
  uint s = gl_GlobalInvocationID.x;
  if (s == 0u) {
    l_indexCount = 0u; l_instanceCount = inst_count; l_firstIndex = 0u; l_vertexOffset = 0u; l_firstInstance = 0u;
  }
  if (s >= 4096u) { return; }
  HIST[s] = 0u;
  ARG[s * 5u + 0u] = 0u;          // indexCount
  ARG[s * 5u + 1u] = inst_count;  // instanceCount
  ARG[s * 5u + 2u] = 0u;          // firstIndex
  ARG[s * 5u + 3u] = 0u;          // vertexOffset
  ARG[s * 5u + 4u] = 0u;          // firstInstance
}
////////////////////////////////////////
// count: per-face triangle-index counts into the face's gid slot (unbound gids fold to slot 0).
compute_shader cs_count : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= num_faces) { return; }
  uint a = FOd[f]; uint b = FOd[f + 1u]; uint n = b - a;
  if (n < 3u) { return; }
  uint g = 0u;
  if (has_tags != 0u) {
    g = (TAGS[f] >> 20u) & 0xFFFu;
    if (((BND[g >> 5u] >> (g & 31u)) & 1u) == 0u) { g = 0u; }
  }
  atomicAdd(HIST[g], (n - 2u) * 3u);
}
////////////////////////////////////////
// scan: serial exclusive prefix over the 4096 bins (1 thread; runs only on topo-dirty frames) ->
// each slot's draw command gets {indexCount, firstIndex}; HIST becomes the scatter write-cursor.
compute_shader cs_scan : iface {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  uint running = 0u;
  for (uint s = 0u; s < 4096u; s++) {
    uint cnt = HIST[s];
    ARG[s * 5u + 0u] = cnt;       // indexCount
    ARG[s * 5u + 2u] = running;   // firstIndex
    HIST[s] = running;            // becomes the bucket write cursor for cs_tri
    running += cnt;
  }
}
////////////////////////////////////////
// emit one LINE segment per POLYGON edge (1 thread / face): n corners -> n edges -> 2n line indices.
// uses the actual polygon edges (CSR), NOT the triangulation -> clean quad/ngon outlines (no diagonals).
compute_shader cs_lines : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= num_faces) { return; }
  uint a = FOd[f]; uint b = FOd[f + 1u]; uint n = b - a;
  if (n < 2u) { return; }
  uint base = atomicAdd(l_indexCount, n * 2u);
  for (uint k = 0u; k < n; k++) {
    LIDX[base + k * 2u + 0u] = VId[a + k];
    LIDX[base + k * 2u + 1u] = VId[a + ((k + 1u) % n)];
  }
}
////////////////////////////////////////
// E.5 — triangulate one face (1 thread / face) into its gid bucket's contiguous index range.
// tris (exact), quads and small ngons EAR-CLIP on the Newell best-fit plane (CONCAVE-safe;
// ring-order emission preserves the polygon's winding); n > 60 or degenerate falls back to
// the classic corner-0 fan. Output is ALWAYS exactly (n-2) triangles, so bucket histograms
// and index capacity are method-agnostic.
compute_shader cs_tri : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= num_faces) { return; }
  uint a = FOd[f]; uint b = FOd[f + 1u]; uint n = b - a;
  if (n < 3u) { return; }
  uint g = 0u;
  if (has_tags != 0u) {
    g = (TAGS[f] >> 20u) & 0xFFFu;
    if (((BND[g >> 5u] >> (g & 31u)) & 1u) == 0u) { g = 0u; }
  }
  uint ntris = n - 2u;
  uint base  = atomicAdd(HIST[g], ntris * 3u);      // reserve this face's slice of its bucket
  uint tbase = base / 3u;
  uint emit  = 0u;                                   // triangles emitted so far
  if (n == 3u) {
    IDX[base + 0u] = VId[a]; IDX[base + 1u] = VId[a + 1u]; IDX[base + 2u] = VId[a + 2u];
    TFd[tbase] = f;
    return;
  }
  bool earclip = (n <= 60u);
  if (earclip) {
    // Newell normal -> best-fit plane basis (U, V)
    vec3 nrm = vec3(0.0, 0.0, 0.0);
    for (uint k = 0u; k < n; k++) {
      vec3 p0 = Pd[VId[a + k]].xyz;
      vec3 p1 = Pd[VId[a + ((k + 1u) % n)]].xyz;
      nrm += vec3((p0.y - p1.y) * (p0.z + p1.z),
                  (p0.z - p1.z) * (p0.x + p1.x),
                  (p0.x - p1.x) * (p0.y + p1.y));
    }
    float nl = length(nrm);
    if (nl < 1.0e-12) {
      earclip = false;                               // degenerate polygon -> fan
    } else {
      vec3 NN  = nrm / nl;
      vec3 ax  = (abs(NN.x) < 0.9) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
      vec3 U   = normalize(cross(NN, ax));
      vec3 V   = cross(NN, U);
      vec2 pp[60];
      uint ring[60];
      vec3 c0 = Pd[VId[a]].xyz;
      for (uint k = 0u; k < n; k++) {
        vec3 p  = Pd[VId[a + k]].xyz - c0;
        pp[k]   = vec2(dot(p, U), dot(p, V));
        ring[k] = k;
      }
      // orientation of the projected ring (signed area x2)
      float area2 = 0.0;
      for (uint k = 0u; k < n; k++) {
        vec2 q0 = pp[k]; vec2 q1 = pp[(k + 1u) % n];
        area2 += q0.x * q1.y - q1.x * q0.y;
      }
      float orient = (area2 >= 0.0) ? 1.0 : -1.0;
      // on-edge tolerance (units of area x2, so scale-relative): a vertex lying ON a
      // candidate ear's edge projects to s==0 analytically, but the Newell-plane basis
      // (normalize/cross/dot) rounds that to a tiny +/- value. Bias the containment test
      // toward INSIDE by this margin so an on/near-edge vertex reliably BLOCKS the ear —
      // clipping such a "buried on the diagonal" reflex vertex leaves a reversed remainder
      // triangle. Blocking is the safe direction (the two-ears theorem guarantees another).
      float onedge = 1.0e-5 * abs(area2);
      uint remaining = n;
      // clip ears until a triangle remains (bounded; each iteration removes one
      // vertex). NOTE: `pass` is a shadlang KEYWORD (technique pass blocks) —
      // never use it as an identifier in shader text.
      for (uint clipiter = 0u; clipiter < 60u; clipiter++) {
        if (remaining <= 3u) { break; }
        uint ear = 0xFFFFFFFFu;
        for (uint i = 0u; i < remaining; i++) {
          uint ip = (i + remaining - 1u) % remaining;
          uint in_ = (i + 1u) % remaining;
          vec2 A2 = pp[ring[ip]]; vec2 B2 = pp[ring[i]]; vec2 C2 = pp[ring[in_]];
          float cr = (B2.x - A2.x) * (C2.y - B2.y) - (B2.y - A2.y) * (C2.x - B2.x);
          if (cr * orient <= 1.0e-12) { continue; }  // reflex or degenerate corner
          // no other remaining vertex inside the candidate ear
          bool clear = true;
          for (uint j = 0u; j < remaining; j++) {
            if (j == ip || j == i || j == in_) { continue; }
            vec2 Q = pp[ring[j]];
            float s0 = ((B2.x - A2.x) * (Q.y - A2.y) - (B2.y - A2.y) * (Q.x - A2.x)) * orient;
            float s1 = ((C2.x - B2.x) * (Q.y - B2.y) - (C2.y - B2.y) * (Q.x - B2.x)) * orient;
            float s2 = ((A2.x - C2.x) * (Q.y - C2.y) - (A2.y - C2.y) * (Q.x - C2.x)) * orient;
            if (s0 >= -onedge && s1 >= -onedge && s2 >= -onedge) { clear = false; }
          }
          if (clear) { ear = i; break; }
        }
        if (ear == 0xFFFFFFFFu) { break; }           // no ear (self-intersecting?) -> fan the rest
        uint ip = (ear + remaining - 1u) % remaining;
        uint in_ = (ear + 1u) % remaining;
        IDX[base + emit * 3u + 0u] = VId[a + ring[ip]];
        IDX[base + emit * 3u + 1u] = VId[a + ring[ear]];
        IDX[base + emit * 3u + 2u] = VId[a + ring[in_]];
        TFd[tbase + emit]          = f;
        emit++;
        for (uint k = ear; k + 1u < remaining; k++) { ring[k] = ring[k + 1u]; }
        remaining--;
      }
      // emit the remainder as a fan from ring[0] (the final triangle, or the
      // no-ear fallback — total is exactly n-2 either way)
      for (uint k = 1u; k + 1u < remaining; k++) {
        IDX[base + emit * 3u + 0u] = VId[a + ring[0]];
        IDX[base + emit * 3u + 1u] = VId[a + ring[k]];
        IDX[base + emit * 3u + 2u] = VId[a + ring[k + 1u]];
        TFd[tbase + emit]          = f;
        emit++;
      }
      return;
    }
  }
  // classic corner-0 fan (n > 60 or degenerate-normal fallback)
  uint v0 = VId[a];
  for (uint k = 0u; k < ntris; k++) {
    IDX[base + k * 3u + 0u] = v0;
    IDX[base + k * 3u + 1u] = VId[a + k + 1u];
    IDX[base + k * 3u + 2u] = VId[a + k + 2u];
    TFd[tbase + k]          = f;
  }
}
)S";
}

///////////////////////////////////////////////////////////////////////////////
// M2.5 — GPU-RESIDENT COUNT import. For a mesh whose LIVE face count lives on the
// GPU (GpuMesh::_gpuResidentCount; the producer writes it into _header each frame
// with NO readback), this 1-thread kernel copies that live count into the
// triangulator's _params.num_faces BEFORE cs_count/cs_tri run. The host-written
// _params then carries the CAPACITY (for dispatch sizing); this overwrites only the
// num_faces field with the live value, so the fan-triangulation early-outs at the
// REAL primitive count -> exact draw, no degenerate tail, no CPU sync. Header layout
// (see SdfToMeshInst::onTopologyReady / GpuMeshData): uint[0]=nv uint[1]=nc uint[2]=nf.
static std::string _importcount_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface ic_par (descriptor_set 0) { buffer layout(std430) icpb { uint num_faces; uint inst_count; uint has_tags; uint pad2; }; }
storage_interface ic_hdr (descriptor_set 0) { buffer layout(std430) ichb { uint HDR[]; }; }
compute_interface ic_iface { storage { ic_par ic_hdr } inputs { layout(local_size_x = 1); } }
compute_shader cs_importcount : ic_iface {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  num_faces = HDR[2];   // live face count (producer's GPU-side write); inst_count/has_tags untouched
}
)S";
}

///////////////////////////////////////////////////////////////////////////////
// E.4 — per-view GPU frustum cull for INSTANCED hypermesh. Sphere-vs-frustum
// per instance (the rigid_primitive cs_cull_mtxonly recipe), compacting BOTH
// the matrices AND the E.2 typed attrs into culled buffers the graphics
// pipes bind; the visible count fans out into EVERY bound gid slot's
// indirect instanceCount (E.3 buckets share one instance set). Runs in
// ComputeDrawable::_perViewCompute (per VP, own dispatch phase).
///////////////////////////////////////////////////////////////////////////////

static std::string _instcull_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface cif_par (descriptor_set 0) { buffer layout(std430) cpar {
  // HOST-WRITTEN prefix [0,128) (writeParams) ...
  mat4 u_vp; vec4 u_bound; vec4 u_eye_cd; uint u_count; float u_tighten;
  uint u_hzb_w; uint u_hzb_h; uint u_hzb_mips; uint u_hzb_mode;
  uint u_num_bounds; float u_box_scale;
  // ... then SHADER-OWNED stat counters at the tail: the host must NEVER write these (a host write
  // of a host-visible buffer shadows the GPU's atomic writes on readback), so they live past the
  // host-written prefix and are reset by cs_cull_reset each frame.
  uint u_visible; uint u_occluded; uint u_frustum; }; }
// u_eye_cd: xyz = camera eye (world), w = cull_distance (meters; 0 = no distance cull)
storage_interface cif_inm  (descriptor_set 0) { buffer layout(std430) cinm  { mat4 IN_M[];  }; }
storage_interface cif_outm (descriptor_set 0) { buffer layout(std430) coutm { mat4 OUT_M[]; }; }
storage_interface cif_ina  (descriptor_set 0) { buffer layout(std430) cina  { vec4 IN_A[];  }; }
storage_interface cif_outa (descriptor_set 0) { buffer layout(std430) couta { vec4 OUT_A[]; }; }
storage_interface cif_arg  (descriptor_set 0) { buffer layout(std430) carg  { uint ARG[];  }; }
storage_interface cif_bnd  (descriptor_set 0) { buffer layout(std430) cbnd  { uint BND[];  }; }
storage_interface cif_hzb  (descriptor_set 0) { buffer layout(std430) chzb  { float HZB[]; }; }
// per-variant OCCLUDEE decomposition: K object-space AABBs, 2 vec4 each (min @2k, max @2k+1). K=1
// is the whole-mesh AABB; K=n is a tighter shape decomposition (vertical slabs / clusters). An
// instance is occluded iff ALL K sub-boxes are occluded — the tight, correct hidden test.
storage_interface cif_bnds (descriptor_set 0) { buffer layout(std430) cbnds { vec4 BOUNDS[]; }; }
// LOD tiers (Phase 3): bin each visible instance by eye-distance into u_num_tiers ranges.
// VIS[t] = per-tier visible count. OUT_M / OUT_A are INTERLEAVED — tier t's compacted instances
// live at [t*u_count, t*u_count + VIS[t]) — so N draws read tier-offset ranges via firstInstance.
// u_lod_dist.{x,y,z} = the up-to-3 tier boundaries (ascending). u_num_tiers=1 -> tier 0 only,
// VIS[0] == u_visible, OUT_M[0..] == the legacy single-output layout (byte-identical).
storage_interface cif_lod (descriptor_set 0) { buffer layout(std430) clod {
  uint u_num_tiers; uint _lpad0; uint _lpad1; uint _lpad2; vec4 u_lod_dist; uint VIS[4]; }; }
// Perf: u_fanout_tier moved OUT of clod into its own per-dispatch buffer so the N tier
// fanouts can run in ONE dispatch phase (each binds its own pre-written tier index) instead
// of a separate submit+WAIT+readback per tier (the fixed ~5.5ms hm-cull overhead). VIS[]
// stays shared in clod (cull writes it, every fanout reads it).
storage_interface cif_tier (descriptor_set 0) { buffer layout(std430) ctier { uint u_fanout_tier; }; }
compute_interface ciface { storage { cif_par cif_inm cif_outm cif_ina cif_outa cif_arg cif_bnd cif_hzb cif_bnds cif_lod cif_tier }
                           inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
// HZB occlusion: is the bounding sphere fully BEHIND last-frame's depth over its screen footprint?
// Standard-Z (smaller=nearer); HZB holds the MAX (farthest nearest-surface) depth per texel. The
// sphere is occluded iff its NEAREST ndc-z is still > the MAX occluder depth over its screen AABB.
// Conservative: any corner crossing the near plane -> NOT occluded (never over-cull). shadlang needs
// free functions in a libblock the shader INHERITS; it references ciface's globals (HZB[], u_vp,
// u_hzb_*) — storage/uniforms are emitted before libblocks so they're in scope (mirrors lib_pha).
libblock lib_hzb {
  // occlusion of ONE object-space AABB [bmin,bmax] transformed by the instance matrix M. Returns true
  // if that sub-box is fully behind last-frame's depth over its screen footprint.
  bool hzb_box_occluded(mat4 M, vec3 bmin, vec3 bmax) {
    // tightness: scale the box about its center. <1 SHRINKS (more aggressive occlusion — good for
    // sparse foliage you can see through; risks pop) ; >1 grows (safer). 1.0 = geometric AABB.
    vec3 ctr = (bmin + bmax) * 0.5;
    vec3 hlf = (bmax - bmin) * 0.5 * u_box_scale;
    vec3 smin = ctr - hlf;
    vec3 smax = ctr + hlf;
    vec3 ndcmin = vec3( 1.0e9);
    vec3 ndcmax = vec3(-1.0e9);
    bool safe = true;
    for (int k = 0; k < 8; k++) {
      vec3 obj = vec3(((k & 1) != 0) ? smax.x : smin.x,
                      ((k & 2) != 0) ? smax.y : smin.y,
                      ((k & 4) != 0) ? smax.z : smin.z);
      vec4 clip = u_vp * (M * vec4(obj, 1.0));
      if (clip.w <= 0.0001) { safe = false; }
      vec3 ndc = clip.xyz / max(clip.w, 0.0001);
      ndcmin = min(ndcmin, ndc);
      ndcmax = max(ndcmax, ndc);
    }
    if (safe) {
      // screen-space UV AABB. The HZB is built from the depth texture via texelFetch (texel row 0 =
      // framebuffer top), so ndc.y must be FLIPPED to address it (Vulkan ndc.y up here): a terrain
      // rock in the lower screen must map to the lower texel rows, not the upper (sky) rows.
      vec2 uvmin = clamp(vec2(ndcmin.x * 0.5 + 0.5, 1.0 - (ndcmax.y * 0.5 + 0.5)), 0.0, 1.0);
      vec2 uvmax = clamp(vec2(ndcmax.x * 0.5 + 0.5, 1.0 - (ndcmin.y * 0.5 + 0.5)), 0.0, 1.0);
      float znear_obj = ndcmin.z;
      // pick the mip where the AABB spans ~1-2 mip0 texels, so a 2x2 fetch covers the footprint
      float ex = (uvmax.x - uvmin.x) * float(u_hzb_w);
      float ey = (uvmax.y - uvmin.y) * float(u_hzb_h);
      int mip = int(ceil(log2(max(max(ex, ey), 1.0))));
      mip = clamp(mip, 0, int(u_hzb_mips) - 1);
      // mip dims + float-offset (recompute the deterministic pyramid packing)
      uint mw = max(1u, u_hzb_w >> uint(mip));
      uint mh = max(1u, u_hzb_h >> uint(mip));
      uint moff = 0u;
      uint ow = u_hzb_w; uint oh = u_hzb_h;
      for (int j = 0; j < mip; j++) { moff += ow * oh; ow = max(1u, ow >> 1u); oh = max(1u, oh >> 1u); }
      ivec2 mxd = ivec2(int(mw) - 1, int(mh) - 1);
      ivec2 t0 = clamp(ivec2(int(uvmin.x * float(mw)), int(uvmin.y * float(mh))), ivec2(0), mxd);
      ivec2 t1 = clamp(ivec2(int(uvmax.x * float(mw)), int(uvmax.y * float(mh))), ivec2(0), mxd);
      float occ = HZB[moff + uint(t0.y) * mw + uint(t0.x)];
      occ = max(occ, HZB[moff + uint(t0.y) * mw + uint(t1.x)]);
      occ = max(occ, HZB[moff + uint(t1.y) * mw + uint(t0.x)]);
      occ = max(occ, HZB[moff + uint(t1.y) * mw + uint(t1.x)]);
      return znear_obj > occ; // nearest point of the sphere behind the farthest occluder -> hidden
    }
    return false;
  }
}
////////////////////////////////////////
compute_shader cs_cull_reset : ciface {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  u_visible  = 0u;
  u_occluded = 0u;
  u_frustum  = 0u;
  VIS[0] = 0u; VIS[1] = 0u; VIS[2] = 0u; VIS[3] = 0u;
}
////////////////////////////////////////
compute_shader cs_cull : ciface : lib_hzb {
  uint i = gl_GlobalInvocationID.x;
  if (i >= u_count) { return; }
  mat4 M  = IN_M[i];
  vec3 c  = (M * vec4(u_bound.xyz, 1.0)).xyz;
  float r = u_bound.w * max(length(M[0].xyz), max(length(M[1].xyz), length(M[2].xyz)));
  bool inside = true;
  // u_tighten < 0 is the ORKID_DISABLE_FRUSTUM_CULL sentinel (host-stamped): skip ALL frustum + distance
  // rejects so every instance is treated visible. Otherwise u_tighten (>1) narrows the side planes -> the
  // cull frustum is NARROWER than the view (objects pop at the screen edges). 1.0 = exact view; near/far
  // unchanged. Occlusion (below) still applies when frustum is disabled unless it too is disabled.
  if (u_tighten >= 0.0) {
    vec4 rx = vec4(u_vp[0].x, u_vp[1].x, u_vp[2].x, u_vp[3].x);
    vec4 ry = vec4(u_vp[0].y, u_vp[1].y, u_vp[2].y, u_vp[3].y);
    vec4 rz = vec4(u_vp[0].z, u_vp[1].z, u_vp[2].z, u_vp[3].z);
    vec4 rw = vec4(u_vp[0].w, u_vp[1].w, u_vp[2].w, u_vp[3].w);
    vec4 sx = rx * u_tighten; vec4 sy = ry * u_tighten;
    vec4 pl0 = rw + sx; vec4 pl1 = rw - sx;
    vec4 pl2 = rw + sy; vec4 pl3 = rw - sy;
    vec4 pl4 = rz;      vec4 pl5 = rw - rz;
    // distance cull (cheap radial reject): cull beyond cull_distance from the eye. w<=0 = disabled.
    if (u_eye_cd.w > 0.0) { if (length(c - u_eye_cd.xyz) > u_eye_cd.w) { inside = false; } }
    if ((dot(pl0.xyz, c) + pl0.w) < (-r * length(pl0.xyz))) { inside = false; }
    if ((dot(pl1.xyz, c) + pl1.w) < (-r * length(pl1.xyz))) { inside = false; }
    if ((dot(pl2.xyz, c) + pl2.w) < (-r * length(pl2.xyz))) { inside = false; }
    if ((dot(pl3.xyz, c) + pl3.w) < (-r * length(pl3.xyz))) { inside = false; }
    if ((dot(pl4.xyz, c) + pl4.w) < (-r * length(pl4.xyz))) { inside = false; }
    if ((dot(pl5.xyz, c) + pl5.w) < (-r * length(pl5.xyz))) { inside = false; }
  }
  if (inside) { atomicAdd(u_frustum, 1u); } // passed frustum (pre-occlusion count)
  // HZB occlusion (mode 0 = off, 1 = count-only verify, 2 = cull). Frustum-visible only. The instance
  // is occluded iff EVERY one of its K sub-boxes is occluded (a tree's tight slabs cull behind a near
  // tree where the fat whole-mesh box never could). The && short-circuits -> early-out on the first
  // visible sub-box. K=0 (no bounds) -> all_occ false -> never culled (safe).
  if (inside && (u_hzb_mode != 0u)) {
    bool all_occ = (u_num_bounds > 0u);
    for (uint b = 0u; b < u_num_bounds; b++) {
      all_occ = all_occ && hzb_box_occluded(M, BOUNDS[2u * b].xyz, BOUNDS[2u * b + 1u].xyz);
    }
    if (all_occ) {
      atomicAdd(u_occluded, 1u);
      if (u_hzb_mode == 2u) { inside = false; }
    }
  }
  if (inside) {
    atomicAdd(u_visible, 1u);  // total visible (stats + N=1 fanout parity)
    // LOD tier: the highest tier whose lower boundary the eye-distance crosses (tier 0 = nearest).
    // Unrolled (no dynamic vec indexing); guards keep unused tiers inert when u_num_tiers is small.
    float dist = length(c - u_eye_cd.xyz);
    uint tier = 0u;
    if ((u_num_tiers > 1u) && (dist >= u_lod_dist.x)) { tier = 1u; }
    if ((u_num_tiers > 2u) && (dist >= u_lod_dist.y)) { tier = 2u; }
    if ((u_num_tiers > 3u) && (dist >= u_lod_dist.z)) { tier = 3u; }
    uint slot = atomicAdd(VIS[tier], 1u);
    OUT_M[tier * u_count + slot] = M;   // INTERLEAVED: tier t at [t*u_count, ..)
    OUT_A[tier * u_count + slot] = IN_A[i];
  }
}
////////////////////////////////////////
// fan the visible count into every BOUND gid slot's indirect instanceCount
// (slot 0 = the default bucket, always bound).
compute_shader cs_cull_fanout : ciface {
  uint s = gl_GlobalInvocationID.x;
  if (s >= 4096u) { return; }
  bool bound = (s == 0u) || (((BND[s >> 5u] >> (s & 31u)) & 1u) != 0u);
  if (bound) {
    ARG[s * 5u + 1u] = VIS[u_fanout_tier]; // instanceCount for the tier being fanned (N=1: VIS[0]==u_visible)
    ARG[s * 5u + 4u] = 0u;                 // firstInstance 0 — the graphics sub-range bind slices OUT_M
  }
}
)S";
}

///////////////////////////////////////////////////////////////////////////////
// Per-FRAME cull stats. There is one MeshInstCull per instanced hypermesh variant (scn_forest = 16
// tree variants); each accumulates its per-view readback into CullStats (ork.lev2 renderphasestats.h),
// which the ork.ecs.player perf HUD renders as the [hmcull] line. CullStats::commit() (once per frame
// in _renderIMPL) publishes; readback happens ONLY while the HUD enables it (off = zero cost).
///////////////////////////////////////////////////////////////////////////////

struct MeshInstCull {
  Context* _ctx                        = nullptr;
  const FxComputeShader* _cs_reset     = nullptr;
  const FxComputeShader* _cs_cull      = nullptr;
  const FxComputeShader* _cs_fanout    = nullptr;
  FxShaderStorageBuffer* _params       = nullptr; // {vp, bound, count, visible}
  FxShaderStorageBuffer* _culledMtx    = nullptr;
  FxShaderStorageBuffer* _culledAttr   = nullptr;
  FxShaderStorageBuffer* _srcMtx       = nullptr;
  FxShaderStorageBuffer* _srcAttr      = nullptr;
  FxShaderStorageBuffer* _args         = nullptr; // the triangulator's 4096-slot command array
  FxShaderStorageBuffer* _boundGidMask = nullptr;
  FxShaderStorageBuffer* _hzbDummy     = nullptr; // bound at slot 7 when no HZB (mode 0 never reads it)
  FxShaderStorageBuffer* _boundsSSBO   = nullptr; // slot 8: K object-space AABBs (2 vec4 each, min/max)
  FxShaderStorageBuffer* _lodSSBO      = nullptr; // slot 9: clod { num_tiers, lod_dist, VIS[4] }
  // cascade-cull fix — the SUN-SHADOW buffer set. perViewShadow runs the SAME reset/cull/fanout with a
  // union sun camera (occlusion OFF, single tier) into these, so the sun-cascade depth passes draw the
  // union-sun survivors. The EYE set above is never touched by the shadow cull -> color byte-identity.
  FxShaderStorageBuffer* _paramsShadow    = nullptr;
  FxShaderStorageBuffer* _culledMtxShadow = nullptr; // single-tier OUT_M (all sun-visible)
  FxShaderStorageBuffer* _culledAttrShadow= nullptr;
  FxShaderStorageBuffer* _lodShadow       = nullptr; // clod (num_tiers forced 1)
  FxShaderStorageBuffer* _argsShadow      = nullptr; // per-gid indexCounts copied from _args + shadow instanceCounts
  FxShaderStorageBuffer* _tierIdxShadow   = nullptr; // cif_tier fanout index (fixed 0)
  int _numBounds = 0;                             // K (0 => occlusion never culls — safe)
  float _boxScale = 1.0f;                         // per-variant occludee tightness (<1 = cull harder)
  float _cullDistance = 0.0f;                     // per-variant radial distance cull (0 = off)
  // LOD (Phase 3): partition visible instances into _numTiers eye-distance ranges. _lodDist[0..2] =
  // the ascending tier boundaries (meters). _numTiers=1 => single tier (legacy, OUT_M not interleaved).
  int _numTiers = 1;
  float _lodDist[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  // Phase 3b — per-tier triangulator args + gid masks (one entry per LOD tier; [0] = the main tri's).
  // The fanout runs once per tier (its own phase, u_fanout_tier host-bumped), stamping VIS[t] into that
  // tier's indirect instanceCount. Empty -> single-tier (uses _args/_boundGidMask).
  std::vector<FxShaderStorageBuffer*> _tierArgs;
  std::vector<FxShaderStorageBuffer*> _tierGidMask;
  std::vector<FxShaderStorageBuffer*> _tierIdxBuf; // per-tier fanout index (pre-written = t) -> one dispatch phase
  fvec4 _bound; // xyz = object-space center, w = radius (the SPHERE — frustum test only)
  int _count = 0;

  void build(Context* ctx, int count) {
    _ctx     = ctx;
    _count   = count;
    auto fxi = ctx->FXI();
    auto sh  = fxi->shaderFromShaderText("hypermesh_instcull", _instcull_text());
    _cs_reset  = fxi->computeShader(sh, "cs_cull_reset");
    _cs_cull   = fxi->computeShader(sh, "cs_cull");
    _cs_fanout = fxi->computeShader(sh, "cs_cull_fanout");
    _params    = fxi->createStorageBuffer(160); // cpar: mat4(64) + 2*vec4(32) + 11 scalars(44) = 140, round up
    // OUT_M/OUT_A are tier-INTERLEAVED: _numTiers * count slots (tier t at [t*count, ..)). N=1 => count.
    const size_t tslots = size_t(std::max(1, _numTiers)) * size_t(count);
    _culledMtx  = fxi->createStorageBuffer(tslots * 64);
    _culledAttr = fxi->createStorageBuffer(tslots * 16);
    _hzbDummy   = fxi->createStorageBuffer(16);
    _boundsSSBO = fxi->createStorageBuffer(32); // dummy (1 box) until setBounds; _numBounds stays 0
    _lodSSBO    = fxi->createStorageBuffer(48); // clod: uint(4)+pad(12)+vec4(16)+uint[4](16)
    // cascade-cull fix — the sun-shadow set (single tier: count slots, no LOD interleave).
    _paramsShadow     = fxi->createStorageBuffer(160);
    _culledMtxShadow  = fxi->createStorageBuffer(size_t(count) * 64);
    _culledAttrShadow = fxi->createStorageBuffer(size_t(count) * 16);
    _lodShadow        = fxi->createStorageBuffer(48);
    _argsShadow       = fxi->createStorageBuffer(size_t(kGidSlots) * 5 * 4);
    _tierIdxShadow    = fxi->createStorageBuffer(16);
    { // cif_tier fanout index is fixed at tier 0 for the (single-tier) shadow fanout
      uint32_t z = 0u;
      auto tm = fxi->mapStorageBuffer(_tierIdxShadow, 0, 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(tm->_mappedaddr, &z, 4);
      fxi->unmapStorageBuffer(tm.get());
    }
  }
  // OCCLUDEE decomposition: a flat list of K object-space AABBs (each pushed as {min.xyz,0},{max.xyz,0}).
  // K=1 = whole-mesh AABB; K=n = vertical slabs / clusters. The cull occludes iff ALL K are occluded.
  void setBounds(Context* ctx, const std::vector<fvec4>& minmax) {
    if (minmax.empty()) return;
    auto fxi    = ctx->FXI();
    _boundsSSBO = fxi->createStorageBuffer(minmax.size() * 16);
    auto m = fxi->mapStorageBuffer(_boundsSSBO, 0, minmax.size() * 16, BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, minmax.data(), minmax.size() * 16);
    fxi->unmapStorageBuffer(m.get());
    _numBounds = int(minmax.size()) / 2;
  }
  void perView(Context* ctx, const CameraMatrices& cammtx) {
    // params: PRE-phase host write of the PREFIX ONLY (vp..box_scale). The stat counters
    // (visible/occluded/frustum) live at the TAIL and are GPU-owned (cs_cull_reset zeroes + cs_cull
    // atomicAdds): the host must not write them, or the host-visible-buffer readback shadows the GPU
    // writes (confirmed via a host sentinel). Layout MUST match cif_par's std430.
    struct P { float vp[16]; float bound[4]; float eye_cd[4]; uint32_t count; float tighten;
               uint32_t hzb_w; uint32_t hzb_h; uint32_t hzb_mips; uint32_t hzb_mode;
               uint32_t num_bounds; float box_scale;
               uint32_t visible; uint32_t occluded; uint32_t frustum; } p;
    const size_t P_PREFIX = 128; // offsetof(P, visible): mat4(64)+2*vec4(32)+8*4(32); host writes only this
    std::memcpy(p.vp, cammtx.GetVPMatrix().asArray(), 64);
    p.bound[0] = _bound.x; p.bound[1] = _bound.y; p.bound[2] = _bound.z; p.bound[3] = _bound.w;
    const float* iv = cammtx.GetIVMatrix().asArray(); // inverse-view translation = eye (col-major)
    p.eye_cd[0] = iv[12]; p.eye_cd[1] = iv[13]; p.eye_cd[2] = iv[14]; p.eye_cd[3] = _cullDistance;
    // CullFrustumScale: frame-global cull aggressiveness stamped onto the RCFD in Scene::preRender
    // (>1 widen/cull-less, 1.0 exact, <1 narrow/cull-more). The shader's u_tighten is the INVERSE
    // (>1 NARROWS — hmdflow_render.cpp:295-297), so feed 1/scale. Replaces the per-drawable _tighten
    // AND the VR ORKEXP_VRCULL_MARGIN — one frame-global knob drives desktop, SGVP, and VR identically.
    float cfs = 1.0f;
    auto rcfd = ctx->topRenderContextFrameData();
    if (rcfd)
      if (auto v = rcfd->tryUserProperty<float>("CullFrustumScale"_crc))
        cfs = v.value();
    // ORKID_DISABLE_FRUSTUM_CULL debug lever: stamp the pass-all sentinel (u_tighten < 0) so cs_cull
    // skips the frustum + distance rejects (occlusion, if enabled, still applies). Cached bool, no cost
    // when unset. u_tighten is otherwise 1/scale, always > 0, so a negative value is unambiguous.
    p.count = uint32_t(_count);
    p.tighten = cullFrustumDisabled() ? -1.0f : ((cfs > 0.0f) ? (1.0f / cfs) : 1.0f);
    // HZB 1-phase occlusion: the pyramid built from LAST frame's depth, stamped into the RCFD in
    // Scene::preRender. mode: 0 off, 1 count-only (verify, don't cull), 2 cull. ORKID_HZB_OCCLUSION
    // selects the mode (default 2 once verified); absent/invalid HZB -> mode 0 (frustum-only, safe).
    static const int s_mode = []() { const char* e = getenv("ORKID_HZB_OCCLUSION"); return e ? atoi(e) : 2; }();
    HZBBuilder* hzb = nullptr;
    if (rcfd)
      if (auto v = rcfd->tryUserProperty<uint64_t>("HZB"_crc))
        hzb = reinterpret_cast<HZBBuilder*>(uintptr_t(v.value()));
    bool hzb_ok = hzb and hzb->_valid and hzb->_ssbo and (s_mode != 0) and not cullOcclusionDisabled();
    p.hzb_w = hzb_ok ? uint32_t(hzb->_baseW) : 0u;
    p.hzb_h = hzb_ok ? uint32_t(hzb->_baseH) : 0u;
    p.hzb_mips = hzb_ok ? uint32_t(hzb->_mips) : 0u;
    p.hzb_mode = hzb_ok ? uint32_t(s_mode) : 0u;
    p.num_bounds = uint32_t(_numBounds);
    p.box_scale  = _boxScale;
    auto fxi = ctx->FXI();
    // write ONLY the host prefix — leave the GPU-owned stat counters (tail) untouched.
    auto m   = fxi->mapStorageBuffer(_params, 0, P_PREFIX, BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, &p, P_PREFIX);
    fxi->unmapStorageBuffer(m.get());
    // LOD config host-write (PRE-phase): num_tiers + boundaries; VIS[] zeroed (cs_cull_reset re-zeros
    // on GPU, the cull fills, we read back below). Layout MUST match clod's std430.
    // clod std430 layout (MUST match the shader): num_tiers, fanout_tier, pad, lod_dist, VIS[4].
    struct L { uint32_t num_tiers; uint32_t fanout_tier; uint32_t pad[2]; float lod_dist[4]; uint32_t vis[4]; };
    {
      L lc;
      lc.num_tiers   = uint32_t(std::max(1, _numTiers));
      lc.fanout_tier = 0u; // tier 0's fanout rides the reset+cull phase
      lc.pad[0] = lc.pad[1] = 0u;
      for (int t = 0; t < 4; t++) { lc.lod_dist[t] = _lodDist[t]; lc.vis[t] = 0u; }
      auto lm = fxi->mapStorageBuffer(_lodSSBO, 0, sizeof(lc), BufferMapAccess::WRITE_ONLY);
      std::memcpy(lm->_mappedaddr, &lc, sizeof(lc));
      fxi->unmapStorageBuffer(lm.get());
    }
    FxShaderStorageBuffer* hzbssbo = hzb_ok ? hzb->_ssbo : _hzbDummy;
    auto ci   = ctx->CI();
    // tier 0's args/gidMask default to the main tri's; per-tier fanout rebinds slots 5/6.
    FxShaderStorageBuffer* args0 = _tierArgs.empty()    ? _args         : _tierArgs[0];
    FxShaderStorageBuffer* gid0  = _tierGidMask.empty() ? _boundGidMask : _tierGidMask[0];
    auto bind = [&](const FxComputeShader* cs,
                    FxShaderStorageBuffer* args,
                    FxShaderStorageBuffer* gidmask,
                    FxShaderStorageBuffer* tieridx) {
      ci->bindStorageBuffer(cs, 0, _params);
      ci->bindStorageBuffer(cs, 1, _srcMtx);
      ci->bindStorageBuffer(cs, 2, _culledMtx);
      ci->bindStorageBuffer(cs, 3, _srcAttr);
      ci->bindStorageBuffer(cs, 4, _culledAttr);
      ci->bindStorageBuffer(cs, 5, args);
      ci->bindStorageBuffer(cs, 6, gidmask);
      ci->bindStorageBuffer(cs, 7, hzbssbo);
      ci->bindStorageBuffer(cs, 8, _boundsSSBO);
      ci->bindStorageBuffer(cs, 9, _lodSSBO);
      ci->bindStorageBuffer(cs, 10, tieridx); // cif_tier: the fanout reads its tier here (cull/reset ignore)
    };
    // Per-tier fanout-index buffers (pre-written ONCE; the tier index never changes). Giving each
    // fanout its OWN tiny tier buffer is what lets all tier fanouts batch into the SINGLE dispatch
    // phase below — replacing the old per-tier separate submit+WAIT+readback (the fixed ~5.5ms
    // hm-cull overhead that was independent of instance count). cull/reset bind [0] (unused by them).
    if (int(_tierIdxBuf.size()) != _numTiers) {
      _tierIdxBuf.resize(size_t(std::max(1, _numTiers)), nullptr);
      for (int t = 0; t < int(_tierIdxBuf.size()); t++) {
        if (not _tierIdxBuf[t])
          _tierIdxBuf[t] = fxi->createStorageBuffer(16);
        uint32_t tv = uint32_t(t);
        auto     tm = fxi->mapStorageBuffer(_tierIdxBuf[t], 0, 4, BufferMapAccess::WRITE_ONLY);
        std::memcpy(tm->_mappedaddr, &tv, 4);
        fxi->unmapStorageBuffer(tm.get());
      }
    }
    FxShaderStorageBuffer* tidx0 = _tierIdxBuf.empty() ? _hzbDummy : _tierIdxBuf[0];

    // ONE dispatch phase: reset -> cull -> ALL tier fanouts. Was N phases (N submit+WAIT + N readbacks);
    // now one submit (+ one WAIT, or fully async under ORK_HM_NB_SUBMIT). The tier fanouts write DISJOINT
    // args buffers and only READ the cull's VIS[], so they need no barrier between them — just the
    // post-cull barrier so they see VIS[]. The trailing endDispatchPhase barrier hands args -> indirect draw.
    ci->beginDispatchPhase();
    bind(_cs_reset, args0, gid0, tidx0);
    ci->dispatchCompute(_cs_reset, 1, 1, 1);
    ci->storageBarrier();
    bind(_cs_cull, args0, gid0, tidx0);
    ci->dispatchCompute(_cs_cull, (uint32_t(_count) + 63) / 64, 1, 1);
    ci->storageBarrier();
    bind(_cs_fanout, args0, gid0, tidx0); // tier 0 (VIS[0])
    ci->dispatchCompute(_cs_fanout, 4096 / 64, 1, 1);
    for (int t = 1; t < _numTiers && t < int(_tierArgs.size()); t++) {
      bind(_cs_fanout, _tierArgs[t], _tierGidMask[t], _tierIdxBuf[t]); // VIS[t]
      ci->dispatchCompute(_cs_fanout, 4096 / 64, 1, 1);
    }
    ci->endDispatchPhase(); // ONE submit (+ WAIT) -> u_visible / args readable below
    // Accumulate THIS variant into the per-frame CullStats total (the phase waited, so the readback is
    // ready). The perf HUD renders the aggregate [hmcull] line; readback happens ONLY while the HUD
    // enables it (off = zero cost). CullStats::commit() (once per frame in _renderIMPL) publishes.
    if (CullStats::instance().enabled()) {
      // MUST match cif_par std430: stat counters (visible/occluded/frustum) at the TAIL (GPU-owned).
      struct P { float vp[16]; float bound[4]; float eye_cd[4]; uint32_t count; float tighten;
                 uint32_t hzb_w; uint32_t hzb_h; uint32_t hzb_mips; uint32_t hzb_mode;
                 uint32_t num_bounds; float box_scale;
                 uint32_t visible; uint32_t occluded; uint32_t frustum; } rb;
      auto m = fxi->mapStorageBuffer(_params, 0, sizeof(rb), BufferMapAccess::READ_ONLY);
      std::memcpy(&rb, m->_mappedaddr, sizeof(rb));
      fxi->unmapStorageBuffer(m.get());
      CullStats::instance().addHyperVariant(rb.count, rb.frustum, rb.visible, rb.occluded);
    }
    // LOD partition verification (Phase 3 3a): read the per-tier counts and assert the camera-
    // independent invariant sum(VIS[t]) == total visible (exhaustive + disjoint partition). Only when
    // multi-tier (N=1 has nothing to check). ORKID_DEBUG_LOD=1.
    static const bool s_dbglod = (getenv("ORKID_DEBUG_LOD") != nullptr);
    if (s_dbglod and _numTiers > 1) {
      struct L { uint32_t num_tiers; uint32_t pad[3]; float lod_dist[4]; uint32_t vis[4]; } lc;
      auto lm = fxi->mapStorageBuffer(_lodSSBO, 0, sizeof(lc), BufferMapAccess::READ_ONLY);
      std::memcpy(&lc, lm->_mappedaddr, sizeof(lc));
      fxi->unmapStorageBuffer(lm.get());
      // MUST match cif_par std430: u_visible is now at the TAIL (offset 128), not right after count.
      struct PR { float vp[16]; float bound[4]; float eye_cd[4]; uint32_t count; float tighten;
                  uint32_t hzb_w; uint32_t hzb_h; uint32_t hzb_mips; uint32_t hzb_mode;
                  uint32_t num_bounds; float box_scale; uint32_t visible; } pr;
      auto pm = fxi->mapStorageBuffer(_params, 0, sizeof(pr), BufferMapAccess::READ_ONLY);
      std::memcpy(&pr, pm->_mappedaddr, sizeof(pr));
      fxi->unmapStorageBuffer(pm.get());
      uint32_t s = 0;
      for (int t = 0; t < _numTiers; t++) s += lc.vis[t];
      printf("[lodpart] tiers<%d> dist<%.0f %.0f %.0f> VIS[%u %u %u %u] sum<%u> visible<%u> %s\n",
             _numTiers, _lodDist[0], _lodDist[1], _lodDist[2],
             lc.vis[0], lc.vis[1], lc.vis[2], lc.vis[3], s, pr.visible,
             (s == pr.visible) ? "OK" : "*** MISMATCH ***");
    }
  }

  ////////////////////////////////////////////////////////////////////////////
  // cascade-cull fix — the SUN-SHADOW cull. Same reset/cull/fanout as perView but: a UNION sun camera
  // (from the prologue, superset of every cascade slice), occlusion OFF (a light cull must be frustum-
  // only — the eye HZB is meaningless in light space), NO distance cull (a far caster still shadows),
  // NO CullFrustumScale narrowing (the union frustum is already conservative), and a SINGLE tier (every
  // sun-visible instance casts the full mesh). Targets the shadow buffer set; the eye set is untouched.
  // Runs once per composited frame from Scene::shadowCull, inside its (reentrant) dispatch phase.
  ////////////////////////////////////////////////////////////////////////////
  void perViewShadow(Context* ctx, const CameraMatrices& cammtx) {
    if (not _args) // no triangulated args to source per-gid indexCounts from -> nothing to cast
      return;
    auto fxi = ctx->FXI();
    auto ci  = ctx->CI();
    // host params prefix (layout MUST match cif_par's std430; stat counters at the tail stay GPU-owned).
    struct P { float vp[16]; float bound[4]; float eye_cd[4]; uint32_t count; float tighten;
               uint32_t hzb_w; uint32_t hzb_h; uint32_t hzb_mips; uint32_t hzb_mode;
               uint32_t num_bounds; float box_scale;
               uint32_t visible; uint32_t occluded; uint32_t frustum; } p;
    const size_t P_PREFIX = 128;
    std::memcpy(p.vp, cammtx.GetVPMatrix().asArray(), 64);
    p.bound[0] = _bound.x; p.bound[1] = _bound.y; p.bound[2] = _bound.z; p.bound[3] = _bound.w;
    const float* iv = cammtx.GetIVMatrix().asArray();
    p.eye_cd[0] = iv[12]; p.eye_cd[1] = iv[13]; p.eye_cd[2] = iv[14];
    p.eye_cd[3] = 0.0f; // no distance cull for shadows
    p.count = uint32_t(_count);
    p.tighten = cullFrustumDisabled() ? -1.0f : 1.0f; // exact union frustum (already conservative)
    p.hzb_w = 0u; p.hzb_h = 0u; p.hzb_mips = 0u; p.hzb_mode = 0u; // occlusion OFF (frustum-only)
    p.num_bounds = uint32_t(_numBounds);
    p.box_scale  = _boxScale;
    { auto m = fxi->mapStorageBuffer(_paramsShadow, 0, P_PREFIX, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, &p, P_PREFIX);
      fxi->unmapStorageBuffer(m.get()); }
    // single-tier LOD config (VIS[] zeroed on-GPU by cs_cull_reset)
    struct L { uint32_t num_tiers; uint32_t fanout_tier; uint32_t pad[2]; float lod_dist[4]; uint32_t vis[4]; } lc;
    lc.num_tiers = 1u; lc.fanout_tier = 0u; lc.pad[0] = lc.pad[1] = 0u;
    for (int t = 0; t < 4; t++) { lc.lod_dist[t] = 0.0f; lc.vis[t] = 0u; }
    { auto lm = fxi->mapStorageBuffer(_lodShadow, 0, sizeof(lc), BufferMapAccess::WRITE_ONLY);
      std::memcpy(lm->_mappedaddr, &lc, sizeof(lc));
      fxi->unmapStorageBuffer(lm.get()); }
    auto bind = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, _paramsShadow);
      ci->bindStorageBuffer(cs, 1, _srcMtx);
      ci->bindStorageBuffer(cs, 2, _culledMtxShadow);
      ci->bindStorageBuffer(cs, 3, _srcAttr);
      ci->bindStorageBuffer(cs, 4, _culledAttrShadow);
      ci->bindStorageBuffer(cs, 5, _argsShadow);
      ci->bindStorageBuffer(cs, 6, _boundGidMask);
      ci->bindStorageBuffer(cs, 7, _hzbDummy);
      ci->bindStorageBuffer(cs, 8, _boundsSSBO);
      ci->bindStorageBuffer(cs, 9, _lodShadow);
      ci->bindStorageBuffer(cs, 10, _tierIdxShadow);
    };
    ci->beginDispatchPhase();
    // copy the tier-0 per-gid commands (indexCounts written by the triangulator) into the shadow args;
    // the fanout below overwrites ONLY instanceCount/firstInstance, so shadow args = per-gid indexCounts
    // + shadow instanceCounts. _args is the SAME buffer the eye path reads (culler->_args = tri->_args).
    ci->copyBufferRegion(_args, 0, _argsShadow, 0, size_t(kGidSlots) * 5 * 4);
    bind(_cs_reset);
    ci->dispatchCompute(_cs_reset, 1, 1, 1);
    ci->storageBarrier();
    bind(_cs_cull);
    ci->dispatchCompute(_cs_cull, (uint32_t(_count) + 63) / 64, 1, 1);
    ci->storageBarrier(); // cull VIS + the args copy both visible to the fanout
    bind(_cs_fanout);
    ci->dispatchCompute(_cs_fanout, 4096 / 64, 1, 1);
    ci->endDispatchPhase();
    // shadow-flicker instrumentation (ORKID_SHADOWCULL_TRACE=1; OFF = not one instruction of cost).
    // The eye path aggregates into the perf HUD's CullStats; the sun-shadow cull had no counters at
    // all, so a per-frame survivor series needs this readback. This end is NESTED inside
    // Scene::shadowCull's phase, so the submit is deferred -> the tail counters read here are the
    // PREVIOUS frame's completed cull (a constant one-frame lag; oscillation analysis is unaffected).
    static const bool s_shadowtrace = (getenv("ORKID_SHADOWCULL_TRACE") != nullptr);
    if (s_shadowtrace) {
      // MUST match cif_par std430: stat counters (visible/occluded/frustum) at the TAIL (GPU-owned).
      struct RB { float vp[16]; float bound[4]; float eye_cd[4]; uint32_t count; float tighten;
                  uint32_t hzb_w; uint32_t hzb_h; uint32_t hzb_mips; uint32_t hzb_mode;
                  uint32_t num_bounds; float box_scale;
                  uint32_t visible; uint32_t occluded; uint32_t frustum; } rb;
      auto m = fxi->mapStorageBuffer(_paramsShadow, 0, sizeof(rb), BufferMapAccess::READ_ONLY);
      std::memcpy(&rb, m->_mappedaddr, sizeof(rb));
      fxi->unmapStorageBuffer(m.get());
      printf(
          "[shadowcull] frame<%d> mesh<%p> count<%u> frustum<%u> visible<%u>\n",
          ctx->GetTargetFrame(),
          (const void*)this,
          rb.count,
          rb.frustum,
          rb.visible);
      fflush(stdout);
    }
  }
};

///////////////////////////////////////////////////////////////////////////////
// MeshRenderTri — the persistent triangulator state (compiled once; buffers pooled-by-growth).
///////////////////////////////////////////////////////////////////////////////

struct MeshRenderTri {
  Context* _ctx                    = nullptr;
  const FxComputeShader* _cs_reset = nullptr;
  const FxComputeShader* _cs_count = nullptr; // E.3: per-gid histogram
  const FxComputeShader* _cs_scan  = nullptr; // E.3: prefix -> per-gid commands/cursors
  const FxComputeShader* _cs_tri   = nullptr;
  const FxComputeShader* _cs_lines = nullptr;
  const FxComputeShader* _cs_importcount = nullptr; // M2.5: load live count from _header (GPU-resident-count meshes)
  FxShaderStorageBuffer* _triIndex = nullptr; // uint32[ >= 3*num_corners ]
  FxShaderStorageBuffer* _triFace  = nullptr; // uint32[ >= num_corners ]  out-tri -> source face id (face-viz)
  FxShaderStorageBuffer* _args     = nullptr; // E.3: kGidSlots x VkDrawIndexedIndirectCommand (slot g at byte g*20)
  FxShaderStorageBuffer* _hist     = nullptr; // E.3: kGidSlots uints (counts -> cursors)
  FxShaderStorageBuffer* _boundGidMask = nullptr; // E.3: 128 uints (bit g = gid g has a bound material)
  FxShaderStorageBuffer* _dummyTags    = nullptr; // bound at the TAGS slot when the mesh has no __tags
  std::vector<int> _boundGids;                    // gids with their own material (slot 0 implicit)
  FxShaderStorageBuffer* _lineIndex = nullptr; // uint32[ >= 2*num_corners ] (wireframe: per-edge LINE indices)
  FxShaderStorageBuffer* _lineArgs  = nullptr; // VkDrawIndexedIndirectCommand (lines)
  FxShaderStorageBuffer* _params   = nullptr; // { num_faces, ... } (host-written PRE dispatch phase)
  int _indexCap                    = 0;       // uint capacity of _triIndex
  int _faceCap                     = 0;       // uint capacity of _triFace
  int _lineCap                     = 0;       // uint capacity of _lineIndex
  bool _wireframe                  = false;   // also build the per-edge LINE index buffer each frame
  int _instCount                   = 1;       // indirect instanceCount (1 = non-instanced); -> cs_reset
  FxShaderStorageBuffer* _instMtx  = nullptr; // per-instance matrices SSBO (mat4[_instCount]); null if not instanced
  // C.1b DIRTY GATE — the topology KEY this triangulation was built from. Triangulation (cs_reset/
  // cs_tri/cs_lines) depends ONLY on connectivity, so it re-runs ONLY when the key moves: _topoVersion
  // covers explicit rebuild bumps (extrude/inset/normals + the allocMesh auto-bump); counts + the
  // vidx/face_offsets COW handles are observed directly (catches count-set-within-capacity paths,
  // e.g. ripple's else-branch). Positions-only animation never re-triangulates; static meshes
  // triangulate exactly once.
  uint64_t _builtTopoV = ~0ull;
  int _builtNF = -1, _builtNC = -1;
  const void* _builtVidx = nullptr;
  const void* _builtFO   = nullptr;
  bool topoDirty(gpumesh_ptr_t m) const {
    return m->_topoVersion != _builtTopoV or m->_num_faces != _builtNF or m->_num_corners != _builtNC or
           (const void*)m->_vidx.get() != _builtVidx or (const void*)m->_face_offsets.get() != _builtFO;
  }
  void ackTopo(gpumesh_ptr_t m) {
    _builtTopoV = m->_topoVersion;
    _builtNF    = m->_num_faces;
    _builtNC    = m->_num_corners;
    _builtVidx  = m->_vidx.get();
    _builtFO    = m->_face_offsets.get();
  }

  void build(Context* ctx) {
    _ctx     = ctx;
    auto fxi = ctx->FXI();
    auto sh  = fxi->shaderFromShaderText("hypermesh_triangulate", _triangulate_text());
    _cs_reset = fxi->computeShader(sh, "cs_reset");
    _cs_count = fxi->computeShader(sh, "cs_count");
    _cs_scan  = fxi->computeShader(sh, "cs_scan");
    _cs_tri   = fxi->computeShader(sh, "cs_tri");
    _cs_lines = fxi->computeShader(sh, "cs_lines");
    auto icsh = fxi->shaderFromShaderText("hypermesh_importcount", _importcount_text()); // M2.5 (own 2-binding iface)
    _cs_importcount = fxi->computeShader(icsh, "cs_importcount");
    _args     = fxi->createStorageBuffer(size_t(kGidSlots) * 5 * 4); // one command per gid slot
    _hist     = fxi->createStorageBuffer(size_t(kGidSlots) * 4);
    _boundGidMask = fxi->createStorageBuffer(128 * 4);
    _dummyTags    = fxi->createStorageBuffer(4);
    _lineArgs = fxi->createStorageBuffer(sizeof(uint32_t) * 8); // line draw command (always allocated)
    _lineIndex = fxi->createStorageBuffer(4);                   // dummy until ensureIndex (so slot 6 binds)
    _lineCap  = 1;
    _params   = fxi->createStorageBuffer(16);
  }
  // grow the index buffer to hold the worst-case tri count (<= 3*num_corners indices) + the per-tri
  // face-id buffer (<= num_corners tris). pow2-stepped; only re-creates on genuine growth.
  void ensureIndex(int num_corners) {
    int need = meshNextPow2(num_corners * 3 + 1);
    if (need > _indexCap) {
      _triIndex = _ctx->FXI()->createStorageBuffer(size_t(need) * 4);
      _indexCap = need;
    }
    int fneed = meshNextPow2(num_corners + 1);
    if (fneed > _faceCap) {
      _triFace = _ctx->FXI()->createStorageBuffer(size_t(fneed) * 4);
      _faceCap = fneed;
    }
    if (_wireframe) {                       // worst case: 2 line indices per corner (one edge / corner)
      int lneed = meshNextPow2(num_corners * 2 + 1);
      if (lneed > _lineCap) {
        _lineIndex = _ctx->FXI()->createStorageBuffer(size_t(lneed) * 4);
        _lineCap   = lneed;
      }
    }
  }
  // PRE-dispatch-phase host write (a map mid-phase is invisible to the dispatch).
  void writeParams(int num_faces, bool has_tags) {
    uint32_t p[4] = {uint32_t(num_faces), uint32_t(_instCount), has_tags ? 1u : 0u, 0u};
    auto m = _ctx->FXI()->mapStorageBuffer(_params, 0, sizeof(p), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, p, sizeof(p));
    _ctx->FXI()->unmapStorageBuffer(m.get());
    // bound-gid bitmask (slot 0 always set — the default-material bucket;
    // unbound gids FOLD to it at count/scatter time)
    uint32_t mask[128] = {1u};
    for (int g : _boundGids)
      if (g >= 0 and g < kGidSlots)
        mask[g >> 5] |= (1u << (g & 31));
    auto bm = _ctx->FXI()->mapStorageBuffer(_boundGidMask, 0, sizeof(mask), BufferMapAccess::WRITE_ONLY);
    std::memcpy(bm->_mappedaddr, mask, sizeof(mask));
    _ctx->FXI()->unmapStorageBuffer(bm.get());
  }
  // IN-dispatch-phase: reset the draw command, then fan-triangulate every face. Both shaders share
  // ONE interface, so both bind all 5 slots (par,fo,vi,idx,arg) — cs_reset writes only arg, but the
  // descriptor set covers the full interface (mirrors the generator modules' cs_setup binding).
  void dispatch(gpumesh_ptr_t mesh) {
    auto ci   = _ctx->CI();
    auto tags = mesh->face("__tags");
    auto tags_ssbo = tags ? tags->_ssbo : _dummyTags; // has_tags=0 -> never read
    auto bind = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, _params);
      ci->bindStorageBuffer(cs, 1, mesh->_face_offsets->_ssbo);
      ci->bindStorageBuffer(cs, 2, mesh->_vidx->_ssbo);
      ci->bindStorageBuffer(cs, 3, _triIndex);
      ci->bindStorageBuffer(cs, 4, _args);
      ci->bindStorageBuffer(cs, 5, _triFace);
      ci->bindStorageBuffer(cs, 6, _lineIndex);
      ci->bindStorageBuffer(cs, 7, _lineArgs);
      ci->bindStorageBuffer(cs, 8, _hist);
      ci->bindStorageBuffer(cs, 9, _boundGidMask);
      ci->bindStorageBuffer(cs, 10, tags_ssbo);
      auto pch = mesh->channel(MeshChannel::POSITION);
      ci->bindStorageBuffer(cs, 11, pch ? pch->_ssbo : _dummyTags); // E.5: ear clip reads P
    };
    int fgroups = (mesh->_num_faces + 63) / 64;
    // M2.5 — GPU-RESIDENT COUNT: _params.num_faces was host-written to the CAPACITY (mesh->_num_faces);
    // overwrite it on the GPU with the LIVE count from _header so cs_count/cs_tri early-out at the real
    // primitive count. fgroups dispatches the full capacity; threads past the live count early-out (cheap).
    // No CPU readback anywhere -> the producer's per-frame count never touches the host.
    if (mesh->_gpuResidentCount and mesh->_header) {
      ci->bindStorageBuffer(_cs_importcount, 0, _params);
      ci->bindStorageBuffer(_cs_importcount, 1, mesh->_header);
      ci->dispatchCompute(_cs_importcount, 1, 1, 1);
      ci->storageBarrier();
    }
    bind(_cs_reset);
    ci->dispatchCompute(_cs_reset, kGidSlots / 64, 1, 1);
    ci->storageBarrier();
    bind(_cs_count);
    ci->dispatchCompute(_cs_count, fgroups, 1, 1);
    ci->storageBarrier();
    bind(_cs_scan);
    ci->dispatchCompute(_cs_scan, 1, 1, 1);
    ci->storageBarrier();
    bind(_cs_tri);
    ci->dispatchCompute(_cs_tri, fgroups, 1, 1);
    ci->storageBarrier();
    if (_wireframe) {                       // build the per-edge LINE index buffer (wireframe overlay)
      bind(_cs_lines);
      ci->dispatchCompute(_cs_lines, fgroups, 1, 1);
      ci->storageBarrier();
    }
  }
};

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Impostor bake (A2) — render the base mesh from a hemi-octahedral grid of ortho angles into a
// 3-target PBR MRT atlas (albedo+coverage / world-normal+ao / metal+rough), mip it, and (for now)
// dump the three atlases to PNG. Off-compositor camera: ortho V/P on the matrix stack, so the capture
// material's MVP provider falls back to MTXI->RefVPMatrix() (EyePostion -> 0; fine for Solid).
//
// SPLIT to honor the in-frame + async contract (the cubemap probe renders the same way):
//   prepareImpostorBake()  = COMPUTE half  — triangulate + bounds + MRT RTG + capture pipeline,
//                            at materialization (NO render pass active; compute dispatch is legal).
//   renderInFrame()        = GRAPHICS half — the per-cell ortho draws as a PRE-PASS inside an active
//                            graphics frame (the drawable's per-view one-shot hook), then mips + the
//                            ASYNC PNG capture (rtg/tri/pipe kept alive via the capture lambda).
///////////////////////////////////////////////////////////////////////////////
struct ImpostorBakeJob {
  livehypermesh_ptr_t            _live;
  std::shared_ptr<MeshRenderTri> _tri;      // the bake's OWN tri (its args carry instanceCount=1)
  std::shared_ptr<MeshRenderTri> _mainTri;  // the MAIN render's tri — its clean/dirty state is the readiness gate
  bool                           _triDone = false; // the bake tri has been dispatched (topology was valid)
  rtgroup_ptr_t                  _rtg;
  rtbuffer_ptr_t                 _bAlb, _bNrm, _bMR;
  // one capture pipeline per gid bucket (gid 0 = base material, then each bound-gid material). renderInFrame
  // draws each bucket (args offset gid*20) with ITS material's capture technique, so a multi-material mesh
  // (tree: bark/branch/leaf) bakes correctly per-region into the one atlas.
  std::vector<std::pair<int, fxpipeline_ptr_t>> _gidPipes;
  std::vector<freestyle_mtl_ptr_t>              _gidCfs; // parallel: each gid's freestyle view, to RE-BIND the
                                                         // current (re-pooled) vertex channels each bake frame
  fvec3                          _center;
  float                          _radius  = 1.0f;
  int                            _gridN   = 8;
  int                            _tileRes = 128;
  int                            _nverts  = 0;
  std::string                    _outBase;
  bool                           _done = false;
  // the BILLBOARD draw side (the far tier): the impostor draws with the BASE PBRMaterial via its
  // FWD_SSBO_CUSTOM_IMPOSTOR technique (the atlas is bound on the material; permu._is_impostor selects it),
  // plus a 6-index quad + indirect args. The drawable emits a bucket from these.
  material_ptr_t                 _impMtl;                  // = the base material (boulder_mat), drawn impostor
  const FxShaderStorageBlock*    _impInstBlock = nullptr; // storage_inst_mtx (the instance matrices)
  FxShaderStorageBuffer*         _quadIndex = nullptr;    // 0,1,2,0,2,3
  FxShaderStorageBuffer*         _quadArgs  = nullptr;    // VkDrawIndexedIndirectCommand {6, VIS[tier], 0,0,0}
  FxShaderStorageBuffer*         _quadGidMask = nullptr;  // slot-0-only bound mask for the tier fanout
  FxShaderStorageBuffer*         _bakeArgs    = nullptr;  // copy of the main tri's per-gid args, instanceCount forced to 1
  // GRAPHICS half — runs inside an active graphics frame. Returns true once it has captured (or there is
  // nothing to capture); returns false while the mesh tri is still dirty so the caller RETRIES next frame.
  bool renderInFrame(Context* ctx);
};
using impostorbakejob_ptr_t = std::shared_ptr<ImpostorBakeJob>;

// Impostor atlas dump location (ORKID_IMPOSTOR_DUMP). Default /tmp/orkid_impostor; if the env value is a
// PATH (contains '/'), that directory is used instead. Created once; the destination is printed once so it
// is always easy to find the PNGs (one numbered set imp<N>_{albedo,normal,mr}.png per baked variant).
static std::string impostorDumpDir() {
  const char* e   = getenv("ORKID_IMPOSTOR_DUMP");
  std::string dir = (e and std::string(e).find('/') != std::string::npos) ? std::string(e)
                                                                          : std::string("/tmp/orkid_impostor");
  static bool s_once = false;
  if (not s_once) {
    s_once = true;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    printf("================================================================\n");
    printf("ORKID_IMPOSTOR_DUMP -> %s/imp<N>_{albedo,normal,mr}.png\n", dir.c_str());
    printf("================================================================\n");
  }
  return dir;
}

static impostorbakejob_ptr_t prepareImpostorBake(
    Context* ctx, livehypermesh_ptr_t live, pbrmaterial_ptr_t mtl,
    const std::map<int, pbrmaterial_ptr_t>& gidMaterials,
    std::shared_ptr<MeshRenderTri> mainTri,
    int gridN, int tileRes, int ssaa, int msaa, float maxDist, const std::string& outBase) {
  auto mesh = live ? live->_mesh : nullptr;
  if (not mesh) return nullptr;
  auto captech = mtl ? mtl->_tek_FWD_SSBO_CUSTOM_CAPTURE : nullptr;
  auto fsmtl   = mtl ? mtl->_as_freestyle : nullptr;
  if (not captech or not fsmtl) {
    printf("bakeImpostor: material has no capture technique (opt-in with impostor=True)\n");
    return nullptr;
  }
  static const char* kChan[5] = {"sif_ptex_vtx", "sif_N", "sif_B", "sif_uv", "sif_clr"};
  auto ci  = ctx->CI();
  auto fxi = ctx->FXI();
  auto job = std::make_shared<ImpostorBakeJob>();
  job->_live = live; job->_gridN = gridN; job->_tileRes = tileRes; job->_outBase = outBase;

  // REUSE the MAIN render's tri ENTIRELY (its _triIndex + per-gid _args) — byte-identical triangulation to
  // the viewport, so the leaf-card faces CANNOT triangulate differently in the bake. The capture VS is
  // non-instanced (ignores gl_InstanceIndex), so the culled instanceCount in the args just draws that many
  // perfectly-overlapping object-space copies (== one); renderInFrame gates on this tri being clean.
  job->_mainTri = mainTri;
  job->_tri     = mainTri;

  // object-space bound (position readback)
  fvec3 bmin(0, 0, 0), bmax(0, 0, 0);
  int nv = mesh->_num_verts;
  if (auto pch = mesh->channel(MeshChannel::POSITION)) {
    if (nv > 0) {
      std::vector<float> P(size_t(nv) * 4);
      auto m = fxi->mapStorageBuffer(pch->_ssbo, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
      std::memcpy(P.data(), m->_mappedaddr, size_t(nv) * 16);
      fxi->unmapStorageBuffer(m.get());
      bmin = bmax = fvec3(P[0], P[1], P[2]);
      for (int i = 1; i < nv; i++) {
        fvec3 p(P[i * 4], P[i * 4 + 1], P[i * 4 + 2]);
        bmin = fvec3(std::min(bmin.x, p.x), std::min(bmin.y, p.y), std::min(bmin.z, p.z));
        bmax = fvec3(std::max(bmax.x, p.x), std::max(bmax.y, p.y), std::max(bmax.z, p.z));
      }
    }
  }
  job->_nverts = nv;
  job->_center = (bmin + bmax) * 0.5f;
  float radius = (bmax - bmin).length() * 0.5f;
  job->_radius = (radius <= 0.0f) ? 1.0f : radius * 1.05f;

  // MRT RTG (3 targets + depth), auto-mipped. SSAA = render each view at tileRes*ssaa (the atlas is stored
  // at that supersampled resolution; trilinear+mips downsample at sample time). MSAA = multisample the bake
  // RTG (resolves to the single-sample atlas). Both reduce the captured-geometry aliasing (esp. leaf edges).
  ssaa = std::max(1, ssaa);
  int effTile = tileRes * ssaa;
  int atlas   = gridN * effTile;
  tileRes     = effTile;                                       // renderInFrame viewports use the supersampled tile
  MsaaSamples msaaEnum = (msaa >= 8) ? MsaaSamples::MSAA_8X
                       : (msaa >= 4) ? MsaaSamples::MSAA_4X
                       : (msaa >= 2) ? MsaaSamples::MSAA_2X
                                     : MsaaSamples::MSAA_1X;
  static int s_atlasId = 0;                                   // unique id per baked variant (GPU-debug tracking)
  std::string aname = "impostorAtlas" + std::to_string(s_atlasId++);
  auto rtg  = std::make_shared<RtGroup>(ctx, atlas, atlas, msaaEnum);
  rtg->_name = aname;
  job->_bAlb = rtg->createRenderTarget(EBufferFormat::RGBA8);
  job->_bNrm = rtg->createRenderTarget(EBufferFormat::RGBA8); // octa-encoded normal+ao; 8-bit is fine for a far impostor
  job->_bMR  = rtg->createRenderTarget(EBufferFormat::RGBA8);
  // debug names + TRILINEAR sampling on the atlas textures. The default min-filter is LINEAR (bilinear,
  // mip-0 ONLY) — so the generated mip chain was never sampled and the minified billboards aliased hard.
  // presetTrilinearClamp samples the mip chain; clamp (not wrap) avoids sampling across the atlas edge.
  const char* chans[3] = {"_albedo", "_normal", "_metalrough"};
  for (int t = 0; t < 3; t++)
    if (auto tex = rtg->texture(t)) {
      tex->_debugName = aname + chans[t];
      auto& sm = tex->TexSamplingMode();
      sm._texFiltModeMin = ETextureMinifyFilterMode::LINEAR_MIPMAP_LINEAR; // trilinear: sample the mip chain
      sm._texFiltModeMag = ETextureMagnifyFilterMode::LINEAR;
      sm._texAddrModeS   = TextureAddressMode::CLAMP; // don't sample across the atlas edge / into other tiles
      sm._texAddrModeT   = TextureAddressMode::CLAMP;
      sm._maxMipLevel    = 16;
    }
  job->_bAlb->_mipgen = RtBuffer::EMG_AUTOCOMPUTE;
  job->_bNrm->_mipgen = RtBuffer::EMG_AUTOCOMPUTE;
  job->_bMR->_mipgen  = RtBuffer::EMG_AUTOCOMPUTE;
  // PREMULTIPLIED atlas: every channel (incl. alpha=coverage) clears to 0 so the silhouette's bilinear/mip
  // blend is a coverage-weighted sum — the uncovered background contributes nothing (the FS unpremultiplies
  // by the blended coverage). a:0 also drives the coverage-cutout discard.
  job->_bAlb->_clearColor = fvec4(0, 0, 0, 0);
  job->_bNrm->_clearColor = fvec4(0, 0, 0, 0);
  job->_bMR->_clearColor  = fvec4(0, 0, 0, 0);
  rtg->createDepthBuffer(EBufferFormat::Z32F, true);
  rtg->_autoclear = true;
  job->_rtg = rtg;

  // capture pipeline (freestyle cache + FORCED technique; the PBR cache asserts on it) + bind channels.
  // One per material — the base (gid 0) + each bound-gid material. The freestyle forced-technique pipeline
  // does NOT auto-bind the std matrices (the PBR forward builder does), so bind the ones vs_ptex_ssbo uses
  // to their providers (no CPD -> MTXI/RCID fallback = our pushed ortho V/P + identity model): mvp
  // (gl_Position), m (frg_wpos), mrot (the world NORMAL: wnormal = mrot*nrm; without it the normal is black).
  auto buildCapturePipe = [&](pbrmaterial_ptr_t m) -> fxpipeline_ptr_t {
    auto ctek = m ? m->_tek_FWD_SSBO_CUSTOM_CAPTURE : nullptr;
    auto cfs  = m ? m->_as_freestyle : nullptr;
    if (not ctek or not cfs) return nullptr;
    FxPipelinePermutation permu;
    permu._is_vertex_ssbo   = true;
    permu._forced_technique = ctek;
    auto pipe = cfs->pipelineCache()->findPipeline(permu);
    if (pipe) {
      for (int c = 0; c < 5; c++)
        if (auto blk = cfs->storageBlock(kChan[c]))
          if (auto ch = mesh->channel(MeshChannel(c)))
            pipe->bindStorage(blk, ch->_ssbo);
      if (auto p = cfs->param("mvp"))  pipe->bindParam(p, "RCFD_Camera_MVP_Mono"_crcsh);
      if (auto p = cfs->param("m"))    pipe->bindParam(p, "RCFD_M"_crcsh);
      if (auto p = cfs->param("mrot")) pipe->bindParam(p, "RCFD_Model_Rot"_crcsh);
    }
    return pipe;
  };
  job->_gidPipes.push_back({0, buildCapturePipe(mtl)}); // gid 0 = base material (the default bucket)
  job->_gidCfs.push_back(mtl->_as_freestyle);
  for (auto& [gid, gm] : gidMaterials) {                // each bound gid draws its bucket with its material
    job->_gidPipes.push_back({gid, buildCapturePipe(gm)});
    job->_gidCfs.push_back(gm->_as_freestyle);
  }
  if (getenv("ORKID_IMPOSTOR_DUMP")) { // diagnostics: which gid buckets have a capture pipe
    printf("bakeImpostor[%s]: faces=%d  gid-pipes:", outBase.c_str(), mesh->_num_faces);
    for (auto& gp : job->_gidPipes)
      printf(" gid%d=%s", gp.first, gp.second ? "ok" : "NOPIPE");
    printf("\n");
  }

  // Phase B — the BILLBOARD draw side. The impostor is drawn by the BASE PBRMaterial (the same boulder_mat)
  // via its FWD_SSBO_CUSTOM_IMPOSTOR technique (selected by permu._is_impostor) — so it gets the IDENTICAL
  // forward PBR lighting as the mesh LOD tiers (color-matched), NOT a separate hand-lit material. Here we
  // just hand the material its baked atlas + grid/radius (bound by the forward pipeline's impostor branch)
  // and grab its instance-matrix block; the drawable emits a bucket carrying the quad + the cull tier slice.
  {
    mtl->bindImpostorAtlas(rtg->texture(0), rtg->texture(1), rtg->texture(2),
                           job->_center, job->_radius, float(gridN), maxDist);
    job->_impMtl       = mtl;                                  // the impostor draws with the BASE material
    job->_impInstBlock = fsmtl->storageBlock("storage_inst_mtx");
    printf("bakeImpostor: atlas bound to material<%p> instblk<%p> tek<%p>\n",
           (void*)mtl.get(), (void*)job->_impInstBlock, (void*)mtl->_tek_FWD_SSBO_CUSTOM_IMPOSTOR);
    // quad index (0,1,2,0,2,3) + indirect args (slot 0): indexCount=6, instanceCount=0. The cull's per-tier
    // fanout (cs_cull_fanout, slot 0 always bound) stamps instanceCount = VIS[tier] — so only the impostor
    // band's instances draw a billboard; the matrix slice rides the cull's OUT_M tier offset.
    job->_quadIndex = fxi->createStorageBuffer(6 * 4);
    {
      uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
      auto m          = fxi->mapStorageBuffer(job->_quadIndex, 0, 6 * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, idx, 6 * 4);
      fxi->unmapStorageBuffer(m.get());
    }
    job->_quadArgs = fxi->createStorageBuffer(5 * 4);
    {
      uint32_t arg[5] = {6u, 0u, 0u, 0u, 0u};
      auto m          = fxi->mapStorageBuffer(job->_quadArgs, 0, 5 * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, arg, 5 * 4);
      fxi->unmapStorageBuffer(m.get());
    }
    // a slot-0-only bound mask (128 uints, bit 0 set) for the impostor tier's fanout — keeps the stamp on the
    // quad's single command regardless of the base mesh's gid set.
    job->_bakeArgs    = fxi->createStorageBuffer(16 * 5 * 4); // 16 gid slots; filled in renderInFrame (instanceCount=1)
    job->_quadGidMask = fxi->createStorageBuffer(128 * 4);
    {
      uint32_t mask[128] = {1u};
      auto m             = fxi->mapStorageBuffer(job->_quadGidMask, 0, sizeof(mask), BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, mask, sizeof(mask));
      fxi->unmapStorageBuffer(m.get());
    }
  }
  return job;
}

bool ImpostorBakeJob::renderInFrame(Context* ctx) {
  if (_done) return true;
  auto mesh = _live ? _live->_mesh : nullptr;
  if (_gidPipes.empty() or not _rtg or not _tri or not mesh) return true; // nothing to capture — don't retry
  // GATE: the MAIN render's tri must be clean -> _liveRecompute has triangulated the current topology AND
  // waited on the graph compute (endDispatchPhase submit+wait). Until then, retry next frame — NEVER capture
  // stale topology. This is the "100% valid topology + sync" guarantee.
  if (not _mainTri or _mainTri->topoDirty(mesh)) return false;
  // _tri IS the main tri (already triangulated + synced by _liveRecompute's endDispatchPhase submit+wait),
  // so its _triIndex/_args are valid here — nothing to dispatch; we draw the viewport's exact triangulation.
  _done = true;
  // The main tri's per-gid args carry the CULLED instanceCount (VIS — 0 when this variant has no near trees
  // at the bake frame -> blank atlas). Copy them with instanceCount forced to 1: the capture VS is non-
  // instanced, so 1 object-space draw is correct regardless of the cull. firstIndex/indexCount are untouched.
  {
    auto fxiB = ctx->FXI();
    uint32_t bargs[16 * 5] = {0};
    {
      auto m = fxiB->mapStorageBuffer(_tri->_args, 0, sizeof(bargs), BufferMapAccess::READ_ONLY);
      std::memcpy(bargs, m->_mappedaddr, sizeof(bargs));
      fxiB->unmapStorageBuffer(m.get());
    }
    for (auto& gp : _gidPipes) bargs[gp.first * 5 + 1] = 1u;
    auto m = fxiB->mapStorageBuffer(_bakeArgs, 0, sizeof(bargs), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, bargs, sizeof(bargs));
    fxiB->unmapStorageBuffer(m.get());
  }
  // DEBUG OBJ: dump the EXACT renderable geometry the capture draws (positions indexed by _triIndex, per gid)
  // so the bake's leaf topology can be inspected as a mesh (trimesh/numpy) instead of from the atlas PNG.
  if (getenv("ORKID_IMPOSTOR_DUMP")) {
    auto fxiD = ctx->FXI();
    int  nv   = mesh->_num_verts;
    std::vector<float> P(size_t(nv) * 4, 0.0f);
    if (auto pch = mesh->channel(MeshChannel::POSITION)) {
      auto m = fxiD->mapStorageBuffer(pch->_ssbo, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
      std::memcpy(P.data(), m->_mappedaddr, size_t(nv) * 16);
      fxiD->unmapStorageBuffer(m.get());
    }
    uint32_t args[16 * 5] = {0};
    {
      auto m = fxiD->mapStorageBuffer(_tri->_args, 0, sizeof(args), BufferMapAccess::READ_ONLY);
      std::memcpy(args, m->_mappedaddr, sizeof(args));
      fxiD->unmapStorageBuffer(m.get());
    }
    uint32_t maxidx = 0;
    for (auto& gp : _gidPipes) { uint32_t e = args[gp.first * 5 + 2] + args[gp.first * 5 + 0]; if (e > maxidx) maxidx = e; }
    std::vector<uint32_t> idx(maxidx ? maxidx : 1u, 0u);
    if (maxidx) {
      auto m = fxiD->mapStorageBuffer(_tri->_triIndex, 0, maxidx * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(idx.data(), m->_mappedaddr, maxidx * 4);
      fxiD->unmapStorageBuffer(m.get());
    }
    std::string opath = _outBase + ".obj";
    if (FILE* of = fopen(opath.c_str(), "w")) {
      for (int v = 0; v < nv; v++) fprintf(of, "v %f %f %f\n", P[v * 4], P[v * 4 + 1], P[v * 4 + 2]);
      for (auto& gp : _gidPipes) {
        int gid = gp.first;
        uint32_t fi = args[gid * 5 + 2], ic = args[gid * 5 + 0];
        fprintf(of, "o gid%d\n", gid);
        for (uint32_t t = fi; t + 2 < fi + ic && (t + 2) < maxidx; t += 3)
          fprintf(of, "f %u %u %u\n", idx[t] + 1, idx[t + 1] + 1, idx[t + 2] + 1);
      }
      fclose(of);
      printf("bakeImpostor: wrote %s (nv=%d, indices=%u)\n", opath.c_str(), nv, maxidx);
    }
  }
  // RE-BIND each capture pipe's vertex channels to the mesh's CURRENT (re-pooled) SSBOs. The pipes were built
  // at materialize, but dynamic topology (the leaf merge) re-pools the channel buffers -> the capture would
  // otherwise pull the leaf indices from the STALE trunk-only position buffer (out of bounds -> spaghetti).
  {
    static const char* kChanRB[5] = {"sif_ptex_vtx", "sif_N", "sif_B", "sif_uv", "sif_clr"};
    for (size_t k = 0; k < _gidPipes.size(); k++) {
      auto pipe = _gidPipes[k].second;
      auto cfs  = (k < _gidCfs.size()) ? _gidCfs[k] : nullptr;
      if (not pipe or not cfs) continue;
      for (int c = 0; c < 5; c++)
        if (auto blk = cfs->storageBlock(kChanRB[c]))
          if (auto ch = mesh->channel(MeshChannel(c)))
            pipe->bindStorage(blk, ch->_ssbo);
    }
  }
  auto fbi  = ctx->FBI();
  auto gbi  = ctx->GBI();
  auto mtxi = ctx->MTXI();
  auto txi  = ctx->TXI();
  auto fxi  = ctx->FXI();
  auto rcfd = std::make_shared<RenderContextFrameData>(ctx);
  // OWNER LAW (2026-07-24): cull is ENTIRELY disabled during baking — every bake draw, not
  // just the atlas-space one. Here the impostor sources include single-sided leaf/card
  // geometry, and cull mode is a sticky DYNAMIC pipeline state the indirect path never
  // re-applies: leaked PASS_FRONT silently drops back-wound cards from half the view tiles.
  // Depth testing still resolves closed-mesh occlusion correctly with cull off.
  static const RasterState s_bakeCullOff = [] {
    RasterState r;
    r.setCullTest(ECullTest::OFF);
    r._priority = 1 << 20; // outrank any technique state block
    return r;
  }();
  fbi->PushRtGroup(_rtg.get());
  for (int j = 0; j < _gridN; j++) {
    for (int i = 0; i < _gridN; i++) {
      float u  = (float(i) + 0.5f) / float(_gridN);
      float v  = (float(j) + 0.5f) / float(_gridN);
      float ex = u * 2.0f - 1.0f, ey = v * 2.0f - 1.0f;
      fvec3 dir((ex + ey) * 0.5f, 0.0f, (ex - ey) * 0.5f); // hemi-oct: Y-up upper hemisphere
      dir.y = 1.0f - std::abs(dir.x) - std::abs(dir.z);
      dir   = dir.normalized();
      fvec3 eye = _center + dir * (_radius * 2.0f);
      fvec3 up  = (std::abs(dir.y) > 0.99f) ? fvec3(0, 0, 1) : fvec3(0, 1, 0);
      fmtx4 V, P;
      V.lookAt(eye, _center, up);
      P.ortho(-_radius, _radius, _radius, -_radius, 0.01f, _radius * 4.0f);
      mtxi->PushPMatrix(P);
      mtxi->PushVMatrix(V);
      mtxi->PushMMatrix(fmtx4::Identity());
      ViewportRect vp(i * _tileRes, j * _tileRes, _tileRes, _tileRes);
      fbi->pushViewport(vp);
      fbi->pushScissor(vp);
      RenderContextInstData RCID(rcfd);
      RCID._isSSBOSourced = true;
      // draw each gid bucket with ITS capture pipeline (args offset gid*20) — the atlas accumulates the
      // per-region surfaces (bark trunk / branch / leaf), depth-tested so closer buckets occlude.
      for (auto& gp : _gidPipes) {
        if (not gp.second) continue;
        int gid = gp.first;
        _announceBakeDraw(RCID, gp.second, "impostor_atlas", gid);
        gp.second->wrappedDrawCall(RCID, [&]() {
          fxi->applyRasterState(s_bakeCullOff); // bake law: cull OFF (see above)
          gbi->DrawIndexedIndirectEML(_tri->_triIndex, PrimitiveType::TRIANGLES, _bakeArgs, size_t(gid) * 20, 4);
        });
      }
      fbi->popScissor();
      fbi->popViewport();
      mtxi->PopPMatrix();
      mtxi->PopVMatrix();
      mtxi->PopMMatrix();
    }
  }
  // PopRtGroup (usage "user") transitions the 3 color targets to SHADER_READ_ONLY; generateMipMaps' blit
  // chain leaves them shader-readable too — so after this the atlas is directly sampleable by the billboard
  // pass IN THE SAME FRAME. (Do NOT captureAsFormat here on the live path: the PNG readback transitions the
  // textures to host-read, un-doing the shader-read layout and tripping the bindParam layout assert.)
  fbi->PopRtGroup();
  txi->generateMipMaps(_rtg->texture(0).get());
  txi->generateMipMaps(_rtg->texture(1).get());
  txi->generateMipMaps(_rtg->texture(2).get());
  // BUILD the trilinear sampler NOW — the textures are GPU-initialized + mipped at this point. Setting the
  // TexSamplingMode fields at create time is not enough: ApplySamplingMode early-returns if the texture is
  // not yet initialized, so the sampler stayed at the point/mip-0 default -> the holes + aliasing.
  for (int t = 0; t < 3; t++)
    if (auto tex = _rtg->texture(t)) txi->ApplySamplingMode(tex.get());
  if (getenv("ORKID_IMPOSTOR_DUMP"))
    printf("bakeImpostor: atlas %dx%d  _num_mips=%d (1 => generateMipMaps no-op -> impostor aliases)\n",
           _gridN * _tileRes, _gridN * _tileRes, _rtg->texture(0)->_num_mips);
  // Opt-in PNG dump for offline atlas verification only (ORKID_IMPOSTOR_DUMP=1). This consumes the atlas as
  // a host readback, so the billboard draw must NOT also run this frame — guard the live draw accordingly.
  if (getenv("ORKID_IMPOSTOR_DUMP")) {
    const char* names[3]   = {"albedo", "normal", "mr"};
    rtbuffer_ptr_t bufs[3] = {_bAlb, _bNrm, _bMR};
    auto rtg = _rtg; auto tri = _tri; auto pipes = _gidPipes; // keep alive past the async readback
    for (int t = 0; t < 3; t++) {
      auto capbuf = std::make_shared<CaptureBuffer>();
      std::string path = _outBase + "_" + names[t] + ".png";
      fbi->captureAsFormat(bufs[t].get(), capbuf, EBufferFormat::RGBA8, [capbuf, path, rtg, tri, pipes, rcfd]() {
        capbuf->_image->writeToFile(file::Path(path.c_str()));
        printf("bakeImpostor: wrote %s\n", path.c_str());
      });
    }
  }
  printf("bakeImpostor: %dx%d hemi-oct atlas (tile %d, %d verts) center<%.2f %.2f %.2f> r<%.2f>\n",
         _gridN, _gridN, _tileRes, _nverts, _center.x, _center.y, _center.z, _radius);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// O3 stage 2 — the per-section texture-array bake job. Renders each SECTION (its SectionUnwrap gid
// bucket) into its OWN 2D MRT via the material's FWD_SSBO_CUSTOM_CAPTURE technique (routed through the
// section cap-VS: gl_Position = mvp*vec4(uv0.xy,0,1) over an ortho covering the 0-1 UV domain — the
// MoltenVK-safe path), then host round-trips each target (captureAsFormat -> CaptureBuffer). The Python
// section_bake cache assembles those into ONE sampler2DArray layer per section. Mirrors ImpostorBakeJob.
///////////////////////////////////////////////////////////////////////////////
struct SectionBakeJobImpl {
  livehypermesh_ptr_t            _live;
  std::shared_ptr<MeshRenderTri> _tri;        // our OWN gid-bucketed tri (non-instanced; instanceCount=1)
  // O3 stage 3 — PER-LAYER forced-capture pipeline: each layer's section is baked with ITS gid material's
  // capture technique (the per-gid bake map). The single-material entry (prepareSectionBake) fills every
  // slot with one pipe; the mapped entry (prepareSectionBakeMapped) selects per gid. _layerCfs is the
  // parallel freestyle view used to RE-BIND the (possibly re-pooled) vertex channels each bake frame.
  std::vector<fxpipeline_ptr_t>    _layerPipes;
  std::vector<freestyle_mtl_ptr_t> _layerCfs;
  std::vector<int>               _layerGids;  // layer -> section gid (drives the per-layer gid-bucket draw)
  int                            _bakeRes    = 256;
  int                            _numTargets = 1;
  bool                           _fired      = false; // the in-frame render has run (captures issued)
  std::vector<rtgroup_ptr_t>          _rtgs;     // kept alive until the host readbacks drain
  std::vector<capturebuffer_ptr_t>    _captures; // [layer*numTargets + target] -> host RGBA8
  std::vector<captureasync_ptr_t>     _futures;  // readback completion
  bool isReady() const {
    if (not _fired) return false;
    for (auto& f : _futures)
      if (f and not f->isReady()) return false;
    return true;
  }
  bool renderInFrame(Context* ctx);
};

// build the forced-capture pipeline for ONE material (freestyle cache; the PBR cache asserts on the
// capture frame-type). Binds the mesh's vertex channels + the matrices vs_ptex_ssbo_cap uses (no CPD ->
// MTXI/RCID fallback = the pushed ortho V/P + identity M) + the material's stamped params (ctx.param
// uniforms) so the baked procedural matches the live proc exactly. Shared by both bake entry points.
static fxpipeline_ptr_t buildSectionCapturePipe(gpumesh_ptr_t mesh, pbrmaterial_ptr_t material) {
  auto ctek = material ? material->_tek_FWD_SSBO_CUSTOM_CAPTURE : nullptr;
  auto cfs  = material ? material->_as_freestyle : nullptr;
  if (not ctek or not cfs or not mesh)
    return nullptr;
  static const char* kChan[5] = {"sif_ptex_vtx", "sif_N", "sif_B", "sif_uv", "sif_clr"};
  FxPipelinePermutation permu;
  permu._is_vertex_ssbo   = true;
  permu._forced_technique = ctek;
  auto pipe = cfs->pipelineCache()->findPipeline(permu);
  if (pipe) {
    for (int c = 0; c < 5; c++)
      if (auto blk = cfs->storageBlock(kChan[c]))
        if (auto ch = mesh->channel(MeshChannel(c)))
          pipe->bindStorage(blk, ch->_ssbo);
    if (auto p = cfs->param("mvp"))  pipe->bindParam(p, "RCFD_Camera_MVP_Mono"_crcsh);
    if (auto p = cfs->param("m"))    pipe->bindParam(p, "RCFD_M"_crcsh);
    if (auto p = cfs->param("mrot")) pipe->bindParam(p, "RCFD_Model_Rot"_crcsh);
    for (auto item : material->_bound_params)
      pipe->bindParam(item.first, item.second);
  }
  return pipe;
}

// public pimpl wrapper methods (see hmdflow.h).
bool SectionBakeJob::isReady() const { return _impl ? _impl->isReady() : true; }
int  SectionBakeJob::numLayers() const { return _impl ? int(_impl->_layerGids.size()) : 0; }
int  SectionBakeJob::numTargets() const { return _impl ? _impl->_numTargets : 0; }
capturebuffer_ptr_t SectionBakeJob::layerCapture(int layer, int target) const {
  if (not _impl) return nullptr;
  int idx = layer * _impl->_numTargets + target;
  if (idx < 0 or idx >= int(_impl->_captures.size())) return nullptr;
  return _impl->_captures[idx];
}

sectionbakejob_ptr_t prepareSectionBake(
    Context* ctx, ComputeDrawableData* cdd, livehypermesh_ptr_t live, pbrmaterial_ptr_t material,
    const std::vector<int>& layerGids, int bakeRes, int numTargets) {
  auto mesh = live ? live->_mesh : nullptr;
  if (not mesh) return nullptr;
  auto ctek = material ? material->_tek_FWD_SSBO_CUSTOM_CAPTURE : nullptr;
  auto cfs  = material ? material->_as_freestyle : nullptr;
  // OPS SELF-DEFEND / FAIL LOUD (A owner law): a section-array material MUST carry the capture technique
  // (author it with capture=True + self.capture(...) + surface_stored()). No silent placeholder fallback.
  OrkAssertI(ctek and cfs,
             "prepareSectionBake: material has no FWD_SSBO_CUSTOM_CAPTURE technique — author the "
             "SectionArray material with capture=True + self.capture(...) + surface_stored()");
  auto job         = std::make_shared<SectionBakeJob>();
  auto impl        = std::make_shared<SectionBakeJobImpl>();
  job->_impl       = impl;
  impl->_live      = live;
  impl->_layerGids = layerGids;
  impl->_bakeRes    = std::max(8, bakeRes);
  impl->_numTargets = std::max(1, numTargets);
  impl->_captures.resize(size_t(std::max<size_t>(1, layerGids.size())) * size_t(impl->_numTargets));

  // our OWN triangulator, gid-bucketed by the section gids so each layer draws exactly its section's faces.
  auto tri          = std::make_shared<MeshRenderTri>();
  tri->_boundGids   = layerGids;                 // every section gid gets its own indirect bucket
  tri->build(ctx);
  tri->ensureIndex(mesh ? mesh->_num_corners : 1);
  impl->_tri = tri;

  // single material for every layer (the classic stage-2 shape).
  auto pipe = buildSectionCapturePipe(mesh, material);
  impl->_layerPipes.assign(layerGids.size(), pipe);
  impl->_layerCfs.assign(layerGids.size(), cfs);

  // register the in-frame one-shot (chained; mirrors the impostor bake). It fires once the mesh tri is
  // clean+synced, renders every layer, and issues the async captures.
  auto prev           = cdd->_oneShotRender;
  cdd->_oneShotRender = [prev, impl](Context* c) -> bool {
    bool a = prev ? prev(c) : true;
    bool b = impl->renderInFrame(c);
    return a and b;
  };
  printf("prepareSectionBake: %zu layers @ %dpx, %d target(s) — gids[", layerGids.size(), impl->_bakeRes, impl->_numTargets);
  for (int g : layerGids) printf(" %d", g);
  printf(" ]\n");
  return job;
}

///////////////////////////////////////////////////////////////////////////////
// O3 stage 3 — per-LAYER material bake (the per-gid BAKE MAP). Each section-layer's gid bucket is baked
// with THAT gid's material's capture technique (adobe content into adobe layers, timber into timber);
// unbound gids fall back to `defaultMaterial`. All bound materials MUST share the capture-target schema
// (same names, same MRT order) so the assembled arrays are coherent — the stored sampler material defines
// it. Mirrors prepareSectionBake's one-shot orchestration.
///////////////////////////////////////////////////////////////////////////////
sectionbakejob_ptr_t prepareSectionBakeMapped(
    Context* ctx, ComputeDrawableData* cdd, livehypermesh_ptr_t live,
    pbrmaterial_ptr_t defaultMaterial, const std::map<int, pbrmaterial_ptr_t>& gidMaterials,
    const std::vector<int>& layerGids, int bakeRes, int numTargets) {
  auto mesh = live ? live->_mesh : nullptr;
  if (not mesh) return nullptr;
  // OPS SELF-DEFEND / FAIL LOUD: a stored-mode section bake needs a capture-enabled material for every
  // layer (the default, or the gid's bake material). No silent proc fallback (owner law).
  OrkAssertI(defaultMaterial and defaultMaterial->_tek_FWD_SSBO_CUSTOM_CAPTURE,
             "prepareSectionBakeMapped: default (sampler) material has no FWD_SSBO_CUSTOM_CAPTURE technique "
             "— author it with capture=True + self.capture(...) + surface_stored()");
  auto job         = std::make_shared<SectionBakeJob>();
  auto impl        = std::make_shared<SectionBakeJobImpl>();
  job->_impl       = impl;
  impl->_live      = live;
  impl->_layerGids = layerGids;
  impl->_bakeRes    = std::max(8, bakeRes);
  impl->_numTargets = std::max(1, numTargets);
  impl->_captures.resize(size_t(std::max<size_t>(1, layerGids.size())) * size_t(impl->_numTargets));

  auto tri        = std::make_shared<MeshRenderTri>();
  tri->_boundGids = layerGids;
  tri->build(ctx);
  tri->ensureIndex(mesh->_num_corners);
  impl->_tri = tri;

  // one capture pipe per UNIQUE bake material (dedup by material pointer); each layer selects by its gid.
  std::map<pbrmaterial_ptr_t, fxpipeline_ptr_t> pipeCache;
  auto pipeFor = [&](pbrmaterial_ptr_t m) -> fxpipeline_ptr_t {
    auto it = pipeCache.find(m);
    if (it != pipeCache.end())
      return it->second;
    auto p = buildSectionCapturePipe(mesh, m);
    pipeCache[m] = p;
    return p;
  };
  impl->_layerPipes.resize(layerGids.size());
  impl->_layerCfs.resize(layerGids.size());
  for (size_t L = 0; L < layerGids.size(); L++) {
    int gid  = layerGids[L];
    auto git = gidMaterials.find(gid);
    auto m   = (git != gidMaterials.end() and git->second) ? git->second : defaultMaterial;
    // a gid whose bake material lacks the capture technique falls back to the default (never silent black).
    if (not m or not m->_tek_FWD_SSBO_CUSTOM_CAPTURE)
      m = defaultMaterial;
    impl->_layerPipes[L] = pipeFor(m);
    impl->_layerCfs[L]   = m->_as_freestyle;
  }

  auto prev           = cdd->_oneShotRender;
  cdd->_oneShotRender = [prev, impl](Context* c) -> bool {
    bool a = prev ? prev(c) : true;
    bool b = impl->renderInFrame(c);
    return a and b;
  };
  printf("prepareSectionBakeMapped: %zu layers @ %dpx, %d target(s) — layer->gid[", layerGids.size(), impl->_bakeRes, impl->_numTargets);
  for (int g : layerGids) printf(" %d", g);
  printf(" ] baked_materials=%zu\n", pipeCache.size());
  return job;
}

bool SectionBakeJobImpl::renderInFrame(Context* ctx) {
  if (_fired) return true;
  auto mesh = _live ? _live->_mesh : nullptr;
  if (not mesh or _layerGids.empty() or _layerPipes.empty() or not _tri) return true; // nothing to bake — don't retry
  // triangulate our OWN tri once (gid-bucketed). A dispatch phase is legal here — onPreRender runs the
  // one-shot AFTER _perViewCompute (its own dispatch phase) and BEFORE any graphics pass. submit+wait via
  // endDispatchPhase, so _triIndex/_args are valid for the draws below.
  if (_tri->topoDirty(mesh)) {
    _tri->ensureIndex(mesh->_num_corners);
    _tri->writeParams(mesh->_num_faces, mesh->face("__tags") != nullptr);
    auto ci = ctx->CI();
    ci->beginDispatchPhase();
    _tri->dispatch(mesh);
    ci->endDispatchPhase();
    _tri->ackTopo(mesh);
  }
  _fired = true;
  // RE-BIND each UNIQUE capture pipe's vertex channels to the mesh's CURRENT (possibly re-pooled) SSBOs.
  {
    static const char* kChanRB[5] = {"sif_ptex_vtx", "sif_N", "sif_B", "sif_uv", "sif_clr"};
    std::set<FxPipeline*> rebound;
    for (size_t L = 0; L < _layerPipes.size(); L++) {
      auto pipe = _layerPipes[L];
      auto cfs  = (L < _layerCfs.size()) ? _layerCfs[L] : nullptr;
      if (not pipe or not cfs or not rebound.insert(pipe.get()).second)
        continue;
      for (int c = 0; c < 5; c++)
        if (auto blk = cfs->storageBlock(kChanRB[c]))
          if (auto ch = mesh->channel(MeshChannel(c)))
            pipe->bindStorage(blk, ch->_ssbo);
    }
  }
  auto fbi  = ctx->FBI();
  auto gbi  = ctx->GBI();
  auto mtxi = ctx->MTXI();
  auto fxi  = ctx->FXI();
  auto rcfd = std::make_shared<RenderContextFrameData>(ctx);
  // Each section rasterizes over its OWN 0-1 UV atlas; xatlas assigns chart winding
  // ARBITRARILY, so a whole section (or many faces of one) can be back-wound in the
  // atlas — backface culling there drops them to black (a uniformly back-wound section
  // vanishes entirely; a mixed one loses ~half its texels). Cull mode is a DYNAMIC
  // pipeline state, and the SSBO indirect draw path (DrawIndexedIndirectEML) never
  // re-applies it, so the sticky cull from prior forward draws (PASS_FRONT) leaks in.
  // Force it OFF for every bake draw — the atlas bake paints every texel a face covers.
  static const RasterState s_bakeCullOff = [] {
    RasterState r;
    r.setCullTest(ECullTest::OFF);
    r._priority = 1 << 20; // outrank any technique state block so applyRasterState uses THIS
    return r;
  }();
  // ortho over the 0-1 UV domain: mvp*vec4(uv0.x,uv0.y,0,1) -> full-NDC (the section cap-VS rasterizes the
  // section's faces filling the atlas). bottom=0,top=1 writes uv.y ASCENDING down the atlas rows so the stored
  // sampler's NATURAL read (surface_stored at ctx.uv) hits each face's OWN chart. The old top>bottom form
  // stored the atlas v-FLIPPED vs that read: charts sat correct, but faces whose chart landed near the black
  // gutter after the flip sampled the gutter and rendered BLACK (pueblo base flare / parapet caps / recesses —
  // ~18% of gid0 faces). Both capture targets rasterize through this ONE ortho, so albedo + params stay in lockstep.
  fmtx4 P, V, M;
  P.ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
  V = fmtx4::Identity();
  M = fmtx4::Identity();
  static int s_id = 0;
  for (int L = 0; L < int(_layerGids.size()); L++) {
    int  gid  = _layerGids[L];
    auto pipe = (L < int(_layerPipes.size())) ? _layerPipes[L] : nullptr; // this layer's gid-material capture pipe
    if (not pipe)
      continue;
    auto rtg = std::make_shared<RtGroup>(ctx, _bakeRes, _bakeRes, MsaaSamples::MSAA_1X);
    rtg->_name = "sectionBake" + std::to_string(s_id++);
    std::vector<rtbuffer_ptr_t> bufs;
    for (int t = 0; t < _numTargets; t++) {
      auto rb         = rtg->createRenderTarget(EBufferFormat::RGBA8); // one MRT per capture target
      rb->_clearColor = fvec4(0, 0, 0, 0);
      bufs.push_back(rb);
    }
    rtg->createDepthBuffer(EBufferFormat::Z32F, true);
    rtg->_autoclear = true;
    fbi->PushRtGroup(rtg.get());
    mtxi->PushPMatrix(P);
    mtxi->PushVMatrix(V);
    mtxi->PushMMatrix(M);
    ViewportRect vp(0, 0, _bakeRes, _bakeRes);
    fbi->pushViewport(vp);
    fbi->pushScissor(vp);
    RenderContextInstData RCID(rcfd);
    RCID._isSSBOSourced = true;
    _announceBakeDraw(RCID, pipe, "section_atlas", gid);
    pipe->wrappedDrawCall(RCID, [&]() {
      // beginBlock has bound the capture pass; override the sticky dynamic cull mode to OFF
      // right before the indirect draw (the draw path itself never sets it).
      fxi->applyRasterState(s_bakeCullOff);
      gbi->DrawIndexedIndirectEML(_tri->_triIndex, PrimitiveType::TRIANGLES, _tri->_args, size_t(gid) * 20, 4);
    });
    fbi->popScissor();
    fbi->popViewport();
    mtxi->PopPMatrix();
    mtxi->PopVMatrix();
    mtxi->PopMMatrix();
    fbi->PopRtGroup();
    // host round-trip each target (captureAsFormat -> CaptureBuffer); the section_bake cache assembles
    // one array LAYER per section from these (uploadTextureRegion -> finalizeUpload).
    for (int t = 0; t < _numTargets; t++) {
      auto capbuf = std::make_shared<CaptureBuffer>();
      auto fut    = fbi->captureAsFormat(bufs[t].get(), capbuf, EBufferFormat::RGBA8);
      _captures[size_t(L) * _numTargets + t] = capbuf;
      _futures.push_back(fut);
    }
    _rtgs.push_back(rtg); // keep the target alive until its capture drains
  }
  printf("SectionBakeJob: rendered %zu layers @ %dpx (%d targets) -> %zu captures issued\n",
         _layerGids.size(), _bakeRes, _numTargets, _futures.size());
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// O3 stage 3 — the C++ player-path section-array CACHE + array assembly (the port of section_bake.py).
//
// Cache layout / interop: MIRRORS section_bake.py's dir template + per-layer PNG scheme so a Python or C++
// caller computing the SAME content key hits the SAME files. Deviations from the Python side (stated):
//   (1) the dir hash uses the ork DataBlock hasher (12 hex of a uint64), not python's sha1[:12] — the two
//       sides never share a key string in practice (Python callers pass an explicit key; the C++ driver
//       derives one from graph+materials), so the hash FUNCTION is unobservable across them;
//   (2) MULTI-TARGET: the capture-target NAME is folded into the content key -> one dir PER target, each
//       holding layer<NN>.png (byte-identical filename scheme to Python's single-target case).
///////////////////////////////////////////////////////////////////////////////

// dir for ONE target's per-section array: <assetcache>/ptex3d_capture/section/<hash>__r<res>__L<layers>/
std::string sectionArrayCacheDir(const std::string& contentKey, int bakeRes, int numLayers) {
  auto h = DataBlock::createHasher();
  h->accumulateString(contentKey);
  h->accumulateItem<int>(bakeRes);
  h->accumulateItem<int>(numLayers);
  h->finish();
  char hex[32];
  snprintf(hex, sizeof(hex), "%012llx", (unsigned long long)(h->result() & 0xFFFFFFFFFFFFull));
  return file::Path::expandPathString(FormatString(
      "<assetcache>/ptex3d_capture/section/%s__r%d__L%d", hex, bakeRes, numLayers));
}

static std::string _sectionLayerPng(const std::string& dir, int layer) {
  return FormatString("%s/layer%02d.png", dir.c_str(), layer);
}

// content key from the graph (mesh identity via per-module content hashes) + material names + section
// layout + res. The caller folds the per-TARGET name on top (see sectionArrayCacheWarm/assemble/load).
std::string sectionBakeContentKey(dflow::graphdata_ptr_t graph, const std::string& mainMtl,
                                  const std::map<int, std::string>& gidMtls,
                                  const std::vector<int>& layerGids, int bakeRes) {
  auto h = DataBlock::createHasher();
  h->accumulateString("hm.section.v5");         // format epoch (v5: coverage-weighted mip downsample; v4: gutter dilation; v3: bake-atlas v-orientation fix)
  if (graph)
    for (size_t im = 0; im < graph->numModules(); im++)
      if (auto mod = graph->module(im))
        h->accumulateItem<uint64_t>(hypermeshModuleIdentityHash(mod.get()));
  h->accumulateString(mainMtl);                 // stored sampler material identity (asset name)
  for (const auto& [gid, name] : gidMtls) {     // the bake map (gid -> material asset name), std::map = sorted
    h->accumulateItem<int>(gid);
    h->accumulateString(name);
  }
  for (int g : layerGids)                        // the section layout (layer -> gid)
    h->accumulateItem<int>(g);
  h->accumulateItem<int>(bakeRes);
  h->finish();
  char hex[32];
  snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)h->result());
  return std::string(hex);
}

// true iff every target's every-layer PNG is already on disk (a WARM run needs no GPU bake).
bool sectionArrayCacheWarm(const std::string& baseKey, const std::vector<std::string>& targets,
                           int bakeRes, int numLayers) {
  if (numLayers < 1 or targets.empty())
    return false;
  for (const auto& tgt : targets) {
    std::string dir = sectionArrayCacheDir(baseKey + "::" + tgt, bakeRes, numLayers);
    for (int L = 0; L < numLayers; L++) {
      std::error_code ec;
      if (not std::filesystem::exists(_sectionLayerPng(dir, L), ec))
        return false;
    }
  }
  return true;
}

// Section-array MIP control (A8). `genMips` is the reflected per-drawable knob (default ON, threaded from
// HypermeshDrawableData::_section_mips); the ORKID_SECTION_MIPS env var is a gate/debug OVERRIDE (0=force
// off, 1=force on) that the minification A/B flips to render the identical .ecs both ways — unset defers to
// the knob. Kept out of the content key on purpose: mips regenerate from mip-0 at build (see _buildSectionArray),
// so the on-disk cache (mip-0 PNGs) is backward-readable and one cache serves both modes. The cached mip-0 PNG
// is POST gutter-dilation (written by assembleSectionArraysFromJob after _dilateCoverageRGBA8), so a WARM load
// is byte-identical to the COLD result and only the (cheap, per-level re-dilated) mip chain regenerates.
static bool _sectionMipsEnabled(bool knob) {
  if (const char* e = getenv("ORKID_SECTION_MIPS"))
    return atoi(e) != 0;
  return knob;
}

// full mip-chain level count for a square bakeRes (down to 1x1); 256 -> 9 levels (0..8).
static int _sectionMipCount(int bakeRes) {
  int n = 1;
  for (int d = bakeRes; d > 1; d >>= 1)
    n++;
  return n;
}

// GUTTER DILATION (atlas chart-border bleed fix). The per-section xatlas charts are painted islands on a
// (0,0,0,0)-cleared black gutter with no padding. Because the unwrap is PER SECTION, every face boundary is a
// chart boundary, so bilinear sampling at any mesh edge mixes gutter black into the edge texels -> thin black
// lines along the mesh edges (and mips compound it: each triangle downsample averages more gutter into the
// border, so the lines WIDEN with distance). Fix = flood covered-texel RGB outward into the uncovered gutter.
// COVERAGE is the alpha channel: the capture shader writes alpha=1.0 where a fragment lands (emit_captures
// coverage-alpha), the clear leaves gutter alpha=0.0. Each pass copies the mean RGB of covered 8-neighbors
// into an uncovered texel and marks it covered (alpha=255) so the front advances one ring per pass. Reads the
// per-pass SNAPSHOT so exactly one ring grows per iteration (no in-pass runaway). Forward samples .xyz only,
// so writing alpha here is invisible to the render; alpha's sole job is to be this coverage mask (also at each
// mip, where the resampled alpha re-encodes coverage for the per-level re-dilation).
//
// N (kSectionDilateTexels): bilinear needs >=1 valid texel beyond a chart edge; trilinear/anisotropic
// footprints and xatlas' half-texel chart insets want a few more. 4 covers the mip-0 sampling footprint. The
// deeper mip chain no longer re-dilates from mip-0; it COVERAGE-WEIGHTS the downsample (see _buildSectionArray)
// so gutter black is excluded at every level regardless of chart size. 4 is small enough not to bridge a
// typical xatlas gutter into a neighbor chart.
static const int kSectionDilateTexels = 4;

static void _dilateCoverageRGBA8(uint8_t* px, int w, int h, int iters) {
  if (not px or w <= 0 or h <= 0 or iters <= 0)
    return;
  const size_t n = size_t(w) * size_t(h);
  std::vector<uint8_t> snap(n * 4);
  for (int it = 0; it < iters; it++) {
    std::memcpy(snap.data(), px, n * 4); // snapshot: this pass only reads coverage from the PRIOR ring
    bool grew = false;
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        uint8_t* d = px + (size_t(y) * w + x) * 4;
        if (d[3] != 0)
          continue; // already covered/valid
        int r = 0, g = 0, b = 0, cnt = 0;
        for (int dy = -1; dy <= 1; dy++) {
          for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 and dy == 0)
              continue;
            int nx = x + dx, ny = y + dy;
            if (nx < 0 or ny < 0 or nx >= w or ny >= h)
              continue;
            const uint8_t* s = snap.data() + (size_t(ny) * w + nx) * 4;
            if (s[3] != 0) { r += s[0]; g += s[1]; b += s[2]; cnt++; }
          }
        }
        if (cnt > 0) {
          d[0] = uint8_t(r / cnt);
          d[1] = uint8_t(g / cnt);
          d[2] = uint8_t(b / cnt);
          d[3] = 255; // now a valid (dilated) texel — advances the front next pass
          grew  = true;
        }
      }
    }
    if (not grew)
      break; // fully flooded (no uncovered texel had a covered neighbor)
  }
}

// build a fresh RGBA8 TextureArray, upload every layer's bytes (numLayers x res x res x 4), finalize ONCE
// (the correct non-streaming sequence — reserve -> upload every layer[/mip] -> finalize). `layerBytes[L]` is
// the RGBA8 mip-0 pixel buffer for layer L; every entry must be res*res*4 bytes. When mips are enabled the
// chunked upload path carries a full CPU-generated chain per layer (reserve num_mips, uploadTextureRegion per
// mip level, finalize transitions ALL levels); finalizeUpload then installs the trilinear sampler (num_mips>3),
// so minified section surfaces stop aliasing.
static texturearray_ptr_t _buildSectionArray(
    Context* ctx, int bakeRes, const std::vector<std::vector<uint8_t>>& layerBytes, bool genMips,
    const std::string& targetName = "") {
  // A downsampled tangent/world-space NORMAL map is no longer unit-length (box/triangle filtering
  // AVERAGES the encoded vectors), so its mips read as shortened normals -> flattened lighting under
  // minification. For the "SectionNormal" target ONLY, renormalize each mip texel after resample.
  // Keyed by NAME (not a per-target flag) so it lands even though SectionNormal's DSL lane is unmerged.
  const bool renormMips = (targetName == "SectionNormal");
  const int  numLayers = int(layerBytes.size());
  const bool mips      = _sectionMipsEnabled(genMips);
  const int  numMips   = mips ? _sectionMipCount(bakeRes) : 1;
  auto txi = ctx->TXI();
  auto arr = std::make_shared<TextureArray>();
  arr->_requires_mips = mips;
  arr->resize(size_t(bakeRes), size_t(bakeRes), size_t(std::max(1, numLayers)), EBufferFormat::RGBA8);
  txi->reserveTextureArray(arr.get(), bakeRes, bakeRes, std::max(1, numLayers), numMips, EBufferFormat::RGBA8);
  const size_t bytes0 = size_t(bakeRes) * size_t(bakeRes) * 4;
  for (int L = 0; L < numLayers; L++) {
    OrkAssert(layerBytes[L].size() == bytes0);
    // mip 0 — the captured/cached full-res section surface (byte-identical to the no-mip path).
    TextureRegionUpload up0;
    up0._mip_level   = 0;
    up0._array_layer = L;
    up0._extent_w    = bakeRes;
    up0._extent_h    = bakeRes;
    up0._extent_d    = 1;
    up0._data        = layerBytes[L].data();
    up0._data_size   = bytes0;
    txi->uploadTextureRegion(arr->_tex.get(), up0);
    if (mips) {
      // COVERAGE-WEIGHTED mip chain (successive halving). Each mip texel is the coverage-weighted mean of
      // its 4 children: rgb = Σ(child_rgb·child_cov)/Σ(child_cov), where cov is the alpha coverage mask
      // (255 covered, 0 gutter). This EXCLUDES gutter-black from the average, so deep mips no longer darken
      // as charts shrink below a fixed dilation radius (the old resample+dilate failure -> distance
      // outlines). mip-0 is already gutter-dilated (kept as-is) and seeds the chain. A 2x2 block that is
      // entirely gutter (Σcov==0) gets a saturation fill (repeat-dilate until no uncovered texel remains —
      // cheap at these small levels) and is marked covered so it contributes real color upward. Order per
      // level: coverage-weighted downsample -> zero-cov fill -> SectionNormal renorm. uploadTextureRegion
      // copies host bytes into its staging buffer synchronously before returning.
      Image cur; // the previous (parent) level; starts as mip-0
      cur.init(size_t(bakeRes), size_t(bakeRes), 4, 1);
      std::memcpy(cur.pixel8(0, 0), layerBytes[L].data(), bytes0);
      for (int m = 1; m < numMips; m++) {
        const int pw = std::max(1, bakeRes >> (m - 1));
        const int ph = std::max(1, bakeRes >> (m - 1));
        const int mw = std::max(1, bakeRes >> m);
        const int mh = std::max(1, bakeRes >> m);
        Image mimg;
        mimg.init(size_t(mw), size_t(mh), 4, 1);
        for (int y = 0; y < mh; y++) {
          for (int x = 0; x < mw; x++) {
            uint32_t accR = 0, accG = 0, accB = 0, accCov = 0;
            for (int dy = 0; dy < 2; dy++) {
              for (int dx = 0; dx < 2; dx++) {
                const int sx = std::min(2 * x + dx, pw - 1);
                const int sy = std::min(2 * y + dy, ph - 1);
                const uint8_t* s = cur.pixel8(sx, sy);
                const uint32_t cov = s[3];
                accR += uint32_t(s[0]) * cov;
                accG += uint32_t(s[1]) * cov;
                accB += uint32_t(s[2]) * cov;
                accCov += cov;
              }
            }
            uint8_t* d = mimg.pixel8(x, y);
            if (accCov > 0) {
              d[0] = uint8_t(accR / accCov);
              d[1] = uint8_t(accG / accCov);
              d[2] = uint8_t(accB / accCov);
              d[3] = 255; // covered
            } else {
              d[0] = d[1] = d[2] = 0;
              d[3] = 0; // fully-gutter block -> filled below
            }
          }
        }
        // zero-cov fill: flood covered RGB into any texel a fully-gutter 2x2 left uncovered. mw+mh passes
        // guarantee a full flood at these small levels (the loop self-terminates when no texel grows).
        _dilateCoverageRGBA8(mimg.pixel8(0, 0), mw, mh, mw + mh);
        if (renormMips) {
          for (int y = 0; y < mh; y++) {
            for (int x = 0; x < mw; x++) {
              uint8_t* p = mimg.pixel8(x, y);
              float nx = p[0] * (2.0f / 255.0f) - 1.0f;
              float ny = p[1] * (2.0f / 255.0f) - 1.0f;
              float nz = p[2] * (2.0f / 255.0f) - 1.0f;
              float len = std::sqrt(nx * nx + ny * ny + nz * nz);
              if (len > 1e-6f) { nx /= len; ny /= len; nz /= len; }
              auto enc = [](float v) -> uint8_t {
                float e = (v * 0.5f + 0.5f) * 255.0f + 0.5f;
                return uint8_t(e < 0.0f ? 0.0f : (e > 255.0f ? 255.0f : e));
              };
              p[0] = enc(nx); p[1] = enc(ny); p[2] = enc(nz); // alpha (AO/coverage) untouched
            }
          }
        }
        TextureRegionUpload up;
        up._mip_level   = m;
        up._array_layer = L;
        up._extent_w    = mw;
        up._extent_h    = mh;
        up._extent_d    = 1;
        up._data        = mimg.pixel8(0, 0);
        up._data_size   = size_t(mw) * size_t(mh) * 4;
        txi->uploadTextureRegion(arr->_tex.get(), up);
        std::swap(cur, mimg); // successive halving: this level becomes the parent of the next
      }
    }
  }
  txi->finalizeUpload(arr->_tex.get());
  return arr;
}

// a VALID sampleable placeholder array (neutral gray every layer) — bound to the stored sampler during the
// few-frame cold bake so its sampler2DArray reads safely BEFORE the real content lands. Writes NO cache.
texturearray_ptr_t placeholderSectionArray(Context* ctx, int numLayers, int bakeRes) {
  const size_t bytes = size_t(bakeRes) * size_t(bakeRes) * 4;
  std::vector<uint8_t> gray(bytes);
  for (size_t i = 0; i < bytes; i += 4) { gray[i] = gray[i + 1] = gray[i + 2] = 48; gray[i + 3] = 255; }
  std::vector<std::vector<uint8_t>> layers(std::max(1, numLayers), gray);
  // Placeholder is a transient single-mip gray fill (bound only during the few-frame cold bake, then
  // replaced by the real mipped array): no minification concern, so never build a chain for it.
  return _buildSectionArray(ctx, bakeRes, layers, /*genMips*/ false);
}

// pull an Image's RGBA8 bytes (already destination format after captureAsFormat/readFromFile-convert).
static std::vector<uint8_t> _imageRGBA8Bytes(const Image& img, int bakeRes) {
  const size_t bytes = size_t(bakeRes) * size_t(bakeRes) * 4;
  std::vector<uint8_t> out(bytes, 0);
  if (img._data and img._data->length() >= bytes and img._numcomponents == 4 and
      int(img._width) == bakeRes and int(img._height) == bakeRes) {
    std::memcpy(out.data(), img._data->data(), bytes);
    return out;
  }
  // conform / resize path (warm-load of a differently-sized or RGB PNG): convert to RGBA8 then copy.
  Image rgba;
  img.convertToRGBA(rgba, true);
  if (rgba._data and rgba._data->length() >= bytes and int(rgba._width) == bakeRes and int(rgba._height) == bakeRes)
    std::memcpy(out.data(), rgba._data->data(), bytes);
  return out;
}

// COLD: assemble one TextureArray per target from a completed job's host captures, WRITE the cache PNGs.
// Returns the arrays in `targets` order. FAILS LOUD on a missing capture (ops self-defend).
std::vector<texturearray_ptr_t> assembleSectionArraysFromJob(
    Context* ctx, sectionbakejob_ptr_t job, const std::string& baseKey,
    const std::vector<std::string>& targets, int bakeRes, bool genMips) {
  std::vector<texturearray_ptr_t> arrays;
  if (not job)
    return arrays;
  const int numLayers  = job->numLayers();
  const int numTargets = std::max(1, int(targets.size()));
  for (int t = 0; t < numTargets; t++) {
    std::string dir = sectionArrayCacheDir(baseKey + "::" + targets[t], bakeRes, numLayers);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const size_t bytes0 = size_t(bakeRes) * size_t(bakeRes) * 4;
    std::vector<std::vector<uint8_t>> layerBytes(numLayers);
    for (int L = 0; L < numLayers; L++) {
      auto cb = job->layerCapture(L, t);
      OrkAssertI(cb and cb->_image,
                 "assembleSectionArraysFromJob: SectionBakeJob missing a capture (layer/target)");
      layerBytes[L] = _imageRGBA8Bytes(*cb->_image, bakeRes);
      // GUTTER DILATION on mip-0 (kills the chart-border black-edge bleed). The cache stores the
      // POST-dilation mip-0 PNG, so a WARM load is byte-identical + needs no re-dilate at mip-0 (mips
      // still regenerate + re-dilate per level in _buildSectionArray). See _dilateCoverageRGBA8.
      _dilateCoverageRGBA8(layerBytes[L].data(), bakeRes, bakeRes, kSectionDilateTexels);
      Image dimg;
      dimg.initWithFormat(size_t(bakeRes), size_t(bakeRes), EBufferFormat::RGBA8);
      std::memcpy(dimg.pixel8(0, 0), layerBytes[L].data(), bytes0);
      dimg.writeToFile(file::Path(_sectionLayerPng(dir, L).c_str())); // cold: write the POST-dilation cache PNG
    }
    arrays.push_back(_buildSectionArray(ctx, bakeRes, layerBytes, genMips, targets[t]));
    printf("section_bake(C++): COLD target<%s> %d layers @ %dpx mips=%d -> %s\n",
           targets[t].c_str(), numLayers, bakeRes, int(_sectionMipsEnabled(genMips)), dir.c_str());
  }
  return arrays;
}

// WARM: load one TextureArray per target from the cache PNGs (no bake). Returns arrays in `targets` order.
std::vector<texturearray_ptr_t> loadSectionArraysFromCache(
    Context* ctx, const std::string& baseKey, const std::vector<std::string>& targets,
    int bakeRes, int numLayers, bool genMips) {
  std::vector<texturearray_ptr_t> arrays;
  const int numTargets = std::max(1, int(targets.size()));
  for (int t = 0; t < numTargets; t++) {
    std::string dir = sectionArrayCacheDir(baseKey + "::" + targets[t], bakeRes, numLayers);
    std::vector<std::vector<uint8_t>> layerBytes(numLayers);
    for (int L = 0; L < numLayers; L++) {
      Image img;
      bool ok = img.readFromFile(file::Path(_sectionLayerPng(dir, L).c_str()));
      OrkAssertI(ok, "loadSectionArraysFromCache: cache PNG read failed (warm run on incomplete cache?)");
      layerBytes[L] = _imageRGBA8Bytes(img, bakeRes);
    }
    arrays.push_back(_buildSectionArray(ctx, bakeRes, layerBytes, genMips, targets[t]));
    printf("section_bake(C++): WARM target<%s> %d layers @ %dpx mips=%d <- %s\n",
           targets[t].c_str(), numLayers, bakeRes, int(_sectionMipsEnabled(genMips)), dir.c_str());
  }
  return arrays;
}

// faceViz=true -> the per-triangle source-face-id buffer (_triFace) is exposed + kept refreshed for the
// face-visualization FS (which reads TFd[gl_PrimitiveID]); returns that buffer so make_drawable can bind
// it to the FS storage block (graphics-storage index 5, right after the 5 vertex channels). Returns null
// when faceViz=false. The vert channels live at graphics-storage indices 0..4 (see make_drawable).
MeshRenderBuffers setupMeshRender(
    ComputeDrawableData* cdd, livehypermesh_ptr_t live, Context* ctx, bool animated, bool faceViz,
    bool tagViz, bool wireframe, int instanceCount, const std::vector<float>& instanceMatrices,
    const std::vector<int>& boundGids, bool cull, const fvec4& cullBound,
    int cullSlabs, float cullTightness, float cullDistance,
    const std::vector<livehypermesh_ptr_t>& lodLives, const std::vector<float>& lodDistances,
    const std::vector<int>& impostorTiers,
    const std::map<int, pbrmaterial_ptr_t>& gidMaterials,
    int impostorGrid, int impostorTile, int impostorSsaa, int impostorMsaa) {
  faceViz = faceViz or tagViz; // the tag-viz FS indexes __tags by the per-triangle face id (slot 5)
  auto isImpostorTier = [&](int extraIdx) {
    return std::find(impostorTiers.begin(), impostorTiers.end(), extraIdx) != impostorTiers.end();
  };
  auto tri = std::make_shared<MeshRenderTri>();
  tri->_wireframe = wireframe;
  tri->_boundGids = boundGids; // E.3: the fold mask (everything else lands in slot 0)
  tri->build(ctx);
  auto mesh0 = live->_mesh;
  tri->ensureIndex(mesh0 ? mesh0->_num_corners : 1);
  // Phase 3c — one triangulator per EXTRA LOD tier (its own distinct mesh/topology). Captured by the
  // live hook (each triangulates its own static mesh on the first frame) and turned into tier draws.
  std::vector<std::shared_ptr<MeshRenderTri>>      lodTris;
  std::vector<livehypermesh_ptr_t>                 lodTierLives;
  for (auto& ll : lodLives) {
    auto lt          = std::make_shared<MeshRenderTri>();
    lt->_boundGids   = boundGids; // same gid set as tier 0 (per-tier materials mirror the base)
    lt->build(ctx);
    auto lm = ll ? ll->_mesh : nullptr;
    lt->ensureIndex(lm ? lm->_num_corners : 1);
    lodTris.push_back(lt);
    lodTierLives.push_back(ll);
  }
  // INSTANCING: cs_reset writes instanceCount into the indirect command.
  // E.2 TYPED EDGE: a graph-carried InstanceSet (live->_instances, a ScatterSource output)
  // IS the instance source — its matrices + attrs SSBOs bind as-is, count from the set.
  // LEGACY (instances= floats): buffers are created here; the matrix BOTTOM ROWS are split
  // into a synthesized attrs SSBO and ZEROED on upload — the instanced VS now ALWAYS reads
  // per-instance data from storage_inst_attr (the bottom-row smuggle is retired).
  FxShaderStorageBuffer* inst_attr = nullptr;
  if (live->_instances and live->_instances->_count > 0) {
    tri->_instCount = live->_instances->_count;
    tri->_instMtx   = live->_instances->_matrices;
    inst_attr       = live->_instances->_attrs;
  } else {
    tri->_instCount = (instanceCount > 1) ? instanceCount : 1;
    if (tri->_instCount > 1 and not instanceMatrices.empty()) {
      const size_t n     = size_t(tri->_instCount);
      const size_t bytes = n * 64; // mat4 = 64 bytes
      std::vector<float> mats(n * 16, 0.0f);
      std::memcpy(mats.data(), instanceMatrices.data(),
                  std::min(bytes, instanceMatrices.size() * sizeof(float)));
      std::vector<float> attrs(n * 4, 0.0f);
      for (size_t i = 0; i < n; i++) { // bottom row (m[0..2].w = floats 3/7/11) -> attrs; rows zeroed
        attrs[i * 4 + 0] = mats[i * 16 + 3];
        attrs[i * 4 + 1] = mats[i * 16 + 7];
        attrs[i * 4 + 2] = mats[i * 16 + 11];
        attrs[i * 4 + 3] = 1.0f;
        mats[i * 16 + 3] = mats[i * 16 + 7] = mats[i * 16 + 11] = 0.0f;
        mats[i * 16 + 15] = 1.0f;
      }
      auto fxi      = ctx->FXI();
      tri->_instMtx = fxi->createStorageBuffer(bytes);
      auto m = fxi->mapStorageBuffer(tri->_instMtx, 0, bytes, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, mats.data(), bytes);
      fxi->unmapStorageBuffer(m.get());
      inst_attr = fxi->createStorageBuffer(n * 16);
      auto a = fxi->mapStorageBuffer(inst_attr, 0, n * 16, BufferMapAccess::WRITE_ONLY);
      std::memcpy(a->_mappedaddr, attrs.data(), n * 16);
      fxi->unmapStorageBuffer(a.get());
    }
  }
  // E.4 — per-view instance cull: compacts matrices+attrs into culled buffers
  // (the graphics pipes bind THOSE) and fans the visible count into every
  // bound gid slot's indirect command. Requires a positive bound radius.
  FxShaderStorageBuffer* gfx_instMtx  = tri->_instMtx;
  FxShaderStorageBuffer* gfx_instAttr = inst_attr;
  FxShaderStorageBuffer* gfx_instMtxShadow  = nullptr; // cascade-cull fix (set in the cull branch)
  FxShaderStorageBuffer* gfx_instAttrShadow = nullptr;
  std::vector<MeshRenderBuffers::TierDraw> lodTiers; // Phase 3b: extra LOD tiers (hm_drawable -> buckets)
  if (cull and tri->_instCount > 1 and tri->_instMtx) {
    // E.4 cull bound: AUTO (w<=0) computes the object-space sphere once from a position readback of
    // the materialized mesh (+5% pad) — shared by the scene (HypermeshDrawableData) AND the python
    // make_drawable paths. Animated meshes that outgrow their static bounds should pass cullBound.
    fvec4 bound = cullBound;
    std::vector<fvec4> occBounds; // OCCLUDEE = K object-space AABBs (2 vec4 each); K=1 AABB or N slabs
    if (bound.w <= 0.0f and live->_mesh) {
      int nv   = live->_mesh->_num_verts;
      auto pch = live->_mesh->channel(MeshChannel::POSITION);
      if (nv > 0 and pch) {
        auto fxi = ctx->FXI();
        std::vector<float> P(size_t(nv) * 4);
        auto m = fxi->mapStorageBuffer(pch->_ssbo, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
        std::memcpy(P.data(), m->_mappedaddr, size_t(nv) * 16);
        fxi->unmapStorageBuffer(m.get());
        fvec3 bmin(P[0], P[1], P[2]), bmax = bmin;
        for (int i = 1; i < nv; i++) {
          fvec3 p(P[i * 4], P[i * 4 + 1], P[i * 4 + 2]);
          bmin = fvec3(std::min(bmin.x, p.x), std::min(bmin.y, p.y), std::min(bmin.z, p.z));
          bmax = fvec3(std::max(bmax.x, p.x), std::max(bmax.y, p.y), std::max(bmax.z, p.z));
        }
        bound = fvec4((bmin + bmax) * 0.5f, (bmax - bmin).length() * 0.5f * 1.05f); // SPHERE = frustum only
        // occludee decomposition (drives occlusion): K=1 whole-mesh AABB, or N vertical slabs (bin P by
        // Y) — slabs separate a thin trunk from a wide canopy so each part occludes independently.
        int K = std::max(1, cullSlabs);
        if (K <= 1) {
          occBounds = {fvec4(bmin.x, bmin.y, bmin.z, 0), fvec4(bmax.x, bmax.y, bmax.z, 0)};
        } else {
          float y0 = bmin.y, dy = (bmax.y - bmin.y) / float(K);
          std::vector<fvec3> smn(K, fvec3(1e30f, 1e30f, 1e30f)), smx(K, fvec3(-1e30f, -1e30f, -1e30f));
          std::vector<bool>  used(K, false);
          for (int i = 0; i < nv; i++) {
            fvec3 p(P[i * 4], P[i * 4 + 1], P[i * 4 + 2]);
            int s = dy > 0.0f ? std::max(0, std::min(K - 1, int((p.y - y0) / dy))) : 0;
            smn[s] = fvec3(std::min(smn[s].x, p.x), std::min(smn[s].y, p.y), std::min(smn[s].z, p.z));
            smx[s] = fvec3(std::max(smx[s].x, p.x), std::max(smx[s].y, p.y), std::max(smx[s].z, p.z));
            used[s] = true;
          }
          for (int s = 0; s < K; s++)
            if (used[s]) {
              occBounds.push_back(fvec4(smn[s].x, smn[s].y, smn[s].z, 0));
              occBounds.push_back(fvec4(smx[s].x, smx[s].y, smx[s].z, 0));
            }
        }
        printf("hypermesh::setupMeshRender: auto cull bound c<%.2f %.2f %.2f> r<%.2f> occ_boxes<%d>\n",
               bound.x, bound.y, bound.z, bound.w, int(occBounds.size() / 2));
      }
    }
    if (bound.w <= 0.0f) {
      printf("hypermesh::setupMeshRender: cull requested but no bound (empty mesh) — cull DISABLED\n");
    } else {
      auto culler = std::make_shared<MeshInstCull>();
      // Phase 3c — DISTANCE LOD: tier 0 = the main mesh, tiers 1..N = lodLives at lodDistances (clamped
      // to 4 tiers / 3 boundaries). _numTiers set BEFORE build so OUT_M sizes for all tiers.
      int nLod = std::min(int(lodLives.size()), 3); // up to 3 extra tiers (4 total)
      culler->_numTiers = 1 + nLod;
      for (int i = 0; i < nLod && i < int(lodDistances.size()); i++)
        culler->_lodDist[i] = lodDistances[i];
      culler->build(ctx, tri->_instCount); // sizes OUT_M by _numTiers — set tiers FIRST
      culler->_bound    = bound;
      culler->_boxScale = (cullTightness > 0.0f) ? cullTightness : 1.0f; // per-variant occludee tightness
      culler->_cullDistance = cullDistance;                              // 0 = no distance cull
      // explicit cullBound (no mesh readback) -> the sphere's cube as the single occludee box.
      if (occBounds.empty()) {
        fvec3 c(bound.x, bound.y, bound.z);
        occBounds = {fvec4(c.x - bound.w, c.y - bound.w, c.z - bound.w, 0),
                     fvec4(c.x + bound.w, c.y + bound.w, c.z + bound.w, 0)};
      }
      culler->setBounds(ctx, occBounds);
      culler->_srcMtx  = tri->_instMtx;
      if (inst_attr) {
        culler->_srcAttr = inst_attr;
      } else { // matrices-only: a zeroed attr source keeps the shader uniform
        auto fxi         = ctx->FXI();
        culler->_srcAttr = fxi->createStorageBuffer(size_t(tri->_instCount) * 16);
        std::vector<float> zz(size_t(tri->_instCount) * 4, 0.0f);
        auto zm = fxi->mapStorageBuffer(culler->_srcAttr, 0, zz.size() * 4, BufferMapAccess::WRITE_ONLY);
        std::memcpy(zm->_mappedaddr, zz.data(), zz.size() * 4);
        fxi->unmapStorageBuffer(zm.get());
      }
      culler->_args         = tri->_args;
      culler->_boundGidMask = tri->_boundGidMask;
      // Phase 3c — LOD tiers. tier 0 = the main draw (tri, OUT_M offset 0). Each EXTRA tier draws its OWN
      // LOD mesh (lodTris[t-1], distinct topology -> its own triangulated args carrying its indexCount;
      // the per-tier fanout only stamps instanceCount=VIS[t]) and reads its OUT_M slice at offset t*count.
      // hm_drawable turns _lodTiers into bucket draws (main + gid materials, mirroring tier 0).
      culler->_tierArgs    = {tri->_args};
      culler->_tierGidMask = {tri->_boundGidMask};
      // LOD step #3 — the bake reads the base material's FWD_SSBO_CUSTOM_CAPTURE technique (impostor=True).
      auto pbrBase = std::dynamic_pointer_cast<PBRMaterial>(cdd->_material);
      for (int t = 1; t < culler->_numTiers; t++) {
        int    extraIdx = t - 1;
        size_t tierOff  = size_t(t) * size_t(tri->_instCount) * 64; // this tier's OUT_M slice (sub-range bind)
        // IMPOSTOR tier — bake the base mesh's hemi-oct atlas ONCE (in-frame) and draw one camera-facing
        // billboard per band instance; the matrix slice IS this tier's OUT_M offset, instanceCount = VIS[t]
        // (the cull's per-tier fanout stamps the quad's single command — slot-0-only mask).
        // ORKID_IMPOSTOR_DUMP: bake + write the atlas PNGs to /tmp/orkid_impostor/ (one numbered set per
        // baked variant) instead of drawing the billboards — the readback transitions the atlas to host-read,
        // which the billboard sample can't use (so the live draw is suppressed this mode). See impostorDumpDir.
        static int  s_impDumpIdx = 0;
        const bool  s_impDump    = (getenv("ORKID_IMPOSTOR_DUMP") != nullptr);
        std::string outBase      = s_impDump ? (impostorDumpDir() + "/imp" + std::to_string(s_impDumpIdx++))
                                             : std::string("impostor"); // non-dump: name unused (no readback)
        impostorbakejob_ptr_t job =
            (isImpostorTier(extraIdx) and pbrBase)
                ? prepareImpostorBake(ctx, live, pbrBase, gidMaterials, tri, impostorGrid, impostorTile,
                                      impostorSsaa, impostorMsaa, cullDistance, outBase)
                : nullptr;
        if (job and job->_impMtl and job->_impInstBlock and pbrBase->_tek_FWD_SSBO_CUSTOM_IMPOSTOR) {
          auto prev           = cdd->_oneShotRender; // chain: multiple impostor tiers each bake their atlas
          cdd->_oneShotRender = [prev, job](Context* c) -> bool {
            bool a = prev ? prev(c) : true;      // each link retries independently until its tri is clean;
            bool b = job->renderInFrame(c);      // the chain reports done only when EVERY bake has captured.
            return a and b;
          };
          culler->_tierArgs.push_back(job->_quadArgs);
          culler->_tierGidMask.push_back(job->_quadGidMask);
          if (not s_impDump) { // dump mode samples the host-read atlas -> skip the billboard draw (no crash)
            ComputeDrawable::BucketDraw imp;
            imp._material       = job->_impMtl;       // the BASE PBRMaterial (SHARED across variants), drawn impostor
            imp._isImpostor     = true;               // -> RCID._isImpostor -> FWD_SSBO_CUSTOM_IMPOSTOR + fwd lighting
            imp._indexSSBO      = job->_quadIndex;
            imp._argsSSBO       = job->_quadArgs;
            imp._instMtxBlock   = job->_impInstBlock; // storage_inst_mtx, bound at the tier slice of...
            imp._instMtxBuf     = culler->_culledMtx; // ...the cull's interleaved OUT_M
            imp._instByteOffset = tierOff;
            // THIS variant's atlas + params, bound PER-DRAW (the shared material can't hold per-variant atlases).
            imp._impAtlasAlbedo     = job->_rtg->texture(0);
            imp._impAtlasNormal     = job->_rtg->texture(1);
            imp._impAtlasMetalRough = job->_rtg->texture(2);
            imp._impCenter          = fvec4(job->_center.x, job->_center.y, job->_center.z, job->_radius);
            imp._impGrid            = fvec4(float(job->_gridN), cullDistance, 0.0f, 0.0f);
            imp._parImpAlbedo       = pbrBase->_parImpAlbedo;
            imp._parImpNormal       = pbrBase->_parImpNormal;
            imp._parImpMetalRough   = pbrBase->_parImpMetalRough;
            imp._parImpCenter       = pbrBase->_parImpCenter;
            imp._parImpGrid         = pbrBase->_parImpGrid;
            cdd->_bucketDraws.push_back(imp);
          }
        } else { // MESH tier (legacy LOD path): its own triangulated args/index, OUT_M slice via hm_drawable
          auto& lt = lodTris[t - 1];
          culler->_tierArgs.push_back(lt->_args);
          culler->_tierGidMask.push_back(lt->_boundGidMask);
          lodTiers.push_back({lt->_args, lt->_triIndex, tierOff});
        }
      }
      cdd->_perViewCompute  = [culler](Context* c, const CameraMatrices& m) { culler->perView(c, m); };
      // cascade-cull fix — the sun-shadow cull hook + shadow args. The caller (hm_drawable / python
      // make_drawable) re-binds handles._instMtxShadow/_instAttrShadow on its material's inst blocks
      // to form cdd->_shadowStorageOverrides (main) + per-gid-bucket overrides.
      cdd->_perViewComputeShadow = [culler](Context* c, const CameraMatrices& m) { culler->perViewShadow(c, m); };
      cdd->_argsSSBOShadow       = culler->_argsShadow;
      gfx_instMtx  = culler->_culledMtx;
      gfx_instAttr = culler->_culledAttr;
      gfx_instMtxShadow  = culler->_culledMtxShadow;
      gfx_instAttrShadow = culler->_culledAttrShadow;
    }
  }
  cdd->setIndirect(tri->_args, 0, tri->_triIndex, PrimitiveType::TRIANGLES, 4);
  if (wireframe) // the OVERLAY draw (lines over the fill); caller sets the overlay material + P/N storage
    cdd->setOverlayIndirect(tri->_lineArgs, 0, tri->_lineIndex, PrimitiveType::LINES, 4);

  // the per-frame, IN-FRAME hook (ComputeDrawable::onPreRender runs this; _passes stays empty). One
  // dispatch phase: (animated) graph re-eval -> triangulate -> refresh the drawable's live bindings.
  // MESH-SHADER draw path (ORKID_HYPERMESH_MESHSHADER): resolve capability + material ONCE here;
  // the hook below owns the per-frame decision. Inert (and silent) when the toggle is off.
  auto mlstate = _resolveMeshletDraw(
      cdd, ctx, cdd->_instanced or (tri->_instCount > 1), wireframe, not boundGids.empty());
  // the cluster-bounds pass is built only when a material actually carries the bounds contract
  // (no contract -> no reject -> nothing to compute).
  std::shared_ptr<MeshletBounds> mlbounds;
  if (mlstate->_ready and mlstate->_blkBounds) {
    mlbounds = std::make_shared<MeshletBounds>();
    mlbounds->build(ctx);
  }
  cdd->_liveRecompute = [tri, live, animated, faceViz, tagViz, wireframe, lodTris, lodTierLives, mlstate, mlbounds](
                            Context* ctx, ComputeDrawable* drw) {
    auto mesh = live->_mesh;
    if (not mesh) return;
    // Phase 3c — triangulate each EXTRA LOD tier's (static, distinct) mesh once. topoDirty is true on
    // the first build (rides the main mesh's frame-0 dirty, before the clean early-return below); static
    // tiers go clean after and cost nothing. Each writes its OWN args (indexCount); no clone needed.
    {
      auto lci = ctx->CI();
      for (size_t k = 0; k < lodTris.size(); k++) {
        auto& lt   = lodTris[k];
        auto  lm   = (k < lodTierLives.size() and lodTierLives[k]) ? lodTierLives[k]->_mesh : nullptr;
        if (not lm or not lt->topoDirty(lm)) continue;
        lt->ensureIndex(lm->_num_corners);
        lt->writeParams(lm->_num_faces, lm->face("__tags") != nullptr);
        lci->beginDispatchPhase();
        lt->dispatch(lm);
        lci->endDispatchPhase();
        lt->ackTopo(lm);
      }
    }
    // ---- B.4 CLOCK FEED: the LIVE path's time source (steady, incremental). Fills the family env
    // (module writeParams reads it — e.g. the extrude S.time slot) AND the graph UpdateData (module
    // compute() parity with the ECS host contract). BAKE paths never run this hook -> stay t=0.
    // PAUSE: while (global || per-graph) paused, the accumulator holds and dt=0 — resume is seamless.
    {
      using clk = std::chrono::steady_clock;
      double now = std::chrono::duration<double>(clk::now().time_since_epoch()).count();
      if (live->_clock_last < 0.0)
        live->_clock_last = now;
      bool paused = clockPaused() or live->_paused;
      double dt   = paused ? 0.0 : (now - live->_clock_last);
      live->_clock_last = now;
      live->_clock_abstime += dt;
      if (auto env = live->_ginst->_impl.getShared<MeshEnv>()) {
        env->_dt      = dt;
        env->_abstime = live->_clock_abstime;
      }
      // mirror the SAME clock into the terrain field env, when a field subgraph rides this
      // graph (E.1b: fbm/noise offset_vel pan reads it — one clock, every family env).
      if (auto fenv = live->_ginst->_impl.getShared<terrain::BakeEnv>()) {
        fenv->_dt      = dt;
        fenv->_abstime = live->_clock_abstime;
      }
      live->_updata->_abstime = live->_clock_abstime;
      live->_updata->_dt      = dt;
    }
    // ---- C.1b DIRTY GATE ----
    // graph eval runs only when the graph can actually move: animated AND unpaused. Pause = the
    // graph STOPS (plug edits made while paused take effect on unpause — the B.4 pause contract);
    // a static mesh never re-evals here (initial eval = materializeLive's recompute; a manual
    // live.recompute() between frames mutates topology, which the topo key below catches).
    bool paused    = clockPaused() or live->_paused;
    bool run_graph = animated and not paused;
    auto ci        = ctx->CI();
    bool triangulated = false;
    using perfclk = std::chrono::steady_clock;
    auto perf_g0  = perfclk::now();                 // full-eval wall (HmPerf; clean frames don't count)
    double perf_gw0 = ctx->CI()->_gpuWaitAccum;     // gpu-fence-wait baseline (cpu = full - gpu-wait)
    if (run_graph) {
      // PRE dispatch phase: each module's CPU sizing + uniform host-write (subdivide re-pools
      // here, sets counts). Count/handle topology changes land HERE (allocMesh), so the topo key
      // sampled after this predicts almost every re-triangulation — letting the triangulate ride
      // the SAME dispatch phase as the graph compute (one submit, the pre-C.1b shape).
      for (auto inst : live->_ginst->_ordered_module_insts)
        if (auto pp = std::dynamic_pointer_cast<dflowgfx::IPrePhaseParams>(inst)) { // family-neutral
          auto perf_t0 = perfclk::now();
          pp->writeParams(ctx);
          HmPerf::instance().addModule(
              inst->_dgmodule_data->GetClass()->Name().c_str(),
              std::chrono::duration<double>(perfclk::now() - perf_t0).count());
        }
      bool pre_dirty = tri->topoDirty(mesh);
      if (pre_dirty) {
        tri->ensureIndex(mesh->_num_corners);
        tri->writeParams(mesh->_num_faces, mesh->face("__tags") != nullptr);
      }
      uint64_t _hmgen_t0 = ork::Timer::getSystemTick(); // perf HUD
      ci->beginDispatchPhase();
      live->_ginst->compute(live->_updata);
      if (pre_dirty) {
        ci->storageBarrier(); // graph writes channels/vidx/face_offsets -> triangulate reads them
        tri->dispatch(mesh);
      }
      ci->endDispatchPhase(); // submit + WAIT
      RenderPhaseStats::instance().add(
          "hypermesh-gen", double(ork::Timer::getSystemTick() - _hmgen_t0) * 1.0e-6); // perf HUD
      if (pre_dirty) {
        tri->ackTopo(mesh);   // ack POST-phase: absorbs same-frame compute-time bumps the in-phase
        triangulated = true;  // triangulate already saw (it ran after the barrier)
      }
    }
    // post-eval check — catches the rare COMPUTE-TIME topology rebuild the pre-phase key couldn't
    // see (e.g. an extrude rebuilding off srcTopoDirty cascade); also the first-frame build for the
    // static path and a manual live.recompute() between frames. Positions-only eval (ripple) and
    // the paused/static steady states dispatch NOTHING.
    static const bool s_force_dirty = (getenv("ORK_HM_FORCE_DIRTY") != nullptr); // bisect knob
    bool topo_dirty = (not triangulated) and (s_force_dirty or tri->topoDirty(mesh));
    // ---- MESHLETS: the CPU partition rides the SAME topology key as the triangulation, and the
    //      rebuild is triggered from the same place a re-pool is noticed. This block sits BEFORE the
    //      clean-frame early-out ON PURPOSE: the build completes ASYNCHRONOUSLY (microtask slices),
    //      so for a STATIC mesh every frame in which the partition could possibly land is a clean
    //      frame — returning first meant the drive decision was made exactly once, on the frame the
    //      partition did not exist yet, and the mesh path never engaged. Cost on a clean frame is a
    //      handful of compares and is paid only when the meshlet path is armed.
    if (meshletsEnabled()) {
      if (not live->_meshlets)
        live->_meshlets = MeshletHost::create(ctx);
      auto host = live->_meshlets;
      if (host->topoDirty(mesh)) {
        host->requestRebuild(mesh);
        logchan_hmml->log("partition rebuild requested (faces=%d corners=%d verts=%d)",
                          mesh->_num_faces, mesh->_num_corners, mesh->_num_verts);
      }
      // ---- MESH-SHADER DRAW: switch the drawable onto DrawMeshTasks for exactly as long as a
      //      published partition describes the LIVE topology. Currency is topoVersion + vertex
      //      count — NOT buffer identity (a re-pool that leaves connectivity alone must not retire
      //      a good partition); buffer identity belongs to the rebuild trigger above. While a
      //      rebuild is in flight the draw falls back to pull-VS rather than rasterize stale
      //      buckets. Each distinct reason is logged ONCE (transitions only) — enough to diagnose a
      //      run, not enough to spam it.
      if (mlstate->_ready) {
        auto part    = host->partition();
        int reason   = 0; // 0 = driving
        bool matched = false;
        if (not part)
          reason = 1; // no partition published yet (build still slicing)
        else if (part->topology()->topoVersion() != mesh->_topoVersion or
                 part->topology()->numVerts() != mesh->_num_verts)
          reason = 2; // topology moved under the published partition
        else
          matched = true;
        if (matched and not host->uploadPartition(part)) {
          matched = false;
          reason  = 3; // upload refused
        }
        if (matched) {
          if (mlstate->_slot < 0) { // append ONCE, after every caller-owned storage (the refresh
            mlstate->_slot = int(drw->_graphicsStorage.size()); // loop below owns slots 0..6)
            drw->_graphicsStorage.push_back({mlstate->_blkDesc, host->_descSSBO});
            drw->_graphicsStorage.push_back({mlstate->_blkVtx, host->_vtxSSBO});
            drw->_graphicsStorage.push_back({mlstate->_blkPrim, host->_primSSBO});
            if (mlstate->_blkBounds)
              drw->_graphicsStorage.push_back({mlstate->_blkBounds, host->_boundsSSBO});
            if (mlstate->_blkCull and host->_cullStatSSBO)
              drw->_graphicsStorage.push_back({mlstate->_blkCull, host->_cullStatSSBO});
          } else { // growth re-creates a buffer -> re-point (bindStorage runs off these each frame)
            drw->_graphicsStorage[mlstate->_slot + 0].second = host->_descSSBO;
            drw->_graphicsStorage[mlstate->_slot + 1].second = host->_vtxSSBO;
            drw->_graphicsStorage[mlstate->_slot + 2].second = host->_primSSBO;
            if (mlstate->_blkBounds)
              drw->_graphicsStorage[mlstate->_slot + 3].second = host->_boundsSSBO;
            if (mlstate->_blkCull and host->_cullStatSSBO)
              drw->_graphicsStorage[mlstate->_slot + 4].second = host->_cullStatSSBO;
          }
          // CLUSTER BOUNDS refresh. Re-derived when the partition changed identity (new buckets) or
          // the graph just ran (positions may have moved under an unchanged partition — the case a
          // build-time bound would silently get wrong). Static, unpaused meshes pay this once.
          if (mlstate->_blkBounds and
              (mlstate->_boundsPart != (const void*)part.get() or run_graph)) {
            ci->beginDispatchPhase();
            mlbounds->dispatch(ctx, host.get(), mesh, part->stats()._meshletCount);
            ci->endDispatchPhase();
            mlstate->_boundsPart = (const void*)part.get();
          }
          // REJECT COUNTERS — read at the FRAME BOUNDARY (never mid-graph: a device-local readback
          // inside the dispatch graph aborts the command buffer). The values are last frame's, which
          // is all a counter needs to be; reporting the delta turns the reject from an assertion into
          // a measurement. Every mesh pass that ran contributes, so the raw figure is cluster TESTS
          // across color + depth prepass + any cascade, not distinct clusters — which is why the
          // report below decomposes it into passes x tests-per-pass rather than printing it next to
          // the published count for a reader to compare the two by eye.
          if (mlstate->_blkCull and host->_cullStatSSBO) {
            uint32_t cur[2] = {0, 0};
            auto fxiS = ctx->FXI();
            auto ms   = fxiS->mapStorageBuffer(host->_cullStatSSBO, 0, sizeof(cur), BufferMapAccess::READ_ONLY);
            std::memcpy(cur, ms->_mappedaddr, sizeof(cur));
            fxiS->unmapStorageBuffer(ms.get());
            if (mlstate->_statHave) {
              uint32_t dt = cur[0] - mlstate->_statPrevTested;   // unsigned: wrap-correct from any base
              uint32_t dr = cur[1] - mlstate->_statPrevRejected;
              uint32_t np = part->stats()._meshletCount;
              if (dt and np) {
                // TESTS and PUBLISHED CLUSTERS are different quantities and the line must never let
                // them be read as one. Each pass dispatches one workgroup per published cluster and
                // each workgroup increments once, so the tests divide EXACTLY by the published count
                // and the quotient IS the number of mesh passes that ran. The two failure modes read
                // differently and neither is silent: a partial pass or a pass against a different
                // partition leaves a REMAINDER (flagged inline), while a per-LANE atomic keeps the
                // division exact and instead inflates the pass count by the workgroup size — which
                // is why the pass count is printed rather than divided away.
                uint32_t passes = dt / np;
                uint32_t rem    = dt % np;
                logchan_hmml->log("cluster reject: %u clusters published; %u cluster-tests this "
                                  "frame = %u mesh pass(es) x %u tests/pass%s; %u tests rejected "
                                  "(%.1f%% of tests)",
                                  np, dt, passes, np,
                                  rem ? " [!! not an exact multiple: a pass ran partially, or "
                                        "tested a different partition]"
                                      : "",
                                  dr, 100.0 * double(dr) / double(dt));
              }
            }
            mlstate->_statPrevTested   = cur[0];
            mlstate->_statPrevRejected = cur[1];
            mlstate->_statHave         = true;
          }
          drw->_meshTechnique = mlstate->_tek;
          drw->_meshGroups[0] = part->stats()._meshletCount; // direct-sized: NEVER the indirect draw
          drw->_meshGroups[1] = 1;
          drw->_meshGroups[2] = 1;
          if (mlstate->_lastReason != 0)
            logchan_hmml->log("driving %u meshlets (%u tris) via DrawMeshTasks",
                              part->stats()._meshletCount, part->stats()._triCount);
        } else {
          drw->_meshTechnique = nullptr; // partition not (yet) current: the pull-VS draw stands
          if (mlstate->_lastReason != reason) {
            static const char* kWhy[4] = {"", "no partition published yet",
                                          "topology moved under the partition", "upload refused"};
            logchan_hmml->log("pull-VS this frame: %s (mesh topoV=%llu verts=%d, partition %s)",
                              kWhy[reason],
                              (unsigned long long)mesh->_topoVersion,
                              mesh->_num_verts,
                              part ? "present" : "absent");
          }
        }
        mlstate->_lastReason = reason;
      }
    }
    if (not topo_dirty and not run_graph)
      return;                                      // fully clean: zero GPU work, bindings stand
    if (topo_dirty) {
      tri->ensureIndex(mesh->_num_corners);
      tri->writeParams(mesh->_num_faces, mesh->face("__tags") != nullptr);
      ci->beginDispatchPhase();
      tri->dispatch(mesh);
      ci->endDispatchPhase(); // submit + WAIT
      tri->ackTopo(mesh);
    }
    // ---- refresh the drawable to the mesh's CURRENT pooled buffers (dynamic topology) ----
    size_t n = drw->_graphicsStorage.size();
    for (size_t i = 0; i < n && i < 5; i++) {
      auto ch = mesh->channel(MeshChannel(int(i))); // render order P,N,B,uv,color (see make_drawable)
      if (ch) drw->_graphicsStorage[i].second = ch->_ssbo;
    }
    // E.3: each gid bucket's storage list shares the same first-5-channels contract. LOD TIER buckets
    // (3c) carry their OWN distinct mesh (_indexSSBO set) + static bindings -> skip them (refreshing to
    // the MAIN mesh's channels would corrupt them).
    for (auto& bucket : drw->_bucketDraws) {
      if (bucket._indexSSBO) continue; // LOD tier bucket
      size_t bn = bucket._graphicsStorage.size();
      for (size_t i = 0; i < bn && i < 5; i++) {
        auto ch = mesh->channel(MeshChannel(int(i)));
        if (ch) bucket._graphicsStorage[i].second = ch->_ssbo;
      }
    }
    if (faceViz and n > 5)
      drw->_graphicsStorage[5].second = tri->_triFace; // FS reads TFd[gl_PrimitiveID]; re-pools on growth
    if (tagViz and n > 6) {                            // FS reads STd[ TFd[gl_PrimitiveID] ] (selection groups)
      auto tags = mesh->face("__tags");
      if (tags) drw->_graphicsStorage[6].second = tags->_ssbo; // re-pools on face-count growth
    }
    drw->_argsSSBO  = tri->_args;
    drw->_indexSSBO = tri->_triIndex;
    if (wireframe) {                                   // refresh the wireframe OVERLAY draw (lines on top)
      drw->_overlayArgsSSBO  = tri->_lineArgs;
      drw->_overlayIndexSSBO = tri->_lineIndex;        // re-pools on topology growth -> rebind
      size_t on = drw->_overlayGraphicsStorage.size(); // overlay pull-VS reads P (slot 0) + N (slot 1)
      if (on > 0) { auto ch = mesh->channel(MeshChannel::POSITION); if (ch) drw->_overlayGraphicsStorage[0].second = ch->_ssbo; }
      if (on > 1) { auto ch = mesh->channel(MeshChannel::NORMAL);   if (ch) drw->_overlayGraphicsStorage[1].second = ch->_ssbo; }
    }
    HmPerf::instance().addGraph(
        std::chrono::duration<double>(perfclk::now() - perf_g0).count(),
        ctx->CI()->_gpuWaitAccum - perf_gw0,
        mesh->_num_faces,
        mesh->_num_verts,
        tri->_instCount);
    HmPerf::instance().report();                       // rate-limited: prints once per 5s window
  };
  MeshRenderBuffers out;
  out._faceid   = faceViz ? tri->_triFace : nullptr;
  out._instMtx  = gfx_instMtx;   // E.4: the CULLED buffers when the cull is active
  out._instAttr = gfx_instAttr;
  out._instMtxShadow  = gfx_instMtxShadow;  // cascade-cull fix: shadow OUT_M/OUT_A (null if no cull)
  out._instAttrShadow = gfx_instAttrShadow;
  out._lodTiers = std::move(lodTiers); // Phase 3b: extra LOD tier draws (hm_drawable builds buckets)
  // LOD step #3 — the IMPOSTOR far tier is wired above in the cull tier loop (impostorTiers): bake hook +
  // billboard bucket bound to the band's OUT_M slice. (The ORKID_IMPOSTOR_BAKE env probe is retired.)
  return out;
}

///////////////////////////////////////////////////////////////////////////////
// E.5 gate — hypermeshTriangulationSelfTest: hand-built polygons through the
// REAL triangulator (MeshRenderTri dispatch), index readback, and the AREA
// ORACLE: per-triangle orientation must match the polygon (a folded fan emits
// flipped tris) and the summed triangle area must equal the polygon area (a
// folded fan double-covers). Convex sanity + the concave cases the old fan
// provably broke. Returns the number of FAILED cases.
///////////////////////////////////////////////////////////////////////////////

int hypermeshTriangulationSelfTest(Context* ctx) {
  auto fxi  = ctx->FXI();
  int fails = 0;

  // one polygon in the XY plane (z=0) -> GpuMesh -> triangulate -> readback
  auto run_case = [&](const char* name, const std::vector<fvec2>& poly) -> bool {
    int n = int(poly.size());
    // ---- hand-build the minimal GpuMesh the triangulator consumes ----
    auto mesh = std::make_shared<GpuMesh>(std::make_shared<GpuMeshData>());
    auto mk_chan = [&](int stride, int count) {
      auto ch       = std::make_shared<GpuChannel>();
      ch->_stride   = stride;
      ch->_capacity = count;
      ch->_ssbo     = fxi->createStorageBuffer(size_t(stride) * count);
      return ch;
    };
    auto pch = mk_chan(16, n);
    {
      std::vector<float> P(size_t(n) * 4, 0.0f);
      for (int i = 0; i < n; i++) {
        P[i * 4 + 0] = poly[i].x;
        P[i * 4 + 1] = poly[i].y;
      }
      auto m = fxi->mapStorageBuffer(pch->_ssbo, 0, P.size() * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, P.data(), P.size() * 4);
      fxi->unmapStorageBuffer(m.get());
    }
    mesh->_channels[MeshChannel::POSITION] = pch;
    auto vch = mk_chan(4, n);
    auto fch = mk_chan(4, 2);
    {
      std::vector<uint32_t> vidx(n);
      for (int i = 0; i < n; i++) vidx[i] = uint32_t(i);
      auto m = fxi->mapStorageBuffer(vch->_ssbo, 0, size_t(n) * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, vidx.data(), size_t(n) * 4);
      fxi->unmapStorageBuffer(m.get());
      uint32_t fo[2] = {0u, uint32_t(n)};
      auto m2 = fxi->mapStorageBuffer(fch->_ssbo, 0, 8, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m2->_mappedaddr, fo, 8);
      fxi->unmapStorageBuffer(m2.get());
    }
    mesh->_vidx         = vch;
    mesh->_face_offsets = fch;
    mesh->_num_verts    = n;
    mesh->_num_corners  = n;
    mesh->_num_faces    = 1;

    // ---- the REAL triangulator ----
    auto tri = std::make_shared<MeshRenderTri>();
    tri->build(ctx);
    tri->ensureIndex(n);
    tri->writeParams(1, false);
    auto ci = ctx->CI();
    ci->beginDispatchPhase();
    tri->dispatch(mesh);
    ci->endDispatchPhase();

    // ---- readback: slot-0 command + the index buffer ----
    uint32_t cmd[5];
    {
      auto m = fxi->mapStorageBuffer(tri->_args, 0, 20, BufferMapAccess::READ_ONLY);
      std::memcpy(cmd, m->_mappedaddr, 20);
      fxi->unmapStorageBuffer(m.get());
    }
    int ntris = int(n - 2);
    if (cmd[0] != uint32_t(ntris * 3)) {
      printf("[hm tri-oracle %s] FAIL indexCount<%u> want<%d>\n", name, cmd[0], ntris * 3);
      return false;
    }
    std::vector<uint32_t> idx(size_t(ntris) * 3);
    {
      auto m = fxi->mapStorageBuffer(tri->_triIndex, 0, idx.size() * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(idx.data(), m->_mappedaddr, idx.size() * 4);
      fxi->unmapStorageBuffer(m.get());
    }
    // ---- the AREA ORACLE (XY plane; polygon authored CCW) ----
    auto tri_area2 = [&](uint32_t a, uint32_t b, uint32_t c) -> float { // signed, x2
      fvec2 A = poly[a], B = poly[b], C = poly[c];
      return (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
    };
    float poly_area2 = 0.0f;
    for (int i = 0; i < n; i++) {
      fvec2 A = poly[i], B = poly[(i + 1) % n];
      poly_area2 += A.x * B.y - B.x * A.y;
    }
    float sum2  = 0.0f;
    bool flipped = false;
    for (int t = 0; t < ntris; t++) {
      uint32_t a = idx[t * 3 + 0], b = idx[t * 3 + 1], c = idx[t * 3 + 2];
      if (a >= uint32_t(n) or b >= uint32_t(n) or c >= uint32_t(n)) {
        printf("[hm tri-oracle %s] FAIL out-of-range index\n", name);
        return false;
      }
      float a2 = tri_area2(a, b, c);
      if (a2 <= 0.0f) flipped = true;  // CCW polygon must emit CCW tris only
      sum2 += a2;
    }
    bool area_ok = std::fabs(sum2 - poly_area2) < 1.0e-4f * std::fabs(poly_area2);
    if (flipped or not area_ok) {
      printf("[hm tri-oracle %s] FAIL flipped<%d> area(sum=%.5f poly=%.5f)\n",
             name, int(flipped), sum2 * 0.5f, poly_area2 * 0.5f);
      return false;
    }
    printf("[hm tri-oracle %s] PASS (%d tris, area %.5f)\n", name, ntris, sum2 * 0.5f);
    return true;
  };

  auto P2 = [](std::initializer_list<std::pair<float, float>> pts) {
    std::vector<fvec2> v;
    for (auto& p : pts)
      v.push_back(fvec2(p.first, p.second));
    return v;
  };
  // 1) convex quad (sanity)
  fails += run_case("convex-quad", P2({{0, 0}, {2, 0}, {2, 2}, {0, 2}})) ? 0 : 1;
  // 2) concave (dart) quad — reflex at (1.0, 0.8); a corner-0 fan folds
  fails += run_case("dart-quad", P2({{0, 0}, {2, 0}, {1.0f, 0.8f}, {1.0f, 3.0f}})) ? 0 : 1;
  // 3) L-shaped hexagon — the classic concave case
  fails += run_case("L-hex", P2({{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}})) ? 0 : 1;
  // 4) comb octagon — two notches, multiple reflex vertices
  fails += run_case("comb-oct",
      P2({{0, 0}, {4, 0}, {4, 2}, {3, 2}, {3, 1}, {2, 1}, {2, 2}, {0, 2}})) ? 0 : 1;
  // 5) mirrored L-hex — the notch on the opposite side (reflex at (1,1), other winding
  //    arm layout). Concave-correctness guard across a distinct reflex configuration.
  fails += run_case("L-hex-mirror", P2({{0, 0}, {2, 0}, {2, 2}, {1, 2}, {1, 1}, {0, 1}})) ? 0 : 1;
  // 6) rotated L-hex — same shape carried OFF the integer grid, so the ear-plane
  //    projection is irrational: exercises the winding fix under generic coords.
  {
    auto base = P2({{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}});
    float th = 0.541f, cs = std::cos(th), sn = std::sin(th);
    std::vector<fvec2> rot;
    for (auto& p : base) rot.push_back(fvec2(p.x * cs - p.y * sn, p.x * sn + p.y * cs));
    fails += run_case("L-hex-rot", rot) ? 0 : 1;
  }
  // 7) U-shape — TWO reflex corners (a notch cut into the top of a square).
  fails += run_case("U-shape",
      P2({{0, 0}, {3, 0}, {3, 3}, {2, 3}, {2, 1}, {1, 1}, {1, 3}, {0, 3}})) ? 0 : 1;
  // 8/9) reflex-EXACTLY-on-the-ear-diagonal variants that the pre-fix corner-0 ear
  //      buries (the #52 signature). Non-unit arms + off-origin prove the on-edge
  //      tolerance is scale-relative and translation-invariant, not just tuned to L-hex.
  fails += run_case("L-nonsquare", P2({{0, 0}, {4, 0}, {4, 1}, {2, 1}, {2, 2}, {0, 2}})) ? 0 : 1;
  fails += run_case("L-offset",    P2({{5, 5}, {7, 5}, {7, 6}, {6, 6}, {6, 7}, {5, 7}})) ? 0 : 1;
  return fails;
}

} // namespace ork::lev2::hypermesh
