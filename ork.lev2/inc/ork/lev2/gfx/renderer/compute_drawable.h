////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/gfxenv_enum.h> // PrimitiveType
#include <ork/lev2/gfx/shadman.h>     // FxComputeShader, FxShaderStorageBuffer
#include <ork/lev2/lev2_types.h>      // fxpipeline_ptr_t
#include <functional>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
// ComputeDrawable — GPU-driven geometry. Generic encapsulation of the cull/gen pattern that
// InstancedRigidPrimitiveDrawable hand-rolls: run 1+ compute passes in onPreRender (per-VP, with
// the camera written into a params SSBO), each pass with its own SSBO bindings + dispatch dims,
// then draw the compute-written buffers INDIRECT (DrawIndirectEML / DrawIndexedIndirectEML).
//
// ALL the custom stuff — SSBOs, compute shaders, the render material/pipeline, dispatch sizing,
// indexed-vs-not — is supplied from Python via ComputeDrawableData. This C++ is generic plumbing.
///////////////////////////////////////////////////////////////////////////////

// one compute pass: a shader + ordered (storage_block, ssbo) list + dispatch group counts.
// The binding INDEX is auto-resolved from the storage block's reflected SPIR-V binding within
// the shader (CI->bindStorageBuffer by-block), so callers never hardcode a merged binding id.
struct ComputeDrawablePass {
  const FxComputeShader* _shader = nullptr;
  std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>> _bindings;
  uint32_t _groups_x = 1;
  uint32_t _groups_y = 1;
  uint32_t _groups_z = 1;
};

struct ComputeDrawable : public CallbackDrawable {
  ComputeDrawable();
  // per-FRAME hook (view-independent; runs ONCE, before any viewport): the live dataflow re-eval
  // (_liveRecompute). Graph compute + triangulate touch no camera, so running them per-VP was
  // pure duplication.
  void onGpuUpdate(lev2::Context* ctx) const override;
  // pre-render hook (per-VP; view-DEPENDENT work only): write the camera block, then run the
  // compute passes in one dispatch phase (storageBarrier between passes — e.g. a frustum cull
  // feeding the indirect draw). See Scene::preRender / SceneGraphViewport::DoRePaintSurface.
  void onPreRender(lev2::Context* ctx, const CameraMatrices& cammtx) const override;
  // cascade-cull fix — per-FRAME sun-shadow cull (Scene::shadowCull, prologue): runs _perViewComputeShadow
  // with the UNION sun camera, compacting a SEPARATE shadow survivor set. wantsShadowCull() is true iff a
  // shadow cull hook is wired — the instanced-cull path and (W7-S3) terrain; a compute drawable with no
  // cull of its own has no hook, keeps the eye set in cascade passes, and is unaffected.
  void onShadowPreRender(lev2::Context* ctx, const CameraMatrices& cammtx) const override;
  // true only when the shadow cull is FULLY wired: the hook, the shadow args, AND the instance-block
  // overrides. A partially-wired path (e.g. a python make_drawable that hasn't passed the shadow handles
  // yet) reports false -> no shadow cull dispatched, cascade passes read the eye set (pre-existing
  // behavior, no wrong pixels). The complete hm_drawable / rigid-primitive paths report true.
  bool wantsShadowCull() const override {
    return bool(_perViewComputeShadow) and (_argsSSBOShadow != nullptr) and not _shadowStorageOverrides.empty();
  }
  // render: wrappedDrawCall(pipeline) -> indirect draw from the compute-written args buffer.
  void _renderIndirect(RenderContextInstData& RCID) const;

