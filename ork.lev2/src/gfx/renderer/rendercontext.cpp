////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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

RenderContextInstData::RenderContextInstData(const RenderContextFrameData&RCFD)
  : RenderContextInstData() {
    mpActiveRenderer = RCFD._renderer;
  }


RenderContextInstData::RenderContextInstData()
    : miMaterialIndex(0)
    , miMaterialPassIndex(0)
    , mpActiveRenderer(0)
    , mpDagRenderable(0)
    , mpLightingGroup(0)
    , mDPTopEnvMap(0)
    , mDPBotEnvMap(0)
    , mMaterialInst(0)
    , mbIsSkinned(false)
    , mbForzeNoZWrite(false)
    , mLightMap(0)
    , mbVertexLit(false)
    , mRenderGroupState(ERGST_NONE) {
  for (int i = 0; i < kMaxEngineParamFloats; i++)
    mEngineParamFloats[i] = 0.0f;
}

void RenderContextInstData::SetEngineParamFloat(int idx, float fv) {
  OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

  mEngineParamFloats[idx] = fv;
}

float RenderContextInstData::GetEngineParamFloat(int idx) const {
  OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

  return mEngineParamFloats[idx];
}

///////////////////////////////////////////////////////////////////////////////

RenderContextFrameData::RenderContextFrameData(GfxTarget* ptarg)
    : _lightmgr(0)
    , mpTarget(ptarg) {}

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

const DrawableBuffer* RenderContextFrameData::GetDB() const{
  lev2::rendervar_t pvdb   = getUserProperty("DB"_crc);
  const DrawableBuffer* DB = pvdb.Get<const DrawableBuffer*>();
  return DB;
}

///////////////////////////////////////////////////////////////////////////////

const CompositingPassData& RenderContextFrameData::topCPD() const{
  assert(_cimpl!=nullptr);
  return _cimpl->topCPD();
}


} // namespace ork::lev2
