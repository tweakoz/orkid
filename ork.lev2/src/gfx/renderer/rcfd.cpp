////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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

RenderContextFrameData::RenderContextFrameData(Context* ptarg)
    : _cimpl(nullptr)
    , _lightmgr(0)
    , _target(ptarg) {
    setUserProperty("time"_crc,0.0f);
    setUserProperty("pbr_model"_crc,0);
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
  const DrawableBuffer* DB = pvdb.get<const DrawableBuffer*>();
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
