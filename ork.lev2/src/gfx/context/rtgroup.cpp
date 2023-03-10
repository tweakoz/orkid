////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
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

RtGroup::RtGroup(Context* ptgt, int iW, int iH, MsaaSamples msaa_samples)
    : _parentTarget(ptgt)
    , mDepth(0)
    , mNumMrts(0)
    , miW(iW)
    , miH(iH)
    , _msaa_samples(msaa_samples)
    , mbSizeDirty(true) {
}

///////////////////////////////////////////////////////////////////////////////

RtGroup::~RtGroup() {
}

///////////////////////////////////////////////////////////////////////////////

rtgroup_ptr_t RtGroup::clone() const {
  auto _this = (RtGroup*)this;

  auto rval = std::make_shared<RtGroup>(_parentTarget, miW, miH, _msaa_samples);
  for (int i = 0; i < kmaxmrts; i++)
    rval->mMrt[i] = _this->mMrt[i];
  rval->mDepth      = _this->mDepth;
  rval->mNumMrts    = _this->mNumMrts;
  rval->mbSizeDirty = _this->mbSizeDirty;
  rval->_impl       = _this->_impl;
  rval->_needsDepth = _this->_needsDepth;
  rval->_autoclear  = _this->_autoclear;
  rval->_clearColor = _this->_clearColor;
  rval->_depthOnly  = _this->_depthOnly;
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

RtgSet::RtgSet(Context* ctx, MsaaSamples s, bool do_rendertarget)
    : _context(ctx)
    , _msaasamples(s)
    , _do_rendertarget(do_rendertarget) {
}

rtgroup_ptr_t RtgSet::fetch(uint64_t key) {
  rtgroup_ptr_t rval = nullptr;
  auto it            = _rtgs.find(key);
  if (it == _rtgs.end()) {
    rval = std::make_shared<RtGroup>(_context, 8, 8, _msaasamples);
    rval->_autoclear = _autoclear;
    if(_do_rendertarget){
      rval->_rendertarget = std::make_shared<RtGroupRenderTarget>(rval.get());
    }
    for (auto item : _bufrecs) {
      auto buffer        = rval->createRenderTarget(item._format);
      buffer->_debugName = item._name;
    }
    _rtgs[key] = rval;
  } else {
    rval = it->second;
  }
  return rval;
}

void RtgSet::addBuffer(std::string name, EBufferFormat fmt) {
  BufRec br;
  br._name   = name;
  br._format = fmt;
  _bufrecs.push_back(br);
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
