////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/renderer/compute_drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/ci.h>
#include <ork/lev2/gfx/gbi.h>
#include <ork/lev2/gfx/fx_pipeline.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/reflect/properties/registerX.inl>

namespace ork::lev2 {

ComputeDrawable::ComputeDrawable()
    : CallbackDrawable(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////
// per-FRAME (view-independent, ONCE before any viewport): the live dataflow graph re-eval.
// IN-FRAME, manages its own dispatch phase — NOT a new frame. Also refreshes our
// graphics-storage/args to the mesh's CURRENT channels (dynamic topology re-pools).
///////////////////////////////////////////////////////////////////////////////

void ComputeDrawable::onGpuUpdate(Context* ctx) const {
  if (_liveRecompute)
    _liveRecompute(ctx, const_cast<ComputeDrawable*>(this));
}

///////////////////////////////////////////////////////////////////////////////
// per-VP pre-render: write the camera block, then run the compute passes (one dispatch phase =
// one submit+wait; storageBarrier between passes so each pass sees the previous one's writes).
///////////////////////////////////////////////////////////////////////////////

void ComputeDrawable::onPreRender(Context* ctx, const CameraMatrices& cammtx) const {
  auto FXI = ctx->FXI();
  auto CI  = ctx->CI();

  // CamBlk (std430): mat4 vp; mat4 inv_vp; vec4 eye; vec4 misc; — the cull/gen compute reads this.
  if (_camParamsSSBO) {
    struct CamBlk {
      float vp[16];
      float ivp[16];
      float eye[4];
      float misc[4];
    } blk;
    memcpy(blk.vp, cammtx.GetVPMatrix().asArray(), 64);
    memcpy(blk.ivp, cammtx.GetIVPMatrix().asArray(), 64);
    const float* iv = cammtx.GetIVMatrix().asArray(); // inverse-view translation = eye (col-major)
    blk.eye[0] = iv[12];
    blk.eye[1] = iv[13];
    blk.eye[2] = iv[14];
    blk.eye[3] = 1.0f;
    blk.misc[0] = blk.misc[1] = blk.misc[2] = blk.misc[3] = 0.0f;
    auto m = FXI->mapStorageBuffer(_camParamsSSBO, _camParamsOffset, sizeof(blk), BufferMapAccess::WRITE_ONLY);
    memcpy(m->_mappedaddr, &blk, sizeof(blk));
    m->unmap();
  }

  if (_perViewCompute)  // E.4: e.g. the hypermesh instance cull (own dispatch phase)
    _perViewCompute(ctx, cammtx);

  if (_passes.empty())
    return;
  CI->beginDispatchPhase();
  for (size_t i = 0; i < _passes.size(); i++) {
    const auto& p = _passes[i];
    for (const auto& b : p._bindings)
      CI->bindStorageBufferOnBlock(p._shader, b.second, b.first); // (buffer, block); binding auto-resolved
    CI->dispatchCompute(p._shader, p._groups_x, p._groups_y, p._groups_z);
    if ((i + 1) < _passes.size())
      CI->storageBarrier(); // pass i writes -> pass i+1 reads
  }
  CI->endDispatchPhase(); // submit + WAIT -> results ready for the render pass
}

///////////////////////////////////////////////////////////////////////////////
// render: the material/pipeline binds the vertex SSBO (pipe.bindStorage, set in python) + its
// uniforms; the indirect draw pulls the count from the compute-written args buffer.
///////////////////////////////////////////////////////////////////////////////

void ComputeDrawable::_renderIndirect(RenderContextInstData& RCID) const {
  // explicit _pipeline wins (FreestyleMaterial demos). Otherwise the STANDARD path: flag the RCID as
  // SSBO-sourced and let the material's cache pick its FWD_SSBO_CUSTOM variant (with forward lighting
  // attached the usual way) — no forced technique, no hand-built pipeline.
  fxpipeline_ptr_t pipe = _pipeline;
  if ((not pipe) and _material) {
    RCID._isSSBOSourced = true;
    RCID._isInstanced   = _instanced;   // instanced+ssbo -> FWD_SSBO_CUSTOM_INSTANCED (matrices via graphics storage)
    pipe                = _material->pipelineCache()->findPipeline(RCID);
    // bind our drawable-specific storage (the vertex-source SSBO) onto the cache-built pipeline.
    // Per-frame (not once): there are DISTINCT cached pipes per pass (depth-prepass + color), each
    // needs the binding; bindStorage is an idempotent map insert keyed by block, so this is cheap.
    if (pipe) {
      for (const auto& b : _graphicsStorage)
        pipe->bindStorage(b.first, b.second);
    }
  }
  if (not pipe)
    return;
  auto ctx = RCID.context();
  auto gbi = ctx->GBI();
  pipe->wrappedDrawCall(RCID, [&]() {
    if (_indexSSBO)
      gbi->DrawIndexedIndirectEML(_indexSSBO, _primtype, _argsSSBO, _argsOffset, _indexSize);
    else if (_argsSSBO)
      gbi->DrawIndirectEML(_primtype, _argsSSBO, _argsOffset);
  });

  // E.3 — GID BUCKET DRAWS: same index buffer, per-gid args offset, own material
  // + own storage list (per-shader block handles). Issued right after the main
  // (gid 0 / default-material) draw.
  for (const auto& bucket : _bucketDraws) {
    if (not bucket._material)
      continue;
    RCID._isSSBOSourced = true;
    RCID._isInstanced   = _instanced;
    auto bpipe          = bucket._material->pipelineCache()->findPipeline(RCID);
    if (not bpipe)
      continue;
    for (const auto& b : bucket._graphicsStorage)
      bpipe->bindStorage(b.first, b.second);
    bpipe->wrappedDrawCall(RCID, [&]() {
      gbi->DrawIndexedIndirectEML(_indexSSBO, _primtype, _argsSSBO, bucket._argsOffset, _indexSize);
    });
  }

  // OPTIONAL OVERLAY draw (e.g. wireframe lines over the fill): its own material/pipeline + index/args.
  if ((_overlayMaterial or _overlayPipeline) and _overlayIndexSSBO and _overlayArgsSSBO) {
    fxpipeline_ptr_t opipe = _overlayPipeline;
    if ((not opipe) and _overlayMaterial) {
      RCID._isSSBOSourced = true;
      RCID._isInstanced   = _instanced;   // overlay (wireframe LINES) instances too -> FWD_SSBO_CUSTOM_INSTANCED
      opipe               = _overlayMaterial->pipelineCache()->findPipeline(RCID);
      if (opipe)
        for (const auto& b : _overlayGraphicsStorage)
          opipe->bindStorage(b.first, b.second);
    }
    if (opipe)
      opipe->wrappedDrawCall(RCID, [&]() {
        gbi->DrawIndexedIndirectEML(
            _overlayIndexSSBO, _overlayPrimtype, _overlayArgsSSBO, _overlayArgsOffset, _overlayIndexSize);
      });
  }
}

///////////////////////////////////////////////////////////////////////////////
// ComputeDrawableData
///////////////////////////////////////////////////////////////////////////////

ComputeDrawableData::ComputeDrawableData() {
}

void ComputeDrawableData::addComputePass(
    const FxComputeShader* shader,
    const std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>>& bindings,
    uint32_t gx,
    uint32_t gy,
    uint32_t gz) {
  ComputeDrawablePass pass;
  pass._shader   = shader;
  pass._bindings = bindings;
  pass._groups_x = gx;
  pass._groups_y = gy;
  pass._groups_z = gz;
  _passes.push_back(pass);
}

void ComputeDrawableData::setCameraParams(FxShaderStorageBuffer* ssbo, size_t offset) {
  _camParamsSSBO   = ssbo;
  _camParamsOffset = offset;
}

void ComputeDrawableData::addGraphicsStorage(const FxShaderStorageBlock* block, FxShaderStorageBuffer* ssbo) {
  _graphicsStorage.push_back({block, ssbo});
}

void ComputeDrawableData::setIndirect(
    FxShaderStorageBuffer* args, size_t args_offset, FxShaderStorageBuffer* index, PrimitiveType pt, int index_size) {
  _argsSSBO   = args;
  _argsOffset = args_offset;
  _indexSSBO  = index;
  _primtype   = pt;
  _indexSize  = index_size;
}

void ComputeDrawableData::setOverlayIndirect(
    FxShaderStorageBuffer* args, size_t args_offset, FxShaderStorageBuffer* index, PrimitiveType pt, int index_size) {
  _overlayArgsSSBO   = args;
  _overlayArgsOffset = args_offset;
  _overlayIndexSSBO  = index;
  _overlayPrimtype   = pt;
  _overlayIndexSize  = index_size;
}

void ComputeDrawableData::addOverlayGraphicsStorage(const FxShaderStorageBlock* block, FxShaderStorageBuffer* ssbo) {
  _overlayGraphicsStorage.push_back({block, ssbo});
}

drawable_ptr_t ComputeDrawableData::createDrawable() const {
  auto drw             = std::make_shared<ComputeDrawable>();
  drw->_passes         = _passes;
  drw->_liveRecompute  = _liveRecompute;
  drw->_camParamsSSBO  = _camParamsSSBO;
  drw->_camParamsOffset = _camParamsOffset;
  drw->_material        = _material;
  drw->_pipeline        = _pipeline;
  drw->_graphicsStorage = _graphicsStorage;
  drw->_bucketDraws    = _bucketDraws;
  drw->_perViewCompute = _perViewCompute;
  drw->_argsSSBO       = _argsSSBO;
  drw->_argsOffset     = _argsOffset;
  drw->_indexSSBO      = _indexSSBO;
  drw->_primtype       = _primtype;
  drw->_indexSize      = _indexSize;
  drw->_instanced      = _instanced;
  drw->_overlayMaterial        = _overlayMaterial;
  drw->_overlayPipeline        = _overlayPipeline;
  drw->_overlayGraphicsStorage = _overlayGraphicsStorage;
  drw->_overlayArgsSSBO        = _overlayArgsSSBO;
  drw->_overlayArgsOffset      = _overlayArgsOffset;
  drw->_overlayIndexSSBO       = _overlayIndexSSBO;
  drw->_overlayPrimtype        = _overlayPrimtype;
  drw->_overlayIndexSize       = _overlayIndexSize;
  auto draw_raw        = drw.get();
  drw->setRenderLambda([draw_raw](RenderContextInstData& RCID) { draw_raw->_renderIndirect(RCID); });
  return drw;
}

void ComputeDrawableData::describeX(object::ObjectClass* clazz) {
}

} // namespace ork::lev2

ImplementReflectionX(ork::lev2::ComputeDrawableData, "ComputeDrawableData");
