////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/util/crc.h>
#include <ork/util/crc64.h>
#include <orktool/filter/filter.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/box.h>
#include <algorithm>
#include <ork/kernel/Array.h>
#include <ork/kernel/varmap.inl>

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/submesh.h>
#include <unordered_map>
#include <ork/kernel/datablock.inl>

namespace ork::tool {
struct ColladaMaterial;
struct DaeReadOpts;
struct DaeWriteOpts;
} // namespace ork::tool

///////////////////////////////////////////////////////////////////////////////

namespace ork::MeshUtil {

struct XgmClusterizer;
struct XgmClusterBuilder;

struct MaterialBindingItem {
  std::string mMaterialName;
  std::string mMaterialDaeId;
  // std::vector<FCDMaterialInstanceBind*> mBindings;
};

typedef orkmap<std::string, MaterialBindingItem> material_semanticmap_t;

class Light : public ork::Object {
  RttiDeclareAbstract(Light, ork::Object);

public:
  std::string mName;
  fmtx4 mWorldMatrix;
  fvec3 mColor;
  float mIntensity;
  float mShadowSamples;
  float mShadowBias;
  float mShadowBlur;
  bool mbSpecular;
  bool mbIsShadowCaster;

  virtual bool AffectsSphere(const fvec3& center, float radius) const {
    return false;
  }
  virtual bool AffectsAABox(const AABox& aab) const {
    return false;
  }

  Light()
      : mColor(1.0f, 1.0f, 1.0f)
      , mIntensity(1.0f)
      , mbSpecular(false)
      , mShadowSamples(1.0f)
      , mShadowBlur(0.0f)
      , mShadowBias(0.2f)
      , mbIsShadowCaster(false) {
  }
};

///////////////////////////////////////////////////////////////////////////////

class LightContainer : public ork::Object {
  RttiDeclareConcrete(LightContainer, ork::Object);

public:
  orklut<PoolString, Light*> mLights;
};

///////////////////////////////////////////////////////////////////////////////

class toolmesh;

///////////////////////////////////////////////////////////////////////////////

struct toolmesh {

  /////////////////////////////////////////////////////////////////////////

  toolmesh();
  ~toolmesh();

  void Dump(const std::string& comment) const;

  /////////////////////////////////////////////////////////////////////////

  void SetMergeEdges(bool bflg) {
    mbMergeEdges = bflg;
  }

  const ork::lev2::MaterialMap& RefFxmMaterialMap() const {
    return mFxmMaterialMap;
  }
  const orkmap<std::string, ork::tool::ColladaMaterial*>& RefMaterialsBySG() const {
    return mMaterialsByShadingGroup;
  }
  const orkmap<std::string, ork::tool::ColladaMaterial*>& RefMaterialsByName() const {
    return mMaterialsByName;
  }
  const LightContainer& RefLightContainer() const {
    return mLights;
  }
  LightContainer& RefLightContainer() {
    return mLights;
  }

  void CopyMaterialsFromToolMesh(const toolmesh& from);
  void MergeMaterialsFromToolMesh(const toolmesh& from);

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
  void WriteToDaeFile(const file::Path& outpath, const tool::DaeWriteOpts& writeopts) const;
  void WriteToRgmFile(const file::Path& outpath) const;
  void WriteToXgmFile(const file::Path& outpath) const;
  void WriteAuto(const file::Path& outpath) const;
  /////////////////////////////////////////////////////////////////////////
  void ReadFromXGM(const file::Path& inpath);
  void ReadFromWavefrontObj(const file::Path& inpath);
  void ReadFromDaeFile(const file::Path& inpath, tool::DaeReadOpts& readopts);
  void readFromAssimp(const file::Path& inpath, tool::DaeReadOpts& readopts);

  /////////////////////////////////////////////////////////////////////////

  void ReadAuto(const file::Path& outpath);

  /////////////////////////////////////////////////////////////////////////

  AABox GetAABox() const;

  /////////////////////////////////////////////////////////////////////////

  void SetRangeTransform(const fvec4& VScale, const fvec4& VTrans);

  /////////////////////////////////////////////////////////////////////////

  void MergeToolMeshAs(const toolmesh& sr, const char* pgroupname);
  void MergeToolMeshThreadedExcluding(const toolmesh& sr, int inumthreads, const std::set<std::string>& ExcludeSet);
  void MergeToolMeshThreaded(const toolmesh& sr, int inumthreads);
  void MergeSubMesh(const toolmesh& src, const submesh* pgrp, const char* newname);
  void MergeSubMesh(const submesh& pgrp, const char* newname);
  void MergeSubMesh(const submesh& pgrp);
  submesh& MergeSubMesh(const char* pname);
  submesh& MergeSubMesh(const char* pname, const AnnotationMap& merge_annos);

  /////////////////////////////////////////////////////////////////////////

  const orklut<std::string, submesh*>& RefSubMeshLut() const;
  const material_semanticmap_t& RefShadingGroupToMaterialMap() const {
    return mShadingGroupToMaterialMap;
  }
  material_semanticmap_t& RefShadingGroupToMaterialMap() {
    return mShadingGroupToMaterialMap;
  }

  int numSubMeshes() const {
    return int(mPolyGroupLut.size());
  }

  const submesh* FindSubMeshFromMaterialName(const std::string& materialname) const;
  submesh* FindSubMeshFromMaterialName(const std::string& materialname);
  const submesh* FindSubMesh(const std::string& grpname) const;
  submesh* FindSubMesh(const std::string& grpname);

