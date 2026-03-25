////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/fx_pipeline.h>
#include <ork/lev2/gfx/shadman.h>

#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/registerX.inl>
#include <ork/profiling.inl>
#include "drawable_instanced_impl.inl"

ImplementReflectionX(ork::lev2::InstancedModelDrawableData, "InstancedModelDrawableData");

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void InstancedModelDrawableData::describeX(object::ObjectClass* clazz){
  clazz->directProperty("assetpath", &InstancedModelDrawableData::_assetpath);
  clazz->directMapProperty("assetvars", &InstancedModelDrawableData::_assetvars)
      ->annotate("editor.visible", ConstString("false"));
}

InstancedModelDrawableData::InstancedModelDrawableData(AssetPath path) : _assetpath(path) {
}
///////////////////////////////////////////////////////////////////////////////
drawable_ptr_t InstancedModelDrawableData::createDrawable() const {
  auto drw = std::make_shared<InstancedModelDrawable>();
  drw->_data = this;
  drw->bindModelAsset(_assetpath);
  drw->_modcolor = _modcolor;
  drw->resize(_maxinstances);
  return drw;
}
///////////////////////////////////////////////////////////////////////////////
void InstancedModelDrawableData::reloadDrawable(drawable_ptr_t drw) const {
  auto inst_drw = std::dynamic_pointer_cast<InstancedModelDrawable>(drw);
  if (inst_drw) {
    inst_drw->bindModelAsset(_assetpath);
  }
}
///////////////////////////////////////////////////////////////////////////////
InstancedModelDrawable::InstancedModelDrawable()
    : InstancedDrawable() {
}
/////////////////////////////////////////////////////////////////////
InstancedModelDrawable::~InstancedModelDrawable() {
}
///////////////////////////////////////////////////////////////////////////////
struct IMDIMPL_SUBMESH {
  xgmsubmesh_constptr_t _xgmsubmesh = nullptr;
  fxpipelinecache_constptr_t _fxcache;
};
struct IMDIMPL_MODEL {
  std::vector<IMDIMPL_SUBMESH> _submeshes;
};
using imdimpl_xgmmodel_ptr_t = std::shared_ptr<IMDIMPL_MODEL>;

