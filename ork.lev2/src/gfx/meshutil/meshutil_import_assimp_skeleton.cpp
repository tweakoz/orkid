///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include "assimp_util.inl"

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

parsedskeletonptr_t parseSkeleton(const aiScene* scene) {

  auto rval = std::make_shared<ParsedSkeleton>();

  std::set<std::string> uniqskelnodeset;

  skelnodemap_t& xgmskelnodes = rval->_xgmskelmap;

  /////////////////////////////////////////////////
  // get nodes
  /////////////////////////////////////////////////

  auto ainode_stack = std::queue<aiNode*>();
  ainode_stack.push(scene->mRootNode);

  rval->_rootname = scene->mRootNode->mName.data;

  // deco::printf(fvec3::Green(), "//////////////////////////////////////////////////\n");
  // deco::printf(fvec3::Green(), "// Parsing Assimp Skeleton\n");
  // deco::printf(fvec3::Green(), "//////////////////////////////////////////////////\n");

  // deco::printf(fvec3::Green(), "// traversing nodes\n");

  while (not ainode_stack.empty()) {
    auto n = ainode_stack.front();
    ainode_stack.pop();
    OrkAssert(scene->mRootNode != nullptr);
    OrkAssert(n != nullptr);
    int depth = compute_aiNodeDepth(n);

    auto name = remapSkelName(n->mName.data);
    auto itb  = uniqskelnodeset.find(name);
    if (itb == uniqskelnodeset.end()) {
      int index = uniqskelnodeset.size();
      uniqskelnodeset.insert(name);
      auto xgmnode         = std::make_shared<lev2::XgmSkelNode>(name);
      xgmnode->miSkelIndex = index;
      xgmnode->_depth      = depth;

      deco::printf(
          fvec3::White(),
          "aiNode<%d:%s> depth<%d>  xgmnode<%p> remapped<%s>\n", //
          index,                                                 //
          n->mName.data,                                         //
          depth,
          (void*)xgmnode.get(), //
          name.c_str());

      xgmnode->_nodeMatrix = convertMatrix44(n->mTransformation);
      // deco::printe(fvec3::Yellow(), xgmnode->_nodeMatrix.dump4x3(), true);
      xgmskelnodes[name] = xgmnode;
    }
    for (int i = 0; i < n->mNumChildren; ++i) {
      ainode_stack.push(n->mChildren[i]);
      depth++;
    }
  }

  /////////////////////////////////////////////////
  // get bones
  /////////////////////////////////////////////////

  // deco::printf(fvec3::Green(), "// traversing bones\n");

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

  deco::printf(fvec3::Green(), "// creating xgm topology\n");

  ainode_stack = std::queue<aiNode*>();
  ainode_stack.push(scene->mRootNode);
  while (not ainode_stack.empty()) {
    auto p = ainode_stack.front();
    ainode_stack.pop();
    //////////////////////////////
    auto premapped = remapSkelName(p->mName.data);
    auto itp      = xgmskelnodes.find(premapped);
    OrkAssert(itp != xgmskelnodes.end());
    auto pskelnode = itp->second;
    //////////////////////////////
    deco::printf(
        fvec3::White(),
        "par remapped<%s> depth<%d> : \n", //
        premapped.c_str(),
        pskelnode->_depth);
    //////////////////////////////
    for (int i = 0; i < p->mNumChildren; ++i) {
      auto c         = p->mChildren[i];
      auto cremapped = remapSkelName(c->mName.data);
      auto itc       = xgmskelnodes.find(cremapped);

      OrkAssert(itc != xgmskelnodes.end());
      auto cskelnode = itc->second;

      deco::printf(
          fvec3::White(),
          "    chi remapped<%s> depth<%d> : \n", //
          cremapped.c_str(),
          cskelnode->_depth);

 
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

  lev2::XgmSkelNode::visitHierarchy(root, [](lev2::xgmskelnode_ptr_t node) {
    fmtx4 Bc = node->concatenated_node();
    auto par = node->_parent;
    fmtx4 Bp = par ? par->concatenated_node() : fmtx4::Identity();
    fmtx4 J;
    J.correctionMatrix(Bp, Bc);
    J                  = fmtx4::multiply_ltor(Bp.inverse(), Bc);
    node->_jointMatrix = J;
  });

  /////////////////////////////////////////////////
  // set bindpose/inverse matrices (from _assimpOffsetMatrix)
  /////////////////////////////////////////////////

  lev2::XgmSkelNode::visitHierarchy(root, [](lev2::xgmskelnode_ptr_t node) { //
    node->_bindMatrixInverse = node->_assimpOffsetMatrix;
    node->_bindMatrix        = node->_bindMatrixInverse.inverse();
  });

  /////////////////////////////////////////////////
  // debug dump
  /////////////////////////////////////////////////

  if (1) {
    deco::printf(fvec3::Green(), "// result debug dump\n");

    lev2::XgmSkelNode::visitHierarchy(root, [](lev2::xgmskelnode_ptr_t node) {
      fmtx4 ASSO    = node->_assimpOffsetMatrix;
      fmtx4 nodemtx = node->_nodeMatrix;
      fmtx4 nodecat = node->concatenated_node(); // object space
      fmtx4 Bi      = node->_bindMatrixInverse;
      fmtx4 Bc      = node->_bindMatrix;
      auto par      = node->_parent;
      fmtx4 Bp      = par ? par->_bindMatrix : fmtx4::Identity();
      fmtx4 J       = node->_jointMatrix;
      fmtx4 O       = node->concatenated_joint(); // object space
      auto n        = node->_name;
      auto pn       = par ? par->_name : "---";
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

  // bool fixup_applied = lev2::XgmSkelNode::applyCentimeterToMeterScale(root);

  // OrkAssert(false);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
