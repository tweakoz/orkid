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
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/ISerializer.h>

namespace ork { namespace reflect {

Description::Description()
    : mParentDescription(NULL) {}

void Description::SetParentDescription(const Description* parent) { mParentDescription = parent; }

void Description::AddProperty(const char* key, IObjectProperty* value) { mProperties.AddSorted(key, value); }

Description::PropertyMapType& Description::Properties() { return mProperties; }

const Description::PropertyMapType& Description::Properties() const { return mProperties; }

const IObjectProperty* Description::FindProperty(const ConstString& name) const {
  for (const Description* description = this; description != NULL; description = description->mParentDescription) {
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

void Description::AddFunctor(const char* key, IObjectFunctor* functor) { mFunctions.AddSorted(key, functor); }

const IObjectFunctor* Description::FindFunctor(const ConstString& name) const {
  for (const Description* description = this; description != NULL; description = description->mParentDescription) {
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

void Description::AddSignal(const char* key, object::Signal Object::*pmember) { mSignals.AddSorted(key, pmember); }
void Description::AddAutoSlot(const char* key, object::AutoSlot Object::*pmember) { mAutoSlots.AddSorted(key, pmember); }

object::Signal Object::*Description::FindSignal(const ConstString& key) const {
  for (const Description* description = this; description != NULL; description = description->mParentDescription) {
    const SignalMapType& map         = description->mSignals;
    SignalMapType::const_iterator it = map.find(key);

    if (it != map.end()) {
      return (*it).second;
    }
  }

  return NULL;
}

object::AutoSlot Object::*Description::FindAutoSlot(const ConstString& key) const {
  for (const Description* description = this; description != NULL; description = description->mParentDescription) {
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
  for (const Description* description = this; description != NULL; description = description->mParentDescription) {
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
  for (const Description* description = this; description != NULL; description = description->mParentDescription) {
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

bool Description::SerializeProperties(ISerializer& serializer, const Object* object) const {
  bool result = true;

  if (mParentDescription) {
    if (!mParentDescription->SerializeProperties(serializer, object))
      result = false;
  }

  const PropertyMapType& map = Properties();

  for (PropertyMapType::const_iterator it = map.begin(); it != map.end(); ++it) {
    ConstString name      = (*it).first;
    IObjectProperty* prop = (*it).second;

    Command command(Command::EPROPERTY, name);

    serializer.BeginCommand(command);
    if (false == serializer.Serialize(prop, object))
      result = false;
    serializer.EndCommand(command);
  }

  return result;
}

bool Description::DeserializeProperties(IDeserializer& deserializer, Object* object) const {
  Command command;

  while (deserializer.BeginCommand(command)) {
    if (command.Type() != Command::EPROPERTY) {
      orkprintf("Description::DeserializeProperties:: got command %s, wanted EPROPERTY\n",
                command.Type() == Command::EOBJECT
                    ? "EOBJECT"
                    : command.Type() == Command::EITEM ? "EITEM" : command.Type() == Command::EATTRIBUTE ? "EATTRIBUTE" : "???");
    }

    OrkAssertI(command.Type() == Command::EPROPERTY, "Description::DeserializeProperties: expected a property!");

    if (command.Type() == Command::EPROPERTY) {
      const reflect::IObjectProperty* prop = FindProperty(command.Name());

      // orkprintf( "deserialize prop<%s>\n", command.Name().c_str() );

      if (prop) {
        if (false == deserializer.Deserialize(prop, object)) {
          deserializer.EndCommand(command);
          return false;
        }
      } else {
        orkprintf("Could not find property <%p>'%s'\n", command.Name().c_str(), command.Name().c_str());
      }

      if (false == deserializer.EndCommand(command)) {
        OrkAssertI(prop, "Description::DeserializeProperties: could not skip property!");
        return false;
      }
    } else {
      return false;
    }
  }

  return true;
}

}} // namespace ork::reflect
