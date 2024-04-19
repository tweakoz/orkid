////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/frametek.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

class TexBuffer : public DisplayBuffer {
public:
  TexBuffer(DisplayBuffer* parent, EBufferFormat efmt, int iW, int iH);
};

///////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
