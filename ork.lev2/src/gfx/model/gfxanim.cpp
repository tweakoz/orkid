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

using namespace std::string_literals;

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_anim = logger()->createChannel("gfxanim", fvec3(1, 0.6, 1));

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

// because there are 8 bones per char 
inline int BONE_TO_CHAR(int iboneindex) { return (iboneindex >> 3); }
inline int BONE_TO_BIT(int iboneindex) { return  (iboneindex & 0x7); }

void XgmAnimMask::Enable(xgmskeleton_constptr_t Skel, const std::string& BoneName) {

  int iboneindex = Skel->jointIndex(BoneName);

  // this used to be assert, but designers may someday be setting bone names in tool. Be nicer to them.
  if (iboneindex==-1) {
    logchan_anim->log("Bone does not exist: %s!", BoneName.c_str());
    return;
  }

  Enable(iboneindex);
}

void XgmAnimMask::Enable(int iboneindex) {
  // this used to be assert, but designers may someday be setting bone names in tool. Be nicer to them.
  if (iboneindex==-1) {
    logchan_anim->log("Bone index does not exist: %d!", iboneindex);
    return;
  }

  int icharindex = BONE_TO_CHAR(iboneindex);
  int ibitindex  = BONE_TO_BIT(iboneindex);
  mMaskBits[icharindex] |= (1<<ibitindex);
}

///////////////////////////////////////////////////////////////////////////////

void XgmAnimMask::DisableAll() {
  for (int i = 0; i < knummaskbytes; i++)
    mMaskBits[i] = 0x0;
}

///////////////////////////////////////////////////////////////////////////////

void XgmAnimMask::Disable(xgmskeleton_constptr_t Skel, const std::string& BoneName) {

  int iboneindex = Skel->jointIndex(BoneName);

  // this used to be assert, but designers may someday be setting bone names in tool. Be nicer to them.
  if (iboneindex==-1) {
    logchan_anim->log("Bone does not exist: %s!", BoneName.c_str());
    return;
  }

  Disable(iboneindex);
}

void XgmAnimMask::Disable(int iboneindex) {
  OrkAssert(iboneindex>=0);
  int icharindex = BONE_TO_CHAR(iboneindex);
  int ibitindex  = BONE_TO_BIT(iboneindex);
  mMaskBits[icharindex] &= ~(1<<ibitindex);
  printf( "XgmAnimMask<%p> disable iboneindex<%d> icharindex<%d> ibitindex<%d> mMaskBits<%zx>\n", //
          this, iboneindex, icharindex, ibitindex, mMaskBits[icharindex] );
}

///////////////////////////////////////////////////////////////////////////////

bool XgmAnimMask::isEnabled(xgmskeleton_constptr_t Skel, const std::string& BoneName) const {

  int iboneindex = Skel->jointIndex(BoneName);

  // this used to be assert, but designers may someday be setting bone names in tool. Be nicer to them.
  if (iboneindex==-1) {
    logchan_anim->log("Bone does not exist: %s!", BoneName.c_str());
    return false;
  }

  return isEnabled(iboneindex);
}

bool XgmAnimMask::isEnabled(int iboneindex) const {
  OrkAssert(iboneindex>=0);

  int icharindex = BONE_TO_CHAR(iboneindex);
  int ibitindex  = BONE_TO_BIT(iboneindex);
  int imask = 1<<ibitindex;
  return (mMaskBits[icharindex]&imask)!=0;
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

  const XgmAnimInst& mAnimInst;
  const XgmDecompMatrixAnimChannel* mChannel;

public:
  MaterialInstItem_UvXf(const XgmAnimInst& ai, const XgmDecompMatrixAnimChannel* chan)
      : mAnimInst(ai)
      , mChannel(chan) {
  }

  void set() final {
    const XgmAnim& anim = *mAnimInst._animation;

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
    float fr         = mAnimInst._current_frame;
    const DecompMatrix& decomp = mChannel->GetFrame(int(fr));
    fmtx4 mtx;
    mtx.compose(decomp._position, //
                decomp._orientation, //
                decomp._scale );
    SetMatrix(mtx);
  }
};

/// ////////////////////////////////////////////////////////////////////////////
/// bind the animinst to the materialpose
/// this will figure out which anim channels match (bind) to slots in the materials
/// so that it does not have to be done every frame
/// ////////////////////////////////////////////////////////////////////////////

