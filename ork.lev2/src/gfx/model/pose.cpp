////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/logger.h>

#define ENABLE_ANIM

using namespace std::string_literals;

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_pose  = logger()->createChannel("gfxanim.pose", fvec3(1, 0.7, 1));
static logchannel_ptr_t logchan_pose2 = logger()->createChannel("gfxanim.pose", fvec3(1, 0.8, .9));
///////////////////////////////////////////////////////////////////////////////

XgmBlendPoseInfo::XgmBlendPoseInfo()
    : _posecallback(NULL) {
  initBlendPose();
}

///////////////////////////////////////////////////////////////////////////////

void XgmBlendPoseInfo::initBlendPose() {
  _numanims = 0;
}

///////////////////////////////////////////////////////////////////////////////

void XgmBlendPoseInfo::addPose(const fmtx4& mat, float weight) {
  OrkAssert(_numanims < kmaxblendanims);

  DecompTransform decomp;
  decomp.decompose(mat);

  const auto& pos    = decomp._translation;
  const auto& orient = decomp._rotation;

  if (0)
    logchan_pose->log(
        "XgmBlendPoseInfo pos<%g %g %g> orient<%g %g %g %g>", //
        pos.x,
        pos.y,
        pos.z, //
        orient.w,
        orient.x,
        orient.y,
        orient.z);

  _matrices[_numanims] = mat;
  _weights[_numanims]  = weight;

  _numanims++;
}

///////////////////////////////////////////////////////////////////////////////

void XgmBlendPoseInfo::computeMatrix(fmtx4& outmatrix) const {
  // printf("_numanims<%d>", _numanims);

  switch (_numanims) {
    case 0: // nop
      break;
    case 1: { // just copy the first matrix

      outmatrix = _matrices[0];

      if (_posecallback) // Callback for decomposed, pre-concatenated, blended joint info
        _posecallback->PostBlendPreConcat(outmatrix);

      // auto cdump = outmatrix.dump();
      // logchan_pose->log("cdump: %s", cdump.c_str());

    } break;

    case 2: { // decompose 2 matrices and lerp components (ideally anims will be stored pre-decomposed)

      float fw = fabs(_weights[0] + _weights[1] - 1.0f);
      // printf( "aw0<%f> aw1<%f>", AnimWeight[0], AnimWeight[1]);
      OrkAssert(fw < float(0.01f));

      const fmtx4& a = _matrices[0];
      const fmtx4& b = _matrices[1];

      float flerp = _weights[1];

      if (flerp < 0.0f)
        flerp = 0.0f;
      if (flerp > 1.0f)
        flerp = 1.0f;
      float iflerp = 1.0f - flerp;

      OrkAssert(false);
    } break;

    default: {
      // gotta figure out a N-way quaternion blend first
      // on the DS, Lerping each matrix column (NormalX, NormalY, NormalZ) gives
      // decent results. Try it.
      OrkAssert(false);
      outmatrix = fmtx4::Identity();
    } break;
  }
}

///////////////////////////////////////////////////////////////////////////////

XgmLocalPose::XgmLocalPose(const XgmSkeleton& skel)
    : _skeleton(skel) {
  int inumchannels = skel.numJoints();
  if (inumchannels == 0) {
    inumchannels = 1;
  }

  _local_matrices.resize(inumchannels);
  _concat_matrices.resize(inumchannels);
  _blendposeinfos.resize(inumchannels);
}