  std::vector<ComputeDrawablePass> _passes;
  // HUD-ONLY stats cull passes — used when the DRAW path writes no CHUNK-level VIS header of its own
  // (the taskless mesh-shader terrain path: _passes empty, the per-meshlet cull lives in the mesh
  // stage). onPreRender dispatches these — the SAME reset+cull compute with identical VIS semantics —
  // ONLY when _passes is empty and CullStats is enabled, purely to populate the header the terrain-cull
  // perf HUD reads. Off => skipped (the mesh path stays compute-free); the mesh DRAW never reads them.
  std::vector<ComputeDrawablePass> _statsPasses;
  // E.4 — optional per-VIEW compute hook (runs in onPreRender, BEFORE _passes,
  // with the view's camera matrices): the hypermesh instance cull writes its
  // params + dispatches here (index-bound raw-FXI compute, its own phase).
  std::function<void(Context*, const CameraMatrices&)> _perViewCompute;
  // cascade-cull fix — the SUN-SHADOW variant of _perViewCompute: same cull math, targets the shadow
  // buffer set (its own OUT_M/OUT_A/args), union sun camera, occlusion OFF. Set alongside the shadow
  // buffers below; null -> this drawable doesn't shadow-cull (cascade passes read the eye set).
  std::function<void(Context*, const CameraMatrices&)> _perViewComputeShadow;
  // optional ONE-SHOT in-frame render hook (runs in onPreRender, inside the active graphics frame but
  // BEFORE the main render pass — a legal place for a nested PushRtGroup pre-pass). Used by the impostor
  // bake (renders the hemi-oct atlas once); self-clears after firing (mutable: onPreRender is const).
  mutable std::function<bool(Context*)> _oneShotRender; // returns true when done (cleared); false = retry next frame
  // optional: re-evaluate a live dataflow graph IN-FRAME (its own dispatch phase). Runs in
  // onGpuUpdate (once per frame, ahead of every viewport); used by the hypermesh live path
  // (writeParams + ginst->compute()). It also gets the drawable so it can REFRESH the
  // graphics-storage / args bindings to the mesh's current channels (dynamic-topology ops re-pool
  // their buffers each frame). NEVER opens a frame of its own.
  std::function<void(Context*, ComputeDrawable*)> _liveRecompute;
  FxShaderStorageBuffer* _camParamsSSBO = nullptr; // C++ writes CamBlk{vp,ivp,eye,misc} here each frame
  size_t _camParamsOffset               = 0;
  // HZB 1-phase occlusion (terrain cull): the cull shader's read-only sif_hzb block. When set,
  // onPreRender fetches the per-frame HZB pyramid from the RCFD, packs base w/h/mips into CamBlk.misc
  // .yzw, and binds the HZB SSBO to this block on every pass each frame (a dummy when unavailable).
  const FxShaderStorageBlock* _hzbBlock = nullptr;
  // ... and the GRAPHICS-stage twin, for a producer whose cull lives in a task stage rather than in
  // a compute pass (the grass carpet): same pyramid, same CamBlk.misc.yzw header, but bound onto the
  // material-built pipeline in _renderIndirect instead of onto a dispatch. Either block being set is
  // what arms the per-frame HZB fetch + the misc.yzw pack in onPreRender.
  const FxShaderStorageBlock* _hzbGraphicsBlock = nullptr;
  // render pipeline selection: if _pipeline is set it is used verbatim (override). Otherwise, if
  // _material is set, the pipeline comes from the STANDARD path — findPipeline(RCID) with
  // RCID._isSSBOSourced=true, so the material's cache picks its FWD_SSBO_CUSTOM variant (+ forward
  // lighting). No forced technique / no hand-built pipeline.
  material_ptr_t _material;
  fxpipeline_ptr_t _pipeline;
  // census tag for the bind-time arbiter ([SPVR:CDSEL], drawable_compute.cpp). Every consumer of this
  // drawable reaches the SAME generated-material arms, so the technique name alone cannot say which
  // FAMILY drew — the producer names itself here (terrain / hypermesh / ...); default "compute".
  std::string _spvrFamily = "compute";
  // storage blocks (e.g. the vertex-source SSBO) to bind onto the material-built graphics pipeline(s)
  // after findPipeline (the cache-built pipe doesn't know our drawable-specific buffers). Bound each
  // frame because there are distinct cached pipes per pass (depth-prepass + color).
  std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>> _graphicsStorage;
  FxShaderStorageBuffer* _argsSSBO  = nullptr;     // VkDraw[Indexed]IndirectCommand (compute-written)
  size_t _argsOffset                = 0;           // byte offset of the command within _argsSSBO
  // cascade-cull fix — the SUN-SHADOW args (per-gid indexCounts copied from _argsSSBO, instanceCounts
  // from the shadow cull) + instance-buffer overrides re-bound for sun-cascade depth passes (the
  // storage_inst_mtx/attr blocks re-pointed at the shadow OUT_M/OUT_A). Non-null iff shadow-culling.
  FxShaderStorageBuffer* _argsSSBOShadow = nullptr;
  std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>> _shadowStorageOverrides;
  FxShaderStorageBuffer* _indexSSBO = nullptr;     // non-null -> DrawIndexedIndirectEML
  PrimitiveType _primtype           = PrimitiveType::TRIANGLES;
  int _indexSize                    = 4;
  bool _instanced                   = false;       // RCID._isInstanced -> the material's INSTANCED variant
                                                   // (e.g. FWD_SSBO_CUSTOM_INSTANCED); instanceCount rides in the args
  // TASKLESS MESH-SHADER draw (the terrain A/B path, ORKID_TERRAIN_MESHSHADER). When
  // _meshTechnique is set, the drawable forces that technique on the material pipeline and issues
  // ONE DrawMeshTasksEML over the FIXED workgroup grid _meshGroups instead of the indirect draw —
  // there are no args/index buffers and (for terrain) no compute passes, because each mesh
  // workgroup culls and generates its own meshlet. The caller is responsible for the caps gate
  // (Context::supportsMeshShader) before setting this.
  // INDIRECT mesh draw (ORKID_TERRAIN_MESHSHADER=2): when _meshArgsSSBO is also set, the grid comes
  // from a compute-written VkDrawMeshTasksIndirectCommandEXT{x,y,z} there instead of _meshGroups —
  // a compaction pass writes y=visible-chunk count, so an all-culled view dispatches NOTHING. The
  // caller gates on Context::supportsMeshShaderIndirect before setting it.
  fxtechnique_constptr_t _meshTechnique = nullptr;
  mutable uint32_t _meshGroups[3]       = {1, 1, 1}; // mutable: mode-1 direct-sized rewrites y per frame
  FxShaderStorageBuffer* _meshArgsSSBO  = nullptr;
  size_t _meshArgsOffset                = 0;
  // SUN-SHADOW variants of the mesh draw's grid source (W7-S3), selected in sun-cascade depth passes
  // exactly as _argsSSBOShadow is for the pull path. _meshGroupsShadow is a FIXED full grid — the
  // direct-sized lag-1 count is the EYE view's, and a cascade pass that under-dispatched would drop
  // casters silently (the mesh stage's capacity guard makes the surplus workgroups cheap).
  FxShaderStorageBuffer* _meshArgsSSBOShadow = nullptr;
  uint32_t _meshGroupsShadow[3]              = {1, 1, 1};
  // TERRAIN MODE-1 DIRECT-SIZED mesh dispatch (ORKID_TERRAIN_MESHSHADER=1, improved). The mesh draw is
  // a DIRECT DrawMeshTasksEML over the SAME compacted v_list mode 2 builds (its inline compaction is in
  // _inlineComputePass), but the grid's y (compacted-list capacity) is sized PER FRAME from the LAG-1
  // visible-chunk count — read at the onPreRender frame boundary from the VIS header the inline
  // compaction wrote LAST frame (host-visible map, the established pattern; NEVER a device-local
  // mid-graph readback) — times _meshSizeMargin, floored at _meshSizeFloor, clamped to _meshSizeTotal.
  // Over-dispatched slots read a stale v_list entry, but the mesh stage's capacity guard
  // (gl_WorkGroupID.y >= v_count -> SetMeshOutputsEXT(0,0)) makes them cheap+correct; an UNDER-dispatch
  // (visible grew past the margin in one frame) is detected next frame (count > last capacity) and
  // recovered with a full-grid dispatch, logged once. Distinct from _meshArgsSSBO (mode 2 indirect) —
  // this path NEVER issues DrawMeshTasksIndirectEML (its MoltenVK emulation is the ~32ms trap).
  bool _meshDirectSized     = false;
  float _meshSizeMargin     = 1.5f;  // A8: env-tweakable over-dispatch factor (ORKID_TERRAIN_MESHSIZE_MARGIN)
  uint32_t _meshSizeMps2    = 1;     // grid x = meshlets per chunk
  uint32_t _meshSizeTotal   = 1;     // full chunk count (grid-y clamp cap / fallback)
  uint32_t _meshSizeFloor   = 64;    // A8: minimum capacity (ORKID_TERRAIN_MESHSIZE_FLOOR)
  bool _meshCullChunk       = false; // granularity the material was requested with (log-only; ORKID_TERRAIN_MESHCULL)
  mutable uint32_t _meshLastCapacity = 0;     // last frame's dispatched y capacity (state)
  mutable bool     _meshHasHistory   = false; // false => first frame => full grid
  // OPTIONAL OVERLAY draw: a second indirect primitive drawn right after the main one (e.g. a wireframe
  // line pass over the filled mesh) with its own material + index/args + graphics-storage + primtype.
  // Inert unless _overlayMaterial (or _overlayPipeline) AND _overlayIndexSSBO AND _overlayArgsSSBO are set.
  material_ptr_t _overlayMaterial;
  fxpipeline_ptr_t _overlayPipeline;
  std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>> _overlayGraphicsStorage;
  FxShaderStorageBuffer* _overlayArgsSSBO  = nullptr;
  size_t _overlayArgsOffset                = 0;
  FxShaderStorageBuffer* _overlayIndexSSBO = nullptr;
  PrimitiveType _overlayPrimtype           = PrimitiveType::LINES;
  int _overlayIndexSize                    = 4;

