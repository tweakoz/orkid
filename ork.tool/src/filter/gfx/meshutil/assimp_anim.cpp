///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include "assimp_util.inl"

INSTANTIATE_TRANSPARENT_RTTI(ork::MeshUtil::ASS_XGA_Filter, "ASS_XGA_Filter");

///////////////////////////////////////////////////////////////////////////////
namespace ork::MeshUtil {
///////////////////////////////////////////////////////////////////////////////

ASS_XGA_Filter::ASS_XGA_Filter() {
}

///////////////////////////////////////////////////////////////////////////////

void ASS_XGA_Filter::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

bool ASS_XGA_Filter::ConvertAsset(const tokenlist& toklist) {

  bool rval = false;
  ork::tool::FilterOptMap options;
  options.SetDefault("--in", "yo");
  options.SetDefault("--out", "yo");
  options.SetOptions(toklist);
  std::string inf  = options.GetOption("--in")->GetValue();
  std::string outf = options.GetOption("--out")->GetValue();

  ork::tool::ColladaExportPolicy policy;
  policy.mUnits            = ork::tool::UNITS_METER;
  const PoolString JointPS = AddPooledString("Joint");

  ork::file::Path GlbPath = inf;
  auto base_dir           = GlbPath.toBFS().parent_path();

  OrkAssert(boost::filesystem::exists(GlbPath.toBFS()));
  OrkAssert(boost::filesystem::is_regular_file(GlbPath.toBFS()));

  // printf("base_dir<%s>\n", base_dir.c_str());
  OrkAssert(boost::filesystem::exists(base_dir));
  OrkAssert(boost::filesystem::is_directory(base_dir));

  auto color = fvec3(1, 0, 1);

  deco::printf(color, "BEGIN: importing<%s> via Assimp\n", GlbPath.c_str());
  auto scene = aiImportFile(GlbPath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
  deco::printf(color, "END: importing scene<%p>\n", scene);
  if (scene) {
    lev2::XgmAnim xgmanim;
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
    auto& skelnodes = parsedskel._xgmskelmap;
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
        printf("name<%s> remapped -> <%s>\n", a.c_str(), b.c_str());
        matrix.dump(name);
      }
      for (int i = 0; i < n->mNumChildren; ++i) {
        nodestack.push(n->mChildren[i]);
      }
    }

    ////////////////////////////////////////

    if (scene->mNumAnimations > 0) {

      auto color = fvec3(1, 1, 0);

      aiAnimation* anim = scene->mAnimations[0];
      deco::printf(color, "numchannels<%d>\n", anim->mNumChannels);

      /////////////////////////////////////////////////////
      // compute number of frames
      /////////////////////////////////////////////////////

      int framecount = 0;
      for (int i = 0; i < anim->mNumChannels; i++) {
        aiNodeAnim* channel = anim->mChannels[i];
        if (channel->mNumPositionKeys > framecount)
          framecount = channel->mNumPositionKeys;
        if (channel->mNumRotationKeys > framecount)
          framecount = channel->mNumRotationKeys;
        if (channel->mNumScalingKeys > framecount)
          framecount = channel->mNumScalingKeys;
      }

      xgmanim.SetNumFrames(framecount);

      /////////////////////////////////////////////////////

      for (int i = 0; i < anim->mNumChannels; i++) {
        aiNodeAnim* channel = anim->mChannels[i];

        std::string channel_name = remapSkelName(channel->mNodeName.data);

        auto its        = skelnodes.find(channel_name);
        auto skelnode   = its->second;
        auto bindmatrix = skelnode->bindMatrix();
        auto invbindmtx = skelnode->_bindMatrixInverse;

        auto it = channel_remap.find(channel_name);
        if (it != channel_remap.end()) {
          channel_name = it->second;
        }
        deco::printf(color, "channel<%d:%p:%s>\n", i, channel, channel_name.c_str());
        deco::printf(color, "  num poskeys<%d>\n", channel->mNumPositionKeys);
        deco::printf(color, "  num rotkeys<%d>\n", channel->mNumRotationKeys);
        deco::printf(color, "  num scakeys<%d>\n", channel->mNumScalingKeys);

        /////////////////////////////
        // we assume pre-sampled frames here
        /////////////////////////////

        fvec3 curpos, cursca;
        fquat currot;

        PoolString objnameps         = AddPooledString("");
        PoolString ChannelPooledName = AddPooledString(channel_name.c_str());
        auto XgmChan                 = new ork::lev2::XgmDecompAnimChannel(objnameps, ChannelPooledName, JointPS);
        XgmChan->ReserveFrames(framecount);
        xgmanim.AddChannel(ChannelPooledName, XgmChan);

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
            currot                  = fquat(-rot.x, -rot.y, -rot.z, rot.w);
          }
          if (f < channel->mNumScalingKeys) {
            const aiVectorKey& scakey = channel->mScalingKeys[f];
            double time               = scakey.mTime;
            aiVector3D sca            = scakey.mValue;
            cursca                    = fvec3(sca.x, sca.y, sca.z);
          }
          // deco::printf(color, "frame<%s.%d> pos<%g %g %g>\n", channel_name.c_str(), f, curpos.x, curpos.y, curpos.z);
          // deco::printf(color, "frame<%s.%d> rot<%g %g %g %g>\n", channel_name.c_str(), f, currot.x, currot.y, currot.z,
          // currot.w);
          // deco::printf(color, "frame<%s.%d> sca<%g %g %g>\n", channel_name.c_str(), f, cursca.x, cursca.y, cursca.z);
          // const fmtx4& Matrix = MatrixChannelData->GetFrame(ifr);

          // float s = 0.0f;
          // fmtx4 t;
          // t.SetTranslation(curpos);
          // fmtx4 r;
          // r = currot.ToMatrix();

          // fmtx4 x = r;
          // x.Transpose();
          // x.SetTranslation(curpos);

          ork::lev2::DecompMtx44 decomp;
          decomp.mRot   = currot;
          decomp.mTrans = curpos;
          decomp.mScale = 1.0f;

          XgmChan->AddFrame(decomp);

          fmtx4 x;
          decomp.Compose(x, lev2::XFORM_COMPONENT_ALL);

          auto yel        = fvec3(1, 1, 0);
          auto whi        = fvec3(1, 1, 1);
          std::string xxx = deco::format(color, "fr<%d> ", f);
          xxx += deco::decorate(yel, channel_name + ":");
          xxx += x.dump(whi);
          deco::prints(xxx, true);
        }
      }
    }
    rval = ork::lev2::XgmAnim::Save(file::Path(outf.c_str()), &xgmanim);

  } // if scene

  ///////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////

  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::MeshUtil
///////////////////////////////////////////////////////////////////////////////
