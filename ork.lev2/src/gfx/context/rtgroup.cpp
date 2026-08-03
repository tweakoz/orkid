////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

RtBuffer::RtBuffer(const RtGroup* rtg, int slot, EBufferFormat efmt, int iW, int iH, uint64_t usage, bool with_texture)
    : _rtgroup(rtg)
    , _width(iW)
    , _height(iH)
    , _slot(slot)
    , mFormat(efmt)
    , _mipgen(EMG_NONE)
    , _usage(usage) {

  switch(usage){
    case "swapchain"_crcu:
      _mipgen = EMG_NONE;
      // no texture
      break;
    case "texarray"_crcu:
      _mipgen = EMG_AUTOCOMPUTE;
      break;
    default:
      break;
  }

  _numLayers = rtg->_numLayers;

  if(with_texture){
    _texture = std::make_shared<Texture>();
    _texture->_texFormat = efmt;
    _texture->_texType   = rtg->_cubeMap    ? ETEXTYPE_CUBE       //
                        : (_numLayers > 1)  ? ETEXTYPE_2D_ARRAY   //
                                            : ETEXTYPE_2D;
    _texture->_width     = iW;
    _texture->_height    = iH;
    _texture->_debugName = FormatString("rtg%d", slot);
  }
}

///////////////////////////////////////////////////////////////////////////////

RtGroup::RtGroup(Context* ptgt, int iW, int iH, MsaaSamples msaa_samples, uint64_t usage)
    : _parentTarget(ptgt)
    , mNumMrts(0)
    , miW(iW)
    , miH(iH)
    , _msaa_samples(msaa_samples)
    , mbSizeDirty(true)
    , _usage(usage) {


   switch(usage){
     case "user"_crcu:
       _needsDepth = true;
       _autoclear = true;
       break;
     case "swapchain"_crcu:
       _needsDepth = true;
       _autoclear = true;
       break;
     default:
       _autoclear = true;
       _rendertarget = nullptr;
       break;
   }
}

///////////////////////////////////////////////////////////////////////////////

RtGroup::~RtGroup() {
}

///////////////////////////////////////////////////////////////////////////////

rtbuffer_ptr_t RtGroup::buffer(int idx) const {
  OrkAssert((idx >= 0) && (idx < kmaxmrts));
  return mMrt[idx];
}

///////////////////////////////////////////////////////////////////////////////

texture_ptr_t RtGroup::texture(int idx) const {
  OrkAssert((idx >= 0) && (idx < kmaxmrts));
  auto buf = buffer(idx);
  return buf ? buf->_texture : nullptr;
}

texture_ptr_t RtGroup::depthTexture() const {
  return _depthBuffer ? _depthBuffer->_texture : nullptr;
}

int RtGroup::numImageBuffers(void) const {
  return mNumMrts;
}
void RtGroup::SetSizeDirty(bool bv) {
  mbSizeDirty = bv;
}
bool RtGroup::IsSizeDirty() const {
  return mbSizeDirty;
}
Context* RtGroup::ParentTarget() const {
  return _parentTarget;
}
/////////////////////////////////////////
rtbuffer_ptr_t RtGroup::createDepthBuffer(EBufferFormat efmt, bool with_texture) {
  _depthBuffer = std::make_shared<RtBuffer>(this, -1, efmt, miW, miH, "depth"_crcu, with_texture);
  return _depthBuffer;
}
/////////////////////////////////////////
int RtGroup::width() const {
  return miW;
}
int RtGroup::height() const {
  return miH;
}
ViewportRect RtGroup::viewportRect() const {
  return ViewportRect(0, 0, miW, miH);
}
uint32_t RtGroup::viewMask() const {
  ///////////////////////////////////////////////////////////////////////////////
  // GATE 0 NEGATIVE CONTROL 1 — "viewMask = 0x1" (layer 1 never rendered).
  //  Read ONCE, HERE, inside the single accessor both rendering structs go through
  //  (VkRenderingInfo.viewMask and VkPipelineRenderingCreateInfo.viewMask), so the two
  //  cannot disagree about the mask — a disagreement is invalid at draw time and is
  //  precisely what this one-accessor shape exists to prevent.
  //  Default OFF; announces itself once when armed. It can only narrow a group that is
  //  ALREADY multiview, so no mono RTG is reachable from it.
  ///////////////////////////////////////////////////////////////////////////////
  static const uint32_t _gate0_override = []() -> uint32_t {
    auto env = std::getenv("ORKID_GATE0_VIEWMASK");
    if (not env)
      return 0u;
    uint32_t v = uint32_t(std::strtoul(env, nullptr, 0));
    if (v)
      printf("GATE0: ORKID_GATE0_VIEWMASK=%s — multiview passes FORCED to viewMask 0x%x\n", env, v);
    return v;
  }();

  // one bit per view; 0 means "not a multiview pass" (the only shape every legacy RTG has).
  if (not(_multiview and (_numLayers > 1)))
    return 0u;
  if (_gate0_override)
    return _gate0_override;
  OrkAssert(_numLayers <= 31); // the mask is 32 bits wide; a wider shift is undefined, not merely wrong
  return (1u << uint32_t(_numLayers)) - 1u;
}

