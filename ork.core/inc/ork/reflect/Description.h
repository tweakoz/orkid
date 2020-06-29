////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/ICastable.h>
#include <ork/reflect/Serializable.h>
#include <ork/reflect/Functor.h>
#include <ork/kernel/orklut.h>

#include <ork/config/config.h>
#include <ork/kernel/any.h>

namespace ork {

class ConstString;

namespace object {
class Signal;
class AutoSlot;
} // namespace object

namespace reflect {

class ObjectProperty;
class ISerializer;
class IDeserializer;

class Description {
public:
  typedef orklut<ConstString, ObjectProperty*> PropertyMapType;
  typedef orklut<ConstString, IObjectFunctor*> FunctorMapType;
  typedef orklut<ConstString, object::Signal Object::*> SignalMapType;
  typedef orklut<ConstString, object::AutoSlot Object::*> AutoSlotMapType;
  typedef ork::svar64_t anno_t;

  Description();

  void AddProperty(const char* key, ObjectProperty* value);
  void AddFunctor(const char* key, IObjectFunctor* functor);
  void AddSignal(const char* key, object::Signal Object::*);
  void AddAutoSlot(const char* key, object::AutoSlot Object::*);

  void SetParentDescription(const Description*);

  void annotateProperty(const ConstString& propname, const ConstString& key, const ConstString& val);
  ConstString propertyAnnotation(const ConstString& propname, const ConstString& key) const;

  void annotateClass(const ConstString& key, const anno_t& val);
  const anno_t& classAnnotation(const ConstString& key) const;

  PropertyMapType& Properties();
  const PropertyMapType& Properties() const;

  const ObjectProperty* FindProperty(const ConstString&) const;
  const IObjectFunctor* FindFunctor(const ConstString&) const;
  object::Signal Object::*signal(const ConstString&) const;
  object::AutoSlot Object::*FindAutoSlot(const ConstString&) const;

  template <typename T> inline const T* findTypedProperty(const ConstString& named) const {
    return dynamic_cast<const T*>(FindProperty(named));
  }

  void serializeProperties(ISerializer::node_ptr_t) const;
  void deserializeProperties(IDeserializer::node_ptr_t) const;

  const SignalMapType& GetSignals() const {
    return mSignals;
  }
  const AutoSlotMapType& GetAutoSlots() const {
    return mAutoSlots;
  }
  const FunctorMapType& GetFunctors() const {
    return mFunctions;
  }

private:
  const Description* _parentDescription;

  PropertyMapType mProperties;
  FunctorMapType mFunctions;
  SignalMapType mSignals;
  AutoSlotMapType mAutoSlots;

  orklut<ConstString, anno_t> mClassAnnotations;
};

} // namespace reflect
} // namespace ork
