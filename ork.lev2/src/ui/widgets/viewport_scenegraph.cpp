////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/viewport_scenegraph.h>
#include <ork/lev2/ui/event.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ui::SceneGraphViewport, "ui::SceneGraphViewport");

namespace ork { namespace ui {

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

SceneGraphViewport::SceneGraphViewport(const std::string& name, int x, int y, int w, int h)
    : Viewport(name, x, y, w, h, fvec4(1,0,0,1), 1.0f) {

} 

void SceneGraphViewport::DoRePaintSurface(ui::drawevent_constptr_t drwev) {
  auto tgt    = drwev->GetTarget();
  auto mtxi   = tgt->MTXI();
  auto fbi    = tgt->FBI();
    fbi->Clear(_clearColor,1.0f);
}
// Surface::DoDraw()

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