  /////////////////////////////////////////////////////////////////////////

  varmap::VarMap _varmap;
  orkmap<std::string, ork::tool::ColladaMaterial*> mMaterialsByShadingGroup;
  orkmap<std::string, ork::tool::ColladaMaterial*> mMaterialsByName;

  AABox _vertexExtents;
  AABox _skeletonExtents;
  fvec4 mRangeScale;
  fvec4 mRangeTranslate;
  fmtx4 mMatRange;
  orkmap<std::string, std::string> mAnnotations;
  orklut<std::string, submesh*> mPolyGroupLut;
  material_semanticmap_t mShadingGroupToMaterialMap;
  LightContainer mLights;
  bool mbMergeEdges;
  ork::lev2::MaterialMap mFxmMaterialMap;

private:
  toolmesh(const toolmesh& oth) {
    OrkAssert(false);
  }
};

///////////////////////////////////////////////////////////////////////////////
/*
struct TriStripperPrimGroup
{
    orkvector<unsigned int> mIndices;
};

class TriStripper
{
    triangle_stripper::tri_stripper tristripper;
    orkvector<TriStripperPrimGroup> mStripGroups;
    TriStripperPrimGroup			mTriGroup;

public:

    TriStripper( const std::vector<unsigned int> &InTriIndices, int icachesize, int iminstripsize );

    const orkvector<TriStripperPrimGroup> & GetStripGroups( void ) const
    {
        return mStripGroups;
    }

    const orkvector<unsigned int> & GetStripIndices( int igroup ) const
    {
        return mStripGroups[igroup].mIndices;
    }

    const orkvector<unsigned int> & GetTriIndices( void ) const { return mTriGroup.mIndices; }

};
*/

///////////////////////////////////////////////////////////////////////////////

class OBJ_OBJ_Filter : public ork::tool::AssetFilterBase {
  RttiDeclareConcrete(OBJ_OBJ_Filter, ork::tool::AssetFilterBase);

public: //
  OBJ_OBJ_Filter();
  bool ConvertAsset(const tokenlist& toklist) final;
};
class XGM_OBJ_Filter : public ork::tool::AssetFilterBase {
  RttiDeclareConcrete(XGM_OBJ_Filter, ork::tool::AssetFilterBase);

public: //
  XGM_OBJ_Filter();
  bool ConvertAsset(const tokenlist& toklist) final;
};
class OBJ_XGM_Filter : public ork::tool::AssetFilterBase {
  RttiDeclareConcrete(OBJ_XGM_Filter, ork::tool::AssetFilterBase);

public: //
  OBJ_XGM_Filter();
  bool ConvertAsset(const tokenlist& toklist) final;
};

class ASS_XGM_Filter : public ork::tool::AssetFilterBase {
  RttiDeclareConcrete(ASS_XGM_Filter, ork::tool::AssetFilterBase);

public: //
  ASS_XGM_Filter();
  bool ConvertAsset(const tokenlist& toklist) final;
};
class ASS_XGA_Filter : public ork::tool::AssetFilterBase {
  RttiDeclareConcrete(ASS_XGA_Filter, ork::tool::AssetFilterBase);

public: //
  ASS_XGA_Filter();
  bool ConvertAsset(const tokenlist& toklist) final;
};

///////////////////////////////////////////////////////////////////////////////

class AmbientLight : public Light {
  RttiDeclareConcrete(AmbientLight, Light);

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
  RttiDeclareConcrete(DirLight, Light);

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
  RttiDeclareConcrete(PointLight, Light);

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
  lev2::EVtxStreamFormat evtxformat;
  orkvector<int> TrianglePolyIndices;
  orkvector<int> QuadPolyIndices;
  orkvector<int> MergeTriIndices;
  orkvector<lev2::SVtxV12N12B12T16> MergeVertsT16;
  orkvector<lev2::SVtxV12N12B12T8C4> MergeVertsT8;

  int inumverts;
  int ivtxsize;
  void* poutvtxdata;

  FlatSubMesh(const submesh& mesh);
};

///////////////////////////////////////////////////////////////////////////////

struct ToolMeshConfigurationFlags {
  bool mbSkinned;

  ToolMeshConfigurationFlags()
      : mbSkinned(false) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ToolMaterialGroup {
  enum EMatClass {
    EMATCLASS_STANDARD = 0,
    EMATCLASS_PBR,
    EMATCLASS_FX,
  };

  void Parse(const ork::tool::ColladaMaterial& colladamat);

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

  ToolMaterialGroup()
      : meMaterialClass(EMATCLASS_STANDARD)
      , _orkMaterial(0)
      , _clusterizer(nullptr)
      , mbVertexLit(false) {
  }

  ///////////////////////////////////////////////////////////////////

  XgmClusterizer* _clusterizer;
  std::string mShadingGroupName;
  ToolMeshConfigurationFlags mMeshConfigurationFlags;
  EMatClass meMaterialClass;
  ork::lev2::GfxMaterial* _orkMaterial;
  orkvector<ork::lev2::VertexConfig> mVertexConfigData;
  orkvector<ork::lev2::VertexConfig> mAvailVertexConfigData;
  lev2::EVtxStreamFormat meVtxFormat;
  ork::file::Path mLightMapPath;
  bool mbVertexLit;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::MeshUtil