  // E.3 — GID BUCKET DRAWS: additional indexed-indirect draws from the SAME
  // index buffer (per-gid contiguous ranges; the triangulator wrote one
  // VkDrawIndexedIndirectCommand per gid slot into _argsSSBO), each with its
  // OWN material + args byte-offset + graphics-storage list (storage BLOCK
  // handles are per-shader — each material's generated fxv2 has its own, so
  // buckets can't reuse the main draw's list). The live refresh hook updates
  // each bucket's first 5 entries (the vertex channels) BY INDEX, same
  // contract as _graphicsStorage.
  struct BucketDraw {
    material_ptr_t _material;
    size_t _argsOffset = 0;
    std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>> _graphicsStorage;
    // cascade-cull fix — instance-buffer overrides (this bucket material's storage_inst_mtx/attr blocks
    // re-pointed at the shadow OUT_M/OUT_A) applied in sun-cascade depth passes. Populated ONLY for GID
    // buckets (which share the drawable's args + tier-0 instances); LOD/impostor buckets leave this empty
    // and are SKIPPED in shadow passes (the full tier-0 mesh casts the shadow).
    std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>> _shadowStorageOverrides;
    // LOD TIER overrides (Phase 3b): when _indexSSBO/_argsSSBO are set this bucket draws its OWN
    // mesh (a distance LOD tier) rather than the main mesh — distinct topology -> distinct index +
    // args. _instByteOffset binds the SHARED instance buffer (the cull's interleaved OUT_M / attrs)
    // at this tier's byte offset via the sub-range storage bind, so the VS reads its slice from
    // gl_InstanceIndex==0 (NO baseInstance). _instBlocks name the storage_inst_mtx/attr blocks of
    // THIS bucket's material to re-bind at the offset (block handles are per-shader). 0/null = inherit.
    FxShaderStorageBuffer* _indexSSBO = nullptr; // null -> drawable's _indexSSBO
    FxShaderStorageBuffer* _argsSSBO  = nullptr; // null -> drawable's _argsSSBO
    size_t _instByteOffset            = 0;       // OUT_M/attrs sub-range offset (0 = whole / tier 0)
    const FxShaderStorageBlock* _instMtxBlock  = nullptr;
    FxShaderStorageBuffer*      _instMtxBuf     = nullptr;
    const FxShaderStorageBlock* _instAttrBlock = nullptr;
    FxShaderStorageBuffer*      _instAttrBuf    = nullptr;
    // explicit pipeline override (impostor billboard: a forced-technique FreestyleMaterial pipeline,
    // not the material's RCID-selected one). When set, used verbatim instead of findPipeline.
    fxpipeline_ptr_t _pipeline;
    // LOD impostor: draw through the material's NORMAL forward pipeline but with the _isImpostor RCID flag
    // set, so findPipeline selects FWD_SSBO_CUSTOM_IMPOSTOR (billboard VS + atlas surface) and the material
    // binds the SAME forward lighting as the mesh tiers (color-matched).
    bool _isImpostor = false;
    // PER-VARIANT impostor atlas + params, bound PER-DRAW (the base PBRMaterial is SHARED across all variants,
    // so binding the atlas onto the material collapses every impostor to the last-baked one). The param
    // handles come from the (shared) material; the textures/values are this variant's own.
    texture_ptr_t _impAtlasAlbedo;
    texture_ptr_t _impAtlasNormal;
    texture_ptr_t _impAtlasMetalRough;
    fvec4 _impCenter = fvec4(0, 0, 0, 1); // xyz = object-space bbox center, w = bound radius
    fvec4 _impGrid   = fvec4(8, 0, 0, 0); // x = grid N, y = max draw distance
    fxparam_constptr_t _parImpAlbedo     = nullptr;
    fxparam_constptr_t _parImpNormal     = nullptr;
    fxparam_constptr_t _parImpMetalRough = nullptr;
    fxparam_constptr_t _parImpCenter     = nullptr;
    fxparam_constptr_t _parImpGrid       = nullptr;
  };
  std::vector<BucketDraw> _bucketDraws;
  // INLINE compaction pass (terrain mode 2 / ORKID_TERRAIN_MESHSHADER=2): a single compute pass
  // recorded DIRECTLY onto the frame's primary command buffer in onPreRender (dispatch + a
  // producer->consumer barrier), so it rides the ONE frame submit instead of the _passes dispatch
  // phase's separate compute submit+fence-wait — the whole per-frame cost on MoltenVK when the
  // compaction feeds an indirect mesh draw. _shader==nullptr => unset (the common case; _passes is
  // used instead). Mutually exclusive with putting the same pass in _passes (never both — one
  // dispatch). The VIS header this compaction writes still funnels the CullStats HUD at 1-frame lag.
  // Placed LAST so this new member never shifts a pre-existing member's offset — a partial/stale
  // incremental rebuild (a TU compiled against the old header) still reads every prior field
  // correctly instead of faulting the GPU on a mis-offset _material/_argsSSBO.
  ComputeDrawablePass _inlineComputePass;
};

