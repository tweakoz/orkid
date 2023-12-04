////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/core/singleton.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/util/grid.h>
#include <ork/math/TransformNode.h>
#include <ork/object/AutoConnector.h>
#include <ork/rtti/RTTIX.inl>

namespace ork::lev2 {

////////////////////////////////////////////////////////////////////////////////

struct ManipulatorInterface : public Object {

  DeclareAbstractX(ManipulatorInterface, Object);

  virtual bool supportsTranslation() const { return false; }
  virtual bool supportsRotation() const { return false; }
  virtual bool supportsScaling() const { return false; }

  virtual void _onBeginTranslation(ui::event_constptr_t EV){}
  virtual void _onUpdateTranslation(ui::event_constptr_t EV){}
  virtual void _onEndTranslation(ui::event_constptr_t EV){}

  virtual void _onBeginRotation(ui::event_constptr_t EV){}
  virtual void _onUpdateRotation(ui::event_constptr_t EV){}
  virtual void _onEndRotation(ui::event_constptr_t EV){}

  virtual void _onBeginScaling(ui::event_constptr_t EV){}
  virtual void _onUpdateScaling(ui::event_constptr_t EV){}
  virtual void _onEndScaling(ui::event_constptr_t EV){}
};

} //namespace ork::lev2 {
