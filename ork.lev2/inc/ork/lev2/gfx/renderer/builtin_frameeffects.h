////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/frametek.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

class TexBuffer : public OffscreenBuffer {
public:
  TexBuffer(OffscreenBuffer* parent, EBufferFormat efmt, int iW, int iH);
};

///////////////////////////////////////////////////////////////////////////

class BasicFrameTechnique : public FrameTechniqueBase {
public:
  BasicFrameTechnique();

  virtual void Render(ork::lev2::FrameRenderer& ContextData);

  bool _shouldBeginAndEndFrame;
};

///////////////////////////////////////////////////////////////////////////

class PickFrameTechnique : public FrameTechniqueBase {
public:
  PickFrameTechnique();

  virtual void Render(ork::lev2::FrameRenderer& ContextData);
};

///////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
