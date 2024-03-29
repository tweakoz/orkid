////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "controller.h"
#include "../../../src/core/message_private.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

template <typename T> sys_ref_t Controller::findSystem() {
  uint64_t ID = _objectIdCounter.fetch_add(1);

  //////////////////////////////////////////////////////
  // notify sim to update reference
  //////////////////////////////////////////////////////

  auto simevent = std::make_shared<Event>();
  simevent->_eventID   = EventID::FIND_SYSTEM;
  auto& FSYS          = simevent->_payload.make<impl::_FindSystem>();

  FSYS._sysref = SystemRef{._sysID = ID};
  FSYS._syskey = T::SystemType;

  _enqueueEvent(simevent);

  //////////////////////////////////////////////////////
  // return opaque handle
  //////////////////////////////////////////////////////

  return FSYS._sysref;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> comp_ref_t Controller::findEntityComponent(ent_ref_t ent) {

  uint64_t ID = _objectIdCounter.fetch_add(1);

  //////////////////////////////////////////////////////
  // notify sim to update reference
  //////////////////////////////////////////////////////

  auto simevent = std::make_shared<Event>();
  simevent->_eventID   = EventID::FIND_COMPONENT;
  auto& FCOMP          = simevent->_payload.make<impl::_FindComponent>();

  FCOMP._entref = ent;
  FCOMP._compclazz = T::componentClass();
  FCOMP._compref = ComponentRef({._compID=ID});

  _enqueueEvent(simevent);

  //////////////////////////////////////////////////////
  // return opaque handle
  //////////////////////////////////////////////////////


  return FCOMP._compref;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
