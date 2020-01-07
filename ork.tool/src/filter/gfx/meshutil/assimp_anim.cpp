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

INSTANTIATE_TRANSPARENT_RTTI(ork::MeshUtil::ASS_XGA_Filter, "ASS_XGA_Filter");

///////////////////////////////////////////////////////////////////////////////
namespace ork::MeshUtil {
///////////////////////////////////////////////////////////////////////////////

void get_bounding_box_for_node(const aiScene* scene, const aiNode* nd, aiVector3D& min, aiVector3D& max, aiMatrix4x4& trafo);
fmtx4 convertMatrix44(const aiMatrix4x4& inp);

ASS_XGA_Filter::ASS_XGA_Filter() {
}

///////////////////////////////////////////////////////////////////////////////

void ASS_XGA_Filter::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

bool ASS_XGA_Filter::ConvertAsset(const tokenlist& toklist) {
  ork::tool::FilterOptMap options;
  options.SetDefault("--in", "yo");
  options.SetDefault("--out", "yo");
  options.SetOptions(toklist);
  std::string inf  = options.GetOption("--in")->GetValue();
  std::string outf = options.GetOption("--out")->GetValue();

  ork::file::Path GlbPath = inf;
  auto base_dir           = GlbPath.toBFS().parent_path();

  OrkAssert(boost::filesystem::exists(GlbPath.toBFS()));
  OrkAssert(boost::filesystem::is_regular_file(GlbPath.toBFS()));

  printf("base_dir<%s>\n", base_dir.c_str());
  OrkAssert(boost::filesystem::exists(base_dir));
  OrkAssert(boost::filesystem::is_directory(base_dir));

  printf("BEGIN: importing<%s> via Assimp\n", GlbPath.c_str());
  auto scene = aiImportFile(GlbPath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
  printf("END: importing scene<%p>\n", scene);
  if (scene) {
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
    // we assume a single animation per file
    /////////////////////////////

    int numanims = scene->mNumAnimations;
    printf("numanims<%d>\n", numanims);
    OrkAssert(numanims == 1);

    aiAnimation* anim = scene->mAnimations[0];
    printf("numchannels<%d>\n", anim->mNumChannels);

    for (int i = 0; i < anim->mNumChannels; i++) {
      aiNodeAnim* channel = anim->mChannels[i];
      printf("channel<%d:%p:%s>\n", i, channel, channel->mNodeName.data);
      printf("  num poskeys<%d>\n", channel->mNumPositionKeys);
      printf("  num rotkeys<%d>\n", channel->mNumRotationKeys);
      printf("  num scakeys<%d>\n", channel->mNumScalingKeys);

      /////////////////////////////
      // we assume pre-sampled frames here
      /////////////////////////////

      int framecount = 0;
      if (channel->mNumPositionKeys > framecount)
        framecount = channel->mNumPositionKeys;
      if (channel->mNumRotationKeys > framecount)
        framecount = channel->mNumRotationKeys;
      if (channel->mNumScalingKeys > framecount)
        framecount = channel->mNumScalingKeys;

      fvec3 curpos, cursca;
      fquat currot;

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
        }
        printf("frame<%s.%d> pos<%g %g %g>\n", channel->mNodeName.data, f, curpos.x, curpos.y, curpos.z);
        printf("frame<%s.%d> rot<%g %g %g %g>\n", channel->mNodeName.data, f, currot.x, currot.y, currot.z, currot.w);
        printf("frame<%s.%d> sca<%g %g %g>\n", channel->mNodeName.data, f, cursca.x, cursca.y, cursca.z);
      }
    }
  }
  bool brval = false;

  ///////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////

  return true;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::MeshUtil
///////////////////////////////////////////////////////////////////////////////
