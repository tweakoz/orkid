////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

RenderContextFrameData::RenderContextFrameData(Context* ptarg)
    : _lightmgr(0)
    , _target(ptarg) {
    setUserProperty("time"_crc,0.0f);
    setUserProperty("pbr_model"_crc,0);
}

void RenderContextFrameData::pushCompositor(compositorimpl_ptr_t c){
  __cimplstack.push(c);
}
compositorimpl_ptr_t RenderContextFrameData::popCompositor(){
  __cimplstack.pop();
  return topCompositor();
}
compositorimpl_ptr_t RenderContextFrameData::topCompositor() const {
  if(__cimplstack.empty()){
    return nullptr;
  }
  return __cimplstack.top();
}

bool RenderContextFrameData::hasUserProperty(CrcString key) const {
  auto it = _userProperties.find(key);
  return (it != _userProperties.end());
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

const DrawQueue* RenderContextFrameData::GetDB() const {
  lev2::rendervar_t pvdb   = getUserProperty("DB"_crc);
  const DrawQueue* DB = pvdb.get<const DrawQueue*>();
  return DB;
}

///////////////////////////////////////////////////////////////////////////////

const CompositingPassData& RenderContextFrameData::topCPD() const {
  if( topCompositor() ){
    return topCompositor()->topCPD();
  }
  static const CompositingPassData _default;
  return _default;
}
bool RenderContextFrameData::hasCPD() const {
  bool rval = false;
  if (topCompositor() != nullptr) {
    rval = topCompositor()->hasCPD();
  }
  return rval;
}

bool RenderContextFrameData::isStereo() const {
  bool stereo = false;
  if (topCompositor() != nullptr) {
    if (topCompositor()->hasCPD()) {
      const auto& CPD = topCPD();
      stereo          = CPD.isStereoOnePass();
    }
  }
  return stereo;
}

} // namespace ork::lev2
