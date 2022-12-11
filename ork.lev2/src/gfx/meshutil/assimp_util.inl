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
///////////////////////////////////////////////////////////////////////////////

namespace ork::meshutil {

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
    return _xgmskelmap.find(remapSkelName(_rootname))->second;
  }
  //////////////////////////////////////////////////////////////
  std::string _rootname;
  skelnodemap_t _xgmskelmap;
  bool _isSkinned = false;
};
using parsedskeletonptr_t = std::shared_ptr<ParsedSkeleton>;

///////////////////////////////////////////////////////////////////////////////
// parseSkeleton: create and link skeleton
///////////////////////////////////////////////////////////////////////////////

inline parsedskeletonptr_t parseSkeleton(const aiScene* scene) {

  auto rval = std::make_shared<ParsedSkeleton>();

  std::set<std::string> uniqskelnodeset;

  skelnodemap_t& xgmskelnodes = rval->_xgmskelmap;

  /////////////////////////////////////////////////
  // get nodes
  /////////////////////////////////////////////////

  auto ainode_stack = std::queue<aiNode*>();
  ainode_stack.push(scene->mRootNode);

  rval->_rootname = scene->mRootNode->mName.data;

  //deco::printf(fvec3::Green(), "//////////////////////////////////////////////////\n");
  //deco::printf(fvec3::Green(), "// Parsing Assimp Skeleton\n");
  //deco::printf(fvec3::Green(), "//////////////////////////////////////////////////\n");

  //deco::printf(fvec3::Green(), "// traversing nodes\n");

  while (not ainode_stack.empty()) {
    auto n = ainode_stack.front();
    ainode_stack.pop();
    auto name = remapSkelName(n->mName.data);
    auto itb  = uniqskelnodeset.find(name);
    if (itb == uniqskelnodeset.end()) {
      int index = uniqskelnodeset.size();
      uniqskelnodeset.insert(name);
      auto xgmnode         = std::make_shared<lev2::XgmSkelNode>(name);
      xgmnode->miSkelIndex = index;
      //deco::printf(fvec3::White(), "aiNode<%d:%s> xgmnode<%p> remapped<%s>\n", index, n->mName.data, xgmnode, name.c_str());
      xgmnode->_nodeMatrix = convertMatrix44(n->mTransformation);
      //deco::printe(fvec3::Yellow(), xgmnode->_nodeMatrix.dump4x3(), true);
      xgmskelnodes[name]   = xgmnode;
    }
    for (int i = 0; i < n->mNumChildren; ++i) {
      ainode_stack.push(n->mChildren[i]);
    }
  }

  /////////////////////////////////////////////////
  // get bones
  /////////////////////////////////////////////////

  //deco::printf(fvec3::Green(), "// traversing bones\n");

  ainode_stack = std::queue<aiNode*>();
  ainode_stack.push(scene->mRootNode);
  while (not ainode_stack.empty()) {
    auto n = ainode_stack.front();
    ainode_stack.pop();
    for (int m = 0; m < n->mNumMeshes; ++m) {
      const aiMesh* mesh = scene->mMeshes[n->mMeshes[m]];
      for (int b = 0; b < mesh->mNumBones; b++) {
        auto bone     = mesh->mBones[b];
        auto bonename = remapSkelName(bone->mName.data);
        auto itb      = xgmskelnodes.find(bonename);
        OrkAssert(itb != xgmskelnodes.end());
        if (itb != xgmskelnodes.end()) {
          auto xgmnode = itb->second;
          if (false == xgmnode->_varmap["visited_bone"].isA<bool>()) {
            xgmnode->_varmap["visited_bone"].set<bool>(true);
            xgmnode->_assimpOffsetMatrix = convertMatrix44(bone->mOffsetMatrix);
            rval->_isSkinned             = true;
          }
        }
      }
    }
    // push child ai nodes onto stack
    for (int i = 0; i < n->mNumChildren; ++i) {
      ainode_stack.push(n->mChildren[i]);
    }
  }

  /////////////////////////////////////////////////
  // set parents
  /////////////////////////////////////////////////

  //deco::printf(fvec3::Green(), "// creating xgm topology\n");

  ainode_stack = std::queue<aiNode*>();
  ainode_stack.push(scene->mRootNode);
  while (not ainode_stack.empty()) {
    auto p = ainode_stack.front();
    ainode_stack.pop();
    //////////////////////////////
    auto itp = xgmskelnodes.find(remapSkelName(p->mName.data));
    OrkAssert(itp != xgmskelnodes.end());
    auto pskelnode = itp->second;
    //////////////////////////////
    for (int i = 0; i < p->mNumChildren; ++i) {
      auto c   = p->mChildren[i];
      auto itc = xgmskelnodes.find(remapSkelName(c->mName.data));
      OrkAssert(itc != xgmskelnodes.end());
      auto cskelnode = itc->second;

      if (false == cskelnode->_varmap["visited_2nd"].isA<bool>()) {
        cskelnode->_varmap["visited_2nd"].set<bool>(true);
        ainode_stack.push(c);
        cskelnode->_parent = pskelnode;
        pskelnode->_children.push_back(cskelnode);
      }
    }
    /////////////////////////////////////////////////
  } // while (not ainode_stack.empty())
  /////////////////////////////////////////////////

  auto root = rval->rootXgmSkelNode();

  root->_jointMatrix = root->_assimpOffsetMatrix.inverse();

  /////////////////////////////////////////////////
  // set joints matrices from nodes
  /////////////////////////////////////////////////

  lev2::XgmSkelNode::visitHierarchy(root,[](lev2::xgmskelnode_ptr_t node) {
    fmtx4 Bc = node->concatenated_node();
    auto par = node->_parent;
    fmtx4 Bp = par ? par->concatenated_node() : fmtx4::Identity();
    fmtx4 J;
    J.correctionMatrix(Bp, Bc);
    J                  = fmtx4::multiply_ltor(Bp.inverse(),Bc);
    node->_jointMatrix = J;
  });

  /////////////////////////////////////////////////
  // set bindpose/inverse matrices (from _assimpOffsetMatrix)
  /////////////////////////////////////////////////

  lev2::XgmSkelNode::visitHierarchy(root,[](lev2::xgmskelnode_ptr_t node) { //
    node->_bindMatrixInverse = node->_assimpOffsetMatrix;
    node->_bindMatrix = node->_bindMatrixInverse.inverse();
  });

  /////////////////////////////////////////////////
  // debug dump
  /////////////////////////////////////////////////

  if(1){
    deco::printf(fvec3::Green(), "// result debug dump\n");

    lev2::XgmSkelNode::visitHierarchy(root,[](lev2::xgmskelnode_ptr_t node) {
      fmtx4 ASSO = node->_assimpOffsetMatrix;
      fmtx4 nodemtx    = node->_nodeMatrix;
      fmtx4 nodecat    = node->concatenated_node();  // object space
      fmtx4 Bi   = node->_bindMatrixInverse;
      fmtx4 Bc   = node->_bindMatrix;
      auto par   = node->_parent;
      fmtx4 Bp   = par ? par->_bindMatrix : fmtx4::Identity();
      fmtx4 J    = node->_jointMatrix;
      fmtx4 O   = node->concatenated_joint(); // object space
      auto n     = node->_name;
      auto pn    = par ? par->_name : "---";
      deco::printe(fvec3::White(), n + ".par: " + pn, true);
      deco::printe(fvec3::White(), n + ".ASSO: " + ASSO.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + ".ASSOi: " + ASSO.inverse().dump4x3cn(), true);
      deco::printe(fvec3::White(), n + ".nodemtx: " + nodemtx.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + ".nodecat: " + nodecat.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + ".J: " + J.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + ".Bi: " + Bi.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + ".Bc: " + Bc.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + ".Bp: " + Bp.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + ".O: " + O.dump4x3cn(), true);
      printf("\n");
    });
  }


  /////////////////////////////////////////////////

   //bool fixup_applied = lev2::XgmSkelNode::applyCentimeterToMeterScale(root);

   //OrkAssert(false);
  return rval;
}

// bone   len<1.998>
// bone.1 len<1.42>
// bone.2 len<1.45>
// bone.3 len<1.18>

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::meshutil
