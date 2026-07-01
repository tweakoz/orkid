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
  // render: wrappedDrawCall(pipeline) -> indirect draw from the compute-written args buffer.
  void _renderIndirect(RenderContextInstData& RCID) const;

  std::vector<ComputeDrawablePass> _passes;
  // E.4 — optional per-VIEW compute hook (runs in onPreRender, BEFORE _passes,
  // with the view's camera matrices): the hypermesh instance cull writes its
  // params + dispatches here (index-bound raw-FXI compute, its own phase).
  std::function<void(Context*, const CameraMatrices&)> _perViewCompute;
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
  // render pipeline selection: if _pipeline is set it is used verbatim (override). Otherwise, if
  // _material is set, the pipeline comes from the STANDARD path — findPipeline(RCID) with
  // RCID._isSSBOSourced=true, so the material's cache picks its FWD_SSBO_CUSTOM variant (+ forward
  // lighting). No forced technique / no hand-built pipeline.
  material_ptr_t _material;
  fxpipeline_ptr_t _pipeline;
  // storage blocks (e.g. the vertex-source SSBO) to bind onto the material-built graphics pipeline(s)
  // after findPipeline (the cache-built pipe doesn't know our drawable-specific buffers). Bound each
  // frame because there are distinct cached pipes per pass (depth-prepass + color).
  std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>> _graphicsStorage;
  FxShaderStorageBuffer* _argsSSBO  = nullptr;     // VkDraw[Indexed]IndirectCommand (compute-written)
  size_t _argsOffset                = 0;           // byte offset of the command within _argsSSBO
  FxShaderStorageBuffer* _indexSSBO = nullptr;     // non-null -> DrawIndexedIndirectEML
  PrimitiveType _primtype           = PrimitiveType::TRIANGLES;
  int _indexSize                    = 4;
  bool _instanced                   = false;       // RCID._isInstanced -> the material's INSTANCED variant
                                                   // (e.g. FWD_SSBO_CUSTOM_INSTANCED); instanceCount rides in the args
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
  void setCameraParams(FxShaderStorageBuffer* ssbo, size_t offset);
  void setIndirect(FxShaderStorageBuffer* args, size_t args_offset, FxShaderStorageBuffer* index, PrimitiveType pt, int index_size);
  // storage to bind onto the material-built graphics pipeline (the vertex-source SSBO for the pull VS)
  void addGraphicsStorage(const FxShaderStorageBlock* block, FxShaderStorageBuffer* ssbo);
  // OPTIONAL overlay (second) draw — see ComputeDrawable. material set via the `overlay_material` property.
  void setOverlayIndirect(FxShaderStorageBuffer* args, size_t args_offset, FxShaderStorageBuffer* index, PrimitiveType pt, int index_size);
  void addOverlayGraphicsStorage(const FxShaderStorageBlock* block, FxShaderStorageBuffer* ssbo);

  std::vector<ComputeDrawablePass> _passes;
  std::function<void(Context*, const CameraMatrices&)> _perViewCompute; // E.4 (see ComputeDrawable)
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
};

using computedrawable_ptr_t     = std::shared_ptr<ComputeDrawable>;
using computedrawabledata_ptr_t = std::shared_ptr<ComputeDrawableData>;

} // namespace ork::lev2