struct ComputeDrawableData : public DrawableData {
  DeclareConcreteX(ComputeDrawableData, DrawableData);
  ComputeDrawableData();
  drawable_ptr_t createDrawable() const final;
  bool isSharedDrawable() const final { return true; } // one buffer set, like instanced

  // python builders
  void addComputePass(
      const FxComputeShader* shader,
      const std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>>& bindings,
      uint32_t gx,
      uint32_t gy,
      uint32_t gz);
  // HUD-ONLY stats cull pass (see ComputeDrawable::_statsPasses) — same signature as addComputePass.
  void addStatsComputePass(
      const FxComputeShader* shader,
      const std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>>& bindings,
      uint32_t gx,
      uint32_t gy,
      uint32_t gz);
  // INLINE compaction pass (see ComputeDrawable::_inlineComputePass) — same signature; the pass is
  // recorded onto the frame CB in onPreRender rather than dispatched in the _passes phase.
  void setInlineCompute(
      const FxComputeShader* shader,
      const std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>>& bindings,
      uint32_t gx,
      uint32_t gy,
      uint32_t gz);
  // DIRECT mesh-shader draw: force `technique` and issue ONE DrawMeshTasksEML over the fixed
  //  {gx,gy,gz} grid instead of an indirect draw. When the technique's pass fronts its mesh
  //  stage with a TASK stage, those counts are TASK workgroups and each one emits its own
  //  mesh grid. Caller gates on Context::supportsMeshShader (and supportsTaskShader).
  void setMeshDraw(fxtechnique_constptr_t technique, uint32_t gx, uint32_t gy, uint32_t gz);
  void setCameraParams(FxShaderStorageBuffer* ssbo, size_t offset);
  void setIndirect(FxShaderStorageBuffer* args, size_t args_offset, FxShaderStorageBuffer* index, PrimitiveType pt, int index_size);
  // storage to bind onto the material-built graphics pipeline (the vertex-source SSBO for the pull VS)
  void addGraphicsStorage(const FxShaderStorageBlock* block, FxShaderStorageBuffer* ssbo);
  // OPTIONAL overlay (second) draw — see ComputeDrawable. material set via the `overlay_material` property.
  void setOverlayIndirect(FxShaderStorageBuffer* args, size_t args_offset, FxShaderStorageBuffer* index, PrimitiveType pt, int index_size);
  void addOverlayGraphicsStorage(const FxShaderStorageBlock* block, FxShaderStorageBuffer* ssbo);

