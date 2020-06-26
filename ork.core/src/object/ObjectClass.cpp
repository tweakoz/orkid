////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/object/Object.h>
#include <ork/object/ObjectClass.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/properties/ObjectProperty.h>
#include <boost/uuid/random_generator.hpp>

INSTANTIATE_TRANSPARENT_RTTI(ork::object::ObjectClass, "ObjectClass");

namespace ork { namespace object {
////////////////////////////////////////////////////////////////////////////////

static const reflect::Description* ParentClassDescription(const rtti::Class* clazz) {
  const ObjectClass* object_class = rtti::downcast<const ObjectClass*>(clazz);

  if (object_class) {
    return &object_class->Description();
  } else {
    return NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////

void ObjectClass::Describe() {
}

////////////////////////////////////////////////////////////////////////////////

ObjectClass::ObjectClass(const rtti::RTTIData& data)
    : rtti::Class(data)
    , _description() {
}

static boost::uuids::uuid genUUID() {
  static boost::uuids::random_generator generator;
  return generator();
}

////////////////////////////////////////////////////////////////////////////////

object_ptr_t ObjectClass::createShared() const {
  auto shcast  = _sharedFactory();
  auto asobj   = std::dynamic_pointer_cast<Object>(shcast);
  asobj->_uuid = genUUID();
  return asobj;
}

////////////////////////////////////////////////////////////////////////////////

void ObjectClass::annotate(const ConstString& key, const anno_t& val) {
  _description.annotateClass(key, val);
}

////////////////////////////////////////////////////////////////////////////////

const ObjectClass::anno_t& ObjectClass::annotation(const ConstString& key) {
  return _description.classAnnotation(key);
}

////////////////////////////////////////////////////////////////////////////////

void ObjectClass::Initialize() {
  Class::Initialize();
  _description.SetParentDescription(ParentClassDescription(Parent()));

  reflect::Description::PropertyMapType& propmap = _description.Properties();

  for (auto it : propmap) {
    ConstString name              = it.first;
    reflect::ObjectProperty* prop = it.second;

    // auto propclass = prop->GetClass();
    // propclass->SetName(name, false);
  }
}

////////////////////////////////////////////////////////////////////////////////

reflect::Description& ObjectClass::Description() {
  return _description;
}

////////////////////////////////////////////////////////////////////////////////

const reflect::Description& ObjectClass::Description() const {
  return _description;
}

}} // namespace ork::object
