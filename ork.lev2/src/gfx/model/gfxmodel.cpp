////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/prop.h>
#include <ork/kernel/environment.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/material_freestyle.h>

template class ork::orklut<ork::PoolString, ork::lev2::xgmmesh_ptr_t>;
int eggtestcount = 0;
namespace ork { namespace lev2 {

static bool SHOW_SKELETON(){
  return genviron.has("ORKID_LEV2_SHOW_SKELETON");
}

///////////////////////////////////////////////////////////////////////////////

XgmModel::XgmModel()
    : miBonesPerCluster(0)
    , mpUserData(0)
    , miNumMaterials(0)
    , mbSkinned(false) {
}

XgmModel::~XgmModel() {
}

///////////////////////////////////////////////////////////////////////////////

int XgmModel::meshIndex(const PoolString& name) const {
  auto it = mMeshes.find(name);
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
    , _drawSkeleton(false) {

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
      auto smi     = std::make_shared<XgmSubMeshInst>(submesh.get());
      _submeshinsts.push_back(smi);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

XgmSubMeshInst::XgmSubMeshInst(const XgmSubMesh* submesh)
    : _submesh(submesh)
    , _enabled(true) {

  _fxpipelinecache = submesh->_material->pipelineCache();
  OrkAssert(_fxpipelinecache);
}

material_ptr_t XgmSubMeshInst::material() const {
  if(_override_material) {
    return _override_material;
  }
  return _submesh->_material;
}

void XgmSubMeshInst::overrideMaterial(material_ptr_t m){
  printf( "_override_material<%p> orig<%p>\n",(void*)  m.get(), (void*) _submesh->_material.get() );
  _override_material = m;
  _fxpipelinecache = m->pipelineCache();
  OrkAssert(_fxpipelinecache);
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

void XgmModelInst::enableMesh(xgmmesh_constptr_t mesh) {
  for (auto submeshinst : _submeshinsts) {
    auto item_mesh = submeshinst->_submesh->_parentmesh;
    if (item_mesh == mesh.get())
      submeshinst->_enabled = true;
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelInst::disableMesh(xgmmesh_constptr_t mesh) {
  for (auto submeshinst : _submeshinsts) {
    auto item_mesh = submeshinst->_submesh->_parentmesh;
    if (item_mesh == mesh.get())
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
    auto psrcmesh  = pMesh->subMesh(i);
    mSubMeshes[i] = std::make_shared<XgmSubMesh>(*psrcmesh);
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

  auto fxcache = RCID._pipeline_cache;
  OrkAssert(fxcache);
  auto pipeline = fxcache->findPipeline(RCID);
  OrkAssert(pipeline);


  pTARG->debugPushGroup(
      FormatString("XgmModel::RenderRigid stereo1pass<%d> inummesh<%d> inumclusset<%d>", int(stereo1pass), inummesh, inumclusset));

  pTARG->MTXI()->SetMMatrix(WorldMat);
  pTARG->PushModColor(ModColor);
  {
    //////////////////////////////////////////////
    // pipeline wrapped draw call
    //////////////////////////////////////////////
    pipeline->wrappedDrawCall(RCID, [&]() {
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

void XgmModel::RenderSkinned(
    const XgmModelInst* minst,
    const fcolor4& ModColor,
    const fmtx4& WorldMat,
    ork::lev2::Context* pTARG,
    const RenderContextInstData& RCID,
    const RenderContextInstModelData& mdlctx) const {


  static constexpr size_t kMatrixBlockSize = 1024;
  static auto matrix_block = new fmtx4[kMatrixBlockSize];

  auto fxcache = RCID._pipeline_cache;
  auto pipeline  = fxcache->findPipeline(RCID);
  auto pmat    = pipeline->_material;

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

      if (mtl != nullptr) {

        mtl->gpuUpdate(pTARG);

        auto fxcache = RCID._pipeline_cache;
        OrkAssert(fxcache);
        auto pipeline = fxcache->findPipeline(RCID);
        OrkAssert(pipeline);

        pipeline->wrappedDrawCall(RCID, [&]() {
          size_t inumjoints = cluster->mJoints.size();

          OrkAssert(miBonesPerCluster <= kMatrixBlockSize);

          for (size_t ijointreg = 0; ijointreg < inumjoints; ijointreg++) {
            const std::string& JointName = cluster->mJoints[ijointreg];
            int JointSkelIndex          = cluster->mJointSkelIndices[ijointreg];
            const fmtx4& finalmtx       = minst->_worldPose._world_bindrela_matrices[JointSkelIndex];
            //////////////////////////////////////
            matrix_block[ijointreg] = finalmtx;
          }

          //////////////////////////////////////////////////////
          // apply bones
          //////////////////////////////////////////////////////

          MaterialInstItemMatrixBlock mtxblockitem;
          mtxblockitem.SetNumMatrices(inumjoints);
          mtxblockitem.SetMatrixBlock(matrix_block);
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

  if (minst->_drawSkeleton or SHOW_SKELETON()) {
    const auto& worldpose = minst->_worldPose;
    pTARG->debugPushGroup("DrawSkeleton");
    pTARG->PushModColor(fvec4::White());

    static pbrmaterial_ptr_t material = default3DMaterial(pTARG);
    material->_variant = "vertexcolor"_crcu;
    material->_rasterstate.SetDepthTest(ork::lev2::EDepthTest::OFF);
    material->_rasterstate.SetZWriteMask(true);
    auto fxcache = material->pipelineCache();
    OrkAssert(fxcache);
    RenderContextInstData RCIDCOPY = RCID;
    RCIDCOPY._isSkinned = false;
    RCIDCOPY._pipeline_cache = fxcache;
    auto pipeline = fxcache->findPipeline(RCIDCOPY);
    OrkAssert(pipeline);

    //////////////

    {
      typedef SVtxV12N12B12T8C4 vertex_t;
      auto& vtxbuf = GfxEnv::GetSharedDynamicVB2();
      VtxWriter<vertex_t> vw;
      int inumjoints = skeleton().miNumJoints;
      // printf("inumjoints<%d>\n", inumjoints);
      if (inumjoints) {
        vertex_t hvtx, t;
        hvtx.mColor    = uint32_t(0xff00ffff);
        t.mColor       = uint32_t(0xff0000ff);
        hvtx.mUV0      = fvec2(0, 0);
        t.mUV0         = fvec2(1, 1);
        hvtx.mNormal   = fvec3(0, 0, 0);
        t.mNormal      = fvec3(1, 0, 0);
        hvtx.mBiNormal = fvec3(1, 1, 0);
        t.mBiNormal    = fvec3(1, 1, 0);
        int numlines   = inumjoints * (28);
        vw.Lock(pTARG, &vtxbuf, numlines);
        for (int ij = 0; ij < inumjoints; ij++) {
          fmtx4 joint_head     = worldpose._world_concat_matrices[ij];
          fvec3 h             = joint_head.translation();

          float bonelength = 1;
          float bl2        = bonelength * 0.1;

          fvec3 T;  fquat R;  float S;
          joint_head.decompose(T,R,S);
          fmtx4 joint_head_unitscale;
          joint_head_unitscale.compose2(T,R,1);           

          auto a = fvec4(bl2,bl2,0).transform(joint_head_unitscale);
          auto b = fvec4(-bl2,bl2,0).transform(joint_head_unitscale);
          auto c = fvec4(0,bl2,bl2).transform(joint_head_unitscale);
          auto d = fvec4(0,bl2,-bl2).transform(joint_head_unitscale);
          auto t = fvec4(0,1,0).transform(joint_head_unitscale);

          fvec3 hh         = fvec4(0,bl2,0).transform(joint_head_unitscale);

          auto add_vertex = [&](const fvec3 pos, const fvec3& col) {
            hvtx.mPosition = pos;
            hvtx.mColor    = (col*5).ABGRU32();
            vw.AddVertex(hvtx);
          };

          bool bonep = false; //(worldpose._boneprops[bone._parentIndex]==0);

          auto colorN = fvec3::White();
          auto colorX = bonep ? fvec3(1,.5,.5) : fvec3::Yellow();
          auto colorZ = bonep ? fvec3(.5,.5,1) : fvec3::White();

          add_vertex(h, colorX);
          add_vertex(a, colorX);
          add_vertex(a, colorX);
          add_vertex(t, colorX);

          add_vertex(h, colorX);
          add_vertex(b, colorX);
          add_vertex(b, colorX);
          add_vertex(t, colorX);

          add_vertex(a, colorX);
          add_vertex(hh, colorX);

          add_vertex(h, colorZ);
          add_vertex(c, colorZ);
          add_vertex(c, colorZ);
          add_vertex(t, colorZ);

          add_vertex(h, colorZ);
          add_vertex(d, colorZ);
          add_vertex(d, colorZ);
          add_vertex(t, colorZ);

          add_vertex(c, colorZ);
          add_vertex(hh, colorZ);

          add_vertex(a, colorN);
          add_vertex(c, colorN);
          add_vertex(c, colorN);
          add_vertex(b, colorN);

          add_vertex(b, colorN);
          add_vertex(d, colorN);
          add_vertex(d, colorN);
          add_vertex(a, colorN);
        }
        vw.UnLock(pTARG);
        pTARG->MTXI()->PushMMatrix(fmtx4::Identity());
        pipeline->wrappedDrawCall(RCID, [&]() {
          pTARG->GBI()->DrawPrimitiveEML(vw, PrimitiveType::LINES, numlines);
        });
        pTARG->MTXI()->PopMMatrix();
      }
      for (int ij = 0; ij < inumjoints; ij++) {
        fmtx4 joint_head     = worldpose._world_bindrela_matrices[ij];
         joint_head = joint_head * mSkeleton._bindMatrices[ij];
        pTARG->MTXI()->PushMMatrix(joint_head);
        pipeline->wrappedDrawCall(RCID, [&]() {
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
