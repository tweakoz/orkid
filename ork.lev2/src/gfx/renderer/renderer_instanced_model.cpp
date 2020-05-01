////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
InstancedModelDrawable::InstancedModelDrawable(DrawableOwner* pent)
    : Drawable() {
  _instancedata = std::make_shared<InstancedDrawableData>();
}
/////////////////////////////////////////////////////////////////////
InstancedModelDrawable::~InstancedModelDrawable() {
}
///////////////////////////////////////////////////////////////////////////////
void InstancedModelDrawable::resize(size_t count) {
  OrkAssert(count <= k_max_instances);
  _instancedata->resize(count);
  _count = count;
}
///////////////////////////////////////////////////////////////////////////////
void InstancedModelDrawable::enqueueToRenderQueue(
    const DrawableBufItem& item, //
    lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  auto context                         = renderer->GetTarget();
  auto RCFD                            = context->topRenderContextFrameData();
  const auto& topCPD                   = RCFD->topCPD();
  const auto& monofrustum              = topCPD.monoCamFrustum();
  lev2::CallbackRenderable& renderable = renderer->enqueueCallback();
  ////////////////////////////////////////////////////////////////////
  if (not _model)
    return;
  ////////////////////////////////////////////////////////////////////
  bool isSkinned   = _model->isSkinned();
  bool isPickState = context->FBI()->isPickState();
  OrkAssert(false == isSkinned); // not yet..
  ////////////////////////////////////////////////////////////////////
  renderable.SetObject(GetOwner());
  renderable.SetSortKey(0x00000001);
  renderable.SetDrawableDataA(GetUserDataA());
  renderable.SetDrawableDataB(GetUserDataB());
  renderable.SetUserData0(item.mUserData0);
  renderable.SetUserData1(item.mUserData1);
  ////////////////////////////////////////////////////////////////////
  renderable.SetRenderCallback([this](lev2::RenderContextInstData& RCID) { //
    auto context   = RCID.context();
    auto GBI       = context->GBI();
    int inummeshes = _model->numMeshes();
    for (int imesh = 0; imesh < inummeshes; imesh++) {
      auto mesh       = _model->mesh(imesh);
      int inumclusset = mesh->numSubMeshes();
      for (int ics = 0; ics < inumclusset; ics++) {
        auto submesh = mesh->subMesh(ics);
        auto mtlinst = submesh->_materialinst;
        OrkAssert(mtlinst);
        mtlinst->wrappedDrawCall(RCID, [&]() {
          int inumclus = submesh->_clusters.size();
          for (int ic = 0; ic < inumclus; ic++) {
            auto cluster    = submesh->cluster(ic);
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
        });
      }
    }
  });
  ////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
/////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
