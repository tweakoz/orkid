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
    case EBufferFormat::R32F:
    case EBufferFormat::RG16F:
    case EBufferFormat::RGB10A2:
    case EBufferFormat::RGBA8:
    case EBufferFormat::Z32:
    case EBufferFormat::Z24S8:
      length *= 4;
      break;
    case EBufferFormat::RG32F:
    case EBufferFormat::RGBA16F:
      length *= 8;
      break;
    case EBufferFormat::RGBA32F:
    case EBufferFormat::RGB32UI:
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
