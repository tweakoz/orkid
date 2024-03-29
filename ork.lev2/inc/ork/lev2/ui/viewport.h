////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/surface.h>

namespace ork { namespace ui {

struct Viewport : public Surface {
  RttiDeclareAbstract(Viewport, Surface);

public:
  ///////////////////////////////////////

  enum ERefreshState {
    EREFRESH_PLAY = 0, // Refreshes at full framerate
    EREFRESH_UIEVENT,  // Refreshes on any UI input event
    EREFRESH_REQUEST,  // Refreshes on any draw request
  };

  struct ViewportFlags {
    ERefreshState meRefreshState : 2;
    unsigned mPad : 30;
  };

  ///////////////////////////////////////

  void BeginFrame(lev2::Context* pTARG);
  void EndFrame(lev2::Context* pTARG);

  ///////////////////////////////////////

  Viewport(const std::string& name, int x, int y, int w, int h, fcolor3 color, F32 depth);
  virtual ~Viewport() {
  }
};

}} // namespace ork::ui
