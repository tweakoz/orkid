///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/file/cfs.inl>
#include <ork/application/application.h>
#include <ork/math/plane.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/filter/filter.h>
#include <ork/kernel/spawner.h>
#include <ork/kernel/string/deco.inl>

///////////////////////////////////////////////////////////////////////////////
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>
#include <assimp/material.h>

#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/filter/gfx/meshutil/clusterizer.h>

#include <ork/lev2/gfx/material_pbr.inl>

namespace ork::MeshUtil {
///////////////////////////////////////////////////////////////////////////////

inline fmtx4 convertMatrix44(const aiMatrix4x4& inp) {
  fmtx4 rval;
  for (int i = 0; i < 16; i++) {
    int x = i % 4;
    int y = i / 4;
    rval.SetElemXY(x, y, inp[y][x]);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

inline void
get_bounding_box_for_node(const aiScene* scene, const aiNode* nd, aiVector3D& min, aiVector3D& max, aiMatrix4x4& trafo) {
  aiMatrix4x4 prev;
  unsigned int n = 0, t;

  prev = trafo;
  aiMultiplyMatrix4(&trafo, &nd->mTransformation);

  for (; n < nd->mNumMeshes; ++n) {
    const aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]];

    for (t = 0; t < mesh->mNumVertices; ++t) {

      aiVector3D tmp = mesh->mVertices[t];
      aiTransformVecByMatrix4(&tmp, &trafo);

      min.x = std::min(min.x, tmp.x);
      min.y = std::min(min.y, tmp.y);
      min.z = std::min(min.z, tmp.z);

      max.x = std::max(max.x, tmp.x);
      max.y = std::max(max.y, tmp.y);
      max.z = std::max(max.z, tmp.z);
    }
  }

  for (n = 0; n < nd->mNumChildren; ++n) {
    get_bounding_box_for_node(scene, nd->mChildren[n], min, max, trafo);
  }
  trafo = prev;
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

typedef std::map<int, GltfMaterial*> gltfmaterialmap_t;
typedef std::map<std::string, ork::lev2::XgmSkelNode*> skelnodemap_t;

///////////////////////////////////////////////////////////////////////////////

inline std::string remapSkelName(std::string inp) {
  // fixup blender naming
  auto remapped_name = ork::string::replaced(inp, "Armature_", "");
  remapped_name      = ork::string::replaced(remapped_name, "_", ".");
  return remapped_name;
}

///////////////////////////////////////////////////////////////////////////////

struct ParsedSkeleton {
  std::string _rootname;
  skelnodemap_t _xgmskelmap;
  bool _isSkinned = false;
  //////////////////////////////////////////////////////////////
  inline lev2::XgmSkelNode* rootXgmSkelNode() {
    return _xgmskelmap.find(remapSkelName(_rootname))->second;
  }
};
typedef std::shared_ptr<ParsedSkeleton> parsedskeletonptr_t;

///////////////////////////////////////////////////////////////////////////////

inline parsedskeletonptr_t parseSkeleton(const aiScene* scene) {

  auto rval = std::make_shared<ParsedSkeleton>();

  std::queue<aiNode*> nodestack;
  std::set<std::string> uniqskelnodeset;

  skelnodemap_t& xgmskelnodes = rval->_xgmskelmap;

  /////////////////////////////////////////////////
  // get nodes
  /////////////////////////////////////////////////

  nodestack = std::queue<aiNode*>();
  nodestack.push(scene->mRootNode);

  rval->_rootname = scene->mRootNode->mName.data;

  while (not nodestack.empty()) {
    auto n = nodestack.front();
    nodestack.pop();
    auto name = remapSkelName(n->mName.data);
    auto itb  = uniqskelnodeset.find(name);
    if (itb == uniqskelnodeset.end()) {
      int index = uniqskelnodeset.size();
      uniqskelnodeset.insert(name);
      auto xgmnode         = new ork::lev2::XgmSkelNode(name);
      xgmnode->miSkelIndex = index;
      xgmskelnodes[name]   = xgmnode;
      printf("aiNode<%d:%s> xgmnode<%p> remapped<%s>\n", index, n->mName.data, xgmnode, name.c_str());
      xgmnode->_nodeMatrix = convertMatrix44(n->mTransformation);
      deco::prints(xgmnode->_nodeMatrix.dump4x3(), true);
    }
    for (int i = 0; i < n->mNumChildren; ++i) {
      nodestack.push(n->mChildren[i]);
    }
  }

  /////////////////////////////////////////////////
  // get bones
  /////////////////////////////////////////////////

  nodestack = std::queue<aiNode*>();
  nodestack.push(scene->mRootNode);
  while (not nodestack.empty()) {
    auto n = nodestack.front();
    nodestack.pop();
    for (int m = 0; m < n->mNumMeshes; ++m) {
      const aiMesh* mesh = scene->mMeshes[n->mMeshes[m]];
      for (int b = 0; b < mesh->mNumBones; b++) {
        auto bone     = mesh->mBones[b];
        auto bonename = remapSkelName(bone->mName.data);
        auto itb      = xgmskelnodes.find(bonename);
        OrkAssert(itb != xgmskelnodes.end());
        if (itb != xgmskelnodes.end()) {
          auto xgmnode = itb->second;
          if (false == xgmnode->_varmap["visited_bone"].IsA<bool>()) {
            xgmnode->_varmap["visited_bone"].Set<bool>(true);
            //////////////////////////////////////////
            // according to what I read
            //  aiBone::mOffsetMatrix is the inverse bind pose
            //  an odd name to be sure. we shall see..
            //////////////////////////////////////////

            auto invbindpose = convertMatrix44(bone->mOffsetMatrix);
            // lev2::DecompMtx44 decomp1;
            // lev2::DecompMtx44 decomp2;
            // invbindpose.decompose(decomp1.mTrans, decomp1.mRot, decomp1.mScale);
            // invbindpose.rotMatrix44().decompose(decomp2.mTrans, decomp2.mRot, decomp2.mScale);
            // invbindpose.compose(decomp1.mTrans * 0.01, decomp2.mRot, 1.0f);
            // invbindpose = xgmnode->concatenatednode().inverse();
            xgmnode->_bindMatrixInverse = invbindpose;
            rval->_isSkinned            = true;
          }
        }
      }
    }
    for (int i = 0; i < n->mNumChildren; ++i) {
      nodestack.push(n->mChildren[i]);
    }
  }

  /////////////////////////////////////////////////
  // set parents
  /////////////////////////////////////////////////

  nodestack = std::queue<aiNode*>();
  nodestack.push(scene->mRootNode);
  while (not nodestack.empty()) {
    auto p = nodestack.front();
    nodestack.pop();
    //////////////////////////////
    auto itp = xgmskelnodes.find(remapSkelName(p->mName.data));
    OrkAssert(itp != xgmskelnodes.end());
    auto pskelnode = itp->second;
    // fmtx4 pmtx     = pskelnode->bindMatrix();
    // deco::printf(fvec3::White(), "pskelnode<%s> %s\n", pskelnode->_name.c_str(), pskelnode->_jointMatrix.dump().c_str());
    // deco::printf(fvec3::White(), "pskelnode<%s> %s\n", pskelnode->_name.c_str(), pmtx.dump().c_str());
    //////////////////////////////
    for (int i = 0; i < p->mNumChildren; ++i) {
      auto c   = p->mChildren[i];
      auto itc = xgmskelnodes.find(remapSkelName(c->mName.data));
      OrkAssert(itc != xgmskelnodes.end());
      auto cskelnode = itc->second;
      nodestack.push(c);
      cskelnode->_parent = pskelnode;
      pskelnode->mChildren.push_back(cskelnode);
    }
    /////////////////////////////////////////////////
  } // while (not nodestack.empty())
  /////////////////////////////////////////////////

  auto root = rval->rootXgmSkelNode();

  root->_jointMatrix = root->bindMatrix();

  /////////////////////////////////////////////////
  // set parents
  /////////////////////////////////////////////////

  root->visitHierarchy([root](lev2::XgmSkelNode* node) {
    fmtx4 N  = node->_nodeMatrix;
    fmtx4 K  = node->concatenatednode();
    fmtx4 I  = node->_bindMatrixInverse;
    fmtx4 C  = node->bindMatrix();
    auto par = node->_parent;
    fmtx4 P  = par ? par->bindMatrix() : fmtx4::Identity;
    node->_jointMatrix.CorrectionMatrix(P, C);
    fmtx4 P2C = node->_jointMatrix;
    fmtx4 D   = P * P2C;
    // fmtx4 D = P2C * P;
    auto n = node->_name;
    deco::printe(fvec3::White(), n + ".N: " + N.dump4x3(fvec3::White()), true);
    deco::printe(fvec3::White(), n + ".K: " + K.dump4x3(fvec3::White()), true);
    deco::printe(fvec3::White(), n + ".I: " + I.dump4x3(fvec3::White()), true);
    deco::printe(fvec3::White(), n + ".P: " + P.dump4x3(fvec3::White()), true);
    deco::printe(fvec3::White(), n + ".C: " + C.dump4x3(fvec3::White()), true);
    deco::printe(fvec3::White(), n + ".P2C: " + P2C.dump4x3(fvec3::White()), true);
    deco::printe(fvec3::White(), n + ".P*P2C: " + D.dump4x3(fvec3::White()), true);
    printf("\n");
  });

  /////////////////////////////////////////////////

  // bool fixup_applied = root->applyCentimeterToMeterScale();

  // OrkAssert(false);
  return rval;
}

// bone   len<1.998>
// bone.1 len<1.42>
// bone.2 len<1.45>
// bone.3 len<1.18>

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::MeshUtil
