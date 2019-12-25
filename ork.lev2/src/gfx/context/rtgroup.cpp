////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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

RtBuffer::RtBuffer(RtGroup* pgroup, ETargetType etype, EBufferFormat efmt, int iW, int iH)
    : mParentGroup(pgroup)
    , miW(iW)
    , miH(iH)
    , mType(etype)
    , mFormat(efmt)
    , mMaterial(nullptr)
    , mTexture(nullptr)
    , _mipgen(EMG_NONE) {
}

///////////////////////////////////////////////////////////////////////////////

RtGroup::RtGroup(GfxTarget* ptgt, int iW, int iH, int iSamples)
    : mParentTarget(ptgt)
    , miW(iW)
    , miH(iH)
    , mNumMrts(0)
    , mInternalHandle(0)
    , miSamples(iSamples)
    , mDepth(0)
    , mbSizeDirty(true) {
  for (int i = 0; i < kmaxmrts; i++) {
    mMrt[i] = nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////

RtGroup::~RtGroup() {
  for (int i = 0; i < kmaxmrts; i++) {
    if (mMrt[i])
      delete mMrt[i];
  }
}

///////////////////////////////////////////////////////////////////////////////

void RtGroup::SetMrt(int idx, RtBuffer* buf) {
  OrkAssert((idx >= 0) && (idx < kmaxmrts)); // ensure our mrt index is in range
  // OrkAssert( (ETGTTYPE_MRT0+idx) == Buffer->GetTargetType() );	// ensure our mrt type matches the index
  OrkAssert(idx == mNumMrts); // ensure we add mrt's sequentially
  mMrt[mNumMrts] = buf;
  mNumMrts++;
  OrkAssert(buf->mParentGroup == this);
  // buf->mParentGroup = buf;
  // buf->SetParentMrt( this );
}

///////////////////////////////////////////////////////////////////////////////

void RtGroup::Resize(int iw, int ih) {
  if ((iw != miW) || (ih != miH)) {
    miW         = iw;
    miH         = ih;
    mbSizeDirty = true;
    for (int i = 0; i < kmaxmrts; i++) {
      if (mMrt[i]) {

        mMrt[i]->miW = iw;
        mMrt[i]->miH = ih;
        mMrt[i]->SetSizeDirty(true);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