///////////////////////////////////////////////////////////////////////////////

rtgroup_ptr_t RtGroup::clone() const {
  auto _this = (RtGroup*)this;

  auto rval = std::make_shared<RtGroup>(_parentTarget, miW, miH, _msaa_samples);
  for (int i = 0; i < kmaxmrts; i++)
    rval->mMrt[i] = _this->mMrt[i];
  rval->mNumMrts    = _this->mNumMrts;
  rval->mbSizeDirty = _this->mbSizeDirty;
  rval->_impl       = _this->_impl;
  rval->_autoclear  = _this->_autoclear;
  rval->_depthOnly  = _this->_depthOnly;
  rval->_numLayers  = _this->_numLayers;
  rval->_multiview  = _this->_multiview;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

rtbuffer_ptr_t RtGroup::createRenderTarget(EBufferFormat efmt, uint64_t usage, bool with_texture) {
  int islot = mNumMrts++;
  if(0)printf("RtGroup::createRenderTarget usage=0x%zx (%zu) efmt=%d with_texture=%d\n", usage, usage, int(efmt), with_texture);
  rtbuffer_ptr_t rtb = std::make_shared<RtBuffer>(this, islot, efmt, miW, miH, usage, with_texture);
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
    if(0)printf("RtGroup<%p> Resize prev<%d %d> new<%d %d>\n", this, miW, miH, iw, ih);
    miW         = iw;
    miH         = ih;
    mbSizeDirty = true;
    // single source of truth for the group's extent: the RtBuffer dims AND their backing Texture
    // metadata (both write-once in the RtBuffer ctor) follow the new size, for color AND depth.
    // if the Texture keeps reporting the first-paint size, downstream consumers that read it as an
    // extent source (e.g. the HZB depth pyramid) lock at that size forever.
    for (int i = 0; i < kmaxmrts; i++) {
      if (mMrt[i]) {
        mMrt[i]->_width  = iw;
        mMrt[i]->_height = ih;
        if (mMrt[i]->_texture) {
          mMrt[i]->_texture->_width  = iw;
          mMrt[i]->_texture->_height = ih;
        }
        mMrt[i]->SetSizeDirty(true);
      }
    }
    if (_depthBuffer) {
      _depthBuffer->_width  = iw;
      _depthBuffer->_height = ih;
      if (_depthBuffer->_texture) {
        _depthBuffer->_texture->_width  = iw;
        _depthBuffer->_texture->_height = ih;
      }
      _depthBuffer->SetSizeDirty(true);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

RtgSet::RtgSet(Context* ctx, int w, int h,
         MsaaSamples s, std::string name, uint64_t usage, bool do_rendertarget)
    : _context(ctx)
    , _width(w)
    , _height(h)
    , _msaasamples(s)
    , _do_rendertarget(do_rendertarget)
    , _name(name)
    , _usage(usage) {
}

rtgroup_ptr_t RtgSet::fetch(uint64_t key) {
  rtgroup_ptr_t rval = nullptr;
  auto it            = _rtgs.find(key);
  if (it == _rtgs.end()) {
    rval = std::make_shared<RtGroup>(_context, _width, _height, _msaasamples);
    rval->_name = _name + FormatString(".%zx", key);
    rval->_autoclear = _autoclear;
    // BEFORE any buffer is created: RtBuffer copies _numLayers at construction, so a
    //  layer count applied afterwards would leave 2D buffers inside a layered group.
    rval->_numLayers = _numLayers;
    rval->_multiview = _multiview;

    rval->createDepthBuffer(EBufferFormat::Z32F, true);

    if(_do_rendertarget){
      rval->_rendertarget = std::make_shared<RtGroupRenderTarget>(rval.get());
    }
    for (auto item : _bufrecs) {
      auto buffer        = rval->createRenderTarget(item._format, _usage);
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
