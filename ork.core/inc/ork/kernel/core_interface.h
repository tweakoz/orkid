////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/any.h>
#include <ork/event/Event.h>
#include <ork/object/Object.h>

#if defined(ORK_CONFIG_DARWIN)
#include <dispatch/dispatch.h>
dispatch_queue_t EditOnlyQueue();
#endif

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

namespace reflect {
struct ObjectProperty;
}

class IUserChoiceDelegate {

public:
  typedef svar256_t ValueType;

  virtual void EnumerateChoices(orkmap<PoolString, ValueType>& Choices) = 0;
  virtual void SetObject(Object* pobj, Object* puserdata = 0)           = 0;
};

///////////////////////////////////////////////////////////////////////////////

class ItemRemovalEvent : public event::Event {

public:
  const reflect::ObjectProperty* mProperty;
  int miMultiIndex;
  svar256_t mKey;
  svar256_t mOldValue;

  ItemRemovalEvent()
      : mProperty(0)
      , miMultiIndex(-1) {
  }
};

///////////////////////////////////////////////////////////////////////////////

class MapItemCreationEvent : public event::Event {

public:
  const reflect::ObjectProperty* mProperty;
  // int								miMultiIndex;
  svar256_t mKey;
  svar256_t mNewItem;

  MapItemCreationEvent()
      : mProperty(0) {
  } //, miMultiIndex(-1) {}
};

///////////////////////////////////////////////////////////////////////////////

class ObjectGedVisitEvent : public event::Event {

public:
  const reflect::ObjectProperty* mProperty;
  ObjectGedVisitEvent()
      : mProperty(0) {
  }
};

///////////////////////////////////////////////////////////////////////////////

class ObjectGedEditEvent : public event::Event {

public:
  const reflect::ObjectProperty* mProperty;
  ObjectGedEditEvent()
      : mProperty(0) {
  }
};

///////////////////////////////////////////////////////////////////////////////

class ObjectFactoryFilter : public event::Event {

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
