////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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
#include <ork/lev2/gfx/gfxmaterial_basic.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/kernel/string/deco.inl>

#define ENABLE_ANIM

using namespace std::string_literals;

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

XgmBlendPoseInfo::XgmBlendPoseInfo()
    : mPoseCallback(NULL) {
  InitBlendPose();
}

///////////////////////////////////////////////////////////////////////////////

void XgmBlendPoseInfo::InitBlendPose() {
  miNumAnims = 0;
}

///////////////////////////////////////////////////////////////////////////////

void XgmBlendPoseInfo::AddPose(const DecompMtx44& mat, float weight, EXFORM_COMPONENT components) {
  OrkAssert(miNumAnims < kmaxblendanims);

  AnimMat[miNumAnims]        = mat;
  AnimWeight[miNumAnims]     = weight;
  Ani_components[miNumAnims] = components;

  miNumAnims++;
}

///////////////////////////////////////////////////////////////////////////////

void DecompMtx44::Compose(fmtx4& mtx, EXFORM_COMPONENT components) const {
  if (XFORM_COMPONENT_ALL == components)
    mtx.compose(mTrans, mRot, mScale); // = scaleMat * rotMat * transMat;
  else if (XFORM_COMPONENT_ORIENT == components)
    mtx = mRot.ToMatrix();
  else if (XFORM_COMPONENT_TRANS == components) {
    mtx.SetToIdentity();
    mtx.SetTranslation(mTrans);
  } else if ((XFORM_COMPONENT_ORIENT | XFORM_COMPONENT_SCALE) == components) {
    ork::fmtx4 scaleMat;
    scaleMat.SetScale(mScale, mScale, mScale);
    ork::fmtx4 rotMat = mRot.ToMatrix();
    mtx               = scaleMat * rotMat;
  } else if ((XFORM_COMPONENT_TRANS | XFORM_COMPONENT_SCALE) == components) {
    ork::fmtx4 scaleMat;
    scaleMat.SetScale(mScale, mScale, mScale);
    ork::fmtx4 transMat;
    mtx.SetTranslation(mTrans);
    mtx = scaleMat * transMat;
  } else if (XFORM_COMPONENT_SCALE == components) {
    mtx.SetToIdentity();
    mtx.SetScale(mScale, mScale, mScale);
  } else if (XFORM_COMPONENT_TRANSORIENT == components) {
    ork::fmtx4 transMat;
    transMat.SetTranslation(mTrans);
    ork::fmtx4 rotMat = mRot.ToMatrix();
    mtx               = rotMat * transMat;
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmBlendPoseInfo::ComputeMatrix(fmtx4& outmatrix) const {
  // printf("miNumAnims<%d>\n", miNumAnims);

  switch (miNumAnims) {
    case 1: // just copy the first matrix
    {
      DecompMtx44 c = AnimMat[0];

      if (mPoseCallback) // Callback for decomposed, pre-concatenated, blended joint info
        mPoseCallback->PostBlendPreConcat(c);

      c.Compose(outmatrix, Ani_components[0]);
    } break;

    case 2: // decompose 2 matrices and lerp components (ideally anims will be stored pre-decomposed)
    {
      float fw = fabs(AnimWeight[0] + AnimWeight[1] - 1.0f);
      // printf( "aw0<%f> aw1<%f>\n", AnimWeight[0], AnimWeight[1]);
      OrkAssert(fw < float(0.01f));

      const DecompMtx44& a = AnimMat[0];
      const DecompMtx44& b = AnimMat[1];
      DecompMtx44 c;

      const EXFORM_COMPONENT& acomp = Ani_components[0];
      const EXFORM_COMPONENT& bcomp = Ani_components[0];

      // TODO: Callback for decomposed, pre-concatenated, pre-blended joint info

      float flerp = AnimWeight[1];

      if (flerp < 0.0f)
        flerp = 0.0f;
      if (flerp > 1.0f)
        flerp = 1.0f;
      float iflerp = 1.0f - flerp;

      if (acomp & XFORM_COMPONENT_ORIENT && bcomp & XFORM_COMPONENT_ORIENT)
        c.mRot = fquat::Lerp(a.mRot, b.mRot, flerp);
      else if (acomp & XFORM_COMPONENT_ORIENT)
        c.mRot = a.mRot;
      else if (bcomp & XFORM_COMPONENT_ORIENT)
        c.mRot = b.mRot;

      if (acomp & XFORM_COMPONENT_TRANS && bcomp & XFORM_COMPONENT_TRANS)
        c.mTrans.Lerp(a.mTrans, b.mTrans, flerp);
      else if (acomp & XFORM_COMPONENT_TRANS)
        c.mTrans = a.mTrans;
      else if (bcomp & XFORM_COMPONENT_TRANS)
        c.mTrans = b.mTrans;

      if (acomp & XFORM_COMPONENT_SCALE && bcomp & XFORM_COMPONENT_SCALE)
        c.mScale = (a.mScale * iflerp) + (b.mScale * flerp);
      else if (acomp & XFORM_COMPONENT_SCALE)
        c.mScale = a.mScale;
      else if (bcomp & XFORM_COMPONENT_SCALE)
        c.mScale = b.mScale;

      // Callback for decomposed, pre-concatenated, blended joint info
      if (mPoseCallback)
        mPoseCallback->PostBlendPreConcat(c);

      outmatrix.compose(c.mTrans, c.mRot, c.mScale);
    } break;

    default:
      // gotta figure out a N-way quaternion blend first
      // on the DS, Lerping each matrix column (NormalX, NormalY, NormalZ) gives
      // decent results. Try it.
      {
        OrkAssert(false);
        outmatrix = fmtx4::Identity;
      }
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

XgmLocalPose::XgmLocalPose(const XgmSkeleton& Skeleton)
    : mSkeleton(Skeleton) {
  int inumchannels = Skeleton.numJoints();
  if (inumchannels == 0) {
    inumchannels = 1;
  }

  mLocalMatrices.resize(inumchannels);
  mBlendPoseInfos.resize(inumchannels);
}

/// ////////////////////////////////////////////////////////////////////////////
/// bind the animinst to the pose
/// this will figure out which anim channels match (bind) to joints in the skeleton
/// so that it does not have to be done every frame
/// ////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::BindAnimInst(XgmAnimInst& AnimInst) {
  if (AnimInst.GetAnim()) {
    float fweight = AnimInst.GetWeight();

    const XgmAnim& anim                                  = *AnimInst.GetAnim();
    const ork::lev2::XgmAnim::JointChannelsMap& Channels = anim.RefJointChannels();

    const orklut<PoolString, DecompMtx44>& StaticPose = anim.GetStaticPose();

    int ipidx = 0;
    for (orklut<PoolString, DecompMtx44>::const_iterator it = StaticPose.begin(); it != StaticPose.end(); it++) {
      const PoolString& JointName = it->first;

      int iskelindex = mSkeleton.jointIndex(JointName);

      int inumjinmap = mSkeleton.mmJointNameMap.size();

      // printf("spose jname<%s> iskelindex<%d> inumjinmap<%d>\n", JointName.c_str(), iskelindex, inumjinmap);

      if (-1 != iskelindex) {
        EXFORM_COMPONENT components = AnimInst.RefMask().GetComponents(iskelindex);
        if (XFORM_COMPONENT_NONE != components) {
          ork::lev2::XgmAnim::JointChannelsMap::const_iterator itanim = Channels.find(JointName);

          bool found = itanim == Channels.end();
          // printf( "spose jname<%s> found<%d>\n", JointName.c_str(), int(found) );

          if (itanim == Channels.end()) {
            int ispi = int(it - StaticPose.begin());

            const DecompMtx44& PoseMatrix = it->second;
            // TODO: Callback for programmer-controlled joints, pre-blended
            // RefBlendPoseInfo(iskelindex).AddPose(PoseMatrix, fweight, components);

            AnimInst.SetPoseBinding(ipidx++, XgmAnimInst::Binding(iskelindex, ispi));
          }
        }
      }
    }
    AnimInst.SetPoseBinding(ipidx++, XgmAnimInst::Binding(0xffff, 0xffff));

    size_t inumanimchannels = anim.GetNumJointChannels();
    // float frame = AnimInst.GetCurrentFrame();
    // float numframes = AnimInst.GetNumFrames();
    // int iframe = int(frame);
    int ichidx = 0;

    // for( int is=0; is<mSkeleton.numJoints(); is++ )
    //	printf( "skel bone<%d:%s>\n", is, mSkeleton.GetJointName(is).c_str() );

    for (ork::lev2::XgmAnim::JointChannelsMap::const_iterator it = Channels.begin(); it != Channels.end(); it++) {
      const PoolString& ChannelName                                    = it->first;
      const ork::lev2::XgmDecompAnimChannel* __restrict MtxChannelData = it->second;
      int iskelindex                                                   = mSkeleton.jointIndex(ChannelName);

      // printf("bind channel<%s> skidx<%d>\n", ChannelName.c_str(), iskelindex);

      if (-1 != iskelindex) {
        EXFORM_COMPONENT components = AnimInst.RefMask().GetComponents(iskelindex);
        if (XFORM_COMPONENT_NONE != components) {
          int ichiti = int(it - Channels.begin());
          AnimInst.SetAnimBinding(ichidx++, XgmAnimInst::Binding(iskelindex, ichiti));
          // const DecompMtx44 &ChannelDecomp = MtxChannelData->GetFrame(iframe);
          // TODO: Callback for programmer-controlled joints, pre-blended
          // RefBlendPoseInfo(iskelindex).AddPose(ChannelDecomp, fweight, components);
        }
      }
    }
    AnimInst.SetAnimBinding(ichidx++, XgmAnimInst::Binding(0xffff, 0xffff));
  }
}

/// ////////////////////////////////////////////////////////////////////////////
/// unbind the animinst to the pose
/// ////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::UnBindAnimInst(XgmAnimInst& AnimInst) {
  AnimInst.SetPoseBinding(0, XgmAnimInst::Binding(0xffff, 0xffff));
  AnimInst.SetAnimBinding(0, XgmAnimInst::Binding(0xffff, 0xffff));
  if (AnimInst.GetAnim()) {
    const XgmAnim& anim = *AnimInst.GetAnim();

    size_t inumjointchannels = anim.GetNumJointChannels();
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::ApplyAnimInst(const XgmAnimInst& AnimInst) {
#ifdef ENABLE_ANIM
  const XgmAnimMask& Mask = AnimInst.RefMask();
  float fweight           = AnimInst.GetWeight();
  ////////////////////////////////////////////////////
  // retrieve anim information
  const ork::lev2::AnimType* __restrict pl2anim = AnimInst.GetAnim();
  ////////////////////////////////////////////////////
  if (pl2anim) {
    const ork::lev2::XgmAnim::JointChannelsMap& Channels = pl2anim->RefJointChannels();
    ////////////////////////////////////////////////////
    // set pose channels (that do not have animation) first
    ////////////////////////////////////////////////////
    const orklut<PoolString, DecompMtx44>& StaticPose = pl2anim->GetStaticPose();
    for (int ipidx = 0; ipidx < XgmAnimInst::kmaxbones; ipidx++) {
      const XgmAnimInst::Binding& binding = AnimInst.GetPoseBinding(ipidx);
      int iskelindex                      = binding.mSkelIndex;
      if (iskelindex != 0xffff) {

        int iposeindex                = binding.mChanIndex;
        const DecompMtx44& decompPose = StaticPose.GetItemAtIndex(iposeindex).second;
        EXFORM_COMPONENT components   = AnimInst.RefMask().GetComponents(iskelindex);
        RefBlendPoseInfo(iskelindex).AddPose(decompPose, fweight, components);
      } else {
        break;
      }
    }
    //////////////////////////////////////////////////////////////////////////////////////////
    // normal anim on a skinned model
    //////////////////////////////////////////////////////////////////////////////////////////
    size_t inumanimchannels = pl2anim->GetNumJointChannels();
    float frame             = AnimInst.GetCurrentFrame();
    float numframes         = AnimInst.GetNumFrames();
    int iframe              = int(frame);
    for (int iaidx = 0; iaidx < XgmAnimInst::kmaxbones; iaidx++) {
      const XgmAnimInst::Binding& binding = AnimInst.GetAnimBinding(iaidx);
      int iskelindex                      = binding.mSkelIndex;
      // printf( "iaidx<%d> iskelindex<%d> inumanimchannels<%d>\n", iaidx, iskelindex, inumanimchannels );
      if (iskelindex != 0xffff) {
        int ichanindex                                                   = binding.mChanIndex;
        const ork::lev2::XgmDecompAnimChannel* __restrict MtxChannelData = Channels.GetItemAtIndex(ichanindex).second;
        const DecompMtx44& AnimMtx                                       = MtxChannelData->GetFrame(iframe);
        EXFORM_COMPONENT components                                      = AnimInst.RefMask().GetComponents(iskelindex);
        RefBlendPoseInfo(iskelindex).AddPose(AnimMtx, fweight, components);

        // printf( "apply frame<%d> on iskelidx<%d>\n", iframe, iskelindex );

      } else {
        break;
      }
    }
    /*
    //////////////////////////////////////////////////////////////////////////////////////////
    // root anim on a rigid model (ala destructables in sushi)
    //////////////////////////////////////////////////////////////////////////////////////////
    if( (1==inumanimchannels) && (1==XgmSkl.numJoints()) )
    {	const PoolString& channelname = Channels.begin()->first;
        const ork::lev2::XgmMatrixAnimChannel* ChannelData = ork::lev2::XgmMatrixAnimChannel::Cast(Channels.begin()->second);
        PoolString JointName = FindPooledString( channelname.c_str() );
        int iskelindex = XgmSkl.jointIndex( JointName );
        if( -1 != iskelindex )
        {	const fmtx4 & ChannelMatrix = ChannelData->GetFrame(iframe);
            LocalPose.RefBlendPoseInfo( 0 ).AddPose( ChannelMatrix, ratio );
        }
    }
    */
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::BindPose(void) {
#ifdef ENABLE_ANIM
  int inumjoints = NumJoints();

  ///////////////////////////////////////////
  // initialize to Skeletons Bind Pose
  ///////////////////////////////////////////
  for (int ij = 0; ij < inumjoints; ij++) {
    mLocalMatrices[ij] = mSkeleton.RefJointMatrix(ij);
  }

  ///////////////////////////////////////////
  // Init Matrix Blending infos
  ///////////////////////////////////////////

  for (int ij = 0; ij < inumjoints; ij++) {
    mBlendPoseInfos[ij].InitBlendPose();
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Blended Matrices and Concatenate
///////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::BuildPose(void) {
#ifdef ENABLE_ANIM
  int inumjoints = NumJoints();

  // printf("XgmLocalPose<%p>::BuildPose inumjoints<%d>\n", this, inumjoints);

  static int gctr = 0;
  for (int i = 0; i < inumjoints; i++) {
    int inumanms = mBlendPoseInfos[i].GetNumAnims();

    // printf("j<%d> inumanms<%d>\n", i, inumanms);
    if (inumanms) {
      mBlendPoseInfos[i].ComputeMatrix(mLocalMatrices[i]);

      if (1) //( i == ((gctr/1000)%inumjoints) )
      {
        const auto& name = mSkeleton.GetJointName(i);
        ork::FixedString<64> fxs;
        fxs.format("buildpose i<%s>", name.c_str());
        mLocalMatrices[i].dump((char*)fxs.c_str());
      }
      // TODO: Callback for after previous/current have been blended in local space
    }
  }
  gctr++;

#endif
}

///////////////////////////////////////////////////////////////////////////////

void XgmLocalPose::Concatenate(void) {
  fmtx4* __restrict pmats = &RefLocalMatrix(0);

  float fminx = std::numeric_limits<float>::max();
  float fminy = std::numeric_limits<float>::max();
  float fminz = std::numeric_limits<float>::max();
  float fmaxx = -std::numeric_limits<float>::max();
  float fmaxy = -std::numeric_limits<float>::max();
  float fmaxz = -std::numeric_limits<float>::max();

  if (mSkeleton.miRootNode >= 0) {
    const fmtx4& RootAnimMat    = RefLocalMatrix(mSkeleton.miRootNode);
    pmats[mSkeleton.miRootNode] = RootAnimMat.Concat43(mSkeleton.mTopNodesMatrix);

    int inumbones = mSkeleton.numBones();
    for (int ib = 0; ib < inumbones; ib++) {
      const XgmBone& Bone       = mSkeleton.bone(ib);
      int iparent               = Bone._parentIndex;
      int ichild                = Bone._childIndex;
      const fmtx4& ParentMatrix = pmats[iparent];
      const fmtx4& LocMatrix    = pmats[ichild];

      std::string parname = mSkeleton.GetJointName(iparent).c_str();
      std::string chiname = mSkeleton.GetJointName(ichild).c_str();

      fmtx4 temp    = (ParentMatrix * LocMatrix);
      pmats[ichild] = temp;

      if (RefBlendPoseInfo(ichild).GetPoseCallback())
        RefBlendPoseInfo(ichild).GetPoseCallback()->PostBlendPostConcat(pmats[ichild]);

      fvec3 vtrans = pmats[ichild].GetTranslation();

      fminx = std::min(fminx, vtrans.GetX());
      fminy = std::min(fminy, vtrans.GetY());
      fminz = std::min(fminz, vtrans.GetZ());

      fmaxx = std::max(fmaxx, vtrans.GetX());
      fmaxy = std::max(fmaxy, vtrans.GetY());
      fmaxz = std::max(fmaxz, vtrans.GetZ());
    }
  }

  float fmidx = (fminx + fmaxx) * 0.5f;
  float fmidy = (fminy + fmaxy) * 0.5f;
  float fmidz = (fminz + fmaxz) * 0.5f;

  fvec3 range((fmaxx - fminx), (fmaxy - fminy), (fmaxz - fminz));

  float frange = range.Mag() * 0.5f;

  mObjSpaceBoundingSphere = fvec4(fmidx, fmidy, fmidz, frange);

  mObjSpaceAABoundingBox.SetMinMax(fvec3(fminx, fminy, fminz), fvec3(fmaxx, fmaxy, fmaxz));
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmLocalPose::dump() const {
  std::string rval;
  if (mSkeleton.miRootNode >= 0) {
    const fmtx4& RootAnimMat = RefLocalMatrix(mSkeleton.miRootNode);
    int inumjoints           = mSkeleton.numJoints();
    for (int ij = 0; ij < inumjoints; ij++) {
      std::string name = mSkeleton.GetJointName(ij).c_str();
      const auto& jmtx = RefLocalMatrix(ij);
      rval += FormatString("%28s", name.c_str());
      rval += ": "s + jmtx.dump() + "\n"s;
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmLocalPose::dumpc(fvec3 color) const {
  std::string rval;
  if (mSkeleton.miRootNode >= 0) {
    const fmtx4& RootAnimMat = RefLocalMatrix(mSkeleton.miRootNode);
    int inumjoints           = mSkeleton.numJoints();
    fvec3 ca                 = color;
    fvec3 cb                 = color * 0.7;

    for (int ij = 0; ij < inumjoints; ij++) {
      fvec3 cc         = (ij & 1) ? cb : ca;
      std::string name = mSkeleton.GetJointName(ij).c_str();
      const auto& jmtx = RefLocalMatrix(ij);
      rval += deco::format(cc, "%28s: ", name.c_str());
      rval += jmtx.dump4x3cn() + "\n"s;
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmLocalPose::invdumpc(fvec3 color) const {
  std::string rval;
  if (mSkeleton.miRootNode >= 0) {
    const fmtx4& RootAnimMat = RefLocalMatrix(mSkeleton.miRootNode);
    int inumjoints           = mSkeleton.numJoints();
    fvec3 ca                 = color;
    fvec3 cb                 = color * 0.7;

    for (int ij = 0; ij < inumjoints; ij++) {
      fvec3 cc         = (ij & 1) ? cb : ca;
      std::string name = mSkeleton.GetJointName(ij).c_str();
      auto jmtx        = RefLocalMatrix(ij).inverse();
      rval += deco::asciic_rgb(cc);
      rval += FormatString("%28s", name.c_str());
      rval += ": "s + jmtx.dump4x3(cc) + "\n"s;
      rval += deco::asciic_reset();
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

int XgmLocalPose::NumJoints() const {
  return mSkeleton.numJoints();
}

///////////////////////////////////////////////////////////////////////////////
// WorldPose
///////////////////////////////////////////////////////////////////////////////

XgmWorldPose::XgmWorldPose(const XgmSkeleton& skel)
    : mSkeleton(skel) {
}

///////////////////////////////////////////////////////////////////////////////

void XgmWorldPose::apply(const fmtx4& worldmtx, const XgmLocalPose& localpose) {
  int inumj = localpose.NumJoints();
  mWorldMatrices.resize(inumj);
  for (int ij = 0; ij < inumj; ij++) {
    fmtx4 MatAnimJCat = localpose.RefLocalMatrix(ij);
    auto InvBind      = mSkeleton.RefInverseBindMatrix(ij);
    auto finalmtx     = worldmtx * (MatAnimJCat * InvBind);
    // auto finalmtx      = InvBind;
    mWorldMatrices[ij] = finalmtx;
  }
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmWorldPose::dumpc(fvec3 color) const {
  std::string rval;
  if (mSkeleton.miRootNode >= 0) {
    int inumjoints = mSkeleton.numJoints();
    fvec3 ca       = color;
    fvec3 cb       = color * 0.7;

    for (int ij = 0; ij < inumjoints; ij++) {
      fvec3 cc         = (ij & 1) ? cb : ca;
      std::string name = mSkeleton.GetJointName(ij).c_str();
      const auto& jmtx = mWorldMatrices[ij];
      rval += deco::format(cc, "%28s: ", name.c_str());
      rval += jmtx.dump4x3cn() + "\n"s;
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
