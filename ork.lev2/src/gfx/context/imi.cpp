////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
    : mTarget(target)
    , mVtxBufUILine(16 << 10, 4096, PrimitiveType::LINES)
    , mVtxBufUIQuad(16 << 10, 8, PrimitiveType::TRIANGLES)
    , mVtxBufUITexQuad(16 << 10, 8, PrimitiveType::TRIANGLES)
    , mVtxBufText(256 << 10, 0, PrimitiveType::TRIANGLES) {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
