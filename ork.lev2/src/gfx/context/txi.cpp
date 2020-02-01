////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/gfx/ctxbase.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

size_t TextureInitData::computeSize() const {
  size_t length = _w*_h;
  switch(_format){
    case EBUFFMT_R32F:
    case EBUFFMT_RG16F:
    case EBUFFMT_RGB10A2:
    case EBUFFMT_RGBA8:
    case EBUFFMT_Z32:
    case EBUFFMT_Z24S8:
      length *= 4;
      break;
    case EBUFFMT_RG32F:
    case EBUFFMT_RGBA16F:
      length *= 8;
      break;
    case EBUFFMT_RGBA32F:
    case EBUFFMT_RGB32UI:
      length *= 16;
      break;
    default:
      OrkAssert(false);
      break;
  }
  return length;
}


///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