  std::vector<ComputeDrawablePass> _passes;
  std::vector<ComputeDrawablePass> _statsPasses; // HUD-only stats cull (see ComputeDrawable::_statsPasses)
  std::function<void(Context*, const CameraMatrices&)> _perViewCompute; // E.4 (see ComputeDrawable)
  std::function<void(Context*, const CameraMatrices&)> _perViewComputeShadow; // cascade-cull fix (see ComputeDrawable)
  FxShaderStorageBuffer* _argsSSBOShadow = nullptr;                     // cascade-cull fix
  std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>> _shadowStorageOverrides; // cascade-cull fix
  std::function<bool(Context*)> _oneShotRender;                         // A2 impostor bake (see ComputeDrawable)
  std::function<void(Context*, ComputeDrawable*)> _liveRecompute; // in-frame live re-eval (see ComputeDrawable)
  FxShaderStorageBuffer* _camParamsSSBO = nullptr;
  size_t _camParamsOffset               = 0;
  material_ptr_t _material;          // standard findPipeline(RCID, _isSSBOSourced) selects FWD_SSBO_CUSTOM...
  fxpipeline_ptr_t _pipeline;        // ...unless _pipeline is set (then it overrides)
  std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>> _graphicsStorage;
  FxShaderStorageBuffer* _argsSSBO  = nullptr;
  size_t _argsOffset                = 0;
  FxShaderStorageBuffer* _indexSSBO = nullptr;
  PrimitiveType _primtype           = PrimitiveType::TRIANGLES;
  int _indexSize                    = 4;
  fxtechnique_constptr_t _meshTechnique = nullptr;   // -> ComputeDrawable._meshTechnique (see setMeshDraw)
  uint32_t _meshGroups[3]               = {1, 1, 1};
  bool _instanced                   = false;   // -> ComputeDrawable._instanced (selects the INSTANCED variant)
  material_ptr_t _overlayMaterial;
  fxpipeline_ptr_t _overlayPipeline;
  std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>> _overlayGraphicsStorage;
  FxShaderStorageBuffer* _overlayArgsSSBO  = nullptr;
  size_t _overlayArgsOffset                = 0;
  FxShaderStorageBuffer* _overlayIndexSSBO = nullptr;
  PrimitiveType _overlayPrimtype           = PrimitiveType::LINES;
  int _overlayIndexSize                    = 4;
  // E.3 — gid bucket draws (copied onto the ComputeDrawable; see its decl)
  std::vector<ComputeDrawable::BucketDraw> _bucketDraws;
  ComputeDrawablePass _inlineComputePass; // inline frame-CB compaction; LAST (ABI-stable — see ComputeDrawable)
};

using computedrawable_ptr_t     = std::shared_ptr<ComputeDrawable>;
using computedrawabledata_ptr_t = std::shared_ptr<ComputeDrawableData>;

} // namespace ork::lev2
