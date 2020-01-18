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

using namespace std::string_literals;

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::XgmAnimChannel, "XgmAnimChannel");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::XgmFloatAnimChannel, "XgmFloatAnimChannel");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::XgmVect3AnimChannel, "XgmVect3AnimChannel");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::XgmVect4AnimChannel, "XgmVect4AnimChannel");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::XgmMatrixAnimChannel, "XgmMatrixAnimChannel");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::XgmDecompAnimChannel, "XgmDecompAnimChannel");

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

void XgmAnimChannel::Describe() {
}
void XgmFloatAnimChannel::Describe() {
}
void XgmVect3AnimChannel::Describe() {
}
void XgmVect4AnimChannel::Describe() {
}
void XgmMatrixAnimChannel::Describe() {
}
void XgmDecompAnimChannel::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

XgmAnimMask::XgmAnimMask() {
  EnableAll();
}

XgmAnimMask::XgmAnimMask(const XgmAnimMask& mask) {
  for (int i = 0; i < knummaskbytes; i++)
    mMaskBits[i] = mask.mMaskBits[i];
}

XgmAnimMask& XgmAnimMask::operator=(const XgmAnimMask& mask) {
  for (int i = 0; i < knummaskbytes; i++)
    mMaskBits[i] = mask.mMaskBits[i];
  return *this;
}

///////////////////////////////////////////////////////////////////////////////

void XgmAnimMask::EnableAll() {
  for (int i = 0; i < knummaskbytes; i++)
    mMaskBits[i] = 0xff;
}

///////////////////////////////////////////////////////////////////////////////

// because there are 2 bones per char (4 bits each)
#define BONE_TO_CHAR(iboneindex) (iboneindex >> 1)
#define BONE_TO_BIT(iboneindex) ((iboneindex & 0x1) << 2)

#define CHECK_BONE(iboneindex) (iboneindex >= 0 && iboneindex < ((XgmAnimMask::knummaskbytes << 3) >> 2))
#define ASSERT_BONE(iboneindex) OrkAssert(CHECK_BONE(iboneindex))

#define SET_BITS(c, ibitindex, bits) (c |= (bits << ibitindex))
#define CLEAR_BITS(c, ibitindex, bits) (c &= ~(bits << ibitindex))
#define CHECK_BITS(c, ibitindex, bits) ((c >> ibitindex) & bits)

void XgmAnimMask::Enable(const XgmSkeleton& Skel, const PoolString& BoneName, EXFORM_COMPONENT components) {
  if (XFORM_COMPONENT_NONE == components)
    return;

  int iboneindex = Skel.jointIndex(BoneName);

  // this used to be assert, but designers may someday be setting bone names in tool. Be nicer to them.
  if (!CHECK_BONE(iboneindex)) {
    orkprintf("Bone does not exist: %s!\n", BoneName.c_str());
    return;
  }

  Enable(iboneindex, components);
}

void XgmAnimMask::Enable(int iboneindex, EXFORM_COMPONENT components) {
  // this used to be assert, but designers may someday be setting bone names in tool. Be nicer to them.
  if (!CHECK_BONE(iboneindex)) {
    orkprintf("Bone index does not exist: %d!\n", iboneindex);
    return;
  }

  if (XFORM_COMPONENT_NONE == components)
    return;

  int icharindex = BONE_TO_CHAR(iboneindex);
  int ibitindex  = BONE_TO_BIT(iboneindex);
  SET_BITS(mMaskBits[icharindex], ibitindex, components);
}

///////////////////////////////////////////////////////////////////////////////

void XgmAnimMask::DisableAll() {
  for (int i = 0; i < knummaskbytes; i++)
    mMaskBits[i] = 0x0;
}

///////////////////////////////////////////////////////////////////////////////

void XgmAnimMask::Disable(const XgmSkeleton& Skel, const PoolString& BoneName, EXFORM_COMPONENT components) {
  if (XFORM_COMPONENT_NONE == components)
    return;

  int iboneindex = Skel.jointIndex(BoneName);

  // this used to be assert, but designers may someday be setting bone names in tool. Be nicer to them.
  if (!CHECK_BONE(iboneindex)) {
    orkprintf("Bone does not exist: %s!\n", BoneName.c_str());
    return;
  }

  Disable(iboneindex, components);
}

