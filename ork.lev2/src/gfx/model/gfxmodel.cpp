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
  miNumChannels = Model->skeleton().numJoints();
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
    : mBoundingSphere(fvec3::Zero(), 0.0f) {
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
  int imat                     = RCID.GetMaterialIndex();
  OrkAssert(imat < inumclusset);
  material_ptr_t pmat = XgmClusSet.GetMaterial();

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
      static void RenderPrim(ork::lev2::Context* pTARG, xgmcluster_ptr_t clust) {
        auto vtxbuffer = clust->_vertexBuffer;
        int inumprim   = clust->numPrimGroups();
        for (int iprim = 0; iprim < inumprim; iprim++) {
          auto primgroup = clust->primgroup(iprim);
          auto idxbuffer = primgroup->GetIndexBuffer();
          pTARG->GBI()->DrawIndexedPrimitiveEML(*vtxbuffer, *idxbuffer, primgroup->GetPrimType());
        }
      }
      static void RenderStd(ork::lev2::Context* pTARG, ork::lev2::GfxMaterial* pmat, xgmcluster_ptr_t clust, int inumpasses) {
        for (int ipass = 0; ipass < inumpasses; ipass++) {
          if (pmat->BeginPass(pTARG, ipass)) {
            RenderClus::RenderPrim(pTARG, clust);
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
      static RenderGroupState gLASTSTATE = ork::lev2::RenderGroupState::NONE;

      switch (rgs) {
        /////////////////////////////////////////////////////
        case ork::lev2::RenderGroupState::NONE: {
          pTARG->debugPushGroup("XgmModel::RenderRigid::RenderGroupState::NONE");
          // pTARG->BindMaterial(pmat.get());
          int inumpasses = pmat->BeginBlock(pTARG, RCID);
          { RenderClus::RenderStd(pTARG, pmat.get(), cluster, inumpasses); }
          pmat->EndBlock(pTARG);
          gbGROUPENABLED = false;
          pTARG->debugPopGroup();
          break;
        }
        /////////////////////////////////////////////////////
        case ork::lev2::RenderGroupState::FIRST: {
          // pTARG->BindMaterial(pmat.get());
          giNUMPASSES = pmat->BeginBlock(pTARG, RCID);
          if (giNUMPASSES == 1) {
            gbGROUPENABLED = true;
            gbDRAW         = pmat->BeginPass(pTARG, 0);
            if (gbDRAW) {
              RenderClus::RenderPrim(pTARG, cluster);
            }
          } else {
            gbGROUPENABLED = false;
            {
              RenderClus::RenderStd(pTARG, pmat.get(), cluster, giNUMPASSES); // inumpasses );
            }
            pmat->EndBlock(pTARG);
          }
          break;
        }
        /////////////////////////////////////////////////////
        case ork::lev2::RenderGroupState::CONTINUE: {
          OrkAssert((gLASTSTATE == ork::lev2::RenderGroupState::FIRST) || (gLASTSTATE == ork::lev2::RenderGroupState::CONTINUE));
          if (gbGROUPENABLED) {
            if (gbDRAW) {
              pmat->UpdateMVPMatrix(pTARG);
              RenderClus::RenderPrim(pTARG, cluster);
            }
          } else {
            // pTARG->BindMaterial(pmat.get());
            int inumpasses = pmat->BeginBlock(pTARG, RCID);
            { RenderClus::RenderStd(pTARG, pmat.get(), cluster, inumpasses); }
            pmat->EndBlock(pTARG);
          }
          break;
        }
        /////////////////////////////////////////////////////
        case ork::lev2::RenderGroupState::LAST: {
          OrkAssert((gLASTSTATE == ork::lev2::RenderGroupState::CONTINUE) || (gLASTSTATE == ork::lev2::RenderGroupState::FIRST));

          if (gbGROUPENABLED) {
            if (gbDRAW) {
              pmat->UpdateMVPMatrix(pTARG);
              RenderClus::RenderPrim(pTARG, cluster);
              pmat->EndPass(pTARG);
            }
            pmat->EndBlock(pTARG);
          } else {
            // pTARG->BindMaterial(pmat.get());
            int inumpasses = pmat->BeginBlock(pTARG, RCID);
            { RenderClus::RenderStd(pTARG, pmat.get(), cluster, inumpasses); }
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

  ///////////////////////////////////
  // apply local pose to world pose
  ///////////////////////////////////

  const auto& localpose = minst->RefLocalPose();
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
    pTARG->MTXI()->PushMMatrix(WorldMat);
    pTARG->PushModColor(ModColor);
    {
      const XgmMesh& XgmMesh       = *mdlctx.mMesh;
      auto cluster                 = mdlctx._cluster;
      const XgmSubMesh& XgmClusSet = *mdlctx.mSubMesh;

      bool bmatpushed = false;

      auto mtl = XgmClusSet.GetMaterial();

      if (minst->_overrideMaterial != 0)
        mtl = minst->_overrideMaterial;

      mtl->gpuUpdate(pTARG);

      if (0 != mtl) {
        // pTARG->BindMaterial(mtl.get());
        int inumpasses = mtl->BeginBlock(pTARG, RCID);

        for (int ip = 0; ip < inumpasses; ip++) {
          OrkAssert(ip < inumpasses);
          bool bDRAW = mtl->BeginPass(pTARG, ip);

          if (bDRAW) {
            //////////////////////////////////////////////////////
            // upload bones to bone registers (probably vertex shader constant registers (Lev2), possibly matrix palette registers
            // (PSP, GameCube)

            size_t inumjoints = cluster->mJoints.size();

            OrkAssert(miBonesPerCluster <= kMatrixBlockSize);

            for (size_t ijointreg = 0; ijointreg < inumjoints; ijointreg++) {
              const PoolString& JointName = cluster->mJoints[ijointreg];
              int JointSkelIndex          = cluster->mJointSkelIndices[ijointreg];
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

    auto material = default3DMaterial();

    //////////////
    // bone x-ray
    //////////////

    material->_rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);

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
          fmtx4 bone_head     = WorldMat * LocalPose.RefLocalMatrix(bone._parentIndex);
          fmtx4 bone_tail     = WorldMat * LocalPose.RefLocalMatrix(bone._childIndex);
          fvec3 h             = bone_head.GetTranslation();
          fvec3 t             = bone_tail.GetTranslation();

          fvec3 hnx, hny, hnz;
          bone_head.toNormalVectors(hnx, hny, hnz);
          hnx.Normalize();
          hny.Normalize();
          hnz.Normalize();

          fvec3 delta      = (t - h);
          float bonelength = delta.length();
          fvec3 n          = delta.Normal();
          float bl2        = bonelength * 0.1;
          fvec3 hh         = h + n * bl2;

          fvec3 a, b, c, d;

          if (math::areValuesClose(abs(n.Dot(hnz)), 1, 0.01)) {
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
            hvtx.mColor    = col.GetABGRU32();
            vw.AddVertex(hvtx);
          };
          // printf("n<%g %g %g>\n", n.x, n.y, n.z);
          // printf("hnx<%g %g %g>\n", hnx.x, hnx.y, hnx.z);
          // printf("hny<%g %g %g>\n", hny.x, hny.y, hny.z);
          // printf("hnz<%g %g %g>\n", hnz.x, hnz.y, hnz.z);

          add_vertex(h, fvec3::White());
          add_vertex(a, fvec3::White());
          add_vertex(a, fvec3::White());
          add_vertex(t, fvec3::White());

          add_vertex(h, fvec3::White());
          add_vertex(b, fvec3::White());
          add_vertex(b, fvec3::White());
          add_vertex(t, fvec3::White());

          add_vertex(h, fvec3::White());
          add_vertex(c, fvec3::White());
          add_vertex(c, fvec3::White());
          add_vertex(t, fvec3::White());

          add_vertex(h, fvec3::White());
          add_vertex(d, fvec3::White());
          add_vertex(d, fvec3::White());
          add_vertex(t, fvec3::White());

          add_vertex(a, fvec3::White());
          add_vertex(c, fvec3::White());
          add_vertex(c, fvec3::White());
          add_vertex(b, fvec3::White());

          add_vertex(b, fvec3::White());
          add_vertex(d, fvec3::White());
          add_vertex(d, fvec3::White());
          add_vertex(a, fvec3::White());
        }
        vw.UnLock(pTARG);
        pTARG->MTXI()->PushMMatrix(fmtx4::Identity());
        pTARG->GBI()->DrawPrimitive(material.get(), vw, PrimitiveType::LINES, numlines);
        pTARG->MTXI()->PopMMatrix();
      }
      for (int ib = 0; ib < inumbones; ib++) {
        const XgmBone& bone = skeleton().bone(ib);
        fmtx4 bone_head     = WorldMat * LocalPose.RefLocalMatrix(bone._parentIndex);
        fmtx4 bone_tail     = WorldMat * LocalPose.RefLocalMatrix(bone._childIndex);
        pTARG->MTXI()->PushMMatrix(bone_head);
        // GfxPrimitives::GetRef().RenderAxis(pTARG);
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
    : mbisSkinned(false)
    , mMesh(nullptr)
    , mSubMesh(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
