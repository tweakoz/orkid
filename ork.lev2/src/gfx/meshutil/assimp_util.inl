///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/cas.inl>
#include <ork/application/application.h>
#include <ork/math/plane.h>
#include <ork/kernel/spawner.h>
#include <ork/kernel/string/deco.inl>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/meshutil/clusterizer.h>
#include <ork/lev2/gfx/material_pbr.inl>
///////////////////////////////////////////////////////////////////////////////
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>
#include <assimp/material.h>
#include <assimp/GltfMaterial.h>
#include <deque>
///////////////////////////////////////////////////////////////////////////////

namespace ork::meshutil {

using bonemarkset_t = std::set<std::string>;

void configureXgmSkeleton (const ork::meshutil::Mesh& input, //
                           lev2::XgmModel& xgmmdlout);

///////////////////////////////////////////////////////////////////////////////

inline uint32_t assimpImportFlags() {
  uint32_t flags = aiProcessPreset_TargetRealtime_MaxQuality | //
                   aiProcess_LimitBoneWeights |                //
                   // aiProcess_GenNormals |                      //
                   aiProcess_CalcTangentSpace |        //
                   aiProcess_RemoveRedundantMaterials; // aiProcess_MakeLeftHanded
  return flags;
}

///////////////////////////////////////////////////////////////////////////////

inline fmtx4 convertMatrix44(const aiMatrix4x4& inp) {
  fmtx4 rval;
  for (int i = 0; i < 16; i++) {
    int x = i % 4;
    int y = i / 4;
    rval.setElemXY(x, y, inp[y][x]);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

inline void
get_bounding_box_for_node(const aiScene* scene, //
                          const aiNode* nd, //
                          aiVector3D& min, //
                          aiVector3D& max, //
                          aiMatrix4x4& transform_register) { //
  aiMatrix4x4 prev;
  unsigned int n = 0, t;

  prev = transform_register;
  aiMultiplyMatrix4(&transform_register, &nd->mTransformation);

  for (; n < nd->mNumMeshes; ++n) {
    const aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]];

    for (t = 0; t < mesh->mNumVertices; ++t) {

      aiVector3D tmp = mesh->mVertices[t];
      aiTransformVecByMatrix4(&tmp, &transform_register);

      min.x = std::min(min.x, tmp.x);
      min.y = std::min(min.y, tmp.y);
      min.z = std::min(min.z, tmp.z);

      max.x = std::max(max.x, tmp.x);
      max.y = std::max(max.y, tmp.y);
      max.z = std::max(max.z, tmp.z);
    }
  }

  for (n = 0; n < nd->mNumChildren; ++n) {
    get_bounding_box_for_node(scene, nd->mChildren[n], min, max, transform_register);
  }
  transform_register = prev;
}

///////////////////////////////////////////////////////////////////////////////

struct GltfMaterial {
  std::string _name;
  std::string _metallicAndRoughnessMap;
  std::string _colormap;
  std::string _normalmap;
  std::string _amboccmap;
  std::string _emissivemap;
  float _metallicFactor  = 0.0f;
  float _roughnessFactor = 1.0f;
  fvec4 _baseColor       = fvec4(1, 1, 1, 1);
};

using gltfmaterialmap_t = std::map<int, GltfMaterial*>;
using skelnodemap_t = std::map<std::string, lev2::xgmskelnode_ptr_t>;

///////////////////////////////////////////////////////////////////////////////

inline std::string remapSkelName(std::string inp) {
  // fixup blender naming
  auto remapped_name = ork::string::replaced(inp, "Armature_", "");
  remapped_name      = ork::string::replaced(remapped_name, "_", ".");
  return remapped_name;
}

///////////////////////////////////////////////////////////////////////////////

struct ParsedSkeleton {
  //////////////////////////////////////////////////////////////
  inline lev2::xgmskelnode_ptr_t rootXgmSkelNode() {
    return _xgmskelmap_by_path.find(_rootpath)->second;
  }
  //////////////////////////////////////////////////////////////
  std::string _rootname;
  std::string _rootpath;
  skelnodemap_t _xgmskelmap_by_name;
  skelnodemap_t _xgmskelmap_by_path;
  skelnodemap_t _xgmskelmap_by_id;
  bool _isSkinned = false;
};
using parsedskeletonptr_t = std::shared_ptr<ParsedSkeleton>;

///////////////////////////////////////////////////////////////////////////////
// parseSkeleton: create and link skeleton
///////////////////////////////////////////////////////////////////////////////

parsedskeletonptr_t parseSkeleton(const aiScene* scene);

// bone   len<1.998>
// bone.1 len<1.42>
// bone.2 len<1.45>
// bone.3 len<1.18>

inline int compute_aiNodeDepth(const aiNode* node, int depth = 0) {
    if (node == nullptr) {
        return -1; // Invalid input
    }
    if( node->mParent == nullptr )
        return depth;

    return compute_aiNodeDepth(node->mParent, depth + 1);
}

using ainode_visitorfn_t = std::function<void(const aiNode*, int depth)>;

void visit_ainodes_down(const aiNode* node, int depth, ainode_visitorfn_t visitor);
void visit_ainodes_up(const aiNode* node, int depth, ainode_visitorfn_t visitor);
std::string aiNodePathName(const aiNode* node);
std::deque<const aiNode*> aiNodePath(const aiNode* node);
///////////////////////////////////////////////////////////////////////////////

template <typename ClusterizerType>
void clusterizeToolMeshToXgmMesh(const ork::meshutil::Mesh& inp_model, ork::lev2::XgmModel& out_model);

} // namespace ork::meshutil
