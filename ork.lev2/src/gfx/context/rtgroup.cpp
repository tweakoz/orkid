////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/rtgroup.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

RtBuffer::RtBuffer(const RtGroup* rtg, int slot, EBufferFormat efmt, int iW, int iH)
    : _rtgroup(rtg)
    , _width(iW)
    , _height(iH)
    , _slot(slot)
    , mFormat(efmt)
    , _mipgen(EMG_NONE) {
  _texture = std::make_shared<Texture>();
}

///////////////////////////////////////////////////////////////////////////////

RtGroup::RtGroup(Context* ptgt, int iW, int iH, int iSamples)
    : _parentTarget(ptgt)
    , mDepth(0)
    , mNumMrts(0)
    , miW(iW)
    , miH(iH)
    , miSamples(iSamples)
    , mbSizeDirty(true) {
}

///////////////////////////////////////////////////////////////////////////////

RtGroup::~RtGroup() {
}

///////////////////////////////////////////////////////////////////////////////

rtgroup_ptr_t RtGroup::clone() const {
  auto _this = (RtGroup*) this;

  auto rval = std::make_shared<RtGroup>(_parentTarget,miW,miH,miSamples);
  for( int i=0; i<kmaxmrts; i++ )
    rval->mMrt[i] = _this->mMrt[i];
  rval->mDepth = _this->mDepth;
  rval->mNumMrts = _this->mNumMrts;
  rval->mbSizeDirty = _this->mbSizeDirty;
  rval->_impl = _this->_impl;
  rval->_needsDepth = _this->_needsDepth;
  rval->_autoclear = _this->_autoclear;
  rval->_clearColor = _this->_clearColor;
  rval->_depthOnly = _this->_depthOnly;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

rtbuffer_ptr_t RtGroup::createRenderTarget(EBufferFormat efmt) {

  int islot = mNumMrts++;

  rtbuffer_ptr_t rtb = std::make_shared<RtBuffer>(this, islot, efmt, miW, miH);
  OrkAssert(islot < kmaxmrts);
  mMrt[islot] = rtb;
  return rtb;
}

///////////////////////////////////////////////////////////////////////////////

void RtGroup::SetMrt(int idx, rtbuffer_ptr_t buf) {
  OrkAssert((idx >= 0) && (idx < kmaxmrts)); // ensure our mrt index is in range
  // OrkAssert( (RtgSlot::Slot0+idx) == Buffer->GetTargetType() );	// ensure our mrt type matches the index
  OrkAssert(idx == mNumMrts); // ensure we add mrt's sequentially
  mMrt[mNumMrts] = buf;
  mNumMrts++;
}

///////////////////////////////////////////////////////////////////////////////

void RtGroup::Resize(int iw, int ih) {
  if ((iw != miW) || (ih != miH)) {
    miW         = iw;
    miH         = ih;
    mbSizeDirty = true;
    for (int i = 0; i < kmaxmrts; i++) {
      if (mMrt[i]) {

        mMrt[i]->_width  = iw;
        mMrt[i]->_height = ih;
        mMrt[i]->SetSizeDirty(true);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
