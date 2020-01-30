////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
//#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/object/AutoConnector.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

ImmInterface::ImmInterface(Context& target)
    : mVtxBufUILine(16 << 10, 4096, EPRIM_LINES)
    , mVtxBufUIQuad(16 << 10, 8, EPRIM_TRIANGLES)
    , mVtxBufUITexQuad(16 << 10, 8, EPRIM_TRIANGLES)
    , mVtxBufText(256 << 10, 0, EPRIM_TRIANGLES)
    , mTarget(target) {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
