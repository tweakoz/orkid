////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
  for (int i = 0; i < kmaxblendanims; i++) {
    _operations[i] = 0;
    _weights[i]    = 0.0f;
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmBlendPoseInfo::addPose(const DecompMatrix& mat, float weight) {
  OrkAssert(_numanims < kmaxblendanims);

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

      const DecompMatrix& decom0 = _matrices[0];

      fmtx4 T, R, S;
      T.setTranslation(decom0._position);
      R = fmtx4(decom0._orientation);
      S.setScale(decom0._scale);

      outmatrix = T * R * S;

      if (_posecallback) // Callback for decomposed, pre-concatenated, blended joint info
        _posecallback->PostBlendPreConcat(outmatrix);

      // auto cdump = outmatrix.dump();
      // logchan_pose->log("cdump: %s", cdump.c_str());

    } break;

    case 2: { // decompose 2 matrices and lerp components (ideally anims will be stored pre-decomposed)

      switch (_operations[1]) {
        case 0: { // blend
          float fw = fabs(_weights[0] + _weights[1] - 1.0f);
          // printf( "aw0<%f> aw1<%f>", AnimWeight[0], AnimWeight[1]);
          OrkAssert(fw < float(0.01f));

          const DecompMatrix& a = _matrices[0];
          const DecompMatrix& b = _matrices[1];

          float flerp = _weights[1];

          if (flerp < 0.0f)
            flerp = 0.0f;
          if (flerp > 1.0f)
            flerp = 1.0f;
          float iflerp = 1.0f - flerp;

          DecompMatrix blended;
          blended._position.lerp(a._position, b._position, flerp);
          blended._scale.lerp(a._scale, b._scale, flerp);
          blended._orientation = fquat::slerp(a._orientation, b._orientation, flerp);

          outmatrix.compose2(
              blended._position,    //
              blended._orientation, //
              blended._scale.x,     //
              blended._scale.y,     //
              blended._scale.z);
          break;
        }
        case 1: { // layerXF

          const DecompMatrix& a = _matrices[0];
          const DecompMatrix& b = _matrices[1];
          fmtx4 mtxA, mtxB;

          mtxA.compose2(
              a._position,    //
              a._orientation, //
              a._scale.x,     //
              a._scale.y,     //
              a._scale.z);
          mtxB.compose2(
              b._position,    //
              b._orientation, //
              b._scale.x,     //
              b._scale.y,     //
              b._scale.z);

          outmatrix = mtxB * mtxA;
          break;
        }
        default:
          OrkAssert(false);
      }

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

XgmLocalPose::XgmLocalPose(xgmskeleton_constptr_t skel)
    : _skeleton(skel) {
  int inumchannels = skel->numJoints();
  if (inumchannels == 0) {
    inumchannels = 1;
  }

  _local_matrices.resize(inumchannels);
  _concat_matrices.resize(inumchannels);
  _bindrela_matrices.resize(inumchannels);
  _blendposeinfos.resize(inumchannels);
  _boneprops.resize(inumchannels);
}

///////////////////////////////////////////////////////////////////////////////

XgmSkelApplicator::XgmSkelApplicator(xgmskeleton_constptr_t skeleton)
    : _skeleton(skeleton) {
}

void XgmSkelApplicator::bindToBone(const std::string& a){

  int skelindex = _skeleton->jointIndex(a);
  _bones2apply.push_back(skelindex);
  //printf("XgmSkelApplicator ska<%d> skb<%d> skc<%d>\n", _iskelindexA, _iskelindexB, _iskelindexC);
}

xgmskelapplicator_ptr_t XgmSkelApplicator::clone() const{
  auto clone = std::make_shared<XgmSkelApplicator>(_skeleton);
  clone->_bones2apply = _bones2apply;
  return clone;
}

fmtx4 DecompMatrix::compose() const {
  fmtx4 rval;
  rval.compose2(
      _position,    //
      _orientation, //
      _scale.x,     //
      _scale.y,     //
      _scale.z);
  return rval;
}

void DecompMatrix::decompose(fmtx4 inp) {
  inp.decompose(
      _position,    //
      _orientation, //
      _scale.x);
  _scale.y = _scale.x;
  _scale.z = _scale.x;
}

void XgmSkelApplicator::apply(fn_t the_fn) const {
  for( int i : _bones2apply ){
    the_fn(i);
  }
}

/// ////////////////////////////////////////////////////////////////////////////
/// bind the animinst to the pose
/// this will figure out which anim channels match (bind) to joints in the skeleton
/// so that it does not have to be done every frame
/// ////////////////////////////////////////////////////////////////////////////

void XgmAnimInst::bindToSkeleton(xgmskeleton_constptr_t skeleton) {
  if (_animation) {
    float fweight = GetWeight();

    const auto& joint_channels = _animation->_jointanimationchannels;

    const XgmAnim::matrix_lut_t& static_pose = _animation->_static_pose;

    int ipidx = 0;
    int ispi  = 0;
    for (auto it : static_pose) {
      const std::string& JointName = it.first;

      int iskelindex = skeleton->jointIndex(JointName);

      int inumjinmap = skeleton->mmJointNameMap.size();

      // logchan_pose->log("bindAnimInst jname<%s> iskelindex<%d> inumjinmap<%d>", JointName.c_str(), iskelindex, inumjinmap);

      if (-1 != iskelindex) {

        auto itanim = joint_channels.find(JointName);

        if (itanim == joint_channels.end()) {

          const DecompMatrix& PoseMatrix = it.second;

          bool enabled = _mask->isEnabled(iskelindex);
          if (enabled) {
            _poser->setPoseBinding(ipidx++, XgmSkeletonBinding(iskelindex, ispi));
          }
        }
      }
      ispi++;
    }
    _poser->setPoseBinding(ipidx++, XgmSkeletonBinding(0xffff, 0xffff));

    size_t inumanimchannels = joint_channels.size();

    int ichidx = 0;

    int ichiti = 0;
    for (auto it : joint_channels) {
      const auto& channel_name = it.first;
      auto matrix_channel      = it.second;
      int iskelindex           = skeleton->jointIndex(channel_name);

      // logchan_pose->log("bind channel<%s> skidx<%d>", channel_name.c_str(), iskelindex);

      if (-1 != iskelindex) {

        bool enabled = _mask->isEnabled(iskelindex);
        if (enabled) {
          _poser->setAnimBinding(ichidx++, XgmSkeletonBinding(iskelindex, ichiti));
        }
      }
      ichiti++;
    }
    _poser->setAnimBinding(ichidx++, XgmSkeletonBinding(0xffff, 0xffff));
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::identityPose(void) {
#ifdef ENABLE_ANIM
  int inumjoints = NumJoints();

  ///////////////////////////////////////////
  // initialize to Skeletons Bind Pose
  ///////////////////////////////////////////
  for (int ij = 0; ij < inumjoints; ij++) {

    _boneprops[ij] = 0;

    _local_matrices[ij] = fmtx4();
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

void XgmLocalPose::bindPose(void) {
#ifdef ENABLE_ANIM
  int inumjoints = NumJoints();

  ///////////////////////////////////////////
  // initialize to Skeletons Bind Pose
  ///////////////////////////////////////////
  for (int ij = 0; ij < inumjoints; ij++) {

    _boneprops[ij] = 0;

    auto this_bind = _skeleton->_bindMatrices[ij];

    int par = _skeleton->GetJointParent(ij);
    // printf( "ij<%d> par<%d>\n", ij, par );
    if (par >= 0) {
      auto par_bind = _skeleton->_bindMatrices[par];
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

void XgmLocalPose::poseJoint(int iskelindex, float fweight, const DecompMatrix& mtx){
    _blendposeinfos[iskelindex].addPose(mtx, fweight);
}

///////////////////////////////////////////////////////////////////////////////

DecompMatrix XgmLocalPose::decompLocal(int iskelindex) const {
  fmtx4 local = _local_matrices[iskelindex];
  DecompMatrix rval;
  rval.decompose(local);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void XgmAnimInst::applyToPose(xgmlocalpose_ptr_t localpose) const {
#ifdef ENABLE_ANIM
  float fweight           = GetWeight();
  ////////////////////////////////////////////////////
  // retrieve anim information
  auto animation = _animation;
  ////////////////////////////////////////////////////
  if (animation) {
    ////////////////////////////////////////////////////
    // set animation's static pose channels (channels that are not animated) first
    ////////////////////////////////////////////////////
    const XgmAnim::matrix_lut_t& static_pose = animation->_static_pose;
    for (int ipidx = 0; ipidx < kmaxbones; ipidx++) {
      const XgmSkeletonBinding& binding = _poser->getPoseBinding(ipidx);
      int iskelindex                    = binding.mSkelIndex;
      if (iskelindex != 0xffff) {
        int iposeindex             = binding.mChanIndex;
        const DecompMatrix& decomp = static_pose.GetItemAtIndex(iposeindex).second;
        localpose->_blendposeinfos[iskelindex].addPose(decomp, fweight);
      } else {
        break;
      }
    }
    //////////////////////////////////////////////////////////////////////////////////////////
    // normal anim on a skinned model
    //////////////////////////////////////////////////////////////////////////////////////////
    size_t inumanimchannels = animation->_jointanimationchannels.size();
    float frame             = _current_frame;
    size_t numframes        = numFrames();
    int iframe              = int(frame);

    if (0) {
      logchan_pose2->log(
          "apply animinst anm<%p> frame<%d> numframes<%zu> inumanimchannels<%zu>", //
          (void*)animation.get(),                                                        //
          iframe,                                                                  //
          numframes,                                                               //
          inumanimchannels);
    }

    //////////////////////////////////////////////////////////////////////
    // dump skeleton for reference
    //////////////////////////////////////////////////////////////////////

    if (0) {
      for (int iaidx = 0; iaidx < kmaxbones; iaidx++) {
        auto& binding  = _poser->getAnimBinding(iaidx);
        int iskelindex = binding.mSkelIndex;
        if (iskelindex != 0xffff) {
          auto jname     = localpose->_skeleton->GetJointName(iskelindex);
          int par        = localpose->_skeleton->GetJointParent(iskelindex);
          auto this_bind = localpose->_skeleton->_bindMatrices[iskelindex];
          auto par_bind  = localpose->_skeleton->_bindMatrices[par];
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

    const auto& joint_channels = animation->_jointanimationchannels;

    for (int iaidx = 0; iaidx < kmaxbones; iaidx++) {
      auto& binding  = _poser->getAnimBinding(iaidx);
      int iskelindex = binding.mSkelIndex;

      if (iskelindex != 0xffff) {

        //////////////////////////////

        int ichanindex  = binding.mChanIndex;
        auto joint_data = joint_channels.GetItemAtIndex(ichanindex).second;
        OrkAssert(joint_data);
        size_t numframes = joint_data->_sampledFrames.size();

        DecompMatrix decomp;

        if (_use_temporal_lerp) {
          int iframeB                = (iframe + 1) % numframes;
          const DecompMatrix& frameA = joint_data->GetFrame(iframe);
          const DecompMatrix& frameB = joint_data->GetFrame(iframeB);
          float alpha                = frame - float(iframe);
          decomp._position.lerp(frameA._position, frameB._position, alpha);
          decomp._scale.lerp(frameA._scale, frameB._scale, alpha);
          decomp._orientation = fquat::slerp(frameA._orientation, frameB._orientation, alpha);
        } else {
          decomp = joint_data->GetFrame(iframe);
        }
        localpose->_blendposeinfos[iskelindex].addPose(decomp, fweight); // yoyo

        //////////////////////////////
        if (0) {
          logchan_pose->log("apply on iskelidx<%d> ichanindex<%d>", iskelindex, ichanindex);
          logchan_pose->log("joint_data<%p> numframes<%zu>", (void*)joint_data.get(), numframes);
          auto jname = localpose->_skeleton->GetJointName(iskelindex);
          fmtx4 as_matrix;
          as_matrix.compose(decomp._position, decomp._orientation, decomp._scale);
          auto adump = as_matrix.dump4x3cn();
          logchan_pose2->log(
              "anmdump: joint<%d:%s> anm_jmtx: %s", //
              iskelindex,                           //
              jname.c_str(),                        //
              adump.c_str());
        }
        //////////////////////////////
      }
    }
    // OrkAssert(false);
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

  if (_skeleton->miRootNode >= 0) {
    const fmtx4& RootAnimMat = _concat_matrices[_skeleton->miRootNode];
    //_concat_matrices[_skeleton->miRootNode] = fmtx4::multiply_ltor(RootAnimMat, _skeleton->mTopNodesMatrix);
    _concat_matrices[_skeleton->miRootNode] = _local_matrices[_skeleton->miRootNode];

    int inumbones = _skeleton->numBones();
    for (int ib = 0; ib < inumbones; ib++) {
      const XgmBone& Bone       = _skeleton->bone(ib);
      int iparent               = Bone._parentIndex;
      int ichild                = Bone._childIndex;
      const fmtx4& ParentMatrix = _concat_matrices[iparent];
      const fmtx4& ChildMatrix  = _local_matrices[ichild];

      std::string parname = _skeleton->GetJointName(iparent).c_str();
      std::string chiname = _skeleton->GetJointName(ichild).c_str();

      fmtx4 this_result = fmtx4::multiply_ltor(ParentMatrix, ChildMatrix);

      _concat_matrices[ichild] = this_result;

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

      /////////////////////////////////////////////////////////////
    }
  }

  /////////////////////////////////////////////////////////////
  // inverse bind pose XF
  /////////////////////////////////////////////////////////////

  int inumbones = _skeleton->numBones();
  for( int i=0; i < inumbones; i++){
    _bindrela_matrices[i] = _concat_matrices[i] //
                          * _skeleton->_inverseBindMatrices[i]; //
   }

  /////////////////////////////////////////////////////////////

  float fmidx = (fminx + fmaxx) * 0.5f;
  float fmidy = (fminy + fmaxy) * 0.5f;
  float fmidz = (fminz + fmaxz) * 0.5f;

  fvec3 range((fmaxx - fminx), (fmaxy - fminy), (fmaxz - fminz));

  float frange = range.magnitude() * 0.5f;

  mObjSpaceBoundingSphere = fvec4(fmidx, fmidy, fmidz, frange);

  mObjSpaceAABoundingBox.SetMinMax(fvec3(fminx, fminy, fminz), fvec3(fmaxx, fmaxy, fmaxz));
}

///////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::decomposeConcatenated() {
  if (_skeleton->miRootNode >= 0) {
    _local_matrices[_skeleton->miRootNode] = _concat_matrices[_skeleton->miRootNode];
    int inumbones = _skeleton->numBones();
    for (int ib = 1; ib < inumbones; ib++) {
      const XgmBone& bone = _skeleton->bone(ib);
      int iparent               = bone._parentIndex;
      int ichild                = bone._childIndex;
      const fmtx4& childConcatMatrix = _concat_matrices[ichild];
      if (iparent >= 0) {
        const fmtx4& parentConcatMatrix = _concat_matrices[iparent];
        fmtx4 invParentConcatMatrix = parentConcatMatrix.inverse();
        _local_matrices[ichild] = childConcatMatrix * invParentConcatMatrix;
      } else {
        _local_matrices[ichild] = childConcatMatrix;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmLocalPose::dump() const {
  std::string rval;
  if (_skeleton->miRootNode >= 0) {
    int inumjoints = _skeleton->numJoints();
    for (int ij = 0; ij < inumjoints; ij++) {
      std::string name = _skeleton->GetJointName(ij).c_str();
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
  if (_skeleton->miRootNode >= 0) {
    const fmtx4& RootAnimMat = _local_matrices[_skeleton->miRootNode];
    int inumjoints           = _skeleton->numJoints();
    fvec3 ca                 = color;
    fvec3 cb                 = color * 0.7;

    for (int ij = 0; ij < inumjoints; ij++) {
      auto jprops = _skeleton->_jointProperties[ij];
      if(jprops->_numVerticesInfluenced){
        fvec3 cc         = (ij & 1) ? cb : ca;
        std::string name = _skeleton->GetJointName(ij).c_str();
        const auto& jmtx = _local_matrices[ij];
        rval += deco::format(cc, "%28s: ", name.c_str());
        rval += jmtx.dump4x3cn() + "\n"s;
      }
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmLocalPose::invdumpc(fvec3 color) const {
  std::string rval;
  if (_skeleton->miRootNode >= 0) {
    const fmtx4& RootAnimMat = _local_matrices[_skeleton->miRootNode];
    int inumjoints           = _skeleton->numJoints();
    fvec3 ca                 = color;
    fvec3 cb                 = color * 0.7;

    for (int ij = 0; ij < inumjoints; ij++) {
      fvec3 cc         = (ij & 1) ? cb : ca;
      std::string name = _skeleton->GetJointName(ij).c_str();
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
  return _skeleton->numJoints();
}

///////////////////////////////////////////////////////////////////////////////
// WorldPose
///////////////////////////////////////////////////////////////////////////////

XgmWorldPose::XgmWorldPose(xgmskeleton_constptr_t skel)
    : _skeleton(skel) {
}

///////////////////////////////////////////////////////////////////////////////

void XgmWorldPose::apply(const fmtx4& worldmtx, xgmlocalpose_ptr_t localpose) {
  int inumj = localpose->NumJoints();
  _world_bindrela_matrices.resize(inumj);
  _world_concat_matrices.resize(inumj);
  _boneprops = localpose->_boneprops;
  for (int ij = 0; ij < inumj; ij++) {
    const auto& C = localpose->_concat_matrices[ij];
    const auto& IB = _skeleton->_inverseBindMatrices[ij];

    auto WC = worldmtx * C;

    _world_concat_matrices[ij]   = WC;
    _world_bindrela_matrices[ij] = WC*IB;
  }
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmWorldPose::dumpc(fvec3 color) const {
  std::string rval;
  if (_skeleton->miRootNode >= 0) {
    int inumjoints = _skeleton->numJoints();
    fvec3 ca       = color;
    fvec3 cb       = color * 0.7;

    for (int ij = 0; ij < inumjoints; ij++) {
      fvec3 cc         = (ij & 1) ? cb : ca;
      std::string name = _skeleton->GetJointName(ij).c_str();
      const auto& jmtx = _world_bindrela_matrices[ij];
      rval += deco::format(cc, "%28s: ", name.c_str());
      rval += jmtx.dump4x3cn() + "\n"s;
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