void XgmAnimMask::Disable(int iboneindex, EXFORM_COMPONENT components) {
  ASSERT_BONE(iboneindex);

  if (XFORM_COMPONENT_NONE == components)
    return;

  int icharindex = BONE_TO_CHAR(iboneindex);
  int ibitindex  = BONE_TO_BIT(iboneindex);
  CLEAR_BITS(mMaskBits[icharindex], ibitindex, components);
}

///////////////////////////////////////////////////////////////////////////////

bool XgmAnimMask::Check(const XgmSkeleton& Skel, const PoolString& BoneName, EXFORM_COMPONENT components) const {
  if (XFORM_COMPONENT_NONE == components)
    return false;

  int iboneindex = Skel.jointIndex(BoneName);

  // this used to be assert, but designers may someday be setting bone names in tool. Be nicer to them.
  if (!CHECK_BONE(iboneindex)) {
    orkprintf("Bone does not exist: %s!\n", BoneName.c_str());
    return false;
  }

  return Check(iboneindex, components);
}

bool XgmAnimMask::Check(int iboneindex, EXFORM_COMPONENT components) const {
  ASSERT_BONE(iboneindex);

  if (XFORM_COMPONENT_NONE == components)
    return false;

  int icharindex = BONE_TO_CHAR(iboneindex);
  int ibitindex  = BONE_TO_BIT(iboneindex);
  return CHECK_BITS(mMaskBits[icharindex], ibitindex, components);
}

EXFORM_COMPONENT XgmAnimMask::GetComponents(const XgmSkeleton& Skel, const PoolString& BoneName) const {
  int iboneindex = Skel.jointIndex(BoneName);

  // this used to be assert, but designers may someday be setting bone names in tool. Be nicer to them.
  if (!CHECK_BONE(iboneindex)) {
    orkprintf("Bone does not exist: %s!\n", BoneName.c_str());
    return XFORM_COMPONENT_NONE;
  }

  return GetComponents(iboneindex);
}

EXFORM_COMPONENT XgmAnimMask::GetComponents(int iboneindex) const {
  // this used to be assert, but designers may someday be setting bone names in tool. Be nicer to them.
  if (!CHECK_BONE(iboneindex)) {
    orkprintf("Bone does not exist: %d!\n", iboneindex);
    return XFORM_COMPONENT_NONE;
  }

  int icharindex = BONE_TO_CHAR(iboneindex);
  int ibitindex  = BONE_TO_BIT(iboneindex);
  return EXFORM_COMPONENT(CHECK_BITS(mMaskBits[icharindex], ibitindex, XFORM_COMPONENT_ALL));
}

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

void DecompMtx44::EndianSwap() {
  float fqx = mRot.GetX();
  float fqy = mRot.GetY();
  float fqz = mRot.GetZ();
  float fqw = mRot.GetW();

  float ftx = mTrans.GetX();
  float fty = mTrans.GetY();
  float ftz = mTrans.GetZ();

  float fs = mScale;

  swapbytes_dynamic(fqx);
  swapbytes_dynamic(fqy);
  swapbytes_dynamic(fqz);
  swapbytes_dynamic(fqw);

  swapbytes_dynamic(ftx);
  swapbytes_dynamic(fty);
  swapbytes_dynamic(ftz);

  swapbytes_dynamic(fs);

  mRot.x = (fqx);
  mRot.y = (fqy);
  mRot.z = (fqz);
  mRot.w = (fqw);

  mTrans.SetX(ftx);
  mTrans.SetY(fty);
  mTrans.SetZ(ftz);

  mScale = fs;
}

///////////////////////////////////////////////////////////////////////////////