void XgmMaterialStateInst::BindAnimInst(const XgmAnimInst& AnimInst) {
  if (AnimInst._animation) {
    const XgmAnim& anim = *AnimInst._animation;

    size_t inummaterialchannels = anim.GetNumMaterialChannels();
    int nummaterials            = mModel->GetNumMaterials();

    const auto& material_channels = anim.RefMaterialChannels();

    for (auto it : material_channels ) {
      const std::string& channelname = it.first;
      auto channel                  = it.second.get();
      const std::string& objectname  = channel->GetObjectName();

      const XgmFloatAnimChannel* __restrict fchan  = rtti::autocast(channel);
      const XgmVect3AnimChannel* __restrict v3chan = rtti::autocast(channel);
      const XgmDecompMatrixAnimChannel* __restrict mchan = rtti::autocast(channel);

      if (mchan) {
        for (int imat = 0; imat < nummaterials; imat++) {
          auto material             = mModel->GetMaterial(imat);
          std::string matname = material->mMaterialName;

          if (matname == objectname) {
            /*
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
            }*/
          }
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmMaterialStateInst::UnBindAnimInst(const XgmAnimInst& AnimInst) {
  auto it = mVarMap.find(&AnimInst);

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

void XgmMaterialStateInst::applyAnimInst(const XgmAnimInst& AnimInst) {
  auto lb = mVarMap.LowerBound(&AnimInst);
  auto ub = mVarMap.UpperBound(&AnimInst);

  for (auto it = lb; it != ub; it++) {
    MaterialInstItem* psetter = it->second;

    if (psetter) {
      psetter->set();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

XgmAnim::XgmAnim()
    : _numframes(0) {
      
}

///////////////////////////////////////////////////////////////////////////////

void XgmAnim::AddChannel(const std::string& Name, animchannel_ptr_t pchan) {
  const std::string& usage = pchan->GetUsageSemantic();

  if (usage == "Joint") {
    auto matrix_channel = std::dynamic_pointer_cast<XgmDecompMatrixAnimChannel>(pchan);
    OrkAssert(matrix_channel);
    _jointanimationchannels.AddSorted(Name,matrix_channel);
  } else if (usage == "UvTransform") {
    auto uvxf_channel = std::dynamic_pointer_cast<XgmDecompMatrixAnimChannel>(pchan);
    OrkAssert(uvxf_channel);
    mMaterialAnimationChannels.AddSorted(Name,pchan);
  } else if (usage == "FxParam") {
    OrkAssert(pchan);
    mMaterialAnimationChannels.AddSorted(Name,pchan);
  }
  else{
    OrkAssert(false);
  }
}


///////////////////////////////////////////////////////////////////////////////

XgmAnimInst::XgmAnimInst()
    : _animation(nullptr)
    , _current_frame(0.0f)
    , mWeight(1.0f) {
_mask = std::make_shared<XgmAnimMask>();
_poser = std::make_shared<XgmPoser>();
}

///////////////////////////////////////////////////////////////////////////////

void XgmAnimInst::bindAnim(xgmanim_constptr_t anim) {
  OrkAssert(anim);
  
  _animation  = anim;
  _current_frame = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

const XgmSkeletonBinding& XgmPoser::getPoseBinding(int i) const {
  OrkAssert(i<kmaxbones);
  return _poseBindings[i];
}

///////////////////////////////////////////////////////////////////////////////

const XgmSkeletonBinding& XgmPoser::getAnimBinding(int i) const {
  OrkAssert(i<kmaxbones);
  return _animBindings[i];
}

///////////////////////////////////////////////////////////////////////////////

void XgmPoser::setPoseBinding(int i, const XgmSkeletonBinding& inp) {
  OrkAssert(i<kmaxbones);
  _poseBindings[i] = inp;
}

///////////////////////////////////////////////////////////////////////////////

void XgmPoser::setAnimBinding(int i, const XgmSkeletonBinding& inp) {
  OrkAssert(i<kmaxbones);
  _animBindings[i] = inp;
}

///////////////////////////////////////////////////////////////////////////////

BoneTransformer::BoneTransformer( xgmskeleton_constptr_t skel)
    : _skeleton(skel) {

}

///////////////////////////////////////////////////////////////////////////////

void BoneTransformer::bindToBone(std::string named){
    int index = _skeleton->jointIndex(named);
    _jointindices.push_back(index);
}

///////////////////////////////////////////////////////////////////////////////

void BoneTransformer::compute(xgmlocalpose_ptr_t localpose, //
                          const fmtx4& xf){

    int count = _jointindices.size();
    for( int i=0; i<count; i++ ){
      int ji = _jointindices[i];
      auto& J = localpose->_concat_matrices[ji];
      J = xf*J;
    }
}

///////////////////////////////////////////////////////////////////////////////


} // namespace ork::lev2
