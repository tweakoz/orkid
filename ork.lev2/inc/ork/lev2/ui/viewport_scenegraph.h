////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>

namespace ork { namespace ui {

struct SceneGraphViewport : public Viewport {
  RttiDeclareAbstract(SceneGraphViewport, Viewport);
public:
  SceneGraphViewport(const std::string& name, int x=0, int y=0, int w=0, int h=0);
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) final;
  void _doGpuInit(lev2::Context* pTARG) final;
  void forkDB();

  lev2::scenegraph::scene_ptr_t _scenegraph;
  lev2::compositoroutnode_rtgroup_ptr_t _outputnode;
  std::string _cameraname = "spawncam";
  lev2::acqdrawbuffer_ptr_t _override_acqdbuf;

};

}} // namespace ork::ui
