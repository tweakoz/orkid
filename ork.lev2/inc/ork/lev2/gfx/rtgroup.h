#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/any.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

class Texture;
class Context;
class GfxMaterial;
class RtGroup;
class RtBuffer;

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// RtGroup (Multiple Render Target Group)
/// collection of buffers that can be rendered to in parallel
/// on Geforce 6800 and lower, blend modes are common to active on all MRT sub buffers
/// on 7xxx and higher this restriction is removed
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

struct RtBuffer final {
  enum EMipGen { EMG_NONE, EMG_AUTOCOMPUTE, EMG_USER };

  RtBuffer(ERtgSlot etype, EBufferFormat efmt, int iW, int iH);

  Texture* texture() const {
    return _texture;
  }
  EBufferFormat format() const {
    return mFormat;
  }

  void SetSizeDirty(bool sd) {
    mSizeDirty = sd;
  }

  int miW, miH;
  Texture* _texture;
  ERtgSlot mType;
  EBufferFormat mFormat;
  svarp_t _impl;
  bool mSizeDirty;
  EMipGen _mipgen;
  std::string _debugName;
};

struct RtGroup final {

  /////////////////////////////////////////
  RtGroup(Context* partarg, int iW, int iH, int iSamples = 1);

  ~RtGroup();
  /////////////////////////////////////////
  RtBuffer* GetMrt(int idx) const {
    OrkAssert((idx >= 0) && (idx < kmaxmrts));
    return mMrt[idx];
  }
  /////////////////////////////////////////
  void SetMrt(int idx, RtBuffer* buffer);
  int GetNumTargets(void) const {
    return mNumMrts;
  }
  void SetInternalHandle(void* h) {
    mInternalHandle = h;
  }
  void* GetInternalHandle(void) const {
    return mInternalHandle;
  }
  void Resize(int iw, int ih);
  void SetSizeDirty(bool bv) {
    mbSizeDirty = bv;
  }
  bool IsSizeDirty() const {
    return mbSizeDirty;
  }
  Context* ParentTarget() const {
    return _parentTarget;
  }
  /////////////////////////////////////////
  int width() const {
    return miW;
  }
  int height() const {
    return miH;
  }
  ViewportRect viewportRect() const {
    return ViewportRect(0, 0, miW, miH);
  }
  int GetSamples() const {
    return miSamples;
  }
  /////////////////////////////////////////
  static const int kmaxmrts = 4;

  Context* _parentTarget;
  RtBuffer* mMrt[kmaxmrts];
  OffscreenBuffer* mDepth;
  Texture* _depthTexture = nullptr;
  int mNumMrts;
  int miW;
  int miH;
  int miSamples;
  bool mbSizeDirty;
  void* mInternalHandle;
  bool _needsDepth = true;
  bool _autoclear  = true;
  fvec4 _clearColor;
};

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
