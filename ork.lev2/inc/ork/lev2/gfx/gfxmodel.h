////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/kernel/varmap.inl>
#include <ork/file/chunkfile.h>
#include <ork/kernel/datablock.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/string/ConstString.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/lev2/gfx/gfxanim.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/math/box.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/sphere.h>
#include <ork/kernel/varmap.inl>
#include <ork/lev2/gfx/texman.h>

#define USE_XGA_FILES

///////////////////////////////////////////////////////////////////////////////
//	Orkid Native Model File Format (XGM = XPlat Skinned Gfx Model)
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

/////////////////////////////////////////////////////
//////////////////////////


struct EmbeddedTexture final {
  int _w               = 0;
  int _h               = 0;
  size_t _srcdatalen   = 0;
  const void* _srcdata = nullptr;
  std::string _format;
  std::string _name;
  bool _compressionPending = true;
  ETextureUsage _usage     = ETEXUSAGE_DATA;
  datablock_ptr_t _ddsdestdatablock;
  datablock_ptr_t compressTexture(uint64_t hash) const;
  void fetchDDSdata();
  varmap::VarMap _varmap;
};

typedef std::map<std::string, EmbeddedTexture*> embtexmap_t;

///////////////////////////////////////////////////////////////////////////////

#define XGMMESHFLG_RIGID 0x00000001

///////////////////////////////////////////////////////////////////////////////

struct XgmPrimGroup final {
public:
  int miNumIndices;

  IndexBufferBase* mpIndices;
  PrimitiveType mePrimType;

  XgmPrimGroup();
  XgmPrimGroup(XgmPrimGroup* pgrp);

  virtual ~XgmPrimGroup();

  int GetNumIndices(void) const {
    return miNumIndices;
  }
  const IndexBufferBase* GetIndexBuffer(void) const {
    return mpIndices;
  }
  PrimitiveType GetPrimType(void) const {
    return mePrimType;
  }
};

using xgmprimgroup_ptr_t = std::shared_ptr<XgmPrimGroup>;

///////////////////////////////////////////////////////////////////////////////

struct XgmCluster final { // Run Time Cluster
  XgmCluster();
  virtual ~XgmCluster();
  void Dump(void);

  inline size_t numPrimGroups(void) const {
    return _primgroups.size();
  }
  inline xgmprimgroup_ptr_t primgroup(int idx) const {
    return _primgroups[idx];
  }
  vtxbufferbase_ptr_t GetVertexBuffer(void) const {
    return _vertexBuffer;
  }
  const PoolString& GetJointBinding(int idx) const {
    return mJoints[idx];
  }
  size_t GetNumJointBindings(void) const {
    return mJoints.size();
  }

  void dump() const;

  orkvector<PoolString> mJoints;
  orkvector<int> mJointSkelIndices;

  std::vector<xgmprimgroup_ptr_t> _primgroups;
  vtxbufferbase_ptr_t _vertexBuffer; // Our Models have 1 VB per cluster
  EVtxStreamFormat meVtxStrFmt;

  AABox mBoundingBox;
  Sphere mBoundingSphere;
};

using xgmcluster_ptr_t      = std::shared_ptr<XgmCluster>;
using xgmcluster_ptr_list_t = std::vector<xgmcluster_ptr_t>;

///////////////////////////////////////////////////////////////////////////////

struct XgmSubMesh final // Run Time Cluster Set
{

  material_ptr_t _material;
  xgmcluster_ptr_list_t _clusters;
  XgmMesh* _parentmesh = nullptr;

  XgmSubMesh()
      : _material(nullptr) {
  }

  ~XgmSubMesh();

  int GetNumClusters(void) const {
    return _clusters.size();
  }
  xgmcluster_ptr_t cluster(int idx) const {
    return _clusters[idx];
  }
  material_ptr_t GetMaterial(void) const {
    return _material;
  }

  void dump() const;
}; // namespace ork::lev2

using xgmsubmesh_ptr_t = std::shared_ptr<XgmSubMesh>;

///////////////////////////////////////////////////////////////////////////////

struct XgmMesh final {

