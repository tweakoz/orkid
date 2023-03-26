////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/config/config.h>
#include <ork/kernel/any.h>
#include <ork/kernel/orklut.h>
#include <ork/rtti/ICastable.h>
#include <ork/reflect/Serializable.h>
#include <ork/reflect/types.h>

namespace ork {

class ConstString;

namespace object {
class Signal;
struct AutoSlot;
} // namespace object

namespace reflect {

class Description {
public:
  using PropertyMapType = orklut<ConstString, ObjectProperty*>;
  using FunctorMapType  = orklut<std::string, IObjectFunctor*>;
  using SignalMapType   = orklut<std::string, object::Signal Object::*>;
  using AutoSlotMapType = orklut<std::string, object::AutoSlot Object::*>;
  using anno_t          = ork::svar64_t;

  Description();

  void addProperty(const char* key, ObjectProperty* value);
  void addFunctor(std::string key, IObjectFunctor* functor);
  void addSignal(std::string key, object::Signal Object::*);
  void addAutoSlot(std::string key, object::AutoSlot Object::*);

  void SetParentDescription(const Description*);

  void annotateProperty(const ConstString& propname, const ConstString& key, const ConstString& val);
  ConstString propertyAnnotation(const ConstString& propname, const ConstString& key) const;

  void annotateClass(const ConstString& key, const anno_t& val);
  const anno_t& classAnnotation(const ConstString& key) const;

  PropertyMapType& properties();
  const PropertyMapType& properties() const;

  const ObjectProperty* property(const ConstString&) const;
  const IObjectFunctor* findFunctor(const std::string&) const;
  object::Signal Object::*findSignal(const std::string&) const;
  object::AutoSlot Object::*FindAutoSlot(const std::string&) const;

  template <typename T> inline const T* findTypedProperty(const ConstString& named) const {
    return dynamic_cast<const T*>(property(named));
  }

  void serializeProperties(serdes::node_ptr_t) const;
  void deserializeProperties(serdes::node_ptr_t) const;

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
