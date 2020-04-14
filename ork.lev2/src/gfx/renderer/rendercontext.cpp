////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>

#include <ork/application/application.h>
#include <ork/kernel/any.h>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/renderer/frametek.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/compositor.h>

template class ork::orklut<ork::CrcString, ork::lev2::rendervar_t>;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

const RenderContextInstData RenderContextInstData::Default;

///////////////////////////////////////////////////////////////////////////////

RenderContextInstData::RenderContextInstData(const RenderContextFrameData* RCFD)
    : miMaterialIndex(0)
    , miMaterialPassIndex(0)
    , mpActiveRenderer(0)
    , _dagrenderable(0)
    , mMaterialInst(0)
    , _isSkinned(false)
    , mRenderGroupState(ERGST_NONE)
    , _RCFD(RCFD) {

  if (_RCFD) {
    mpActiveRenderer = _RCFD->_renderer;
  }
  for (int i = 0; i < kMaxEngineParamFloats; i++)
    mEngineParamFloats[i] = 0.0f;
}

Context* RenderContextInstData::context() const {
  OrkAssert(_RCFD);
  return _RCFD->GetTarget();
}

void RenderContextInstData::SetEngineParamFloat(int idx, float fv) {
  OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

  mEngineParamFloats[idx] = fv;
}

float RenderContextInstData::GetEngineParamFloat(int idx) const {
  OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

  return mEngineParamFloats[idx];
}
void RenderContextInstData::SetRenderer(const IRenderer* rnd) {
  mpActiveRenderer = rnd;
}
void RenderContextInstData::SetRenderable(const IRenderable* rnd) {
  _dagrenderable = rnd;
}
const IRenderer* RenderContextInstData::GetRenderer(void) const {
  return mpActiveRenderer;
}
const IRenderable* RenderContextInstData::GetRenderable(void) const {
  return _dagrenderable;
}
const XgmMaterialStateInst* RenderContextInstData::GetMaterialInst() const {
  return mMaterialInst;
}
int RenderContextInstData::GetMaterialIndex(void) const {
  return miMaterialIndex;
}
int RenderContextInstData::GetMaterialPassIndex(void) const {
  return miMaterialPassIndex;
}
void RenderContextInstData::SetMaterialIndex(int idx) {
  miMaterialIndex = idx;
}
void RenderContextInstData::SetMaterialPassIndex(int idx) {
  miMaterialPassIndex = idx;
}
void RenderContextInstData::SetMaterialInst(const XgmMaterialStateInst* mi) {
  mMaterialInst = mi;
}
void RenderContextInstData::SetRenderGroupState(RenderGroupState rgs) {
  mRenderGroupState = rgs;
}
RenderGroupState RenderContextInstData::GetRenderGroupState() const {
  return mRenderGroupState;
}

///////////////////////////////////////////////////////////////////////////////

RenderContextFrameData::RenderContextFrameData(Context* ptarg)
    : _lightmgr(0)
    , mpTarget(ptarg)
    , _cimpl(nullptr) {
}

void RenderContextFrameData::setUserProperty(CrcString key, rendervar_t val) {
  auto it = _userProperties.find(key);
  if (it == _userProperties.end())
    _userProperties.AddSorted(key, val);
  else
    it->second = val;
}
void RenderContextFrameData::unSetUserProperty(CrcString key) {
  auto it = _userProperties.find(key);
  if (it == _userProperties.end())
    _userProperties.erase(it);
}

rendervar_t RenderContextFrameData::getUserProperty(CrcString key) const {
  auto it = _userProperties.find(key);
  if (it != _userProperties.end()) {
    return it->second;
  }
  rendervar_t rval(nullptr);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

const DrawableBuffer* RenderContextFrameData::GetDB() const {
  lev2::rendervar_t pvdb   = getUserProperty("DB"_crc);
  const DrawableBuffer* DB = pvdb.Get<const DrawableBuffer*>();
  return DB;
}

///////////////////////////////////////////////////////////////////////////////

const CompositingPassData& RenderContextFrameData::topCPD() const {
  OrkAssert(_cimpl != nullptr);
  return _cimpl->topCPD();
}
bool RenderContextFrameData::hasCPD() const {
  bool rval = false;
  if (_cimpl != nullptr) {
    rval = _cimpl->hasCPD();
  }
  return rval;
}

bool RenderContextFrameData::isStereo() const {
  bool stereo = false;
  if (_cimpl != nullptr) {
    if (_cimpl->hasCPD()) {
      const auto& CPD = topCPD();
      stereo          = CPD.isStereoOnePass();
    }
  }
  return stereo;
}

} // namespace ork::lev2