  /////////////////////////////////////
  XgmMesh();
  XgmMesh(XgmMesh* pMesh);
  virtual ~XgmMesh();
  /////////////////////////////////////
  void SetBoundingBox(fvec4& Min, fvec4& Max) {
    mvBoundingBoxMin = Min;
    mvBoundingBoxMax = Max;
  }
  void SetFlags(U32 flags) {
    muFlags |= flags;
  }
  void SetMeshIndex(int i) {
    miMeshIndex = i;
  }
  void SetMeshName(const PoolString& name) {
    mMeshName = name;
  }
  int numSubMeshes(void) const {
    return int(mSubMeshes.size());
  }
  int meshIndex() const {
    return miMeshIndex;
  }
  U32 GetFlags(void) const {
    return muFlags;
  }
  bool CheckFlags(U32 flags) const {
    return ((flags & muFlags) == flags);
  }
  const XgmSubMesh* subMesh(int idx) const {
    return mSubMeshes[idx];
  }
  XgmSubMesh* subMesh(int idx) {
    return mSubMeshes[idx];
  }
  const PoolString& meshName() const {
    return mMeshName;
  }
  const fvec4& RefBoundingBoxMin(void) const {
    return mvBoundingBoxMin;
  }
  const fvec4& RefBoundingBoxMax(void) const {
    return mvBoundingBoxMax;
  }
  /////////////////////////////////////
  void ReserveSubMeshes(int icount) {
    mSubMeshes.reserve(icount);
  }
  void AddSubMesh(XgmSubMesh* psubmesh) {
    psubmesh->_parentmesh = this;
    mSubMeshes.push_back(psubmesh);
  }
  void dump() const;
  /////////////////////////////////////

  orkvector<XgmSubMesh*> mSubMeshes;
  fvec4 mvBoundingBoxMin;
  fvec4 mvBoundingBoxMax;
  U32 muFlags;
  int miNumBoneBindings; // for ps2 truth only
  F32 mfBoundingRadius;
  fvec4 mvBoundingCenter;
  PoolString mMeshName;
  int miMeshIndex;
};

///////////////////////////////////////////////////////////////////////////////

struct XgmModel final {

  /////////////////////////////////////

  void ReserveMeshes(int icount) {
    mMeshes.reserve(icount);
  }
  void AddMesh(const PoolString& name, XgmMesh* pmesh) {
    mMeshes.AddSorted(name, pmesh);
  }

  /////////////////////////////////////

  XgmModel();
  ~XgmModel();

  /////////////////////////////////////

  bool isSkinned() const {
    return mbSkinned;
  }
  void SetSkinned(bool bv) {
    mbSkinned = bv;
  }

  /////////////////////////////////////

  int numMeshes() const {
    return int(mMeshes.size());
  }
  int GetNumMaterials() const {
    return miNumMaterials;
  }

  const XgmSkeleton& skeleton() const {
    return mSkeleton;
  }
  XgmSkeleton& skeleton() {
    return mSkeleton;
  }

  const XgmMesh* mesh(int idx) const {
    return mMeshes.GetItemAtIndex(idx).second;
  }
  XgmMesh* mesh(int idx) {
    return mMeshes.GetItemAtIndex(idx).second;
  }

  const XgmMesh* mesh(const PoolString& name) const {
    return mMeshes.find(name)->second;
  }
  XgmMesh* mesh(const PoolString& name) {
    return mMeshes.find(name)->second;
  }
  int meshIndex(const PoolString& name) const;

  void* GetUserData() {
    return mpUserData;
  }
  material_constptr_t GetMaterial(int idx) const {
    return mvMaterials[idx];
  }
  material_ptr_t GetMaterial(int idx) {
    return mvMaterials[idx];
  }
  void AddMaterial(material_ptr_t hM);

  const fvec3& boundingCenter() const {
    return mBoundingCenter;
  }
  float GetBoundingRadius() const {
    return mBoundingRadius;
  }
  const fvec3& GetBoundingAA_XYZ() const {
    return mAABoundXYZ;
  }
  const fvec3& boundingAA_WHD() const {
    return mAABoundWHD;
  }

  int GetBonesPerCluster() const {
    return miBonesPerCluster;
  }

  /////////////////////////////////////

  void SetBoundingCenter(const fvec3& v) {
    mBoundingCenter = v;
  }
  void SetBoundingRadius(float v) {
    mBoundingRadius = v;
  }
  void SetBoundingAA_XYZ(const fvec3& v) {
    mAABoundXYZ = v;
  }
  void SetBoundingAA_WHD(const fvec3& v) {
    mAABoundWHD = v;
  }

  void SetBonesPerCluster(int i) {
    miBonesPerCluster = i;
  }

  /////////////////////////////////////

  void BeginRigidBlock(
      const fcolor4& ModColor,
      const fmtx4& WorldMat,
      ork::lev2::Context* pTARG,
      const RenderContextInstData& MatCtx,
      const RenderContextInstModelData& MdlCtx) const;
  void EndRigidBlock() const;
  void RenderRigidBlockItem() const;

