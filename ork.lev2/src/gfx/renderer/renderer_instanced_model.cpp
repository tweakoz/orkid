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
}
/////////////////////////////////////////////////////////////////////
InstancedModelDrawable::~InstancedModelDrawable() {
}
///////////////////////////////////////////////////////////////////////////////
void InstancedModelDrawable::setNumInstances(size_t count) {
  _instance_worldmatrices.resize(count);
  _instance_miscdata.resize(count);
  _instance_pickids.resize(count);
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
  auto GBI                             = context->GBI();
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
    int inummeshes = _model->numMeshes();
    for (int imesh = 0; imesh < inummeshes; imesh++) {
      auto mesh       = _model->mesh(imesh);
      int inumclusset = mesh->numSubMeshes();
      for (int ics = 0; ics < inumclusset; ics++) {
        auto submesh  = mesh->subMesh(ics);
        auto material = submesh->mpMaterial;
        ///////////////////////////////////////
        // todo : material setup
        //  select instancing and possibly
        //   picking variant
        // set instancing parameter SSBO
        ///////////////////////////////////////
        int inumclus = submesh->_clusters.size();
        for (int ic = 0; ic < inumclus; ic++) {
          auto cluster    = submesh->cluster(ic);
          auto vtxbuf     = cluster->_vertexBuffer;
          size_t numprims = cluster->numPrimGroups();
          for (size_t ipg = 0; ipg < numprims; ipg++) {
            auto primgroup   = cluster->primgroup(ipg);
            auto indexbuffer = primgroup->mpIndices;
            auto primtype    = primgroup->mePrimType;
            int numindices   = primgroup->miNumIndices;
            // OrkAssert(false);
            // todo : implement DrawInstancedPrimitiveEML()
            // GBI->DrawInstancedPrimitiveEML();
          }
        }
      }
    }
  });
  ////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
#if 0
ModelRenderable::ModelRenderable(IRenderer* renderer)
    : IRenderable()
    , mSortKey(0)
    , mMaterialIndex(0)
    , mMaterialPassIndex(0)
    , mScale(1.0f)
    , mEdgeColor(-1)
    , mMesh(0)
    , mSubMesh(0)
    , mRotate(0.0f, 0.0f, 0.0f)
    , mOffset(0.0f, 0.0f, 0.0f) {
  for (int i = 0; i < kMaxEngineParamFloats; i++)
    mEngineParamFloats[i] = 0.0f;
}
///////////////////////////////////////////////////////////////////////////////
void ModelRenderable::SetEngineParamFloat(int idx, float fv) {
  OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);
  mEngineParamFloats[idx] = fv;
}
///////////////////////////////////////////////////////////////////////////////
float ModelRenderable::GetEngineParamFloat(int idx) const {
  OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);
  return mEngineParamFloats[idx];
}
///////////////////////////////////////////////////////////////////////////////
void ModelRenderable::Render(const IRenderer* renderer) const {
  renderer->RenderModel(*this);
}
///////////////////////////////////////////////////////////////////////////////
bool ModelRenderable::CanGroup(const IRenderable* oth) const {
  auto pren = dynamic_cast<const ModelRenderable*>(oth);
  if (pren) {
    const lev2::XgmSubMesh* submesh = pren->subMesh();
    const GfxMaterial* mtl          = submesh->GetMaterial();
    const GfxMaterial* mtl2         = subMesh()->GetMaterial();
    return (mtl == mtl2);
  }
  return false;
}
/////////////////////////////////////////////////////////////////////
void ModelRenderable::SetMaterialIndex(int idx) {
  mMaterialIndex = idx;
}
/////////////////////////////////////////////////////////////////////
void ModelRenderable::SetMaterialPassIndex(int idx) {
  mMaterialPassIndex = idx;
}
/////////////////////////////////////////////////////////////////////
void ModelRenderable::SetModelInst(xgmmodelinst_constptr_t modelInst) {
  _modelinst = modelInst;
}
/////////////////////////////////////////////////////////////////////
void ModelRenderable::SetEdgeColor(int edge_color) {
  mEdgeColor = edge_color;
}
/////////////////////////////////////////////////////////////////////
void ModelRenderable::SetScale(float scale) {
  mScale = scale;
}
/////////////////////////////////////////////////////////////////////
void ModelRenderable::SetSubMesh(const lev2::XgmSubMesh* cs) {
  mSubMesh = cs;
}
/////////////////////////////////////////////////////////////////////
void ModelRenderable::SetMesh(const lev2::XgmMesh* m) {
  mMesh = m;
}
/////////////////////////////////////////////////////////////////////
float ModelRenderable::GetScale() const {
  return mScale;
}
/////////////////////////////////////////////////////////////////////
xgmmodelinst_constptr_t ModelRenderable::GetModelInst() const {
  return _modelinst;
}
/////////////////////////////////////////////////////////////////////
int ModelRenderable::GetMaterialIndex(void) const {
  return mMaterialIndex;
}
/////////////////////////////////////////////////////////////////////
int ModelRenderable::GetMaterialPassIndex(void) const {
  return mMaterialPassIndex;
}
/////////////////////////////////////////////////////////////////////
int ModelRenderable::GetEdgeColor() const {
  return mEdgeColor;
}
/////////////////////////////////////////////////////////////////////
const lev2::XgmSubMesh* ModelRenderable::subMesh(void) const {
  return mSubMesh;
}
/////////////////////////////////////////////////////////////////////
xgmcluster_ptr_t ModelRenderable::GetCluster(void) const {
  return _cluster;
}
/////////////////////////////////////////////////////////////////////
const lev2::XgmMesh* ModelRenderable::mesh(void) const {
  return mMesh;
}
/////////////////////////////////////////////////////////////////////
void ModelRenderable::SetSortKey(uint32_t skey) {
  mSortKey = skey;
}
/////////////////////////////////////////////////////////////////////
void ModelRenderable::SetRotate(const fvec3& v) {
  mRotate = v;
}
/////////////////////////////////////////////////////////////////////
void ModelRenderable::SetOffset(const fvec3& v) {
  mOffset = v;
}
/////////////////////////////////////////////////////////////////////
const fvec3& ModelRenderable::GetRotate() const {
  return mRotate;
}
/////////////////////////////////////////////////////////////////////
const fvec3& ModelRenderable::GetOffset() const {
  return mOffset;
}
/////////////////////////////////////////////////////////////////////
uint32_t ModelRenderable::ComposeSortKey(const IRenderer* renderer) const {
  return mSortKey;
}
#endif
/////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
