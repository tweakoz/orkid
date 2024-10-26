///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include "assimp_util.inl"
#include<ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_meshutilassimp_skel = logger()->createChannel("meshutil.assimp.skel",fvec3(1,.8,.7));

parsedskeletonptr_t parseSkeleton(const aiScene* scene) {

  auto rval = std::make_shared<ParsedSkeleton>();

  /////////////////////////////////////////////////
  // get nodes
  /////////////////////////////////////////////////

  rval->_rootname = scene->mRootNode->mName.data;
  rval->_rootpath = aiNodePathName(scene->mRootNode);

  // deco::printf(fvec3::Green(), "//////////////////////////////////////////////////\n");
  // deco::printf(fvec3::Green(), "// Parsing Assimp Skeleton\n");
  // deco::printf(fvec3::Green(), "//////////////////////////////////////////////////\n");

  // deco::printf(fvec3::Green(), "// traversing nodes\n");

  OrkAssert(scene->mRootNode != nullptr);

  lev2::xgmskelnode_ptr_t root_skelnode = nullptr;
  std::map<std::string, const aiNode*> uniqskelnodeset;

  auto create_skelnodes = [&](const aiNode* n, int depth) {
    OrkAssert(n != nullptr);
    auto name      = remapSkelName(n->mName.data);
    auto node_path = aiNodePathName(n);
    std::string node_ID = (n->mID.data!=nullptr) ? n->mID.data : "";
    auto itb       = uniqskelnodeset.find(node_path);
    if (itb != uniqskelnodeset.end()) {
      auto prior      = itb->second;
      auto prior_path = aiNodePathName(prior);
      printf(
          "uniqskelnodeset node already present name<%s> prior<%p> new<%p>\n", //
          name.c_str(),                                                        //
          (void*)prior,                                                        //
          (void*)n);

      auto n_metadata = n->mMetaData;
      if (n_metadata) {
        auto numprops = n_metadata->mNumProperties;
        for (int i = 0; i < numprops; i++) {
          auto key = n_metadata->mKeys[i].data;
          // auto val  = metadata->mValues[i]->mData;
          printf("n_metadata prop<%s>\n", key);
        }
      }
      auto p_metadata = prior->mMetaData;
      if (p_metadata) {
        auto numprops = p_metadata->mNumProperties;
        for (int i = 0; i < numprops; i++) {
          auto key = p_metadata->mKeys[i].data;
          // auto val  = p_metadata->mValues[i]->mData;
          printf("p_metadata prop<%s>\n", key);
        }
      }
      printf("prior_path<%s>\n", prior_path.c_str());
      printf("node_path<%s>\n", node_path.c_str());
      printf("node_ID<%s>\n", node_ID.c_str());
      OrkAssert(false);
    }
    int index                  = uniqskelnodeset.size();
    uniqskelnodeset[node_path] = n;
    auto xgmnode               = std::make_shared<lev2::XgmSkelNode>(name);
    xgmnode->_ID               = node_ID;
    xgmnode->_path             = node_path;
    xgmnode->miSkelIndex       = index;
    xgmnode->_depth            = depth;

    if (n == scene->mRootNode) {
      root_skelnode = xgmnode;
      OrkAssert(depth == 0);
    }

    deco::printf(
        fvec3::White(),
        "aiNode<%d:%s> depth<%d>  xgmnode<%p> remapped<%s>\n", //
        index,                                                 //
        node_path.c_str(),                                     //
        depth,
        (void*)xgmnode.get(), //
        name.c_str());

    xgmnode->_nodeMatrix = convertMatrix44(n->mTransformation);
    // deco::printe(fvec3::Yellow(), xgmnode->_nodeMatrix.dump4x3(), true);
    rval->_xgmskelmap_by_path[node_path] = xgmnode;
    rval->_xgmskelmap_by_name[name]      = xgmnode;
    rval->_xgmskelmap_by_id[node_ID]     = xgmnode;
  };

  visit_ainodes_down(scene->mRootNode, 0, create_skelnodes);

  /////////////////////////////////////////////////
  // get bones
  /////////////////////////////////////////////////

  // deco::printf(fvec3::Green(), "// traversing bones\n");

  auto fetch_bones = [&](const aiNode* n, int depth) {
    for (int m = 0; m < n->mNumMeshes; ++m) {
      const aiMesh* mesh = scene->mMeshes[n->mMeshes[m]];
      for (int b = 0; b < mesh->mNumBones; b++) {
        auto bone      = mesh->mBones[b];
        auto bone_node = bone->mNode;
        OrkAssert(bone_node);
        auto bone_path = aiNodePathName(bone_node);
        auto bonename  = remapSkelName(bone->mName.data);
        auto itb       = rval->_xgmskelmap_by_path.find(bone_path);
        OrkAssert(itb != rval->_xgmskelmap_by_path.end());
        if (itb != rval->_xgmskelmap_by_path.end()) {
          auto xgmnode = itb->second;
          if (false == xgmnode->_varmap["visited_bone"].isA<bool>()) {
            xgmnode->_varmap["visited_bone"].set<bool>(true);
            xgmnode->_assimpOffsetMatrix = convertMatrix44(bone->mOffsetMatrix);
            rval->_isSkinned             = true;
          }
        }
      }
    }
  };

  visit_ainodes_down(scene->mRootNode, 0, fetch_bones);

  /////////////////////////////////////////////////
  // set parents
  /////////////////////////////////////////////////

  deco::printf(fvec3::Green(), "// creating xgm topology\n");

  std::set<const aiNode*> visited;
  std::set<std::string> visited_name;
  auto set_parents = [&](const aiNode* p, int depth) {
    auto it = visited.find(p);
    if (it != visited.end()) {
      printf("ALREADY VISITED PAR<%s>\n", p->mName.data);
      OrkAssert(false);
    }
    visited.insert(p);
    //////////////////////////////
    auto premapped = remapSkelName(p->mName.data);
    auto ppath     = aiNodePathName(p);
    auto itp       = rval->_xgmskelmap_by_path.find(ppath);
    OrkAssert(itp != rval->_xgmskelmap_by_path.end());
    auto pskelnode = itp->second;
    auto itn       = visited_name.find(ppath);
    if (itn != visited_name.end()) {
      printf("ALREADY VISITED NAME<%s>\n", ppath.c_str());
      OrkAssert(false);
    }
    //////////////////////////////
    deco::printf(
        fvec3::White(),
        "par path<%s> depth<%d> id<%s>: \n", //
        ppath.c_str(),
        pskelnode->_depth,
        p->mID.data);

    //////////////////////////////
    for (int i = 0; i < p->mNumChildren; ++i) {
      auto c = p->mChildren[i];

      auto cremapped = remapSkelName(c->mName.data);
      auto cpath     = aiNodePathName(c);
      auto itc       = rval->_xgmskelmap_by_path.find(cpath);

      OrkAssert(itc != rval->_xgmskelmap_by_path.end());
      auto cskelnode = itc->second;

      deco::printf(
          fvec3::White(),
          "    chi path<%s> depth<%d> : \n", //
          cpath.c_str(),
          cskelnode->_depth);

      cskelnode->_parent = pskelnode;
      pskelnode->_childrenX.push_back(cskelnode);

      auto it = visited.find(c);
      if (it != visited.end()) {
        printf("ALREADY VISITED CHI<%s>\n", c->mName.data);
        OrkAssert(false);
      }
    }
    /////////////////////////////////////////////////
  }; // while (not ainode_stack.empty())
  visit_ainodes_down(scene->mRootNode, 0, set_parents);

  /////////////////////////////////////////////////
  // test that hierarchy is a tree (no cycles)
  /////////////////////////////////////////////////

  if (1) {
    std::set<lev2::xgmskelnode_ptr_t> visited;
    lev2::XgmSkelNode::visitHierarchy(root_skelnode, [&visited](lev2::xgmskelnode_ptr_t node) { //
      auto it = visited.find(node);
      if (it != visited.end()) {
        printf("ALREADY VISITED NODE<%s>\n", node->_name.c_str());
        OrkAssert(false);
      }
      visited.insert(node);
    });
  }


  /////////////////////////////////////////////////
  // set joints matrices from nodes
  /////////////////////////////////////////////////

  lev2::XgmSkelNode::visitHierarchy(root_skelnode, [](lev2::xgmskelnode_ptr_t node) {
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

  if (1) {
    lev2::XgmSkelNode::visitHierarchy(root_skelnode, [](lev2::xgmskelnode_ptr_t node) { //
      node->_bindMatrixInverse = node->_assimpOffsetMatrix;
      node->_bindMatrix        = node->_bindMatrixInverse.inverse();
    });
  }

  /////////////////////////////////////////////////
  // synthesize end effector nodes
  /////////////////////////////////////////////////

  std::vector<lev2::xgmskelnode_ptr_t> tailnodes;
  lev2::XgmSkelNode::visitHierarchy(root_skelnode,  //
    [&tailnodes](lev2::xgmskelnode_ptr_t node) { //
    if ((node->_depth > 3) and (node->_childrenX.size() == 0)) {
      tailnodes.push_back(node);
    }
  });

  for( auto node : tailnodes ){



      auto endeffector     = std::make_shared<lev2::XgmSkelNode>(node->_name + "_endeffector");
      int index            = uniqskelnodeset.size();
      endeffector->_path   = node->_path + "/" + endeffector->_name;
      endeffector->_depth  = node->_depth + 1;
      endeffector->_parent = node;
      endeffector->miSkelIndex = index;

      auto par = node->_parent;
      fvec3 parpos = par->_nodeMatrix.translation();
      fvec3 nodepos = node->_nodeMatrix.translation();
      float length = (nodepos-parpos).magnitude();
      //fvec3 endpos = nodepos + node->_nodeMatrix.zNormal() * length;

      // endeffector bone will be length distance on node's z axis
      // the basis should inherit the node's basis

      // TODO fix end effector scales via matrices here (not in skeleton renderer)

      fmtx4 end_local;
      end_local.setColumn(3,fvec4(0.0f,0.0f,length,1.0f));

      auto end_concat = node->_nodeMatrix * end_local;

      fmtx4 Bc = end_concat;
      fmtx4 Bp = node->_nodeMatrix;
      fmtx4 J = fmtx4::multiply_ltor(Bp.inverse(), Bc);

      endeffector->_nodeMatrix         = end_concat;
      endeffector->_jointMatrix        = J;
      endeffector->_bindMatrixInverse  = fmtx4();
      endeffector->_bindMatrix         = fmtx4();
      endeffector->_assimpOffsetMatrix = fmtx4();


      rval->_xgmskelmap_by_path[endeffector->_path] = endeffector;
      rval->_xgmskelmap_by_name[endeffector->_name] = endeffector;
      rval->_xgmskelmap_by_id[endeffector->_path] = endeffector;

      uniqskelnodeset.insert(std::make_pair(endeffector->_path, nullptr));
      node->_childrenX.push_back(endeffector);
      //
  }

  /////////////////////////////////////////////////
  // debug dump
  /////////////////////////////////////////////////

  if (1) {
    deco::printf(fvec3::Green(), "// result debug dump\n");

    lev2::XgmSkelNode::visitHierarchy(root_skelnode, [](lev2::xgmskelnode_ptr_t node) {
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
      deco::printe(fvec3::White(), n + "<mtx>.par: " + pn, true);
      deco::printe(fvec3::White(), n + "<mtx>.ASSO: " + ASSO.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + "<mtx>.ASSOi: " + ASSO.inverse().dump4x3cn(), true);
      deco::printe(fvec3::White(), n + "<mtx>.nodemtx: " + nodemtx.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + "<mtx>.nodecat: " + nodecat.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + "<mtx>.J: " + J.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + "<mtx>.Bi: " + Bi.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + "<mtx>.Bc: " + Bc.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + "<mtx>.Bp: " + Bp.dump4x3cn(), true);
      deco::printe(fvec3::White(), n + "<mtx>.O: " + O.dump4x3cn(), true);
      printf("\n");
    });
  }

  /////////////////////////////////////////////////

  // bool fixup_applied = lev2::XgmSkelNode::applyCentimeterToMeterScale(root);

  // OrkAssert(false);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void configureXgmSkeleton(const ork::meshutil::Mesh& input, lev2::XgmModel& xgmmdlout) {

  auto parsedskel = input._varmap->valueForKey("parsedskel").get<parsedskeletonptr_t>();

  //logchan_meshutilassimp_skel->log("NumSkelNodes<%d>\n", int(xgmskelnodes.size()));
  xgmmdlout.SetSkinned(true);
  auto& xgmskel = xgmmdlout.skeleton();
 
  xgmskel.resize(parsedskel->_xgmskelmap_by_path.size());
  
  for (auto& item : parsedskel->_xgmskelmap_by_path) {
    const std::string& JointPath = item.first;
    auto skelnode                = item.second;
    auto JointName = skelnode->_name;
    auto JointID = skelnode->_ID;
    auto parskelnode             = skelnode->_parent;
    std::string ParName = parskelnode ? parskelnode->_name : "none";
    int idx                      = skelnode->miSkelIndex;
    int pidx                     = parskelnode ? parskelnode->miSkelIndex : -1;
    logchan_meshutilassimp_skel->log("JointName<%s> ParName<%s> skelnode<%p> parskelnode<%p> idx<%d> pidx<%d> numinfs<%d>", //
       JointName.c_str(), 
       ParName.c_str(),
       (void*) skelnode.get(), 
       (void*) parskelnode.get(), 
       idx, 
       pidx,
       int(skelnode->_numBoundVertices));

    xgmskel.addJoint(idx, pidx, JointName, JointPath, JointID);
    xgmskel._bindMatrices[idx] = skelnode ? skelnode->_bindMatrix : fmtx4();
    xgmskel._inverseBindMatrices[idx] = skelnode ? skelnode->_bindMatrixInverse : fmtx4();
    xgmskel.RefJointMatrix(idx)       = skelnode ? skelnode->_jointMatrix : fmtx4();
    xgmskel.RefNodeMatrix(idx)        = skelnode ? skelnode->_nodeMatrix : fmtx4();

    auto jprops = std::make_shared<lev2::XgmJointProperties>();
    xgmskel._jointProperties[idx] = jprops;
    jprops->_numVerticesInfluenced = skelnode->_numBoundVertices;

  }

  /////////////////////////////////////
  // flatten the skeleton (WIP)
  //  this means traverse the tree and add bones for each parent/child pair
  //  the bones are added in order of traversal
  //  so that later processing (concatenation) can be done
  //   by traversing an array of bones
  /////////////////////////////////////

  //logchan_meshutilassimp_skel->log("Flatten Skeleton\n");
  const auto& bonemarkset = (*input._varmap)["bonemarkset"].get<bonemarkset_t>();

  auto root          = parsedskel->rootXgmSkelNode();
  xgmskel.miRootNode = root ? root->miSkelIndex : -1;
  size_t add_count = 0;
  if (root) {
    lev2::XgmSkelNode::visitHierarchy(root,[&xgmskel,&add_count](lev2::xgmskelnode_ptr_t node) {
      auto parent = node->_parent;
      if (parent) {
        bool ignore = (parent->_numBoundVertices == 0);
        ignore      = ignore and (node->_numBoundVertices == 0);
        if (ignore){
          //logchan_meshutilassimp_skel->log("xxx IGNORE<%s>\n", node->_name.c_str());
        }
        else {
          int iparentindex   = parent->miSkelIndex;
          int ichildindex    = node->miSkelIndex;
          auto pa_props = xgmskel._jointProperties[iparentindex];
          auto ch_props = xgmskel._jointProperties[ichildindex];

          bool valid = (pa_props->_numVerticesInfluenced > 0) or (ch_props->_numVerticesInfluenced > 0);
          if(valid){
            logchan_meshutilassimp_skel->log("xxx ADD BONE<%d> par<%zu:%s (%d)> chi<%zu:%s (%d)>\n", //
              add_count, //
              iparentindex,
              parent->_name.c_str(),
              pa_props->_numVerticesInfluenced,
              ichildindex,
              node->_name.c_str(),
              ch_props->_numVerticesInfluenced);
            lev2::XgmBone Bone = {iparentindex, ichildindex};
            xgmskel.addBone(Bone);
            add_count++;
          }
        }
      }
    });
    // xgmskel.dump();
  }

  //logchan_meshutilassimp_skel->log("skeleton configuration complete..\n");
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
