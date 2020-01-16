////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/prop.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/material_pbr.inl>

template class ork::orklut<ork::PoolString, ork::lev2::XgmMesh*>;
int eggtestcount = 0;
namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

XgmModel::XgmModel()
    : mpUserData(0)
    , miNumMaterials(0)
    , miBonesPerCluster(0)
    , mbSkinned(false) {
}

XgmModel::~XgmModel() {
  for (orklut<PoolString, XgmMesh*>::iterator it = mMeshes.begin(); it != mMeshes.end(); it++)
    delete it->second;
  for (orkvector<GfxMaterial*>::iterator it = mvMaterials.begin(); it != mvMaterials.end(); it++)
    delete (*it);
}

///////////////////////////////////////////////////////////////////////////////

int XgmModel::meshIndex(const PoolString& name) const {
  orklut<PoolString, XgmMesh*>::const_iterator it = mMeshes.find(name);

  return (it == mMeshes.end()) ? -1 : int(it - mMeshes.begin());
}

///////////////////////////////////////////////////////////////////////////////

XgmModelInst::XgmModelInst(const XgmModel* Model)
    : mXgmModel(Model)
    , mLocalPose(Model->skeleton())
    , _worldPose(Model->skeleton())
    , mMaterialStateInst(*this)
    , mbSkinned(false)
    , mBlenderZup(false)
    , _drawSkeleton(true) {
  EnableAllMeshes();

  OrkAssert(Model != 0);
  miNumChannels = Model->skeleton().GetNumJoints();
  if (miNumChannels == 0) {
    miNumChannels = 1;
  }

  mLocalPose.BindPose();
  mLocalPose.BuildPose();
  mLocalPose.Concatenate();
  _worldPose.apply(fmtx4(), mLocalPose);
}

///////////////////////////////////////////////////////////////////////////////