  void RenderRigid(
      const fcolor4& ModColor,
      const fmtx4& WorldMat,
      ork::lev2::Context* pTARG,
      const RenderContextInstData& MatCtx,
      const RenderContextInstModelData& MdlCtx) const;

  void RenderSkinned(
      const XgmModelInst* minst,
      const fcolor4& ModColor,
      const fmtx4& WorldMat,
      ork::lev2::Context* pTARG,
      const RenderContextInstData& MatCtx,
      const RenderContextInstModelData& MdlCtx) const;

  /////////////////////////////////////

  static bool LoadUnManaged(XgmModel* mdl, const AssetPath& fname, asset::vars_constptr_t vars=nullptr);
  static bool _loaderSelect(XgmModel* mdl, datablock_ptr_t dblock);
  static bool _loadXGM(XgmModel* mdl, datablock_ptr_t dblock);
  static bool _loadAssimp(XgmModel* mdl, datablock_ptr_t dblock);

  /////////////////////////////////////

  void dump() const;

  PoolString GetModelName() const {
    return msModelName;
  }

  orklut<PoolString, XgmMesh*> mMeshes;
  orkvector<material_ptr_t> mvMaterials;
  int miBonesPerCluster;
  XgmSkeleton mSkeleton;
  void* mpUserData;
  int miNumMaterials;
  PoolString msModelName;
  fvec3 mAABoundXYZ;
  fvec3 mAABoundWHD;
  fvec3 mBoundingCenter;
  float mBoundingRadius;
  bool mbSkinned;
  asset::vars_ptr_t _varmap;
};

using model_ptr_t      = std::shared_ptr<XgmModel>;
using model_constptr_t = std::shared_ptr<const XgmModel>;

///////////////////////////////////////////////////////////////////////////////

struct XgmModelInst final {

  XgmModelInst(const XgmModel* Model);
  ~XgmModelInst();

  const XgmModel* xgmModel(void) const {
    return mXgmModel;
  }
  int GetNumChannels(void) const;

  XgmLocalPose& RefLocalPose() {
    return mLocalPose;
  }
  const XgmLocalPose& RefLocalPose() const {
    return mLocalPose;
  }

  XgmMaterialStateInst& RefMaterialInst() {
    return mMaterialStateInst;
  }
  const XgmMaterialStateInst& RefMaterialInst() const {
    return mMaterialStateInst;
  }

  void EnableMesh(const PoolString& ps);
  void DisableMesh(const PoolString& ps);

  void EnableMesh(int index);
  void DisableMesh(int index);

  void EnableAllMeshes();
  void DisableAllMeshes();

  bool isMeshEnabled(int index);
  bool isMeshEnabled(const PoolString& ps);
  bool IsAnyMeshEnabled();
  bool isSkinned() const {
    return mbSkinned;
  }
  void EnableSkinning() {
    mbSkinned = true;
  }
  bool IsBlenderZup() const {
    return mBlenderZup;
  }
  void SetBlenderZup(bool bv) {
    mBlenderZup = bv;
  }

  static const int knummaskbytes = 32;
  U8 mMaskBits[knummaskbytes];
  const XgmModel* mXgmModel;
  XgmLocalPose mLocalPose;
  mutable XgmWorldPose _worldPose;
  XgmMaterialStateInst mMaterialStateInst;
  material_ptr_t _overrideMaterial;
  int miNumChannels;
  bool mbSkinned;
  bool mBlenderZup;
  bool _drawSkeleton;
};

using xgmmodelinst_ptr_t      = std::shared_ptr<XgmModelInst>;
using xgmmodelinst_constptr_t = std::shared_ptr<const XgmModelInst>;

///////////////////////////////////////////////////////////////////////////////

struct RenderContextInstModelData final {
  xgmmodelinst_constptr_t _modelinst;
  const XgmMesh* mMesh;
  const XgmSubMesh* mSubMesh;
  xgmcluster_ptr_t _cluster;

  bool mbisSkinned;
  int miSubMeshIndex;

  //////////////////////////////////////
  // model interface
  //////////////////////////////////////

  bool isSkinned(void) const {
    return mbisSkinned;
  }
  void SetSkinned(bool bv) {
    mbisSkinned = bv;
  }

  void SetModelInst(xgmmodelinst_constptr_t pinst) {
    _modelinst = pinst;
  }
  xgmmodelinst_constptr_t GetModelInst(void) const {
    return _modelinst;
  }

  //////////////////////////////////////

  RenderContextInstModelData();
};

///////////////////////////////////////////////////////////////////////////////

bool SaveXGM(const AssetPath& Filename, const lev2::XgmModel* mdl);
datablock_ptr_t writeXgmToDatablock(const lev2::XgmModel* mdl);

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
