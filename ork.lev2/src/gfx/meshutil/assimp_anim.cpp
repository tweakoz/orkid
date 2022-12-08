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

  auto color = fvec3(1, 0, 1);

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

    auto parsedskel = parseSkeleton(scene); // create and link skeleton
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

    auto& staticpose = xgmanim._static_pose;
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
      auto bindmatrix = skelnode->_bindMatrix;
      auto invbindmtx = skelnode->_bindMatrixInverse;

        auto xdump = bindmatrix.dump4x3cn();
        printf("xdump: channel_name<%s> mtx: %s\n", channel_name.c_str(), xdump.c_str());
        auto ydump = invbindmtx.dump4x3cn();
        printf("ydump: channel_name<%s> mtx: %s\n", channel_name.c_str(), ydump.c_str());
        printf("channel_name<%s> skelnode<%p> par<%p>\n", channel_name.c_str(), (void*) skelnode.get(), (void*) skelnode->_parent.get());

      auto& skelnode_framevect_n = skelnode->_varmap["framevect_n"].make<framevect_t>();

      auto it = channel_remap.find(channel_name);
      if (it != channel_remap.end()) {
        channel_name = it->second;
      }
      deco::printf(fvec3::White(), "/////////////////////////////////////////////\n");
      deco::printf(fvec3::White(), "channel<%d:%p:%s>\n", i, channel, channel_name.c_str());
      deco::printf(fvec3::White(), "  num poskeys<%d>\n", channel->mNumPositionKeys);
      deco::printf(fvec3::White(), "  num rotkeys<%d>\n", channel->mNumRotationKeys);
      deco::printf(fvec3::White(), "  num scakeys<%d>\n", channel->mNumScalingKeys);

      //////////////////////////////////////////////
      // create matrix channel and add to animation
      //////////////////////////////////////////////

      std::string objnameps         = "";
      auto XgmChan                 = std::make_shared<XgmMatrixAnimChannel>(objnameps, channel_name, "Joint");
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
        // The transformation matrix computed from these values replaces the node's original transformation matrix at a specific time
        // https://assimp.sourceforge.net/lib_html/structai_node_anim.html
        /////////////////////////////

        fmtx4 R, S, T;
        R.fromQuaternion(currot);
        S.setScale(cursca.x, cursca.y, cursca.z);
        T.setTranslation(curpos);
        fmtx4 XF_NSPACE = T*(R*S); //yoyo
        skelnode_framevect_n.push_back(XF_NSPACE);

        if(f==0){
          auto yel        = fvec3(1, 1, 0);
          auto whi        = fvec3(1, 1, 1);
          std::string xxx = deco::format(color, "fr<%d> ", f);
          xxx += deco::decorate(yel, channel_name + "(N):");
          xxx += XF_NSPACE.dump4x3cn();
          deco::prints(xxx, true);
        }


      } // for (int f = 0; f < framecount; f++) {
    }   // for (int i = 0; i < anim->mNumChannels; i++) {

    /////////////////////////////////////////////////////
    // compute J frames
    /////////////////////////////////////////////////////

    auto yel = fvec3(1, 1, 0);
    auto whi = fvec3(1, 1, 1);
    if (1) {
      deco::printf(fvec3::White(), "/////////////////////////////////////////////\n");
      deco::printf(fvec3::White(), "// J Space Anim\n");
      deco::printf(fvec3::White(), "/////////////////////////////////////////////\n");
      for (int i = 0; i < anim->mNumChannels; i++) {
        //deco::printf(color, "///////////\n");
        aiNodeAnim* channel        = anim->mChannels[i];
        std::string channel_name   = remapSkelName(channel->mNodeName.data);
        auto its                   = skelnodes.find(channel_name);
        auto skelnode              = its->second;
        auto& skelnode_framevect_n = skelnode->_varmap["framevect_n"].get<framevect_t>();
        auto parskelnode           = skelnode->_parent;
        auto this_bind = skelnode->_bindMatrixInverse.inverse();
        ////////////////////////////////////////////////////////////////////
        bool par_has_fvn = parskelnode->_varmap.hasKey("framevect_n");
        printf( "parskelnode<%p>\n", (void*) parskelnode.get() );
        printf( "par_has_fvn<%d>\n", (int) par_has_fvn );
        for (size_t f = 0; f < framecount; f++) {

          fmtx4 JSPACE;

          auto this_mtx = skelnode_framevect_n[f];

          if(parskelnode and par_has_fvn){
            auto par_bind = parskelnode->_bindMatrixInverse.inverse();
            auto& par_skelnode_framevect_n = parskelnode->_varmap["framevect_n"].get<framevect_t>();
            auto par_mtx = par_skelnode_framevect_n[f];
            JSPACE.correctionMatrix(this_mtx,//
                                    par_mtx);

          }
          ////////////////////////////////////////////////////////////////////
          else{
            JSPACE = this_mtx;
          }

          //JSPACE.compose(fvec3(0,1,0),fquat(),0.5);
          ////////////////////////////////////////////////////////////////////
          
          if(f==0){
            deco::printf(color, "fr<%d> ", f);
            deco::printf(yel, "%s (J): ", channel_name.c_str());
            deco::prints(JSPACE.dump4x3cn(), true);
          }
          ////////////////////////////////////////////////////
          // add to animation ! // yoyo
          ////////////////////////////////////////////////////

          auto XgmChan = skelnode->_varmap["xgmchan"].get<animchannel_ptr_t>();
          auto as_decomchan = std::dynamic_pointer_cast<XgmMatrixAnimChannel>(XgmChan);

          auto jdump = JSPACE.dump4x3cn();
          printf("jdump: joint<%s> mtx: %s\n", channel_name.c_str(), jdump.c_str());

          as_decomchan->setFrame(f,JSPACE);
        }
      }
    }
   // OrkAssert(false);
    ////////////////////////////////////////////////////////////////
    return XgmAnim::Save(&xgmanim);
  } // if scene
  else{
    OrkAssert(false);
  }
  return nullptr;
}

} // namespace ork::meshutil