XgmModelInst::~XgmModelInst() {
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::EnableMesh(const PoolString& ps) {
  int index = mXgmModel->meshIndex(ps);
  EnableMesh(index);
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::DisableMesh(const PoolString& ps) {
  int index = mXgmModel->meshIndex(ps);
  DisableMesh(index);
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::EnableMesh(int imeshindex) {
  int icharindex = (imeshindex >> 3);
  int ibitindex  = (imeshindex & 0x00000007);
  OrkAssert(icharindex < knummaskbytes);
  mMaskBits[icharindex] |= (1 << ibitindex);
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::DisableMesh(int imeshindex) {
  int icharindex = (imeshindex >> 3);
  int ibitindex  = (imeshindex & 0x00000007);
  OrkAssert(icharindex < knummaskbytes);
  mMaskBits[icharindex] &= ~(1 << ibitindex);
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::EnableAllMeshes() {
  for (int i = 0; i < knummaskbytes; i++)
    mMaskBits[i] = 0xff;
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::DisableAllMeshes() {
  for (int i = 0; i < knummaskbytes; i++)
    mMaskBits[i] = 0;
}

///////////////////////////////////////////////////////////////////////////////

bool XgmModelInst::isMeshEnabled(int imeshindex) {
  if (imeshindex >= mXgmModel->numMeshes())
    return false;

  int icharindex = (imeshindex >> 3);
  OrkAssert(icharindex < knummaskbytes);
  int ibitindex = (imeshindex & 0x00000007);
  return (mMaskBits[icharindex] & (1 << ibitindex));
}

///////////////////////////////////////////////////////////////////////////////

bool XgmModelInst::isMeshEnabled(const PoolString& ps) {
  int index = mXgmModel->meshIndex(ps);
  if (index >= 0) {
    return isMeshEnabled(index);
  }
  return false;
}

bool XgmModelInst::IsAnyMeshEnabled() {
  if (mXgmModel->numMeshes() == 1)
    return true;

  for (int i = 0; i < knummaskbytes; i++)
    if (mMaskBits[i])
      return (true);

  return (false);
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
    , mePrimType(EPRIM_NONE) {
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
    : miNumPrimGroups(0)
    , mpPrimGroups(0)
    , _vertexBuffer(0)
    , mBoundingSphere(fvec3::Zero(), 0.0f) {
}

XgmCluster::~XgmCluster() {
  delete[] mpPrimGroups;

  delete _vertexBuffer;
}

///////////////////////////////////////////////////////////////////////////////

XgmSubMesh::~XgmSubMesh() {
  delete[] mpClusters;
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

void XgmModel::AddMaterial(GfxMaterial* Mat) {
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
  auto R                         = RCID.GetRenderer();
  auto RCFD                      = pTARG->topRenderContextFrameData();
  const auto& CPD                = RCFD->topCPD();
  bool stereo1pass               = CPD.isStereoOnePass();
  const XgmMesh& XgmMesh         = *mdlctx.mMesh;
  const XgmCluster& XgmClus      = *mdlctx.mCluster;
  const XgmSubMesh& XgmClusSet   = *mdlctx.mSubMesh;
  const Texture* LightMapTexture = XgmClusSet.mLightMap;
  int inummesh                   = numMeshes();
  int inumclusset                = XgmMesh.numSubMeshes();
  int imat                       = RCID.GetMaterialIndex();
  OrkAssert(imat < inumclusset);
  GfxMaterial* __restrict pmat = XgmClusSet.GetMaterial();

  pTARG->debugPushGroup(FormatString(
      "XgmModel::RenderRigid stereo1pass<%d> inummesh<%d> inumclusset<%d> imat<%d>",
      int(stereo1pass),
      inummesh,
      inumclusset,
      imat));

  ork::lev2::RenderGroupState rgs = RCID.GetRenderGroupState();

  pTARG->MTXI()->SetMMatrix(WorldMat);
  pTARG->PushModColor(ModColor);
  {
    if (mdlctx.GetModelInst()) {
      if (mdlctx.GetModelInst()->_overrideMaterial != nullptr) {
        pmat = mdlctx.GetModelInst()->_overrideMaterial;
      }
    }
    pmat->gpuUpdate(pTARG);
    //////////////////////////////////////////////

    struct RenderClus {
      static void RenderPrim(ork::lev2::Context* pTARG, const XgmCluster& XgmClus) {
        const ork::lev2::VertexBufferBase* pVertexBuffer = XgmClus.GetVertexBuffer();
        int inumprim                                     = XgmClus.GetNumPrimGroups();
        for (int iprim = 0; iprim < inumprim; iprim++) {
          const XgmPrimGroup& PrimGroup                  = XgmClus.mpPrimGroups[iprim];
          const ork::lev2::IndexBufferBase* pIndexBuffer = PrimGroup.GetIndexBuffer();
          pTARG->GBI()->DrawIndexedPrimitiveEML(*pVertexBuffer, *pIndexBuffer, PrimGroup.GetPrimType());
        }
      }

      static void RenderStd(ork::lev2::Context* pTARG, ork::lev2::GfxMaterial* pmat, const XgmCluster& XgmClus, int inumpasses) {
        for (int ipass = 0; ipass < inumpasses; ipass++) {
          OrkAssert(ipass < inumpasses);
          bool bDRAW = pmat->BeginPass(pTARG, ipass);

          if (bDRAW) {
            RenderClus::RenderPrim(pTARG, XgmClus);
            pmat->EndPass(pTARG);
          }
        }
      }
    };

    //////////////////////////////////////////////
    if (0 != pmat) {
      static int giNUMPASSES             = 0;
      static bool gbDRAW                 = false;
      static bool gbGROUPENABLED         = false;
      static RenderGroupState gLASTSTATE = ork::lev2::ERGST_NONE;

      switch (rgs) {
        /////////////////////////////////////////////////////
        case ork::lev2::ERGST_NONE: {
          pTARG->debugPushGroup("XgmModel::RenderRigid::ERGST_NONE");
          pTARG->BindMaterial(pmat);
          int inumpasses = pmat->BeginBlock(pTARG, RCID);
          { RenderClus::RenderStd(pTARG, pmat, XgmClus, inumpasses); }
          pmat->EndBlock(pTARG);
          gbGROUPENABLED = false;
          pTARG->debugPopGroup();
          break;
        }
        /////////////////////////////////////////////////////
        case ork::lev2::ERGST_FIRST: {
          pTARG->BindMaterial(pmat);
          giNUMPASSES = pmat->BeginBlock(pTARG, RCID);
          if (giNUMPASSES == 1) {
            gbGROUPENABLED = true;
            gbDRAW         = pmat->BeginPass(pTARG, 0);
            if (gbDRAW) {
              RenderClus::RenderPrim(pTARG, XgmClus);
            }
          } else {
            gbGROUPENABLED = false;
            {
              RenderClus::RenderStd(pTARG, pmat, XgmClus, giNUMPASSES); // inumpasses );
            }
            pmat->EndBlock(pTARG);
          }
          break;
        }
        /////////////////////////////////////////////////////
        case ork::lev2::ERGST_CONTINUE: {
          OrkAssert((gLASTSTATE == ork::lev2::ERGST_FIRST) || (gLASTSTATE == ork::lev2::ERGST_CONTINUE));
          if (gbGROUPENABLED) {
            if (gbDRAW) {
              pmat->UpdateMVPMatrix(pTARG);
              RenderClus::RenderPrim(pTARG, XgmClus);
            }
          } else {
            pTARG->BindMaterial(pmat);
            int inumpasses = pmat->BeginBlock(pTARG, RCID);
            { RenderClus::RenderStd(pTARG, pmat, XgmClus, inumpasses); }
            pmat->EndBlock(pTARG);
          }
          break;
        }
        /////////////////////////////////////////////////////
        case ork::lev2::ERGST_LAST: {
          OrkAssert((gLASTSTATE == ork::lev2::ERGST_CONTINUE) || (gLASTSTATE == ork::lev2::ERGST_FIRST));

          if (gbGROUPENABLED) {
            if (gbDRAW) {
              pmat->UpdateMVPMatrix(pTARG);
              RenderClus::RenderPrim(pTARG, XgmClus);
              pmat->EndPass(pTARG);
            }
            pmat->EndBlock(pTARG);
          } else {
            pTARG->BindMaterial(pmat);
            int inumpasses = pmat->BeginBlock(pTARG, RCID);
            { RenderClus::RenderStd(pTARG, pmat, XgmClus, inumpasses); }
            pmat->EndBlock(pTARG);
          }
          break;
        }
      }
      gLASTSTATE = rgs;
    }
  }
  pTARG->PopModColor();
  pTARG->debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////

void XgmModel::RenderMultipleRigid(
    const fcolor4& ModColor,
    const fmtx4* WorldMatrices,
    int icount,
    ork::lev2::Context* pTARG,
    const RenderContextInstData& RCID,
    const RenderContextInstModelData& mdlctx) const {
  auto R           = RCID.GetRenderer();
  auto RCFD        = pTARG->topRenderContextFrameData();
  const auto& CPD  = RCFD->topCPD();
  bool stereo1pass = CPD.isStereoOnePass();

  pTARG->MTXI()->SetMMatrix(pTARG->MTXI()->RefMMatrix());
  pTARG->PushModColor(ModColor);
  {
    const XgmMesh& XgmMesh       = *mdlctx.mMesh;
    const XgmCluster& XgmClus    = *mdlctx.mCluster;
    const XgmSubMesh& XgmClusSet = *mdlctx.mSubMesh;
    const auto modelinst         = mdlctx.GetModelInst();

    int inummesh    = numMeshes();
    int inumclusset = XgmMesh.numSubMeshes();
    int imat        = RCID.GetMaterialIndex();
    OrkAssert(imat < inumclusset);
    GfxMaterial* pmaterial = XgmClusSet.GetMaterial();

    if (modelinst) {
      if (nullptr == modelinst->_overrideMaterial)
        pmaterial = modelinst->_overrideMaterial;
    }

    if (nullptr != pmaterial) {
      pTARG->BindMaterial(pmaterial);
      int inumpasses = pmaterial->BeginBlock(pTARG, RCID);

      for (int ipass = 0; ipass < inumpasses; ipass++) {
        OrkAssert(ipass < inumpasses);
        bool bDRAW = pmaterial->BeginPass(pTARG, ipass);

        if (bDRAW) {
          for (int imtx = 0; imtx < icount; imtx++) {

            const fmtx4& mtxW = WorldMatrices[imtx];
            pTARG->MTXI()->SetMMatrix(mtxW);
            pmaterial->UpdateMVPMatrix(pTARG);

            const ork::lev2::VertexBufferBase* pVertexBuffer = XgmClus.GetVertexBuffer();
            int inumprim                                     = XgmClus.GetNumPrimGroups();
            for (int iprim = 0; iprim < inumprim; iprim++) {
              const XgmPrimGroup& PrimGroup                  = XgmClus.mpPrimGroups[iprim];
              const ork::lev2::IndexBufferBase* pIndexBuffer = PrimGroup.GetIndexBuffer();
              pTARG->GBI()->DrawIndexedPrimitiveEML(*pVertexBuffer, *pIndexBuffer, PrimGroup.GetPrimType());
            }
          }
        }
        pmaterial->EndPass(pTARG);
      }
      pmaterial->EndBlock(pTARG);
    }
  }
  pTARG->PopModColor();
  // pTARG->MTXI()->PopMMatrix();
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
  auto R           = RCID.GetRenderer();
  auto RCFD        = pTARG->topRenderContextFrameData();
  const auto& CPD  = RCFD->topCPD();
  bool stereo1pass = CPD.isStereoOnePass();

  // printf("rendering skinned!!\n");

  fvec3 c1(1, .8, .8);
  fvec3 c2(.8, .8, 1);
  const auto& localpose = minst->RefLocalPose();
  deco::printe(c1, "LocalPose (post-concat)", true);
  deco::prints(localpose.dumpc(c1), true);
  // deco::printe(fvec3(1, 1, 1), xfdata.mWorldMatrix.dump(), true);
  minst->_worldPose.apply(WorldMat, localpose);
  deco::printe(c2, "WorldPose (post-concat)", true);
  deco::prints(minst->_worldPose.dumpc(c2), true);

  ////////////////////
  // Draw Skinned Mesh

  if (1) // draw mesh
  {
    const XgmSkeleton& Skeleton = skeleton();

    pTARG->debugPushGroup("RenderSkinnedMesh");
    pTARG->MTXI()->PushMMatrix(WorldMat);
    pTARG->PushModColor(ModColor);
    {
      const XgmMesh& XgmMesh       = *mdlctx.mMesh;
      const XgmCluster& XgmCluster = *mdlctx.mCluster;
      const XgmSubMesh& XgmClusSet = *mdlctx.mSubMesh;

      bool bmatpushed = false;

      auto mtl = XgmClusSet.GetMaterial();

      if (minst->_overrideMaterial != 0)
        mtl = minst->_overrideMaterial;

      mtl->gpuUpdate(pTARG);

      if (0 != mtl) {
        pTARG->BindMaterial(mtl);
        int inumpasses = mtl->BeginBlock(pTARG, RCID);

        for (int ip = 0; ip < inumpasses; ip++) {
          OrkAssert(ip < inumpasses);
          bool bDRAW = mtl->BeginPass(pTARG, ip);

          if (bDRAW) {
            //////////////////////////////////////////////////////
            // upload bones to bone registers (probably vertex shader constant registers (Lev2), possibly matrix palette registers
            // (PSP, GameCube)

            size_t inumjoints = XgmCluster.mJoints.size();

            OrkAssert(miBonesPerCluster <= kMatrixBlockSize);

            for (size_t ijointreg = 0; ijointreg < inumjoints; ijointreg++) {
              const PoolString& JointName = XgmCluster.mJoints[ijointreg];
              int JointSkelIndex          = XgmCluster.mJointSkelIndices[ijointreg];
              const fmtx4& finalmtx       = minst->_worldPose.GetMatrices()[JointSkelIndex];
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
            const ork::lev2::VertexBufferBase* pVertexBuffer = XgmCluster.GetVertexBuffer();
            if (pVertexBuffer) {
              int inumprim = XgmCluster.GetNumPrimGroups();
              for (int iprim = 0; iprim < inumprim; iprim++) {
                const XgmPrimGroup& PrimGroup                  = XgmCluster.mpPrimGroups[iprim];
                const ork::lev2::IndexBufferBase* pIndexBuffer = PrimGroup.GetIndexBuffer();

                // printf( "rskin DrawIndexedPrimitiveEML iprim<%d>\n", iprim );
                pTARG->GBI()->DrawIndexedPrimitiveEML(*pVertexBuffer, *pIndexBuffer, PrimGroup.GetPrimType());
              }
            }
            //////////////////////////////////////////////////////
            mtl->EndPass(pTARG);
          }
        }
        if (inumpasses)
          mtl->EndBlock(pTARG);
      }
    }
    pTARG->PopModColor();
    pTARG->MTXI()->PopMMatrix();
    pTARG->debugPopGroup();
  }

  ////////////////////////////////////////
  // Draw Skeleton

  if (minst->_drawSkeleton) {
    const XgmLocalPose& LocalPose = minst->RefLocalPose();
    pTARG->debugPushGroup("DrawSkeleton");
    pTARG->PushModColor(fvec4::White());

    //////////////
    // bone x-ray
    //////////////

    GfxEnv::GetDefault3DMaterial()->_rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);

    //////////////

    pTARG->BindMaterial(GfxEnv::GetDefault3DMaterial());
    {
      int inumjoints = skeleton().numJoints();

      for (int ij = 0; ij < inumjoints; ij++) {
        fmtx4 MatAnimJCat = LocalPose.RefLocalMatrix(ij);
        auto InvBind      = skeleton().RefInverseBindMatrix(ij);
        auto finalmtx     = WorldMat * MatAnimJCat;
        pTARG->MTXI()->PushMMatrix(finalmtx);
        GfxPrimitives::GetRef().RenderAxis(pTARG);
        pTARG->MTXI()->PopMMatrix();
      }
    }
    pTARG->PopModColor();
    pTARG->debugPopGroup();
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmModel::RenderMultipleSkinned(
    const XgmModelInst* minst,
    const fcolor4& ModColor,
    const fmtx4* WorldMats,
    int icount,
    ork::lev2::Context* pTARG,
    const RenderContextInstData& RCID,
    const RenderContextInstModelData& mdlctx) const {
  auto R           = RCID.GetRenderer();
  auto RCFD        = pTARG->topRenderContextFrameData();
  const auto& CPD  = RCFD->topCPD();
  bool stereo1pass = CPD.isStereoOnePass();

  const XgmSkeleton& Skeleton   = skeleton();
  const XgmLocalPose& LocalPose = minst->RefLocalPose();

  ////////////////////
  // Draw Skinned Mesh

  pTARG->PushModColor(ModColor);
  {
    int inummesh = numMeshes();
    int imat     = RCID.GetMaterialIndex();

    const XgmMesh& XgmMesh       = *mdlctx.mMesh;
    const XgmSubMesh& XgmClusSet = *XgmMesh.subMesh(imat);

    int inumclusset = XgmMesh.numSubMeshes();
    OrkAssert(imat < inumclusset);
    bool bmatpushed   = false;
    GfxMaterial* pmat = XgmClusSet.GetMaterial();

    if (minst->_overrideMaterial != 0)
      pmat = minst->_overrideMaterial;

    if (0 != pmat) {
      pTARG->BindMaterial(pmat);
      int inumpasses = pmat->BeginBlock(pTARG, RCID);
      int ipass      = RCID.GetMaterialPassIndex();
      OrkAssert(ipass < inumpasses);
      bool bDRAW = pmat->BeginPass(pTARG, ipass);

      if (bDRAW) {
        int inumclus = XgmClusSet.GetNumClusters();

        MaterialInstItemMatrixBlock mtxblockitem;
        pmat->BindMaterialInstItem(&mtxblockitem);
        {
          for (int iclus = 0; iclus < inumclus; iclus++) {
            const XgmCluster& XgmClus = XgmClusSet.mpClusters[iclus];

            //////////////////////////////////////////////////////
            // upload bones to bone registers (probably vertex shader constant registers (Lev2), possibly matrix palette registers
            // (PSP, GameCube)

            size_t inumjoints = XgmClus.mJoints.size();

            static const int kMaxBonesPerCluster = miBonesPerCluster;
            static fmtx4* MatrixBlock            = new fmtx4[kMaxBonesPerCluster];

            for (size_t ijointreg = 0; ijointreg < inumjoints; ijointreg++) {
              const PoolString JointName = XgmClus.mJoints[ijointreg];
              int JointSkelIndex         = XgmClus.mJointSkelIndices[ijointreg];
              const fmtx4& MatIBind      = Skeleton.RefInverseBindMatrix(JointSkelIndex);
              const fmtx4& MatAnimJCat   = LocalPose.RefLocalMatrix(JointSkelIndex);
              fmtx4 MatCat               = MatIBind.Concat43(MatAnimJCat);
              MatrixBlock[ijointreg]     = MatCat;
            }

            mtxblockitem.SetNumMatrices(inumjoints);
            mtxblockitem.SetMatrixBlock(MatrixBlock);

            for (int ic = 0; ic < icount; ic++) {
              const fmtx4& WorldMat = WorldMats[ic];
              pTARG->MTXI()->PushMMatrix(WorldMat);
              {
                mtxblockitem.mApplicator->ApplyToTarget(pTARG);
                //////////////////////////////////////////////////////
                const ork::lev2::VertexBufferBase* pVertexBuffer = XgmClus.GetVertexBuffer();
                if (pVertexBuffer) {
                  int inumprim = XgmClus.GetNumPrimGroups();
                  for (int iprim = 0; iprim < inumprim; iprim++) {
                    const XgmPrimGroup& PrimGroup                  = XgmClus.mpPrimGroups[iprim];
                    const ork::lev2::IndexBufferBase* pIndexBuffer = PrimGroup.GetIndexBuffer();
                    pTARG->GBI()->DrawIndexedPrimitiveEML(*pVertexBuffer, *pIndexBuffer, PrimGroup.GetPrimType());
                  }
                }
                //////////////////////////////////////////////////////
              }
              pTARG->MTXI()->PopMMatrix();
            }
          }
        }
        pmat->UnBindMaterialInstItem(&mtxblockitem);
      }
      pmat->EndPass(pTARG);
      pmat->EndBlock(pTARG);
    }
  }
  pTARG->PopModColor();
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
  orkprintf("   NumClusters<%d>\n", miNumClusters);
  for (int i = 0; i < miNumClusters; i++) {
    mpClusters[i].dump();
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmCluster::dump() const {
  int inumskelidc = int(mJointSkelIndices.size());
  int inumjoints  = int(mJoints.size());

  orkprintf("   XgmCluster this<%p>\n", this);
  orkprintf("    NumPrimGroups<%d>\n", miNumPrimGroups);
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
    : mbisSkinned(false)
    , mpModelInst(0)
    , mMesh(0)
    , mSubMesh(0)
    , mCluster(0) {
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
