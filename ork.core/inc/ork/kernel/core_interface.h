////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/any.h>
#include <ork/event/Event.h>

#if defined(ORK_CONFIG_DARWIN)
#include <dispatch/dispatch.h>
dispatch_queue_t EditOnlyQueue();
#endif

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

namespace reflect {
class ObjectProperty;
}

class IUserChoiceDelegate : public Object {
  RttiDeclareAbstract(IUserChoiceDelegate, Object);

public:
  typedef any64 ValueType;

  virtual void EnumerateChoices(orkmap<PoolString, ValueType>& Choices) = 0;
  virtual void SetObject(Object* pobj, Object* puserdata = 0)           = 0;
};

///////////////////////////////////////////////////////////////////////////////

class ItemRemovalEvent : public event::Event {
  RttiDeclareConcrete(ItemRemovalEvent, event::Event);

public:
  const reflect::ObjectProperty* mProperty;
  int miMultiIndex;
  any64 mKey;
  any64 mOldValue;

  ItemRemovalEvent()
      : mProperty(0)
      , miMultiIndex(-1) {
  }
};

///////////////////////////////////////////////////////////////////////////////

class MapItemCreationEvent : public event::Event {
  RttiDeclareConcrete(MapItemCreationEvent, event::Event);

public:
  const reflect::ObjectProperty* mProperty;
  // int								miMultiIndex;
  any64 mKey;
  any64 mNewItem;

  MapItemCreationEvent()
      : mProperty(0) {
  } //, miMultiIndex(-1) {}
};

///////////////////////////////////////////////////////////////////////////////

class ObjectGedVisitEvent : public event::Event {
  RttiDeclareConcrete(ObjectGedVisitEvent, event::Event);

public:
  const reflect::ObjectProperty* mProperty;
  ObjectGedVisitEvent()
      : mProperty(0) {
  }
};

///////////////////////////////////////////////////////////////////////////////

class ObjectGedEditEvent : public event::Event {
  RttiDeclareConcrete(ObjectGedEditEvent, event::Event);

public:
  const reflect::ObjectProperty* mProperty;
  ObjectGedEditEvent()
      : mProperty(0) {
  }
};

///////////////////////////////////////////////////////////////////////////////

class ObjectFactoryFilter : public event::Event {
  RttiDeclareConcrete(ObjectFactoryFilter, event::Event);

public:
  bool mbFactoryOK;
  const object::ObjectClass* mpClass;
  ObjectFactoryFilter()
      : mbFactoryOK(true)
      , mpClass(nullptr) {
  }
};

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
