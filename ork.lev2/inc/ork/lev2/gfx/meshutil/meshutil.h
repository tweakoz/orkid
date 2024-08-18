////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <unordered_map>
#include <algorithm>

#include <ork/pch.h>
#include <ork/util/crc.h>
#include <ork/util/crc64.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/box.h>
#include <ork/kernel/Array.h>
#include <ork/kernel/varmap.inl>

#include <ork/lev2/lev2_types.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/kernel/datablock.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::meshutil {

using bonename_set_t = std::set<std::string>;

struct MaterialBindingItem {
  std::string mMaterialName;
  std::string mMaterialDaeId;
  // std::vector<FCDMaterialInstanceBind*> mBindings;
};

class Light : public ork::Object {
  DeclareAbstractX(Light, ork::Object);

public:
  std::string mName;
  fvec3 mColor;
  float mIntensity;
  float mShadowSamples;
  float mShadowBias;
  float mShadowBlur;
  bool mbSpecular;
  bool mbIsShadowCaster;
  fmtx4 mWorldMatrix;

  virtual bool AffectsSphere(const fvec3& center, float radius) const {
    return false;
  }
  virtual bool AffectsAABox(const AABox& aab) const {
    return false;
  }

  Light()
      : mColor(1.0f, 1.0f, 1.0f)
      , mIntensity(1.0f)
      , mShadowSamples(1.0f)
      , mShadowBias(0.2f)
      , mShadowBlur(0.0f)
      , mbSpecular(false)
      , mbIsShadowCaster(false) {
  }
};

///////////////////////////////////////////////////////////////////////////////

class LightContainer : public ork::Object {
  DeclareConcreteX(LightContainer, ork::Object);

public:
  orklut<PoolString, Light*> mLights;
};

///////////////////////////////////////////////////////////////////////////////

struct MaterialChannel {
  std::string mTextureName;
  std::string mPlacementNodeName;
  float mRepeatU;
  float mRepeatV;