void XgmBlendPoseInfo::ComputeMatrix(fmtx4& outmatrix) const {
  // printf("miNumAnims<%d>\n", miNumAnims);

  switch (miNumAnims) {
    case 1: // just copy the first matrix
    {
      // Callback for decomposed, pre-concatenated, blended joint info
      if (mPoseCallback) {
        DecompMtx44 c = AnimMat[0];
        mPoseCallback->PostBlendPreConcat(c);
        c.Compose(outmatrix, Ani_components[0]);
      } else {
        const DecompMtx44& c = AnimMat[0];
        c.Compose(outmatrix, Ani_components[0]);
      }
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

#define ENABLE_ANIM

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
        const DecompMtx44& PoseMatrix = StaticPose.GetItemAtIndex(iposeindex).second;
        EXFORM_COMPONENT components   = AnimInst.RefMask().GetComponents(iskelindex);
        RefBlendPoseInfo(iskelindex).AddPose(PoseMatrix, fweight, components);
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

      if (0) //( i == ((gctr/1000)%inumjoints) )
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

      pmats[ichild] = (LocMatrix * ParentMatrix);
      // pmats[ichild] = InvBind.inverse();
      // auto bind    = invbind.inverse();

      // pmats[ichild] = ParentMatrix.Concat43(LocMatrix);
      auto parname = mSkeleton.GetJointName(iparent);
      auto chname  = std::string(mSkeleton.GetJointName(ichild).c_str());

      // ParentMatrix.dump(chname + ".par");
      // LocMatrix.dump(chname + ".loc");
      // pmats[ichild].dump(chname + ".concat");
      // invbind.dump(chname + ".invbind");
      //(invbind * pmats[ichild]).dump(chname + ".check");

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
      rval += FormatString("%16s", name.c_str());
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
      rval += deco::asciic_rgb(cc);
      rval += FormatString("%16s", name.c_str());
      rval += ": "s + jmtx.dump(cc) + "\n"s;
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
    // auto finalmtx     = worldmtx * (InvBind * MatAnimJCat);
    auto finalmtx = worldmtx * (InvBind.inverse());
    // auto finalmtx      = (MatAnimJCat)*worldmtx;
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
      rval += deco::asciic_rgb(cc);
      rval += FormatString("%16s", name.c_str());
      rval += ": "s + jmtx.dump(cc) + "\n"s;
      rval += deco::asciic_reset();
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

XgmMaterialStateInst::XgmMaterialStateInst(const XgmModelInst& minst)
    : mModelInst(minst)
    , mModel(minst.xgmModel())
    , mVarMap(EKEYPOLICY_MULTILUT) {
}

///////////////////////////////////////////////////////////////////////////////

class MaterialInstItem_UvXf : public MaterialInstItemMatrix {
  RttiDeclareAbstract(MaterialInstItem_UvXf, MaterialInstItemMatrix);

  const XgmAnimInst& mAnimInst;
  const XgmMatrixAnimChannel* mChannel;

public:
  MaterialInstItem_UvXf(const XgmAnimInst& ai, const XgmMatrixAnimChannel* chan)
      : mAnimInst(ai)
      , mChannel(chan) {
  }

  void Set() final {
    const XgmAnim& anim = *mAnimInst.GetAnim();

    /*float fx = 0.0f;
    if( 0 != strstr( anim.GetName().c_str(), "electric" ) )
    {
        fx = 0.0f;
    }

    if( fx == 1.0f )
    {
        orkprintf( "yo\n" );
    }*/

    float fw         = mAnimInst.GetWeight();
    float fr         = mAnimInst.GetCurrentFrame();
    const fmtx4& mtx = mChannel->GetFrame(int(fr));
    SetMatrix(mtx);
  }
};

void MaterialInstItem_UvXf::Describe() {
}

/// ////////////////////////////////////////////////////////////////////////////
/// bind the animinst to the materialpose
/// this will figure out which anim channels match (bind) to slots in the materials
/// so that it does not have to be done every frame
/// ////////////////////////////////////////////////////////////////////////////

void XgmMaterialStateInst::BindAnimInst(const XgmAnimInst& AnimInst) {
  if (AnimInst.GetAnim()) {
    const XgmAnim& anim = *AnimInst.GetAnim();

    size_t inummaterialchannels = anim.GetNumMaterialChannels();
    int nummaterials            = mModel->GetNumMaterials();

    const XgmAnim::MaterialChannelsMap& map = anim.RefMaterialChannels();

    for (XgmAnim::MaterialChannelsMap::const_iterator it = map.begin(); it != map.end(); it++) {
      const PoolString& channelname            = it->first;
      const ork::lev2::XgmAnimChannel* channel = it->second;
      const PoolString& objectname             = channel->GetObjectName();

      const XgmFloatAnimChannel* __restrict fchan  = rtti::autocast(channel);
      const XgmVect3AnimChannel* __restrict v3chan = rtti::autocast(channel);
      const XgmMatrixAnimChannel* __restrict mchan = rtti::autocast(channel);

      if (mchan) {
        for (int imat = 0; imat < nummaterials; imat++) {
          const GfxMaterial* __restrict material = mModel->GetMaterial(imat);
          const PoolString& matname              = material->GetName();

          if (matname == objectname) {
            const GfxMaterialWiiBasic* __restrict material_basic = rtti::autocast(material);
            const GfxMaterialFx* __restrict material_fx          = rtti::autocast(material);

            if (material_basic) {
              const TextureContext& ctx = material_basic->GetTexture(ETEXDEST_DIFFUSE);
              const Texture* ptex       = ctx.mpTexture;

              MaterialInstItem_UvXf* pxmsis = new MaterialInstItem_UvXf(AnimInst, mchan);
              pxmsis->mObjectName           = objectname;
              pxmsis->mChannelName          = channelname;
              pxmsis->mMaterial             = material_basic;
              material_basic->BindMaterialInstItem(pxmsis);
              mVarMap.AddSorted(&AnimInst, pxmsis);
            } else if (material_fx) {
            }
          }
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmMaterialStateInst::UnBindAnimInst(const XgmAnimInst& AnimInst) {
  ork::orklut<const XgmAnimInst*, MaterialInstItem*>::iterator it = mVarMap.find(&AnimInst);

  while (it != mVarMap.end()) {
    MaterialInstItem* item = it->second;

    const GfxMaterial* pmaterial = item->mMaterial;

    if (pmaterial) {
      pmaterial->UnBindMaterialInstItem(item);
    }

    delete item;
    mVarMap.RemoveItem(it);
    it = mVarMap.find(&AnimInst);
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmMaterialStateInst::ApplyAnimInst(const XgmAnimInst& AnimInst) {
  orklut<const XgmAnimInst*, MaterialInstItem*>::const_iterator lb = mVarMap.LowerBound(&AnimInst);
  orklut<const XgmAnimInst*, MaterialInstItem*>::const_iterator ub = mVarMap.UpperBound(&AnimInst);

  for (orklut<const XgmAnimInst*, MaterialInstItem*>::const_iterator it = lb; it != ub; it++) {
    MaterialInstItem* psetter = it->second;

    if (psetter) {
      psetter->Set();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

XgmAnim::XgmAnim()
    : miNumFrames(0) {
}

///////////////////////////////////////////////////////////////////////////////

void XgmAnim::AddChannel(const PoolString& Name, XgmAnimChannel* pchan) {
  const PoolString& usage = pchan->GetUsageSemantic();

  if (usage == FindPooledString("Joint")) {
    XgmDecompAnimChannel* MtxChan = rtti::autocast(pchan);
    OrkAssert(MtxChan);
    mJointAnimationChannels.AddSorted(Name, MtxChan);
  } else if (usage == FindPooledString("UvTransform")) {
    XgmMatrixAnimChannel* MtxChan = rtti::autocast(pchan);
    OrkAssert(MtxChan);
    mMaterialAnimationChannels.AddSorted(Name, MtxChan);
  } else if (usage == FindPooledString("FxParam")) {
    OrkAssert(pchan);
    mMaterialAnimationChannels.AddSorted(Name, pchan);
  }
}

///////////////////////////////////////////////////////////////////////////////

const XgmAnimInst::Binding XgmAnimInst::gBadBinding;

XgmAnimInst::XgmAnimInst()
    : mAnim(NULL)
    , mFrame(0.0f)
    , mWeight(1.0f) {
}

///////////////////////////////////////////////////////////////////////////////

void XgmAnimInst::BindAnim(const XgmAnim* anim) {
  mAnim  = anim;
  mFrame = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

XgmAnimChannel::XgmAnimChannel(
    const PoolString& ObjName,
    const PoolString& ChanName,
    const PoolString& UsageSemantic,
    EChannelType etype)
    : mObjectName(ObjName)
    , mChannelName(ChanName)
    , meChannelType(etype)
    , miNumFrames(0)
    , mUsageSemantic(UsageSemantic) {
}

///////////////////////////////////////////////////////////////////////////////

XgmAnimChannel::XgmAnimChannel(EChannelType etype)
    : mChannelName()
    , meChannelType(etype)
    , miNumFrames(0)
    , mUsageSemantic() {
}

///////////////////////////////////////////////////////////////////////////////

XgmFloatAnimChannel::XgmFloatAnimChannel()
    : XgmAnimChannel(EXGMAC_FLOAT) {
}

XgmFloatAnimChannel::XgmFloatAnimChannel(const PoolString& ObjName, const PoolString& ChanName, const PoolString& Usage)
    : XgmAnimChannel(ObjName, ChanName, Usage, EXGMAC_FLOAT) {
}

///////////////////////////////////////////////////////////////////////////////

XgmVect3AnimChannel::XgmVect3AnimChannel()
    : XgmAnimChannel(EXGMAC_VECT3) {
}

XgmVect3AnimChannel::XgmVect3AnimChannel(const PoolString& ObjName, const PoolString& ChanName, const PoolString& Usage)
    : XgmAnimChannel(ObjName, ChanName, Usage, EXGMAC_VECT3) {
}

///////////////////////////////////////////////////////////////////////////////

XgmVect4AnimChannel::XgmVect4AnimChannel()
    : XgmAnimChannel(EXGMAC_VECT4) {
}

XgmVect4AnimChannel::XgmVect4AnimChannel(const PoolString& ObjName, const PoolString& ChanName, const PoolString& Usage)
    : XgmAnimChannel(ObjName, ChanName, Usage, EXGMAC_VECT4) {
}

///////////////////////////////////////////////////////////////////////////////

XgmMatrixAnimChannel::XgmMatrixAnimChannel()
    : XgmAnimChannel(EXGMAC_MTX44)
    , mSampledFrames(0)
    , miAddIndex(0) {
}

XgmMatrixAnimChannel::XgmMatrixAnimChannel(const PoolString& ObjName, const PoolString& ChanName, const PoolString& Usage)
    : XgmAnimChannel(ObjName, ChanName, Usage, EXGMAC_MTX44)
    , mSampledFrames(0)
    , miAddIndex(0) {
}

///////////////////////////////////////////////////////////////////////////////

void XgmMatrixAnimChannel::AddFrame(const fmtx4& v) {
  mSampledFrames[miAddIndex] = v;
  miAddIndex++;
  OrkAssert(miAddIndex <= miNumFrames);
}

///////////////////////////////////////////////////////////////////////////////

const fmtx4& XgmMatrixAnimChannel::GetFrame(int index) const {
  OrkAssert(index < miAddIndex);
  return mSampledFrames[index];
}

///////////////////////////////////////////////////////////////////////////////

int XgmMatrixAnimChannel::GetNumFrames() const {
  return miNumFrames;
}

///////////////////////////////////////////////////////////////////////////////

void XgmMatrixAnimChannel::ReserveFrames(int icount) {
  miNumFrames    = icount;
  mSampledFrames = (fmtx4*)new fmtx4[icount];
  for (int i = 0; i < icount; i++) {
    new (mSampledFrames + i) fmtx4;
  }
}

///////////////////////////////////////////////////////////////////////////////

XgmDecompAnimChannel::XgmDecompAnimChannel()
    : XgmAnimChannel(EXGMAC_DCMTX)
    , mSampledFrames(0)
    , miAddIndex(0) {
}

XgmDecompAnimChannel::XgmDecompAnimChannel(const PoolString& ObjName, const PoolString& ChanName, const PoolString& Usage)
    : XgmAnimChannel(ObjName, ChanName, Usage, EXGMAC_DCMTX)
    , mSampledFrames(0)
    , miAddIndex(0) {
}

///////////////////////////////////////////////////////////////////////////////

void XgmDecompAnimChannel::AddFrame(const DecompMtx44& v) {
  mSampledFrames[miAddIndex] = v;
  miAddIndex++;
  OrkAssert(miAddIndex <= miNumFrames);
}

///////////////////////////////////////////////////////////////////////////////

const DecompMtx44& XgmDecompAnimChannel::GetFrame(int index) const {
  OrkAssert(index >= 0 && index < miAddIndex);
  return mSampledFrames[index];
}

///////////////////////////////////////////////////////////////////////////////

int XgmDecompAnimChannel::GetNumFrames() const {
  return miNumFrames;
}

///////////////////////////////////////////////////////////////////////////////

void XgmDecompAnimChannel::ReserveFrames(int icount) {
  miNumFrames    = icount;
  mSampledFrames = (DecompMtx44*)new DecompMtx44[icount];
  for (int i = 0; i < icount; i++) {
    new (mSampledFrames + i) DecompMtx44;
  }
}

///////////////////////////////////////////////////////////////////////////////
struct chansettter {
  static void set(XgmAnim* anm, XgmAnimChannel* Channel, const void* pdata) {
    XgmDecompAnimChannel* DecChannel = rtti::autocast(Channel);
    XgmMatrixAnimChannel* MtxChannel = rtti::autocast(Channel);
    XgmFloatAnimChannel* F32Channel  = rtti::autocast(Channel);
    XgmVect3AnimChannel* Ve3Channel  = rtti::autocast(Channel);
    if (DecChannel) {
      const DecompMtx44* DecBase = (const DecompMtx44*)pdata;
      DecChannel->ReserveFrames(anm->GetNumFrames());
      for (int ifr = 0; ifr < anm->GetNumFrames(); ifr++) {
        DecompMtx44 DecMtx = DecBase[ifr];
        DecChannel->AddFrame(DecMtx);
      }
    }
    if (MtxChannel) {
      const fmtx4* MatBase = (const fmtx4*)pdata;
      MtxChannel->ReserveFrames(anm->GetNumFrames());
      for (int ifr = 0; ifr < anm->GetNumFrames(); ifr++) {
        fmtx4 Matrix = MatBase[ifr];
        MtxChannel->AddFrame(Matrix);
      }
    } else if (F32Channel) {
      const float* f32Base = (const float*)pdata;
      F32Channel->ReserveFrames(anm->GetNumFrames());
      for (int ifr = 0; ifr < anm->GetNumFrames(); ifr++) {
        float value = f32Base[ifr];
        F32Channel->AddFrame(value);
      }
    } else if (Ve3Channel) {
      const fvec3* Ve3Base = (const fvec3*)pdata;
      Ve3Channel->ReserveFrames(anm->GetNumFrames());
      for (int ifr = 0; ifr < anm->GetNumFrames(); ifr++) {
        fvec3 value = Ve3Base[ifr];
        Ve3Channel->AddFrame(value);
      }
    }
  }
}; // namespace lev2
///////////////////////////////////////////////////////////////////////////////
bool XgmAnim::UnLoadUnManaged(XgmAnim* anm) {
#if defined(ORKCONFIG_ASSET_UNLOAD)
#if defined(WII)
  // crap the wii actually does call this...
  // OrkAssert(false);
#else

  anm->mPose.clear();
  anm->miNumFrames = 0;
  anm->mJointAnimationChannels.clear();
  anm->mMaterialAnimationChannels.clear();

#endif
#endif
  return true;
}
///////////////////////////////////////////////////////////////////////////////
bool XgmAnim::LoadUnManaged(XgmAnim* anm, const AssetPath& fname) { /////////////////////////////////////////////////////////////
  AssetPath fnameext(fname);
  fnameext.SetExtension("xga");
  AssetPath ActualPath = fnameext.ToAbsolute();
  /////////////////////////////////////////////////////////////
  OrkHeapCheck();
  chunkfile::DefaultLoadAllocator allocator;
  chunkfile::Reader chunkreader(fnameext, "xga", allocator);
  OrkHeapCheck();
  /////////////////////////////////////////////////////////////
  if (chunkreader.IsOk()) {
    chunkfile::InputStream* HeaderStream   = chunkreader.GetStream("header");
    chunkfile::InputStream* AnimDataStream = chunkreader.GetStream("animdata");
    ////////////////////////////////////////////////////////
    int inumchannels = 0, inumframes = 0;
    int inumjointchannels    = 0;
    int inummaterialchannels = 0;
    int ichannelclass = 0, iobjname = 0, ichnname = 0, iusgname = 0, idataoffset = 0;
    int inumposebones = 0;
    ////////////////////////////////////////////////////////
    HeaderStream->GetItem(inumframes);
    HeaderStream->GetItem(inumchannels);
    anm->SetNumFrames(inumframes);
    ////////////////////////////////////////////////////////
    HeaderStream->GetItem(inumjointchannels);
    for (int ichan = 0; ichan < inumjointchannels; ichan++) {
      HeaderStream->GetItem(ichannelclass);
      HeaderStream->GetItem(iobjname);
      HeaderStream->GetItem(ichnname);
      HeaderStream->GetItem(iusgname);
      HeaderStream->GetItem(idataoffset);
      const char* pchannelclass        = chunkreader.GetString(ichannelclass);
      const char* pobjname             = chunkreader.GetString(iobjname);
      const char* pchnname             = chunkreader.GetString(ichnname);
      const char* pusgname             = chunkreader.GetString(iusgname);
      void* pdata                      = AnimDataStream->GetDataAt(idataoffset);
      ork::object::ObjectClass* pclass = rtti::autocast(rtti::Class::FindClass(pchannelclass));
      XgmAnimChannel* Channel          = rtti::autocast(pclass->CreateObject());

      printf("LoadAnim MatrixChannel<%s> objname<%s> numframes<%d>\n", pchnname, pobjname, inumframes);

      Channel->SetChannelName(AddPooledString(pchnname));
      Channel->SetObjectName(AddPooledString(pobjname));
      Channel->SetChannelUsage(AddPooledString(pusgname));
      chansettter::set(anm, Channel, pdata);
      anm->AddChannel(Channel->GetChannelName(), Channel);
    }
    OrkHeapCheck();
    ////////////////////////////////////////////////////////
    HeaderStream->GetItem(inummaterialchannels);
    for (int ichan = 0; ichan < inummaterialchannels; ichan++) {
      HeaderStream->GetItem(ichannelclass);
      HeaderStream->GetItem(iobjname);
      HeaderStream->GetItem(ichnname);
      HeaderStream->GetItem(iusgname);
      HeaderStream->GetItem(idataoffset);
      const char* pchannelclass        = chunkreader.GetString(ichannelclass);
      const char* pobjname             = chunkreader.GetString(iobjname);
      const char* pchnname             = chunkreader.GetString(ichnname);
      const char* pusgname             = chunkreader.GetString(iusgname);
      void* pdata                      = AnimDataStream->GetDataAt(idataoffset);
      ork::object::ObjectClass* pclass = rtti::autocast(rtti::Class::FindClass(pchannelclass));
      XgmAnimChannel* Channel          = rtti::autocast(pclass->CreateObject());
      Channel->SetChannelName(AddPooledString(pchnname));
      Channel->SetObjectName(AddPooledString(pobjname));
      Channel->SetChannelUsage(AddPooledString(pusgname));
      chansettter::set(anm, Channel, pdata);
      anm->AddChannel(Channel->GetChannelName(), Channel);
    }
    OrkHeapCheck();
    ////////////////////////////////////////////////////////
    HeaderStream->GetItem(inumposebones);
    DecompMtx44 decmtx;
    anm->mPose.reserve(inumposebones);
    printf("inumposebones<%d>\n", inumposebones);
    for (int iposeb = 0; iposeb < inumposebones; iposeb++) {
      HeaderStream->GetItem(ichnname);
      HeaderStream->GetItem(decmtx);
      std::string PoseChannelName = chunkreader.GetString(ichnname);
      PoseChannelName             = ork::string::replaced(PoseChannelName, "_", ".");
      anm->mPose.AddSorted(AddPooledString(PoseChannelName.c_str()), decmtx);
    }
    ////////////////////////////////////////////////////////
  }
  OrkHeapCheck();
  return chunkreader.IsOk();
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::MaterialInstItem_UvXf, "MaterialInstItem_UvXf");
template void ork::chunkfile::OutputStream::AddItem<ork::lev2::DecompMtx44>(const ork::lev2::DecompMtx44& item);
template class ork::orklut<ork::PoolString, ork::lev2::DecompMtx44>;
