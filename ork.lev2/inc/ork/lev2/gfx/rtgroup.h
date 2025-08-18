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

  RtBuffer(const RtGroup* rtg, int slot, EBufferFormat efmt, int iW, int iH, uint64_t usage = 0, bool with_texture = true);

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
  svarshp_t _impl;
  bool mSizeDirty;
  EMipGen _mipgen;
  uint64_t _usage = 0;
  std::string _debugName;
  texturearraysliceref_ptr_t _ta_slice;
  fvec4 _clearColor = fvec4::Black();
  float _clearDepth = 1.0f;
  bool _autoclear = true;
};

struct RtGroup final {

  /////////////////////////////////////////
  RtGroup(Context* partarg, int iW, int iH, MsaaSamples msaa_samples = MsaaSamples::MSAA_1X,uint64_t usage="user"_crcu);
  ~RtGroup();
  /////////////////////////////////////////
  rtgroup_ptr_t clone() const;
  /////////////////////////////////////////
  rtbuffer_ptr_t buffer(int idx) const;
  texture_ptr_t texture(int idx) const;
  texture_ptr_t depthTexture() const;
  /////////////////////////////////////////
  rtbuffer_ptr_t createRenderTarget(EBufferFormat efmt, uint64_t usage = 0, bool with_texture = true);
  rtbuffer_ptr_t createDepthBuffer(EBufferFormat efmt, bool with_texture = true);
  /////////////////////////////////////////
  void SetMrt(int idx, rtbuffer_ptr_t buffer);
  int numImageBuffers(void) const; // number of non-depth image buffers
  void Resize(int iw, int ih);
  void SetSizeDirty(bool bv);
  bool IsSizeDirty() const;
  Context* ParentTarget() const;
  /////////////////////////////////////////
  int width() const;
  int height() const;
  ViewportRect viewportRect() const;
  /////////////////////////////////////////
  static const int kmaxmrts = 8;

  Context* _parentTarget;
  rtbuffer_ptr_t mMrt[kmaxmrts];
  rtbuffer_ptr_t _depthBuffer;
  int mNumMrts;
  int miW;
  int miH;
  bool _cubeMap = false;
  int _cubeRenderFace = 0;
  MsaaSamples _msaa_samples;
  bool mbSizeDirty;
  svarshp_t _impl;
  fvec4 _clearColor;
  float _clearDepth = 1.0f;
  bool _needsDepth = true;
  bool _depthOnly = false;
  bool _autoclear  = true;
  bool _clearMaskColor = true;
  bool _clearMaskDepth = true;
  std::string _name;
  uint64_t _usage = "user"_crcu; 
  rendertarget_rtgroup_ptr_t _rendertarget;
  TextureArraySliceRef* _slice = nullptr;
};

struct RtgSet {
  
  RtgSet(Context* ctx, MsaaSamples s, std::string name, uint64_t usage = "color"_crcu, bool do_rendertarget=false);
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
  uint64_t _usage = "color"_crcu;

};

using rtgset_ptr_t = std::shared_ptr<RtgSet>;

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
