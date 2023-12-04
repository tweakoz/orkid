////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTIX.inl>
#include <ork/kernel/core/singleton.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/ui/ui.h>

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

/////////////////////////////////////////////////////////////////////////////

struct JointManipulatorInterface : ManipulatorInterface {

  DeclareConcreteX(ManipulatorInterface, Object);

  public:

  bool supportsTranslation() const final;
  bool supportsRotation() const final;
  bool supportsScaling() const final;

  void _onBeginTranslation(ui::event_constptr_t EV) final;
  void _onUpdateTranslation(ui::event_constptr_t EV) final;
  void _onEndTranslation(ui::event_constptr_t EV) final;

  void _onBeginRotation(ui::event_constptr_t EV) final;
  void _onUpdateRotation(ui::event_constptr_t EV) final;
  void _onEndRotation(ui::event_constptr_t EV) final;

  void _onBeginScaling(ui::event_constptr_t EV) final;
  void _onUpdateScaling(ui::event_constptr_t EV) final;
  void _onEndScaling(ui::event_constptr_t EV) final;

};

using jointmanipulatorinterface_ptr_t = std::shared_ptr<JointManipulatorInterface>;

} //namespace ork::lev2 {
