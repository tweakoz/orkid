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

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

XgmModel::XgmModel()
    : miBonesPerCluster(0)
    , mpUserData(0)
    , miNumMaterials(0)
    , mbSkinned(false) {
    _skeleton = std::make_shared<XgmSkeleton>();
}

XgmModel::~XgmModel() {
}

bool XgmModel::intersectBoundingBox(const fray3& ray, fvec3& isect_in, fvec3& isect_out) const {
  AABox aabb;
  for(int i = 0; i < 3 ; i++)
  {
    aabb.mMin[i] = mAABoundXYZ[i] - mAABoundWHD[i];
    aabb.mMax[i] = mAABoundXYZ[i] + mAABoundWHD[i]; 
  }
  return aabb.Intersect(ray, isect_in, isect_out);
}
///////////////////////////////////////////////////////////////////////////////

int XgmModel::meshIndex(const PoolString& name) const {
  auto it = mMeshes.find(name);
  return (it == mMeshes.end()) ? -1 : int(it - mMeshes.begin());
}

///////////////////////////////////////////////////////////////////////////////

XgmModelInst::XgmModelInst(const XgmModel* Model)
    : mXgmModel(Model)
    , mMaterialStateInst(*this)
    , mbSkinned(false)
    , mBlenderZup(false)
    , _drawSkeleton(false) {

  OrkAssert(Model != 0);
  miNumChannels = Model->_skeleton->numJoints();
  if (miNumChannels == 0) {
    miNumChannels = 1;
  }

  _localPose = std::make_shared<XgmLocalPose>(Model->_skeleton);
  _worldPose = std::make_shared<XgmWorldPose>(Model->_skeleton);

  _localPose->bindPose();
  _localPose->blendPoses();
  _localPose->concatenate();
  _worldPose->apply(fmtx4(), _localPose);

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
  //printf( "_override_material<%p> orig<%p>\n",(void*)  m.get(), (void*) _submesh->_material.get() );
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
    , mePrimType(PrimitiveType::END) {
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
  int inumjoints  = int(_jointPaths.size());

  orkprintf("   XgmCluster this<%p>\n", this);
  orkprintf("    NumPrimGroups<%zu>\n", numPrimGroups());
  orkprintf("    NumJointSkelIndices<%d>\n", inumskelidc);
  for (int i = 0; i < inumskelidc; i++) {
    orkprintf("     JointSkelIndices<%d>=<%d>\n", i, mJointSkelIndices[i]);
  }
  orkprintf("    NumJointNames<%d>\n", inumjoints);
  for (int i = 0; i < inumjoints; i++) {
    orkprintf("     JointPath<%d>=<%s>\n", i, _jointPaths[i].c_str());
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
