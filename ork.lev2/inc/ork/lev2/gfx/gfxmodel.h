////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
  std::string _origname;
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

///////////////////////////////////////////////////////////////////////////////

struct XgmCluster final { // Run Time Cluster
  XgmCluster();
  virtual ~XgmCluster();
  void Dump(void);

  inline size_t numPrimGroups() const {
    return _primgroups.size();
  }
  inline xgmprimgroup_ptr_t primgroup(int idx) const {
    return _primgroups[idx];
  }
  vtxbufferbase_ptr_t GetVertexBuffer() const {
    return _vertexBuffer;
  }
  const std::string& jointBinding(int idx) const {
    return _jointPaths[idx];
  }
  size_t numJointBindings() const {
    return _jointPaths.size();
  }

  void dump() const;

  orkvector<std::string> _jointPaths;
  orkvector<int> mJointSkelIndices;

  std::vector<xgmprimgroup_ptr_t> _primgroups;
  vtxbufferbase_ptr_t _vertexBuffer; // Our Models have 1 VB per cluster
  EVtxStreamFormat meVtxStrFmt;

  AABox mBoundingBox;
  Sphere mBoundingSphere;
};

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


///////////////////////////////////////////////////////////////////////////////

struct XgmSubMeshInst {

  XgmSubMeshInst(const XgmSubMesh* submesh);

  material_ptr_t material() const;
  void overrideMaterial(material_ptr_t m);

  const XgmSubMesh* _submesh = nullptr;
  bool _enabled = true;
  fxpipelinecache_constptr_t _fxpipelinecache;
  material_ptr_t _override_material;
};


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
  xgmsubmesh_constptr_t subMesh(int idx) const {
    return mSubMeshes[idx];
  }
  xgmsubmesh_ptr_t subMesh(int idx) {
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
  void AddSubMesh(xgmsubmesh_ptr_t psubmesh) {
    psubmesh->_parentmesh = this;
    mSubMeshes.push_back(psubmesh);
  }
  void dump() const;
  /////////////////////////////////////

  orkvector<xgmsubmesh_ptr_t> mSubMeshes;
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
  void AddMesh(const PoolString& name, xgmmesh_ptr_t pmesh) {
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
    return *_skeleton;
  }
  XgmSkeleton& skeleton() {
    return *_skeleton;
  }

  xgmmesh_constptr_t mesh(int idx) const {
    return mMeshes.GetItemAtIndex(idx).second;
  }
  xgmmesh_ptr_t mesh(int idx) {
    return mMeshes.GetItemAtIndex(idx).second;
  }

  xgmmesh_constptr_t mesh(const PoolString& name) const {
    return mMeshes.find(name)->second;
  }
  xgmmesh_ptr_t mesh(const PoolString& name) {
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
      const RenderContextInstModelData& RCID) const;
  void EndRigidBlock() const;
  void RenderRigidBlockItem() const;

  void RenderRigid(
      const fcolor4& ModColor,
      const fmtx4& WorldMat,
      ork::lev2::Context* pTARG,
      const RenderContextInstData& MatCtx,
      const RenderContextInstModelData& RCID) const;

  void RenderSkinned(
      const XgmModelInst* minst,
      const fcolor4& ModColor,
      const fmtx4& WorldMat,
      ork::lev2::Context* pTARG,
      const RenderContextInstData& MatCtx,
      const RenderContextInstModelData& RCID) const;

  void RenderSkeleton(
      const XgmModelInst* minst,
      const fmtx4& WorldMat,
      ork::lev2::Context* pTARG,
      const RenderContextInstData& RCID) const;

  bool intersectBoundingBox(const fray3& ray, fvec3& isect_in, fvec3& isect_out)
  {
    for(int i = 0; i < 3 ; i++)
    {
      _aabb.mMin[i] = mAABoundXYZ[i] - mAABoundWHD[i];
      _aabb.mMax[i] = mAABoundXYZ[i] + mAABoundWHD[i]; 
    }
    return _aabb.Intersect(ray, isect_in, isect_out);
  }
  /////////////////////////////////////

  static bool LoadUnManaged(XgmModel* mdl, const AssetPath& fname, asset::vars_ptr_t vars);
  static bool _loaderSelect(XgmModel* mdl, datablock_ptr_t dblock);
  static bool _loadXGM(XgmModel* mdl, datablock_ptr_t dblock);
  static bool _loadAssimp(XgmModel* mdl, datablock_ptr_t dblock);
  static bool _loadOrkScene(XgmModel* mdl, datablock_ptr_t datablock);

  /////////////////////////////////////

  void dump() const;

  PoolString GetModelName() const {
    return msModelName;
  }

  asset::loadrequest_ptr_t _getLoadRequest();

  orklut<PoolString, xgmmesh_ptr_t> mMeshes;
  orkvector<material_ptr_t> mvMaterials;
  int miBonesPerCluster;
  xgmskeleton_ptr_t _skeleton;
  void* mpUserData;
  int miNumMaterials;
  PoolString msModelName;
  fvec3 mAABoundXYZ;
  fvec3 mAABoundWHD;
  fvec3 mBoundingCenter;
  float mBoundingRadius;
  bool mbSkinned;
  asset::vars_t _varmap;
  XgmModelAsset* _asset = nullptr;
  AABox _aabb;
};


///////////////////////////////////////////////////////////////////////////////

struct  XgmMaterialOverrideMap{
  std::unordered_map<std::string,material_ptr_t> _mtl_map;
};


///////////////////////////////////////////////////////////////////////////////

struct XgmModelInst final {

  XgmModelInst(const XgmModel* Model);
  ~XgmModelInst();

  const XgmModel* xgmModel(void) const {
    return mXgmModel;
  }
  int GetNumChannels(void) const;

  XgmMaterialStateInst& RefMaterialInst() {
    return mMaterialStateInst;
  }
  const XgmMaterialStateInst& RefMaterialInst() const {
    return mMaterialStateInst;
  }

  void enableMesh(const PoolString& ps);
  void disableMesh(const PoolString& ps);

  void enableMesh(xgmmesh_constptr_t mesh);
  void disableMesh(xgmmesh_constptr_t mesh);

  void enableAllMeshes();
  void disableAllMeshes();

  bool isAnyMeshEnabled();
  bool isSkinned() const {
    return mbSkinned;
  }
  void enableSkinning() {
    mbSkinned = true;
  }
  bool isBlenderZup() const {
    return mBlenderZup;
  }
  void setBlenderZup(bool bv) {
    mBlenderZup = bv;
  }

  const XgmModel* mXgmModel;
  xgmlocalpose_ptr_t _localPose;
  xgmworldpose_ptr_t _worldPose;
  XgmMaterialStateInst mMaterialStateInst;
  int miNumChannels;
  bool mbSkinned;
  bool mBlenderZup;
  bool _drawSkeleton;

  std::vector<xgmsubmeshinst_ptr_t> _submeshinsts;

};


///////////////////////////////////////////////////////////////////////////////

struct RenderContextInstModelData final {
  xgmmodelinst_constptr_t _modelinst;
  const XgmMesh* mMesh;
  const XgmSubMesh* mSubMesh;
  xgmsubmeshinst_ptr_t _submeshinst;
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

void computeAmbientOcclusion( int numsamples, meshutil::mesh_ptr_t model, Context* ctx);
void computeLightMaps( meshutil::mesh_ptr_t model, Context* ctx );

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