///////////////////////////////////////////////////////////////////////////////
void InstancedModelDrawable::bindModelAsset(AssetPath assetpath) {
  auto load_req = std::make_shared<asset::LoadRequest>(assetpath);
  _asset = asset::AssetManager<XgmModelAsset>::load(load_req);
  bindModel(_asset->_model.atomicCopy());
}
///////////////////////////////////////////////////////////////////////////////
void InstancedModelDrawable::bindModel(xgmmodel_ptr_t model) {
  _model = model;
  // generate material instance data
  auto impl = std::make_shared<IMDIMPL_MODEL>();
  _impl.set<imdimpl_xgmmodel_ptr_t>(impl);
  int inummeshes = _model->numMeshes();
  for (int imesh = 0; imesh < inummeshes; imesh++) {
    auto mesh       = _model->mesh(imesh);
    int inumclusset = mesh->numSubMeshes();
    for (int ics = 0; ics < inumclusset; ics++) {
      auto xgmsub = mesh->subMesh(ics);
      IMDIMPL_SUBMESH submesh_impl;
      submesh_impl._fxcache = xgmsub->_material->pipelineCache();
      submesh_impl._xgmsubmesh = xgmsub;
      impl->_submeshes.push_back(submesh_impl);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void InstancedModelDrawable::gpuInit(Context* ctx) const {
  auto FXI = ctx->FXI();
  _instanceSSBO = FXI->createStorageBuffer(k_ssbo_total_size);
}
///////////////////////////////////////////////////////////////////////////////
void InstancedModelDrawable::enqueueToRenderQueue(
    drawqueueitem_constptr_t dbufitem, //
    lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  ////////////////////////////////////////////////////////////////////
  if (not _model)
    return;
  if (not _impl.isA<imdimpl_xgmmodel_ptr_t>())
    return;
  ////////////////////////////////////////////////////////////////////
  auto context                         = renderer->GetTarget();
  auto RCFD                            = context->topRenderContextFrameData();
  const auto& topCPD                   = RCFD->topCPD();
  const auto& monofrustum              = topCPD.monoCamFrustum();
  lev2::CallbackRenderable& renderable = renderer->enqueueCallback();
  ////////////////////////////////////////////////////////////////////
  bool isPick    = context->FBI()->isPickState();
  bool isSkinned = _model->isSkinned();
  if (not _instanceSSBO) {
    gpuInit(context); // todo figure out better do-only-once method...
  }
  ////////////////////////////////////////////////////////////////////
  renderable._pickID = _pickID;
  renderable.SetSortKey(0x00000001);
  renderable.SetDrawableDataA(GetUserDataA());
  renderable.SetDrawableDataB(GetUserDataB());
  renderable._instanced = true;
  //printf( "dbufitem _serialno<%d>\n", dbufitem->_serialno );
    //auto it = dbufitem->_usermap.find("rtthread_instance_data"_crcu);
    //OrkAssert(it!=dbufitem->_usermap.end());

  ////////////////////////////////////////////////////////////////////
  renderable.SetRenderCallback([this,dbufitem](lev2::RenderContextInstData& RCID) { //
    EASY_BLOCK("gfxmodel::RINST1", profiler::colors::Red);
    auto context     = RCID.context();
    auto GBI         = context->GBI();
    auto FXI         = context->FXI();
    auto FBI         = context->FBI();
    auto impl        = _impl.getShared<IMDIMPL_MODEL>();
    bool isPick      = FBI->isPickState();
    bool isStereo    = RCID.rcfd()->isStereo();
    int pipeline_index = isStereo ? 1 : (isPick ? 2 : 0);
    ////////////////////////////////////////////////////////
    OrkAssert(_count <= k_max_instances);
    auto instances_copy = _idbuf_pool.begin_pull();
    ////////////////////////////////////////////////////////
    // upload instance data to SSBO
    ////////////////////////////////////////////////////////
    auto ssbo_mapped = FXI->mapStorageBuffer(_instanceSSBO, 0, k_ssbo_total_size, BufferMapAccess::WRITE_ONLY);
    char* base_ptr = (char*)ssbo_mapped->_mappedaddr;
    // copy matrices
    memcpy(base_ptr + k_ssbo_offset_matrices, instances_copy->_worldmatrices.data(), _count * 64);
    // copy colors
    memcpy(base_ptr + k_ssbo_offset_colors, instances_copy->_modcolors.data(), _count * 16);
    // copy pickids (convert from uint64_t to uvec2 - same layout)
    memcpy(base_ptr + k_ssbo_offset_pickids, instances_copy->_pickids.data(), _count * 8);
    ssbo_mapped->unmap();
    EASY_END_BLOCK;
    ////////////////////////////////////////////////////////
    // release pulled instance data
    ////////////////////////////////////////////////////////
    _idbuf_pool.end_pull(instances_copy);
    ////////////////////////////////////////////////////////
    // instanced render
    ////////////////////////////////////////////////////////
    EASY_BLOCK("gfxmodel::RINST2", profiler::colors::Red);
    RCID._isInstanced = true;
    for (auto& sub : impl->_submeshes) {
      auto xgmsub = sub._xgmsubmesh;
      auto fxlut = sub._fxcache;
      OrkAssert(fxlut);
      auto pipeline = fxlut->findPipeline(RCID);
      OrkAssert(pipeline);
      pipeline->wrappedDrawCall(RCID, [&]() {
        ////////////////////////////////////
        // bind instance SSBO
        ////////////////////////////////////
        if (pipeline->_parInstanceBlock) {
          FXI->bindStorageBuffer(pipeline->_parInstanceBlock, _instanceSSBO);
        }
        ////////////////////////////////////
        int inumclus = xgmsub->_clusters.size();
        for (int ic = 0; ic < inumclus; ic++) {
          auto cluster    = xgmsub->cluster(ic);
          auto vtxbuf     = cluster->_vertexBuffer;
          size_t numprims = cluster->numPrimGroups();
          for (size_t ipg = 0; ipg < numprims; ipg++) {
            auto primgroup = cluster->primgroup(ipg);
            auto idxbuf    = primgroup->mpIndices;
            auto primtype  = primgroup->mePrimType;
            int numindices = primgroup->miNumIndices;
            GBI->DrawInstancedIndexedPrimitiveEML(*vtxbuf, *idxbuf, primtype, _count);
          }
        }
      }); // mtlinst->wrappedDrawCall(RCID, [&]() {
    }     // for (auto& sub : impl._submeshes) {
    RCID._isInstanced = false;
  });     // renderable.SetRenderCallback
  ////////////////////////////////////////////////////////////////////
} // InstancedModelDrawable::enqueueToRenderQueue(

/////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
