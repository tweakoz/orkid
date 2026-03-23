///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include "assimp_util.inl"
#include <ork/math/misc_math.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

// TODO - implement datablock cache for anims

datablock_ptr_t assimpToXga(datablock_ptr_t inp_datablock) {

  using namespace ::ork::lev2;

  typedef std::vector<fmtx4> framevect_t;

  auto& extension = inp_datablock->_vars->typedValueForKey<std::string>("file-extension").value();
  auto scene =
      aiImportFileFromMemory((const char*)inp_datablock->data(), inp_datablock->length(), assimpImportFlags(), extension.c_str());
  if (scene) {
    XgmAnim xgmanim;
    aiVector3D scene_min, scene_max, scene_center;
    aiMatrix4x4 identity;
    aiIdentityMatrix4(&identity);
    scene_min.x = scene_min.y = scene_min.z = 1e10f;
    scene_max.x = scene_max.y = scene_max.z = -1e10f;
    get_bounding_box_for_node(scene, scene->mRootNode, scene_min, scene_max, identity);

    scene_center.x = (scene_min.x + scene_max.x) / 2.0f;
    scene_center.y = (scene_min.y + scene_max.y) / 2.0f;
    scene_center.z = (scene_min.z + scene_max.z) / 2.0f;

    /////////////////////////////

    auto parsedskel = parseSkeleton(scene); // create and link skeleton

    /////////////////////////////
    // merge all animations' TRS channels
    /////////////////////////////

    int numanims = scene->mNumAnimations;

    auto& staticpose = xgmanim._static_pose;
    std::set<std::string> uniqskelnodeset;
    std::map<std::string, std::string> channel_remap;

    std::queue<aiNode*> nodestack;
    nodestack.push(scene->mRootNode);
    while (not nodestack.empty()) {
      auto n = nodestack.front();
      nodestack.pop();
      auto name = std::string(n->mName.data);
      auto path = aiNodePathName(n);
      auto itb  = uniqskelnodeset.find(path);
      if (itb == uniqskelnodeset.end()) {
        uniqskelnodeset.insert(path);
        auto matrix         = convertMatrix44(n->mTransformation);
        auto remapped_name  = name;
        channel_remap[name] = remapped_name;
      }
      for (int i = 0; i < n->mNumChildren; ++i) {
        nodestack.push(n->mChildren[i]);
      }
    }

    ////////////////////////////////////////
    // collect all TRS node channels across all animations
    ////////////////////////////////////////

    struct MergedChannel {
      aiNodeAnim* channel;
    };
    std::vector<MergedChannel> all_channels;

    for (int a = 0; a < numanims; a++) {
      aiAnimation* anim = scene->mAnimations[a];
      for (int c = 0; c < anim->mNumChannels; c++) {
        all_channels.push_back({anim->mChannels[c]});
      }
    }

    /////////////////////////////////////////////////////
    // compute number of frames across all channels
    /////////////////////////////////////////////////////

    size_t framecount = 0;

    for (auto& mc : all_channels) {
      aiNodeAnim* channel = mc.channel;
      if (channel->mNumPositionKeys > framecount)
        framecount = channel->mNumPositionKeys;
      if (channel->mNumRotationKeys > framecount)
        framecount = channel->mNumRotationKeys;
      if (channel->mNumScalingKeys > framecount)
        framecount = channel->mNumScalingKeys;
    }

    xgmanim._numframes = framecount;

    /////////////////////////////////////////////////////
    // pull out channel data
    /////////////////////////////////////////////////////

    for (size_t ci = 0; ci < all_channels.size(); ci++) {
      aiNodeAnim* channel = all_channels[ci].channel;

      std::string channel_name = remapSkelName(channel->mNodeName.data);

      auto its = parsedskel->_xgmskelmap_by_name.find(channel_name);
      if (its == parsedskel->_xgmskelmap_by_name.end())
        continue;
      auto skelnode   = its->second;

      // skip if this skelnode already has animation data
      // (multiple animations may target the same node — first one wins)
      if (skelnode->_varmap.hasKey("framevect_n"))
        continue;

      auto& skelnode_framevect_n = skelnode->_varmap["framevect_n"].make<framevect_t>();

      auto it = channel_remap.find(channel_name);
      if (it != channel_remap.end()) {
        channel_name = it->second;
      }

      //////////////////////////////////////////////
      // create matrix channel and add to animation
      //////////////////////////////////////////////

      std::string objnameps = "";
      auto XgmChan          = std::make_shared<XgmDecompMatrixAnimChannel>(objnameps, channel_name, "Joint");
      XgmChan->reserveFrames(framecount);
      xgmanim.AddChannel(channel_name, XgmChan);
      skelnode->_varmap["xgmchan"].make<animchannel_ptr_t>(XgmChan);

      /////////////////////////////
      // we assume pre-sampled frames here
      /////////////////////////////

      fvec3 curpos, cursca;
      fquat currot;

      for (int f = 0; f < framecount; f++) {
        if (f < channel->mNumPositionKeys) {
          const aiVectorKey& poskey = channel->mPositionKeys[f];
          aiVector3D pos            = poskey.mValue;
          curpos                    = fvec3(pos.x, pos.y, pos.z);
        }
        if (f < channel->mNumRotationKeys) {
          const aiQuatKey& rotkey = channel->mRotationKeys[f];
          aiQuaternion rot        = rotkey.mValue;
          currot                  = fquat(rot.x, rot.y, rot.z, rot.w);
        }
        if (f < channel->mNumScalingKeys) {
          const aiVectorKey& scakey = channel->mScalingKeys[f];
          aiVector3D sca            = scakey.mValue;
          cursca                    = fvec3(sca.x, sca.y, sca.z);
        }

        /////////////////////////////
        // compose node-space matrix
        // https://assimp.sourceforge.net/lib_html/structai_node_anim.html
        /////////////////////////////

        fmtx4 R, S, T;
        R.fromQuaternion(currot);
        S.setScale(cursca.x, cursca.y, cursca.z);
        T.setTranslation(curpos);
        fmtx4 XF_NSPACE = T*(R*S);
        skelnode_framevect_n.push_back(XF_NSPACE);

      } // for (int f = 0; f < framecount; f++) {
    }   // for all_channels

    /////////////////////////////////////////////////////
    // compute J frames
    /////////////////////////////////////////////////////

    // collect unique animated skelnodes
    struct AnimatedNode {
      std::string channel_name;
      lev2::xgmskelnode_ptr_t skelnode;
    };
    std::vector<AnimatedNode> animated_nodes;
    for (auto& mc : all_channels) {
      std::string channel_name = remapSkelName(mc.channel->mNodeName.data);
      auto its = parsedskel->_xgmskelmap_by_name.find(channel_name);
      if (its == parsedskel->_xgmskelmap_by_name.end())
        continue;
      auto skelnode = its->second;
      if (not skelnode->_varmap.hasKey("xgmchan"))
        continue;
      bool already = false;
      for (auto& an : animated_nodes) {
        if (an.channel_name == channel_name) { already = true; break; }
      }
      if (not already) {
        animated_nodes.push_back({channel_name, skelnode});
      }
    }

    for (size_t f = 0; f < framecount; f++) {
      // apply anim to skelnodes
      for (auto& an : animated_nodes) {
        auto& skelnode_framevect_n = an.skelnode->_varmap["framevect_n"].get<framevect_t>();
        size_t fi = (f < skelnode_framevect_n.size()) ? f : skelnode_framevect_n.size() - 1;
        an.skelnode->_jointMatrix = skelnode_framevect_n[fi];
      }
      // compute J-space and store
      for (auto& an : animated_nodes) {
        auto skelnode = an.skelnode;
        fmtx4 JSPACE;
        if (skelnode->_parent) {
          JSPACE.correctionMatrix(skelnode->_parent->concatenated_joint(),
                                           skelnode->concatenated_joint() );
        }
        else{
          JSPACE = skelnode->concatenated_joint();
        }

        auto XgmChan      = skelnode->_varmap["xgmchan"].get<animchannel_ptr_t>();
        auto as_decomchan = std::dynamic_pointer_cast<XgmDecompMatrixAnimChannel>(XgmChan);

        DecompMatrix decomp;
        JSPACE.decompose(decomp._position,
                         decomp._orientation,
                         decomp._scale.x );
        decomp._scale.y = decomp._scale.x;
        decomp._scale.z = decomp._scale.x;

        as_decomchan->setFrame(f, decomp);
      }
    }

    ////////////////////////////////////////////////////////////////
    return XgmAnim::Save(&xgmanim);
  } // if scene
  else {
    OrkAssert(false);
  }
  return nullptr;
}

} // namespace ork::meshutil
