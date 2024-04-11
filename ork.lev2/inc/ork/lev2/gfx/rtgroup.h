////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/lev2_types.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/any.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

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

  RtBuffer(const RtGroup* rtg, int slot, EBufferFormat efmt, int iW, int iH);

  Texture* texture() const {
    return _texture.get();
  }
  EBufferFormat format() const {
    return mFormat;
  }

  void SetSizeDirty(bool sd) {
    mSizeDirty = sd;
  }

  const RtGroup* _rtgroup;
  int _width, _height;
  int _slot;
  texture_ptr_t _texture;
  //RtgSlot mType;
  EBufferFormat mFormat;
  svarp_t _impl;
  bool mSizeDirty;
  EMipGen _mipgen;
  std::string _debugName;
};

struct RtGroup final {

  /////////////////////////////////////////
  RtGroup(Context* partarg, int iW, int iH, MsaaSamples msaa_samples = MsaaSamples::MSAA_1X);
  ~RtGroup();
  /////////////////////////////////////////
  rtgroup_ptr_t clone() const;
  /////////////////////////////////////////
  rtbuffer_ptr_t GetMrt(int idx) const {
    OrkAssert((idx >= 0) && (idx < kmaxmrts));
    return mMrt[idx];
  }
  /////////////////////////////////////////
  rtbuffer_ptr_t createRenderTarget(EBufferFormat efmt);
  /////////////////////////////////////////
  void SetMrt(int idx, rtbuffer_ptr_t buffer);
  int GetNumTargets(void) const {
    return mNumMrts;
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
  /////////////////////////////////////////
  static const int kmaxmrts = 4;

  Context* _parentTarget;
  rtbuffer_ptr_t mMrt[kmaxmrts];
  rtbuffer_ptr_t _depthBuffer;
  int mNumMrts;
  int miW;
  int miH;
  MsaaSamples _msaa_samples;
  bool mbSizeDirty;
  svar16_t _impl;
  bool _autoclear  = true;
  fvec4 _clearColor;
  bool _depthOnly = false;
  std::string _name;
  bool _pseudoRTG = false;
  rendertarget_rtgroup_ptr_t _rendertarget;

};

struct RtgSet {
  
  RtgSet(Context* ctx, MsaaSamples s, std::string name, bool do_rendertarget=false);
  rtgroup_ptr_t fetch(uint64_t key);
  void addBuffer(std::string name, EBufferFormat fmt);

  struct BufRec{
    std::string _name;
    EBufferFormat _format;
  };

  Context* _context = nullptr;
  MsaaSamples _msaasamples;
  std::unordered_map<uint64_t,rtgroup_ptr_t> _rtgs;
  std::vector<BufRec> _bufrecs;
  bool _do_rendertarget;
  bool _autoclear = true;
  std::string _name;

};

using rtgset_ptr_t = std::shared_ptr<RtgSet>;

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