/// ////////////////////////////////////////////////////////////////////////////
/// bind the animinst to the pose
/// this will figure out which anim channels match (bind) to joints in the skeleton
/// so that it does not have to be done every frame
/// ////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::bindAnimInst(XgmAnimInst& animinst) {
  if (animinst._animation) {
    float fweight = animinst.GetWeight();

    const XgmAnim& anim        = *animinst._animation;
    const auto& joint_channels = anim._jointanimationchannels;

    const XgmAnim::matrix_lut_t& static_pose = anim._static_pose;

    int ipidx = 0;
    int ispi  = 0;
    for (auto it : static_pose) {
      const std::string& JointName = it.first;

      int iskelindex = _skeleton.jointIndex(JointName);

      int inumjinmap = _skeleton.mmJointNameMap.size();

      // logchan_pose->log("bindAnimInst jname<%s> iskelindex<%d> inumjinmap<%d>", JointName.c_str(), iskelindex, inumjinmap);

      if (-1 != iskelindex) {

        auto itanim = joint_channels.find(JointName);

        // bool found = itanim == joint_channels.end();
        // logchan_pose->log("bindAnimInst jname<%s> found<%d>", JointName.c_str(), int(found));

        if (itanim == joint_channels.end()) {

          const fmtx4& PoseMatrix = it.second;
          // TODO: Callback for programmer-controlled joints, pre-blended
          // _blendposeinfos[iskelindex].AddPose(PoseMatrix, fweight, components);

          animinst.setPoseBinding(ipidx++, XgmAnimInst::Binding(iskelindex, ispi));
        }
      }
      ispi++;
    }
    animinst.setPoseBinding(ipidx++, XgmAnimInst::Binding(0xffff, 0xffff));

    size_t inumanimchannels = joint_channels.size();
    // float frame = animinst.GetCurrentFrame();
    // float numframes = animinst.GetNumFrames();
    // int iframe = int(frame);
    int ichidx = 0;

    // for (int is = 0; is < _skeleton.numJoints(); is++)
    // logchan_pose->log("skel bone<%d:%s>", is, _skeleton.GetJointName(is).c_str());

    int ichiti = 0;
    for (auto it : joint_channels) {
      const auto& channel_name = it.first;
      auto matrix_channel      = it.second;
      int iskelindex           = _skeleton.jointIndex(channel_name);

      // logchan_pose->log("bind channel<%s> skidx<%d>", channel_name.c_str(), iskelindex);

      if (-1 != iskelindex) {
        animinst.setAnimBinding(ichidx++, XgmAnimInst::Binding(iskelindex, ichiti));
        // const fmtx4 &ChannelDecomp = MtxChannelData->GetFrame(iframe);
        // TODO: Callback for programmer-controlled joints, pre-blended
        // _blendposeinfos[iskelindex].AddPose(ChannelDecomp, fweight, components);
      }
      ichiti++;
    }
    animinst.setAnimBinding(ichidx++, XgmAnimInst::Binding(0xffff, 0xffff));
  }
}

