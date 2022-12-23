////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/prop.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/material_freestyle.h>

template class ork::orklut<ork::PoolString, ork::lev2::XgmMesh*>;
int eggtestcount = 0;
namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

XgmModel::XgmModel()
    : miBonesPerCluster(0)
    , mpUserData(0)
    , miNumMaterials(0)
    , mbSkinned(false) {
}

XgmModel::~XgmModel() {
  for (orklut<PoolString, XgmMesh*>::iterator it = mMeshes.begin(); it != mMeshes.end(); it++)
    delete it->second;
}

///////////////////////////////////////////////////////////////////////////////

int XgmModel::meshIndex(const PoolString& name) const {
  orklut<PoolString, XgmMesh*>::const_iterator it = mMeshes.find(name);

  return (it == mMeshes.end()) ? -1 : int(it - mMeshes.begin());
}

///////////////////////////////////////////////////////////////////////////////

XgmModelInst::XgmModelInst(const XgmModel* Model)
    : mXgmModel(Model)
    , _localPose(Model->skeleton())
    , _worldPose(Model->skeleton())
    , mMaterialStateInst(*this)
    , mbSkinned(false)
    , mBlenderZup(false)
    , _drawSkeleton(true) {

  OrkAssert(Model != 0);
  miNumChannels = Model->skeleton().numJoints();
  if (miNumChannels == 0) {
    miNumChannels = 1;
  }

  _localPose.bindPose();
  _localPose.blendPoses();
  _localPose.concatenate();
  _worldPose.apply(fmtx4(), _localPose);

  int nummeshes = Model->numMeshes();
  for (int i = 0; i < nummeshes; i++) {
    auto mesh        = Model->mesh(i);
    int numsubmeshes = mesh->numSubMeshes();
    for (int j = 0; j < numsubmeshes; j++) {
      auto submesh = mesh->subMesh(j);
      auto smi     = std::make_shared<XgmSubMeshInst>(submesh);
      _submeshinsts.push_back(smi);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

XgmSubMeshInst::XgmSubMeshInst(const XgmSubMesh* submesh)
    : _submesh(submesh)
    , _enabled(true) {

  _fxinstancecache = submesh->_material->fxInstanceCache();
  OrkAssert(_fxinstancecache);
}

///////////////////////////////////////////////////////////////////////////////

XgmModelInst::~XgmModelInst() {
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::enableMesh(const PoolString& ps) {
  auto mesh = mXgmModel->mesh(ps);
  enableMesh(mesh);
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::disableMesh(const PoolString& ps) {
  auto mesh = mXgmModel->mesh(ps);
  disableMesh(mesh);
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::enableMesh(const XgmMesh* mesh) {
  for (auto submeshinst : _submeshinsts) {
    auto item_mesh = submeshinst->_submesh->_parentmesh;
    if (item_mesh == mesh)
      submeshinst->_enabled = true;
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::disableMesh(const XgmMesh* mesh) {
  for (auto submeshinst : _submeshinsts) {
    auto item_mesh = submeshinst->_submesh->_parentmesh;
    if (item_mesh == mesh)
      submeshinst->_enabled = false;
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::enableAllMeshes() {
  for (auto submeshinst : _submeshinsts) {
    submeshinst->_enabled = true;
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::disableAllMeshes() {
  for (auto submeshinst : _submeshinsts) {
    submeshinst->_enabled = false;
  }
}

bool XgmModelInst::isAnyMeshEnabled() {
  for (auto submeshinst : _submeshinsts) {
    if (submeshinst->_enabled)
      return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////

XgmPrimGroup::~XgmPrimGroup() {
  if (mpIndices) {
    delete mpIndices;
  }
}

///////////////////////////////////////////////////////////////////////////////

XgmPrimGroup::XgmPrimGroup()
    : miNumIndices(0)
    , mpIndices(0)
    , mePrimType(PrimitiveType::NONE) {
}

///////////////////////////////////////////////////////////////////////////////

XgmPrimGroup::XgmPrimGroup(XgmPrimGroup* pgrp)
    : miNumIndices(pgrp->miNumIndices)
    , mpIndices(0)
    , mePrimType(pgrp->mePrimType) {
  if (miNumIndices) {
    mpIndices = new StaticIndexBuffer<U16>(miNumIndices);
    mpIndices->SetHandle(pgrp->mpIndices->GetHandle());
  }
}

///////////////////////////////////////////////////////////////////////////////

XgmCluster::XgmCluster()
    : mBoundingSphere(fvec3::zero(), 0.0f) {
}

XgmCluster::~XgmCluster() {
}

///////////////////////////////////////////////////////////////////////////////

XgmSubMesh::~XgmSubMesh() {
}

///////////////////////////////////////////////////////////////////////////////

void XgmCluster::Dump(void) {
}

///////////////////////////////////////////////////////////////////////////////

XgmMesh::XgmMesh(XgmMesh* pMesh) {
  mvBoundingBoxMin  = pMesh->RefBoundingBoxMin();
  mvBoundingBoxMax  = pMesh->RefBoundingBoxMax();
  muFlags           = pMesh->muFlags;
  miNumBoneBindings = pMesh->miNumBoneBindings;
  mfBoundingRadius  = pMesh->mfBoundingRadius;
  mvBoundingCenter  = pMesh->mvBoundingCenter;

  int inumsubmeshes = pMesh->numSubMeshes();
  mSubMeshes.reserve(inumsubmeshes);

  for (int i = 0; i < inumsubmeshes; i++) {
    XgmSubMesh* psrcmesh  = pMesh->subMesh(i);
    XgmSubMesh* pdestmesh = subMesh(i);
    new (pdestmesh) XgmSubMesh(*psrcmesh);
  }
}

///////////////////////////////////////////////////////////////////////////////

XgmMesh::XgmMesh()
    : muFlags(0)
    , miNumBoneBindings(0)
    , mfBoundingRadius(0.0f)
    , mvBoundingCenter(0.0f, 0.0f, 0.0f) {
}

XgmMesh::~XgmMesh() {
  for (orkvector<XgmSubMesh*>::iterator it = mSubMeshes.begin(); it != mSubMeshes.end(); it++)
    delete (*it);
}

///////////////////////////////////////////////////////////////////////////////

void XgmModel::AddMaterial(material_ptr_t Mat) {
  if (false == OldStlSchoolIsItemInVector(mvMaterials, Mat)) {
    mvMaterials.push_back(Mat);
  }
  miNumMaterials = int(mvMaterials.size());
}

///////////////////////////////////////////////////////////////////////////////

void XgmModel::RenderRigid(
    const fcolor4& ModColor,
    const fmtx4& WorldMat,
    ork::lev2::Context* pTARG,
    const RenderContextInstData& RCID,
    const RenderContextInstModelData& mdlctx) const {

  auto R                       = RCID.GetRenderer();
  auto RCFD                    = pTARG->topRenderContextFrameData();
  const auto& CPD              = RCFD->topCPD();
  bool stereo1pass             = CPD.isStereoOnePass();
  const XgmMesh& XgmMesh       = *mdlctx.mMesh;
  auto cluster                 = mdlctx._cluster;
  const XgmSubMesh& XgmClusSet = *mdlctx.mSubMesh;
  int inummesh                 = numMeshes();
  int inumclusset              = XgmMesh.numSubMeshes();

  auto fxcache = RCID._fx_instance_cache;
  OrkAssert(fxcache);
  auto fxinst = fxcache->findfxinst(RCID);
  OrkAssert(fxinst);

  pTARG->debugPushGroup(
      FormatString("XgmModel::RenderRigid stereo1pass<%d> inummesh<%d> inumclusset<%d>", int(stereo1pass), inummesh, inumclusset));

  pTARG->MTXI()->SetMMatrix(WorldMat);
  pTARG->PushModColor(ModColor);
  {
    //////////////////////////////////////////////
    // fxinst wrapped draw call
    //////////////////////////////////////////////
    fxinst->wrappedDrawCall(RCID, [&]() {
      auto vtxbuffer = cluster->_vertexBuffer;
      int inumprim   = cluster->numPrimGroups();
      for (int iprim = 0; iprim < inumprim; iprim++) {
        auto primgroup = cluster->primgroup(iprim);
        auto idxbuffer = primgroup->GetIndexBuffer();
        pTARG->GBI()->DrawIndexedPrimitiveEML(*vtxbuffer, *idxbuffer, primgroup->GetPrimType());
      }
    });
    //////////////////////////////////////////////
  }
  pTARG->PopModColor();
  pTARG->debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////

static const int kMatrixBlockSize = 32;
static fmtx4 MatrixBlock[kMatrixBlockSize];
int eggtest = 0;

void XgmModel::RenderSkinned(
    const XgmModelInst* minst,
    const fcolor4& ModColor,
    const fmtx4& WorldMat,
    ork::lev2::Context* pTARG,
    const RenderContextInstData& RCID,
    const RenderContextInstModelData& mdlctx) const {

  auto fxcache = RCID._fx_instance_cache;
  auto fxinst  = fxcache->findfxinst(RCID);
  auto pmat    = fxinst->_material;

  auto R           = RCID.GetRenderer();
  auto RCFD        = pTARG->topRenderContextFrameData();
  const auto& CPD  = RCFD->topCPD();
  bool stereo1pass = CPD.isStereoOnePass();

  ///////////////////////////////////
  // apply local pose to world pose
  ///////////////////////////////////

  const auto& localpose = minst->_localPose;
  minst->_worldPose.apply(WorldMat, localpose);

  if (0) {
    fvec3 c1(1, .8, .8);
    fvec3 c2(.8, .8, 1);
    deco::printe(c1, "LocalPose (post-concat)", true);
    deco::prints(localpose.dumpc(c1), true);
    deco::printe(c2, "WorldPose (post-concat)", true);
    deco::prints(minst->_worldPose.dumpc(c2), true);
  }

  ///////////////////////////////////
  // Draw Skinned Mesh
  ///////////////////////////////////

  if (1) // draw mesh
  {
    const XgmSkeleton& Skeleton = skeleton();

    pTARG->debugPushGroup("RenderSkinnedMesh");
    pTARG->MTXI()->PushMMatrix(fmtx4());
    pTARG->PushModColor(ModColor);
    {
      const XgmMesh& XgmMesh       = *mdlctx.mMesh;
      auto cluster                 = mdlctx._cluster;
      const XgmSubMesh& XgmClusSet = *mdlctx.mSubMesh;

      bool bmatpushed = false;

      auto mtl = XgmClusSet.GetMaterial();

      mtl->gpuUpdate(pTARG);

      if (0 != mtl) {

        auto fxcache = RCID._fx_instance_cache;
        OrkAssert(fxcache);
        auto fxinst = fxcache->findfxinst(RCID);
        OrkAssert(fxinst);

        fxinst->wrappedDrawCall(RCID, [&]() {
          size_t inumjoints = cluster->mJoints.size();

          OrkAssert(miBonesPerCluster <= kMatrixBlockSize);

          for (size_t ijointreg = 0; ijointreg < inumjoints; ijointreg++) {
            const std::string& JointName = cluster->mJoints[ijointreg];
            int JointSkelIndex          = cluster->mJointSkelIndices[ijointreg];
            const fmtx4& finalmtx       = minst->_worldPose._world_bindrela_matrices[JointSkelIndex];
            //////////////////////////////////////
            MatrixBlock[ijointreg] = finalmtx;
          }

          //////////////////////////////////////////////////////
          // apply bones
          //////////////////////////////////////////////////////

          MaterialInstItemMatrixBlock mtxblockitem;
          mtxblockitem.SetNumMatrices(inumjoints);
          mtxblockitem.SetMatrixBlock(MatrixBlock);
          mtl->BindMaterialInstItem(&mtxblockitem);
          { mtxblockitem.mApplicator->ApplyToTarget(pTARG); }
          mtl->UnBindMaterialInstItem(&mtxblockitem);

          mtl->UpdateMVPMatrix(pTARG);

          //////////////////////////////////////////////////////
          auto vtxbuffer = cluster->GetVertexBuffer();
          if (vtxbuffer) {
            int inumprim = cluster->numPrimGroups();
            for (int iprim = 0; iprim < inumprim; iprim++) {
              auto primgroup = cluster->primgroup(iprim);
              auto idxbuffer = primgroup->GetIndexBuffer();
              pTARG->GBI()->DrawIndexedPrimitiveEML(*vtxbuffer, *idxbuffer, primgroup->GetPrimType());
            }
          }
          //////////////////////////////////////////////////////
        });
      }
      pTARG->PopModColor();
      pTARG->MTXI()->PopMMatrix();
      pTARG->debugPopGroup();
    }
  }

  ////////////////////////////////////////
  // Draw Skeleton

  if (minst->_drawSkeleton) {
    const auto& worldpose = minst->_worldPose;
    pTARG->debugPushGroup("DrawSkeleton");
    pTARG->PushModColor(fvec4::White());

    static pbrmaterial_ptr_t material = default3DMaterial(pTARG);
    material->_variant = "vertexcolor"_crcu;
    material->_rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_OFF);
    material->_rasterstate.SetZWriteMask(true);
    auto fxcache = material->fxInstanceCache();
    OrkAssert(fxcache);
    RenderContextInstData RCIDCOPY = RCID;
    RCIDCOPY._isSkinned = false;
    RCIDCOPY._fx_instance_cache = fxcache;
    auto fxinst = fxcache->findfxinst(RCIDCOPY);
    OrkAssert(fxinst);

    //////////////

    {
      typedef SVtxV12N12B12T8C4 vertex_t;
      auto& vtxbuf = GfxEnv::GetSharedDynamicVB2();
      VtxWriter<vertex_t> vw;
      int inumbones = skeleton().numBones();
      // printf("inumbones<%d>\n", inumbones);
      if (inumbones) {
        vertex_t hvtx, t;
        hvtx.mColor    = uint32_t(0xff00ffff);
        t.mColor       = uint32_t(0xff0000ff);
        hvtx.mUV0      = fvec2(0, 0);
        t.mUV0         = fvec2(1, 1);
        hvtx.mNormal   = fvec3(0, 0, 0);
        t.mNormal      = fvec3(1, 0, 0);
        hvtx.mBiNormal = fvec3(1, 1, 0);
        t.mBiNormal    = fvec3(1, 1, 0);
        int numlines   = inumbones * (24);
        vw.Lock(pTARG, &vtxbuf, numlines);
        for (int ib = 0; ib < inumbones; ib++) {
          const XgmBone& bone = skeleton().bone(ib);
          fmtx4 bone_head     = worldpose._world_concat_matrices[bone._parentIndex];
          fmtx4 bone_tail     = worldpose._world_concat_matrices[bone._childIndex];

          fvec3 h             = bone_head.translation();
          fvec3 t             = bone_tail.translation();

          fvec3 delta      = (t - h);
          float bonelength = delta.length();
          fvec3 n          = delta.normalized();
          float bl2        = bonelength * 0.1;
          fvec3 hh         = h + n * bl2;

          fvec3 hnx, hny, hnz;
          bone_head.toNormalVectors(hnx, hny, hnz);
          hnx.normalizeInPlace();
          hny.normalizeInPlace();
          hnz.normalizeInPlace();

          auto hnx_ = hnx.crossWith(n);
          hnx = hnx_.crossWith(n);
          auto hnz_ = hnz.crossWith(n);
          hnz = hnz_.crossWith(n);

          fvec3 a, b, c, d;

          if (math::areValuesClose(abs(n.dotWith(hnz)), 1, 0.01)) {
            a = hh + hnx * bl2;
            b = hh - hnx * bl2;
            c = hh + hny * bl2;
            d = hh - hny * bl2;
          } else {

            a = hh + hnx * bl2;
            b = hh - hnx * bl2;
            c = hh + hnz * bl2;
            d = hh - hnz * bl2;
          }

          auto add_vertex = [&](const fvec3 pos, const fvec3& col) {
            hvtx.mPosition = pos;
            hvtx.mColor    = col.ABGRU32();
            vw.AddVertex(hvtx);
          };
          // printf("n<%g %g %g>\n", n.x, n.y, n.z);
          // printf("hnx<%g %g %g>\n", hnx.x, hnx.y, hnx.z);
          // printf("hny<%g %g %g>\n", hny.x, hny.y, hny.z);
          // printf("hnz<%g %g %g>\n", hnz.x, hnz.y, hnz.z);

          bool bonep = (worldpose._boneprops[bone._parentIndex]==0);

          auto color = bonep ? fvec3::Yellow() : fvec3::Red();

          add_vertex(h, color);
          add_vertex(a, color);
          add_vertex(a, color);
          add_vertex(t, color);

          add_vertex(h, color);
          add_vertex(b, color);
          add_vertex(b, color);
          add_vertex(t, color);

          add_vertex(h, color);
          add_vertex(c, color);
          add_vertex(c, color);
          add_vertex(t, color);

          add_vertex(h, color);
          add_vertex(d, color);
          add_vertex(d, color);
          add_vertex(t, color);

          add_vertex(a, color);
          add_vertex(c, color);
          add_vertex(c, color);
          add_vertex(b, color);

          add_vertex(b, color);
          add_vertex(d, color);
          add_vertex(d, color);
          add_vertex(a, color);
        }
        vw.UnLock(pTARG);
        pTARG->MTXI()->PushMMatrix(fmtx4::Identity());
        fxinst->wrappedDrawCall(RCID, [&]() {
          pTARG->GBI()->DrawPrimitiveEML(vw, PrimitiveType::LINES, numlines);
        });
        pTARG->MTXI()->PopMMatrix();
      }
      for (int ib = 0; ib < inumbones; ib++) {
        const XgmBone& bone = skeleton().bone(ib);
        fmtx4 bone_head     = worldpose._world_bindrela_matrices[bone._parentIndex];
         bone_head = bone_head * mSkeleton._bindMatrices[bone._parentIndex];
        //fmtx4 bone_tail     = worldpose._world_bindrela_matrices[bone._childIndex];
        pTARG->MTXI()->PushMMatrix(bone_head);
        fxinst->wrappedDrawCall(RCID, [&]() {
            auto& axis =  GfxPrimitives::GetRef().mVtxBuf_Axis;
            pTARG->GBI()->DrawPrimitiveEML(axis);
        });
        pTARG->MTXI()->PopMMatrix();
      }
    }
    pTARG->PopModColor();
    pTARG->debugPopGroup();
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmModel::dump() const {
  int inummeshes = numMeshes();

  orkprintf("CXGMModelDump this<%p>\n", this);
  orkprintf(" NumMeshes %d\n", inummeshes);
  for (int i = 0; i < inummeshes; i++) {
    mesh(i)->dump();
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmMesh::dump() const {
  int inumsubmeshes = numSubMeshes();

  orkprintf(" XgmMesh this<%p>\n", this);
  orkprintf(" NumClusterSets %d\n", inumsubmeshes);

  for (int i = 0; i < inumsubmeshes; i++) {
    subMesh(i)->dump();
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmSubMesh::dump() const {
  orkprintf("  XgmSubMesh this<%p>\n", this);
  orkprintf("   NumClusters<%zu>\n", _clusters.size());
  for (int i = 0; i < _clusters.size(); i++) {
    _clusters[i]->dump();
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmCluster::dump() const {
  int inumskelidc = int(mJointSkelIndices.size());
  int inumjoints  = int(mJoints.size());

  orkprintf("   XgmCluster this<%p>\n", this);
  orkprintf("    NumPrimGroups<%zu>\n", numPrimGroups());
  orkprintf("    NumJointSkelIndices<%d>\n", inumskelidc);
  for (int i = 0; i < inumskelidc; i++) {
    orkprintf("     JointSkelIndices<%d>=<%d>\n", i, mJointSkelIndices[i]);
  }
  orkprintf("    NumJointNames<%d>\n", inumjoints);
  for (int i = 0; i < inumjoints; i++) {
    orkprintf("     JointName<%d>=<%s>\n", i, mJoints[i].c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////

RenderContextInstModelData::RenderContextInstModelData()
    : mMesh(nullptr)
    , mSubMesh(nullptr)
    , mbisSkinned(false) {
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
