///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include "assimp_util.inl"

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

// TODO - implement datablock cache for anims

datablock_ptr_t assimpToXga(datablock_ptr_t inp_datablock){

  using namespace ::ork::lev2;

  typedef std::vector<fmtx4> framevect_t;

  //ColladaExportPolicy policy;
  //policy.mUnits            = UNITS_METER;
  const std::string JointPS = "Joint";

  auto color = fvec3(1, 0, 1);

  //deco::printf(color, "BEGIN: importing<%s> via Assimp\n", GlbPath.c_str());

  auto& extension = inp_datablock->_vars->typedValueForKey<std::string>("file-extension").value();
  auto scene = aiImportFileFromMemory((const char*)inp_datablock->data(), inp_datablock->length(), assimpImportFlags(), extension.c_str());
  deco::printf(color, "END: importing scene<%p>\n", scene);
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
    // get skeleton

    auto parsedskel = parseSkeleton(scene);
    auto& skelnodes = parsedskel->_xgmskelmap;
    /////////////////////////////
    // we assume a single animation per file
    /////////////////////////////

    int numanims = scene->mNumAnimations;
    printf("numanims<%d>\n", numanims);

    ////////////////////////////////////////
    // todo - build static pose
    //  from static position of all aiNodes
    ////////////////////////////////////////

    auto& staticpose = xgmanim.GetStaticPose();
    std::set<std::string> uniqskelnodeset;
    std::map<std::string, std::string> channel_remap;
    std::map<std::string, fmtx4> inv_bind_map;

    std::queue<aiNode*> nodestack;
    nodestack.push(scene->mRootNode);
    while (not nodestack.empty()) {
      auto n = nodestack.front();
      nodestack.pop();
      auto name = std::string(n->mName.data);
      auto itb  = uniqskelnodeset.find(name);
      if (itb == uniqskelnodeset.end()) {
        int index = uniqskelnodeset.size();
        uniqskelnodeset.insert(name);
        auto matrix         = convertMatrix44(n->mTransformation);
        auto remapped_name  = remapSkelName(name);
        channel_remap[name] = remapped_name;
        auto a              = deco::decorate(fvec3(0, 1, 1), name);
        auto b              = deco::decorate(fvec3(1, 1, 1), remapped_name);
        printf("aiNode<%s> remapped -> <%s>\n", a.c_str(), b.c_str());
        matrix.dump(name);
      }
      for (int i = 0; i < n->mNumChildren; ++i) {
        nodestack.push(n->mChildren[i]);
      }
    }

    ////////////////////////////////////////

    OrkAssert(scene->mNumAnimations >= 1);

    aiAnimation* anim = scene->mAnimations[0];

    auto color = fvec3(1, 1, 0);

    deco::printf(color, "numchannels<%d>\n", anim->mNumChannels);

    /////////////////////////////////////////////////////
    // compute number of frames
    /////////////////////////////////////////////////////

    size_t framecount = 0;

    for (int i = 0; i < anim->mNumChannels; i++) {
      aiNodeAnim* channel = anim->mChannels[i];
      if (channel->mNumPositionKeys > framecount)
        framecount = channel->mNumPositionKeys;
      if (channel->mNumRotationKeys > framecount)
        framecount = channel->mNumRotationKeys;
      if (channel->mNumScalingKeys > framecount)
        framecount = channel->mNumScalingKeys;
    }

    deco::printf(color, "framecount<%zu>\n", framecount);

    xgmanim._numframes = framecount;

    /////////////////////////////////////////////////////
    // pull out channel data
    /////////////////////////////////////////////////////

    for (int i = 0; i < anim->mNumChannels; i++) {
      aiNodeAnim* channel = anim->mChannels[i];

      std::string channel_name = remapSkelName(channel->mNodeName.data);

      auto its        = skelnodes.find(channel_name);
      auto skelnode   = its->second;
      auto bindmatrix = skelnode->bindMatrix();
      auto invbindmtx = skelnode->_bindMatrixInverse;

      auto& skelnode_framevect_n = skelnode->_varmap["framevect_n"].make<framevect_t>();
      skelnode->_varmap["framevect_j"].make<framevect_t>();

      auto it = channel_remap.find(channel_name);
      if (it != channel_remap.end()) {
        channel_name = it->second;
      }
      deco::printf(fvec3::White(), "/////////////////////////////////////////////\n");
      deco::printf(fvec3::White(), "channel<%d:%p:%s>\n", i, channel, channel_name.c_str());
      deco::printf(fvec3::White(), "  num poskeys<%d>\n", channel->mNumPositionKeys);
      deco::printf(fvec3::White(), "  num rotkeys<%d>\n", channel->mNumRotationKeys);
      deco::printf(fvec3::White(), "  num scakeys<%d>\n", channel->mNumScalingKeys);

      /////////////////////////////

      std::string objnameps         = "";
      auto XgmChan                 = std::make_shared<XgmMatrixAnimChannel>(objnameps, channel_name, JointPS);
      XgmChan->reserveFrames(framecount);
      xgmanim.AddChannel(channel_name, XgmChan);
      skelnode->_varmap["xgmchan"].make<animchannel_ptr_t>(XgmChan);

      /////////////////////////////
      // we assume pre-sampled frames here
      /////////////////////////////

      fvec3 curpos, cursca;
      fquat currot;

      ////////////////////////////////////////
      color = fvec3(1, .5, 0);
      for (int f = 0; f < framecount; f++) {
        if (f < channel->mNumPositionKeys) {
          const aiVectorKey& poskey = channel->mPositionKeys[f];
          double time               = poskey.mTime;
          aiVector3D pos            = poskey.mValue;
          curpos                    = fvec3(pos.x, pos.y, pos.z);
        }
        if (f < channel->mNumRotationKeys) {
          const aiQuatKey& rotkey = channel->mRotationKeys[f];
          double time             = rotkey.mTime;
          aiQuaternion rot        = rotkey.mValue;
          currot                  = fquat(rot.x, rot.y, rot.z, rot.w);
        }
        if (f < channel->mNumScalingKeys) {
          const aiVectorKey& scakey = channel->mScalingKeys[f];
          double time               = scakey.mTime;
          aiVector3D sca            = scakey.mValue;
          cursca                    = fvec3(sca.x, sca.y, sca.z);

          ////////////////////////////////////////////////////
          // we dont support non uniform scale at this time..
          //   we will probably add it at some point
          ////////////////////////////////////////////////////

          OrkAssert(math::areValuesClose(cursca.x, cursca.y, 0.00001f));
          OrkAssert(math::areValuesClose(cursca.x, cursca.z, 0.00001f));
        }

        /////////////////////////////
        // compose matrix
        //  generates node space matrix
        /////////////////////////////

        fmtx4 R, S, T;
        R.fromQuaternion(currot);
        S.setScale(cursca.x, cursca.x, cursca.x);
        T.setTranslation(curpos);
        fmtx4 XF_NSPACE = fmtx4::multiply_ltor(R,T);
        // fmtx4 XF_NSPACE = T * R;
        skelnode_framevect_n.push_back(XF_NSPACE);

        auto yel        = fvec3(1, 1, 0);
        auto whi        = fvec3(1, 1, 1);
        std::string xxx = deco::format(color, "fr<%d> ", f);
        xxx += deco::decorate(yel, channel_name + "(N):");
        xxx += XF_NSPACE.dump4x3cn();
        deco::prints(xxx, true);

      } // for (int f = 0; f < framecount; f++) {
    }   // for (int i = 0; i < anim->mNumChannels; i++) {
    auto yel = fvec3(1, 1, 0);
    auto whi = fvec3(1, 1, 1);
    if (1) {
      deco::printf(fvec3::White(), "/////////////////////////////////////////////\n");
      deco::printf(fvec3::White(), "// O Space Anim\n");
      deco::printf(fvec3::White(), "/////////////////////////////////////////////\n");
      for (int i = 0; i < anim->mNumChannels; i++) {
        deco::printf(color, "///////////\n");
        for (int f = 0; f < framecount; f++) {
          ////////////////////////////////////////////////////
          // apply anim channels to generate pose
          ////////////////////////////////////////////////////
          for (int c = 0; c < anim->mNumChannels; c++) {
            aiNodeAnim* channel        = anim->mChannels[c];
            std::string channel_name   = remapSkelName(channel->mNodeName.data);
            auto its                   = skelnodes.find(channel_name);
            auto skelnode              = its->second;
            auto& skelnode_framevect_j = skelnode->_varmap["framevect_n"].get<framevect_t>();
            fmtx4 joint_JSPACE         = skelnode_framevect_j[f];
            skelnode->_nodeMatrix      = joint_JSPACE;
          }
          ////////////////////////////////////////////////////
          aiNodeAnim* channel      = anim->mChannels[i];
          std::string channel_name = remapSkelName(channel->mNodeName.data);
          auto its                 = skelnodes.find(channel_name);
          auto skelnode            = its->second;
          auto XgmChan             = skelnode->_varmap["xgmchan"].get<animchannel_ptr_t>();
          fmtx4 OSPACE             = skelnode->concatenatednode();
          deco::printf(color, "fr<%d> ", f);
          deco::printf(yel, "%s (O): ", channel_name.c_str());
          deco::prints(OSPACE.dump4x3cn(), true);
        }
      }
    }
    if (1) {
      deco::printf(fvec3::White(), "/////////////////////////////////////////////\n");
      deco::printf(fvec3::White(), "// J Space Anim\n");
      deco::printf(fvec3::White(), "/////////////////////////////////////////////\n");
      for (int i = 0; i < anim->mNumChannels; i++) {
        deco::printf(color, "///////////\n");
        for (size_t f = 0; f < framecount; f++) {
          ////////////////////////////////////////////////////
          // apply anim channels to generate pose
          ////////////////////////////////////////////////////
          for (int c = 0; c < anim->mNumChannels; c++) {
            aiNodeAnim* channel        = anim->mChannels[c];
            std::string channel_name   = remapSkelName(channel->mNodeName.data);
            auto its                   = skelnodes.find(channel_name);
            auto skelnode              = its->second;
            auto& skelnode_framevect_n = skelnode->_varmap["framevect_n"].get<framevect_t>();
            fmtx4 joint_NSPACE         = skelnode_framevect_n[f];
            skelnode->_jointMatrix     = joint_NSPACE;
          }
          ////////////////////////////////////////////////////
          aiNodeAnim* channel      = anim->mChannels[i];
          std::string channel_name = remapSkelName(channel->mNodeName.data);
          auto its                 = skelnodes.find(channel_name);
          auto skelnode            = its->second;
          auto XgmChan             = skelnode->_varmap["xgmchan"].get<animchannel_ptr_t>();
          fmtx4 OSPACE             = skelnode->concatenated2();
          auto par                 = skelnode->_parent;
          fmtx4 POSPACE            = par ? skelnode->_parent->concatenated2() : fmtx4();
          fmtx4 JSPACE             = fmtx4::multiply_ltor(POSPACE.inverse(),OSPACE);
          deco::printf(color, "fr<%d> ", f);
          deco::printf(yel, "%s (J): ", channel_name.c_str());
          deco::prints(JSPACE.dump4x3cn(), true);

          skelnode->_varmap["framevect_j"].get<framevect_t>().push_back(JSPACE);

          auto as_decomchan = std::dynamic_pointer_cast<XgmMatrixAnimChannel>(XgmChan);

          as_decomchan->setFrame(f,JSPACE);
        }
      }
    }
    if (1) {
      deco::printf(fvec3::White(), "/////////////////////////////////////////////\n");
      deco::printf(fvec3::White(), "// O Space Anim (reconstruct from J)\n");
      deco::printf(fvec3::White(), "/////////////////////////////////////////////\n");
      for (int i = 0; i < anim->mNumChannels; i++) {
        deco::printf(color, "///////////\n");
        for (int f = 0; f < framecount; f++) {
          ////////////////////////////////////////////////////
          // apply anim channels to generate pose
          ////////////////////////////////////////////////////
          for (int c = 0; c < anim->mNumChannels; c++) {
            aiNodeAnim* channel        = anim->mChannels[c];
            std::string channel_name   = remapSkelName(channel->mNodeName.data);
            auto its                   = skelnodes.find(channel_name);
            auto skelnode              = its->second;
            auto& skelnode_framevect_j = skelnode->_varmap["framevect_j"].get<framevect_t>();
            fmtx4 joint_JSPACE         = skelnode_framevect_j[f];
            skelnode->_jointMatrix     = joint_JSPACE;
          }
          ////////////////////////////////////////////////////
          aiNodeAnim* channel      = anim->mChannels[i];
          std::string channel_name = remapSkelName(channel->mNodeName.data);
          auto its                 = skelnodes.find(channel_name);
          auto skelnode            = its->second;
          fmtx4 OSPACE             = skelnode->concatenated();
          deco::printf(color, "fr<%d> ", f);
          deco::printf(yel, "%s (O): ", channel_name.c_str());
          deco::prints(OSPACE.dump4x3cn(), true);
        }
      }
    }
    OrkAssert(false);
    ////////////////////////////////////////////////////////////////
    return XgmAnim::Save(&xgmanim);
  } // if scene
  else{
    OrkAssert(false);
  }
  return nullptr;
}

} // namespace ork::meshutil