  MaterialChannel()
      : mRepeatU(1.0f)
      , mRepeatV(1.0f) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct MaterialInfo {

  enum ELightingType { ELIGHTING_LAMBERT = 0, ELIGHTING_BLINN, ELIGHTING_PHONG, ELIGHTING_NONE };

  std::string mShadingGroupName;
  std::string mMaterialName;
  ELightingType mLightingType;
  float mSpecularPower;
  MaterialChannel mDiffuseMapChannel;
  MaterialChannel mSpecularMapChannel;
  MaterialChannel mNormalMapChannel;
  MaterialChannel mAmbientMapChannel;

  ork::lev2::GfxMaterial* _orkMaterial;
  fvec4 mEmissiveColor;
  fvec4 mTransparencyColor;
  orkmap<std::string, std::string> _annotations;

  MaterialInfo();
  virtual ~MaterialInfo() {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct MeshConfigurationFlags {
  bool _skinned = false;
};

///////////////////////////////////////////////////////////////////////////////

struct MaterialGroup {
  enum EMatClass {
    EMATCLASS_STANDARD = 0,
    EMATCLASS_PBR,
    EMATCLASS_FX,
  };

  void Parse(const MaterialInfo& colladamat);

  ///////////////////////////////////////////////////////////////////
  // Build Clusters
  ///////////////////////////////////////////////////////////////////

  lev2::EVtxStreamFormat GetVtxStreamFormat() const {
    return meVtxFormat;
  }

  void ComputeVtxStreamFormat();

  XgmClusterizer* GetClusterizer() const {
    return _clusterizer;
  }
  void SetClusterizer(XgmClusterizer* pcl) {
    _clusterizer = pcl;
  }

  ///////////////////////////////////////////////////////////////////

  MaterialGroup()
      : meMaterialClass(EMATCLASS_STANDARD)
      , _orkMaterial(0)
      , _clusterizer(nullptr)
      , mbVertexLit(false) {
  }

  ///////////////////////////////////////////////////////////////////

  EMatClass meMaterialClass;
  ork::lev2::GfxMaterial* _orkMaterial;
  XgmClusterizer* _clusterizer;
  bool mbVertexLit;

  std::string mShadingGroupName;
  MeshConfigurationFlags mMeshConfigurationFlags;
  orkvector<ork::lev2::VertexConfig> mVertexConfigData;
  orkvector<ork::lev2::VertexConfig> mAvailVertexConfigData;
  lev2::EVtxStreamFormat meVtxFormat;
  ork::file::Path mLightMapPath;
};

///////////////////////////////////////////////////////////////////////////////

struct Mesh {

  /////////////////////////////////////////////////////////////////////////

  Mesh();
  virtual ~Mesh();

  void Dump(const std::string& comment) const;

  /////////////////////////////////////////////////////////////////////////

  void SetMergeEdges(bool bflg) {
    _mergeEdges = bflg;
  }

  const ork::lev2::MaterialMap& RefFxmMaterialMap() const {
    return mFxmMaterialMap;
  }
  const material_info_map_t& materialInfosByShadingGroup() const {
    return _materialsByShadingGroup;
  }
  const material_info_map_t& materialInfosByName() const {
    return _materialsByName;
  }
  const LightContainer& RefLightContainer() const {
    return mLights;
  }
  LightContainer& RefLightContainer() {
    return mLights;
  }

  void CopyMaterialsFromToolMesh(const Mesh& from);
  void MergeMaterialsFromToolMesh(const Mesh& from);

  void RemoveSubMesh(const std::string& pgroup);
  void Prune();

  /////////////////////////////////////////////////////////////////////////

  void SplitSubMeshOnAnno(const submesh& inp, const std::string& annokey);
  void SplitSubMeshOnAnno(const submesh& inp, const std::string& prefix, const std::string& annokey);
  void SplitSubMeshOnAnno(const submesh& inp, const std::string& annokey, const AnnotationMap& MergeAnnos);

  /////////////////////////////////////////////////////////////////////////

  void SetAnnotation(const char* annokey, const char* annoval);
  const char* GetAnnotation(const char* annokey) const;

  /////////////////////////////////////////////////////////////////////////
  void WriteToWavefrontObj(const file::Path& outpath) const;
  /////////////////////////////////////////////////////////////////////////
  void ReadFromXGM(const file::Path& inpath);
  void ReadFromWavefrontObj(const file::Path& inpath);
  void readFromAssimp(const file::Path& inpath);
  void readFromAssimp(datablock_ptr_t datablock);
  /////////////////////////////////////////////////////////////////////////
  AABox GetAABox() const;
  /////////////////////////////////////////////////////////////////////////

  void SetRangeTransform(const fvec4& VScale, const fvec4& VTrans);

  /////////////////////////////////////////////////////////////////////////

  void MergeToolMeshAs(const Mesh& sr, const char* pgroupname);
  void MergeToolMeshThreadedExcluding(const Mesh& sr, int inumthreads, const std::set<std::string>& ExcludeSet);
  void MergeToolMeshThreaded(const Mesh& sr, int inumthreads);
  void MergeSubMesh(const Mesh& src, const submesh& pgrp, const char* newname);
  void MergeSubMesh(const submesh& pgrp, const char* newname);
  void MergeSubMesh(const submesh& pgrp);
  submesh& MergeSubMesh(const char* pname);
  submesh& MergeSubMesh(const char* pname, const AnnotationMap& merge_annos);

  /////////////////////////////////////////////////////////////////////////

  const submesh_lut_t& RefSubMeshLut() const;
  const material_semanticmap_t& RefShadingGroupToMaterialMap() const {
    return mShadingGroupToMaterialMap;
  }
  material_semanticmap_t& RefShadingGroupToMaterialMap() {
    return mShadingGroupToMaterialMap;
  }

  size_t numSubMeshes() const {
    return int(_submeshesByPolyGroup.size());
  }

  void dumpStats() const;

  submesh_constptr_t submeshFromMaterialName(const std::string& materialname) const;
  submesh_ptr_t submeshFromMaterialName(const std::string& materialname);
  submesh_constptr_t submeshFromGroupName(const std::string& grpname) const;
  submesh_ptr_t submeshFromGroupName(const std::string& grpname);

  /////////////////////////////////////////////////////////////////////////

  std::shared_ptr<varmap::VarMap> _varmap;
  material_info_map_t _materialsByShadingGroup;
  material_info_map_t _materialsByName;

  AABox _vertexExtents;
  AABox _skeletonExtents;
  fvec4 mRangeScale;
  fvec4 mRangeTranslate;
  fmtx4 mMatRange;
  fmtx4 _loadXF;
  orkmap<std::string, std::string> _annotations;
  submesh_lut_t _submeshesByPolyGroup;
  material_semanticmap_t mShadingGroupToMaterialMap;
  LightContainer mLights;
  bool _mergeEdges;
  ork::lev2::MaterialMap mFxmMaterialMap;

  Mesh(const Mesh& oth) {
    OrkAssert(false);
  }
};

///////////////////////////////////////////////////////////////////////////////

using mesh_transformer_t = std::function<mesh_ptr_t(mesh_ptr_t)>;
using mesh_transformer_pipe_t = std::vector<mesh_transformer_t>;

///////////////////////////////////////////////////////////////////////////////

class AmbientLight : public Light {
  DeclareConcreteX(AmbientLight, Light);

public:
  AmbientLight() {
  }
  bool AffectsSphere(const fvec3& center, float radius) const final {
    return true;
  }
  bool AffectsAABox(const AABox& aab) const final {
    return true;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct DirLight : public Light {
  DeclareConcreteX(DirLight, Light);

public:
  fvec3 mFrom;
  fvec3 mTo;

  DirLight() {
  }

  bool AffectsSphere(const fvec3& center, float radius) const final {
    return true;
  }
  bool AffectsAABox(const AABox& aab) const final {
    return true;
  }
};

///////////////////////////////////////////////////////////////////////////////

class PointLight : public Light {
  DeclareConcreteX(PointLight, Light);

public:
  fvec3 mPoint;
  float mFalloff;
  float mRadius;

  PointLight()
      : mFalloff(1.0f)
      , mRadius(0.0f) {
  }

  bool AffectsSphere(const fvec3& center, float radius) const final;
  bool AffectsAABox(const AABox& aab) const final;
};

///////////////////////////////////////////////////////////////////////////////

struct FlatSubMesh {
  FlatSubMesh(const submesh& mesh);
  FlatSubMesh( const AssetPath& from_path, lev2::rendervar_strmap_t assetvars );
  void fromSubmesh(const submesh& mesh);

  lev2::EVtxStreamFormat evtxformat;
  orkvector<int> TrianglePolyIndices;
  orkvector<int> QuadPolyIndices;
  orkvector<int> MergeTriIndices;
  orkvector<lev2::SVtxV12N12B12T16> MergeVertsT16;
  orkvector<lev2::SVtxV12N12B12T8C4> MergeVertsT8;

  int inumverts;
  int ivtxsize;
  void* poutvtxdata;
  AABox _aabox; 

};

///////////////////////////////////////////////////////////////////////////////

struct mupoly_clip_adapter {
  typedef vertex VertexType;

  orkvector<VertexType> mVerts;

  mupoly_clip_adapter() {
  }

  void AddVertex(const VertexType& v) {
    mVerts.push_back(v);
  }
  const VertexType& GetVertex(int idx) const {
    return mVerts[idx];
  }

  size_t GetNumVertices() const {
    return mVerts.size();
  }
};

void misc_init();

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::meshutil
