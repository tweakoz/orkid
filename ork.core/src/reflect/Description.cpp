////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/reflect/Command.h>
#include <ork/reflect/Description.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/ISerializer.h>

namespace ork { namespace reflect {

Description::Description()
    : _parentDescription(NULL) {
}

void Description::SetParentDescription(const Description* parent) {
  _parentDescription = parent;
}

void Description::AddProperty(const char* key, ObjectProperty* value) {
  mProperties.AddSorted(key, value);
  value->_name = key;
}

Description::PropertyMapType& Description::Properties() {
  return mProperties;
}

const Description::PropertyMapType& Description::Properties() const {
  return mProperties;
}

const ObjectProperty* Description::FindProperty(const ConstString& name) const {
  for (const Description* description = this; description != NULL; description = description->_parentDescription) {
    const PropertyMapType& map         = description->Properties();
    PropertyMapType::const_iterator it = map.find(name);

    if (it != map.end()) {
      return (*it).second;
    }
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Description::AddFunctor(const char* key, IObjectFunctor* functor) {
  mFunctions.AddSorted(key, functor);
}

const IObjectFunctor* Description::FindFunctor(const ConstString& name) const {
  for (const Description* description = this; description != NULL; description = description->_parentDescription) {
    const FunctorMapType& map         = description->mFunctions;
    FunctorMapType::const_iterator it = map.find(name);

    if (it != map.end()) {
      return (*it).second;
    }
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Description::AddSignal(const char* key, object::Signal Object::*pmember) {
  mSignals.AddSorted(key, pmember);
}
void Description::AddAutoSlot(const char* key, object::AutoSlot Object::*pmember) {
  mAutoSlots.AddSorted(key, pmember);
}

object::Signal Object::*Description::FindSignal(const ConstString& key) const {
  for (const Description* description = this; description != NULL; description = description->_parentDescription) {
    const SignalMapType& map         = description->mSignals;
    SignalMapType::const_iterator it = map.find(key);

    if (it != map.end()) {
      return (*it).second;
    }
  }

  return NULL;
}

object::AutoSlot Object::*Description::FindAutoSlot(const ConstString& key) const {
  for (const Description* description = this; description != NULL; description = description->_parentDescription) {
    const AutoSlotMapType& map         = description->mAutoSlots;
    AutoSlotMapType::const_iterator it = map.find(key);

    if (it != map.end()) {
      return (*it).second;
    }
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Description::annotateProperty(const ConstString& propname, const ConstString& key, const ConstString& val) {
  const PropertyMapType& map         = Properties();
  PropertyMapType::const_iterator it = map.find(propname);
  if (it == map.end()) {
    OrkAssert(false);
  } else {
    (*it).second->Annotate(key, val);
  }
}

ConstString Description::propertyAnnotation(const ConstString& propname, const ConstString& key) const {
  for (const Description* description = this; description != NULL; description = description->_parentDescription) {
    const PropertyMapType& map         = description->Properties();
    PropertyMapType::const_iterator it = map.find(propname);
    if (it != map.end()) {
      return (*it).second->GetAnnotation(key);
    }
  }
  return "";
}

void Description::annotateClass(const ConstString& key, const Description::anno_t& val) {
  orklut<ConstString, anno_t>::const_iterator it = mClassAnnotations.find(key);
  if (it == mClassAnnotations.end()) {
    mClassAnnotations.AddSorted(key, val);
  } else {
    OrkAssert(false);
  }
}

const Description::anno_t& Description::classAnnotation(const ConstString& key) const {
  for (const Description* description = this; description != NULL; description = description->_parentDescription) {
    orklut<ConstString, anno_t>::const_iterator it = description->mClassAnnotations.find(key);
    if (it != description->mClassAnnotations.end()) {
      return (*it).second;
    }
  }
  static const anno_t empty_anno(nullptr);
  return empty_anno;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Description::serializeProperties(
    ISerializer& serializer, //
    object_constptr_t object) const {
  bool result = true;

  if (_parentDescription) {
    _parentDescription->serializeProperties(serializer, object);
  }

  const PropertyMapType& map = Properties();

  for (auto it : map) {
    ConstString name     = it.first;
    ObjectProperty* prop = it.second;

    Command command(Command::EPROPERTY, name);

    serializer.beginCommand(command);
    serializer.serializeObjectProperty(prop, object);
    serializer.endCommand(command);
  }
}

void Description::deserializeProperties(IDeserializer& deserializer, object_ptr_t object) const {
  Command command;
  deserializer._currentObject = object;
  /*
  while (deserializer.beginCommand(command)) {
    if (command.Type() != Command::EPROPERTY) {
      orkprintf(
          "Description::DeserializeProperties:: got command %s, wanted EPROPERTY\n",
          command.Type() == Command::EOBJECT
              ? "EOBJECT"
              : command.Type() == Command::EITEM ? "EITEM" : command.Type() == Command::EATTRIBUTE ? "EATTRIBUTE" : "???");
    }

    OrkAssertI(command.Type() == Command::EPROPERTY, "Description::DeserializeProperties: expected a property!");

    if (command.Type() == Command::EPROPERTY) {
      const reflect::ObjectProperty* prop = FindProperty(command.Name());

      // orkprintf( "deserialize prop<%s>\n", command.Name().c_str() );

      if (prop) {
        deserializer._currentProperty = prop;
        if (false == deserializer.deserializeObjectProperty(prop, object)) {
          deserializer.endCommand(command);
          deserializer._currentProperty = nullptr;
          deserializer._currentObject   = nullptr;
          return false;
        }
        deserializer._currentProperty = nullptr;
      } else {
        orkprintf("Could not find property <%p>'%s'\n", command.Name().c_str(), command.Name().c_str());
      }

      if (false == deserializer.endCommand(command)) {
        OrkAssertI(prop, "Description::DeserializeProperties: could not skip property!");
        deserializer._currentObject = nullptr;
        return false;
      }
    } else {
      deserializer._currentObject = nullptr;
      return false;
    }
  }
  deserializer._currentObject = nullptr;
  return true;*/
}

}} // namespace ork::reflect