/// ////////////////////////////////////////////////////////////////////////////
/// unbind the animinst to the pose
/// ////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::unbindAnimInst(XgmAnimInst& AnimInst) {
  AnimInst.setPoseBinding(0, XgmAnimInst::Binding(0xffff, 0xffff));
  AnimInst.setAnimBinding(0, XgmAnimInst::Binding(0xffff, 0xffff));
  if (AnimInst._animation) {
    const XgmAnim& anim = *AnimInst._animation;

    size_t inumjointchannels = anim._jointanimationchannels.size();
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::applyAnimInst(const XgmAnimInst& animinst) {
#ifdef ENABLE_ANIM
  const XgmAnimMask& Mask = animinst.RefMask();
  float fweight           = animinst.GetWeight();
  ////////////////////////////////////////////////////
  // retrieve anim information
  auto animation = animinst._animation;
  ////////////////////////////////////////////////////
  if (animation) {
    ////////////////////////////////////////////////////
    // set animation's static pose channels (channels that are not animated) first
    ////////////////////////////////////////////////////
    const XgmAnim::matrix_lut_t& static_pose = animation->_static_pose;
    for (int ipidx = 0; ipidx < XgmAnimInst::kmaxbones; ipidx++) {
      const XgmAnimInst::Binding& binding = animinst.getPoseBinding(ipidx);
      int iskelindex                      = binding.mSkelIndex;
      if (iskelindex != 0xffff) {
        int iposeindex      = binding.mChanIndex;
        const fmtx4& matrix = static_pose.GetItemAtIndex(iposeindex).second;
        _blendposeinfos[iskelindex].addPose(matrix, fweight);
      } else {
        break;
      }
    }
    //////////////////////////////////////////////////////////////////////////////////////////
    // normal anim on a skinned model
    //////////////////////////////////////////////////////////////////////////////////////////
    size_t inumanimchannels = animation->_jointanimationchannels.size();
    float frame             = animinst._current_frame;
    size_t numframes        = animinst.numFrames();
    int iframe              = int(frame);

    // logchan_pose2->log("apply animinst anm<%p> frame<%d> numframes<%zu> inumanimchannels<%zu>", //
    //                 (void*) animation, //
    //               iframe, //
    //             numframes, //
    //           inumanimchannels);

    const auto& joint_channels = animation->_jointanimationchannels;

    //////////////////////////////////////////////////////////////////////
    // dump skeleton for reference
    //////////////////////////////////////////////////////////////////////

    if (0) {
      for (int iaidx = 0; iaidx < XgmAnimInst::kmaxbones; iaidx++) {
        auto& binding  = animinst.getAnimBinding(iaidx);
        int iskelindex = binding.mSkelIndex;
        if (iskelindex != 0xffff) {
          auto jname     = _skeleton.GetJointName(iskelindex);
          int par        = _skeleton.GetJointParent(iskelindex);
          auto this_bind = _skeleton._bindMatrices[iskelindex];
          auto par_bind  = _skeleton._bindMatrices[par];
          fmtx4 skel_rel;
          skel_rel.correctionMatrix(par_bind, this_bind);
          auto skdump = skel_rel.dump4x3cn();
          logchan_pose->log(
              "skldump: joint<%d:%s> skel_rel: %s", //
              iskelindex,                           //
              jname.c_str(),                        //
              skdump.c_str());
        }
      }
    }

    //////////////////////////////////////////////////////////////////////

    for (int iaidx = 0; iaidx < XgmAnimInst::kmaxbones; iaidx++) {
      auto& binding  = animinst.getAnimBinding(iaidx);
      int iskelindex = binding.mSkelIndex;
      // printf( "iaidx<%d> iskelindex<%d> inumanimchannels<%zu>", iaidx, iskelindex, inumanimchannels );
      if (iskelindex != 0xffff) {
        int ichanindex = binding.mChanIndex;

        // logchan_pose->log("apply on iskelidx<%d> ichanindex<%d>", iskelindex, ichanindex);

        auto joint_data = joint_channels.GetItemAtIndex(ichanindex).second;
        OrkAssert(joint_data);
        size_t numframes = joint_data->_sampledFrames.size();
        // logchan_pose->log("joint_data<%p> numframes<%zu>", (void*)joint_data, numframes);
        const fmtx4& matrix = joint_data->GetFrame(iframe);

        // auto jname = _skeleton.GetJointName(iskelindex);
        // auto adump = matrix.dump4x3cn();
        // logchan_pose2->log("anmdump: joint<%d:%s> anm_jmtx: %s", //
        //                 iskelindex, //
        //               jname.c_str(), //
        //             adump.c_str());
        //_blendposeinfos[iskelindex].addPose(matrix, fweight);//yoyo
        _blendposeinfos[iskelindex].addPose(matrix, fweight); // yoyo
      }
    }
    // OrkAssert(false);
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::bindPose(void) {
#ifdef ENABLE_ANIM
  int inumjoints = NumJoints();

  ///////////////////////////////////////////
  // initialize to Skeletons Bind Pose
  ///////////////////////////////////////////
  for (int ij = 0; ij < inumjoints; ij++) {

    auto this_bind = _skeleton._bindMatrices[ij];

    int par = _skeleton.GetJointParent(ij);
    // printf( "par[%d]: %d\n", ij, par );
    if (par >= 0) {
      auto par_bind = _skeleton._bindMatrices[par];
      _local_matrices[ij].correctionMatrix(par_bind, this_bind);
    } else {
      _local_matrices[ij] = this_bind;
    }
  }

  ///////////////////////////////////////////
  // Init Matrix Blending infos
  ///////////////////////////////////////////

  for (int ij = 0; ij < inumjoints; ij++) {
    _blendposeinfos[ij].initBlendPose();
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////
// compute blended matrices
///////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::blendPoses(void) {
#ifdef ENABLE_ANIM
  int inumjoints = NumJoints();
  for (int i = 0; i < inumjoints; i++) {
    int inumanms = _blendposeinfos[i]._numanims;
    _blendposeinfos[i].computeMatrix(_local_matrices[i]);
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Concatenate
///////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::concatenate(void) {

  float fminx = std::numeric_limits<float>::max();
  float fminy = std::numeric_limits<float>::max();
  float fminz = std::numeric_limits<float>::max();
  float fmaxx = -std::numeric_limits<float>::max();
  float fmaxy = -std::numeric_limits<float>::max();
  float fmaxz = -std::numeric_limits<float>::max();

  _concat_matrices = _local_matrices;

  if (_skeleton.miRootNode >= 0) {
    const fmtx4& RootAnimMat               = _concat_matrices[_skeleton.miRootNode];
    _concat_matrices[_skeleton.miRootNode] = fmtx4::multiply_ltor(RootAnimMat, _skeleton.mTopNodesMatrix);

    int inumbones = _skeleton.numBones();
    for (int ib = 0; ib < inumbones; ib++) {
      const XgmBone& Bone       = _skeleton.bone(ib);
      int iparent               = Bone._parentIndex;
      int ichild                = Bone._childIndex;
      const fmtx4& ParentMatrix = _concat_matrices[iparent];
      const fmtx4& LocMatrix    = _concat_matrices[ichild];

      std::string parname = _skeleton.GetJointName(iparent).c_str();
      std::string chiname = _skeleton.GetJointName(ichild).c_str();

      // fmtx4 temp    = (ParentMatrix * LocMatrix);
      fmtx4 temp = fmtx4::multiply_ltor(ParentMatrix, LocMatrix);

      logchan_pose2->log("////////\n");
      logchan_pose2->log(
          "ib<%d> ip<%d:%s> ic<%d:%s>", //
          ib,
          iparent,
          parname.c_str(),
          ichild,
          chiname.c_str());

      auto this_bind = _skeleton._bindMatrices[ichild];
      auto par_bind  = _skeleton._bindMatrices[iparent];
      fmtx4 skel_rel;
      skel_rel.correctionMatrix(par_bind, this_bind);

      //logchan_pose2->log(" par<%s>", ParentMatrix.dump4x3cn().c_str());
      //logchan_pose->log(" pbi<%s>", par_bind.dump4x3cn().c_str());
      logchan_pose2->log(" chi<%s>", LocMatrix.dump4x3cn().c_str());
      logchan_pose2->log(" cbi<%s>", this_bind.dump4x3cn().c_str());
      logchan_pose->log(" tmp<%s>", temp.dump4x3cn().c_str());
      //logchan_pose->log(" skr<%s>", skel_rel.dump4x3cn().c_str());

      _concat_matrices[ichild] = temp;

      auto& child_pose_info = _blendposeinfos[ichild];
      auto child_pose_cb    = child_pose_info._posecallback;

      if (child_pose_cb)
        child_pose_cb->PostBlendPostConcat(_concat_matrices[ichild]);

      fvec3 vtrans = _concat_matrices[ichild].translation();

      fminx = std::min(fminx, vtrans.x);
      fminy = std::min(fminy, vtrans.y);
      fminz = std::min(fminz, vtrans.z);

      fmaxx = std::max(fmaxx, vtrans.x);
      fmaxy = std::max(fmaxy, vtrans.y);
      fmaxz = std::max(fmaxz, vtrans.z);
    }
  }

  float fmidx = (fminx + fmaxx) * 0.5f;
  float fmidy = (fminy + fmaxy) * 0.5f;
  float fmidz = (fminz + fmaxz) * 0.5f;

  fvec3 range((fmaxx - fminx), (fmaxy - fminy), (fmaxz - fminz));

  float frange = range.magnitude() * 0.5f;

  mObjSpaceBoundingSphere = fvec4(fmidx, fmidy, fmidz, frange);

  mObjSpaceAABoundingBox.SetMinMax(fvec3(fminx, fminy, fminz), fvec3(fmaxx, fmaxy, fmaxz));
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmLocalPose::dump() const {
  std::string rval;
  if (_skeleton.miRootNode >= 0) {
    int inumjoints = _skeleton.numJoints();
    for (int ij = 0; ij < inumjoints; ij++) {
      std::string name = _skeleton.GetJointName(ij).c_str();
      const auto& jmtx = _local_matrices[ij];
      rval += FormatString("%28s", name.c_str());
      rval += ": "s + jmtx.dump() + "\n"s;
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmLocalPose::dumpc(fvec3 color) const {
  std::string rval;
  if (_skeleton.miRootNode >= 0) {
    const fmtx4& RootAnimMat = _local_matrices[_skeleton.miRootNode];
    int inumjoints           = _skeleton.numJoints();
    fvec3 ca                 = color;
    fvec3 cb                 = color * 0.7;

    for (int ij = 0; ij < inumjoints; ij++) {
      fvec3 cc         = (ij & 1) ? cb : ca;
      std::string name = _skeleton.GetJointName(ij).c_str();
      const auto& jmtx = _local_matrices[ij];
      rval += deco::format(cc, "%28s: ", name.c_str());
      rval += jmtx.dump4x3cn() + "\n"s;
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmLocalPose::invdumpc(fvec3 color) const {
  std::string rval;
  if (_skeleton.miRootNode >= 0) {
    const fmtx4& RootAnimMat = _local_matrices[_skeleton.miRootNode];
    int inumjoints           = _skeleton.numJoints();
    fvec3 ca                 = color;
    fvec3 cb                 = color * 0.7;

    for (int ij = 0; ij < inumjoints; ij++) {
      fvec3 cc         = (ij & 1) ? cb : ca;
      std::string name = _skeleton.GetJointName(ij).c_str();
      auto jmtx        = _local_matrices[ij].inverse();
      rval += deco::asciic_rgb(cc);
      rval += FormatString("%28s", name.c_str());
      rval += ": "s + jmtx.dump4x3(cc) + ""s;
      rval += deco::asciic_reset();
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

int XgmLocalPose::NumJoints() const {
  return _skeleton.numJoints();
}

///////////////////////////////////////////////////////////////////////////////
// WorldPose
///////////////////////////////////////////////////////////////////////////////

XgmWorldPose::XgmWorldPose(const XgmSkeleton& skel)
    : _skeleton(skel) {
}

///////////////////////////////////////////////////////////////////////////////

void XgmWorldPose::apply(const fmtx4& worldmtx, const XgmLocalPose& localpose) {
  int inumj = localpose.NumJoints();
  _worldmatrices.resize(inumj);
  for (int ij = 0; ij < inumj; ij++) {
    fmtx4 invbind     = _skeleton._inverseBindMatrices[ij];
    std::string bname = _skeleton.GetJointName(ij);
    fmtx4 anim_concat = fmtx4::multiply_ltor(
        invbind, //
        localpose._concat_matrices[ij]);
    auto finalmtx = fmtx4::multiply_ltor(anim_concat, worldmtx);
    // auto fdump = finalmtx.dump4x3cn();
    // printf("fdump: joint<%d:%s> mtx: %s\n", //
    //      ij, //
    //    bname.c_str(), //
    //  fdump.c_str());
    _worldmatrices[ij] = finalmtx;
  }
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmWorldPose::dumpc(fvec3 color) const {
  std::string rval;
  if (_skeleton.miRootNode >= 0) {
    int inumjoints = _skeleton.numJoints();
    fvec3 ca       = color;
    fvec3 cb       = color * 0.7;

    for (int ij = 0; ij < inumjoints; ij++) {
      fvec3 cc         = (ij & 1) ? cb : ca;
      std::string name = _skeleton.GetJointName(ij).c_str();
      const auto& jmtx = _worldmatrices[ij];
      rval += deco::format(cc, "%28s: ", name.c_str());
      rval += jmtx.dump4x3cn() + "\n"s;
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
