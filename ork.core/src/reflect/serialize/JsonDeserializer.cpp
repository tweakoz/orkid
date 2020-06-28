////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/Command.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/rtti/Class.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>
#include <ork/object/Object.h>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

#include <ork/orkprotos.h>
#include <cstring>

using namespace rapidjson;

namespace ork::reflect::serialize {

//////////////////////////////////////////////////////////////////////////////

struct JsonDserNode {
  JsonDserNode(const rapidjson::Value& jn)
      : _jsonnode(jn) {
  }
  const rapidjson::Value& _jsonnode;
};

//////////////////////////////////////////////////////////////////////////////

static int unhex(char c);

JsonDeserializer::JsonDeserializer(const std::string& jsondata)
    : _document() {
  _allocator = &_document.GetAllocator();
  _document.Parse(jsondata.c_str());
  bool is_object = _document.IsObject();
  bool has_top   = _document.HasMember("top");
  OrkAssert(is_object);
  OrkAssert(has_top);
}
/*
void JsonDeserializer::deserializeItem() {
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserializeObjectProperty(
    const ObjectProperty* prop, //
    object_ptr_t object) {
  prop->deserialize(*this, object);
}
*/
//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserializeTop(object_ptr_t& instance_out) {
  const auto& rootnode   = _document["top"]["object"];
  auto topnode           = std::make_shared<IDeserializer::Node>();
  topnode->_deserializer = this;
  auto dserjsonnode      = topnode->_impl.makeShared<JsonDserNode>(rootnode);
  instance_out           = _parseObjectNode(topnode);
}

//////////////////////////////////////////////////////////////////////////////

object_ptr_t JsonDeserializer::_parseObjectNode(IDeserializer::node_ptr_t dsernode) {

  const rapidjson::Value& objnode = dsernode->_impl.getShared<JsonDserNode>()->_jsonnode;
  object_ptr_t instance_out       = nullptr;

  bool has_class = objnode.HasMember("class");
  bool has_uuid  = objnode.HasMember("uuid");
  OrkAssert(has_class);
  OrkAssert(has_uuid);

  auto classstr = objnode["class"].GetString();
  auto uuidstr  = objnode["uuid"].GetString();

  boost::uuids::string_generator gen;
  auto uuid     = gen(uuidstr);
  auto clazz    = rtti::Class::FindClass(classstr);
  auto objclazz = dynamic_cast<object::ObjectClass*>(clazz);
  OrkAssert(objclazz);
  const auto& description = objclazz->Description();

  instance_out        = objclazz->createShared();
  instance_out->_uuid = uuid;

  ///////////////////////////////////
  // deserialize properties
  ///////////////////////////////////

  const auto& propsnode = objnode["properties"];
  OrkAssert(propsnode.IsObject());

  ///////////////////////////////////

  for (auto it = propsnode.MemberBegin(); //
       it != propsnode.MemberEnd();
       ++it) {

    const auto& propkey  = it->name;
    const auto& propnode = it->value;

    // OrkAssert(propnode.IsObject());

    auto propname = propkey.GetString();
    auto prop     = description.FindProperty(propname);

    if (prop) {
      printf("found propname<%s> prop<%p>\n", propname, prop);
      auto child_node = std::make_shared<IDeserializer::Node>();

      child_node->_parent       = dsernode;
      child_node->_property     = prop;
      child_node->_deserializer = this;
      child_node->_instance     = instance_out;

      auto dserjsonnode = child_node->_impl.makeShared<JsonDserNode>(propnode);

      switch (propnode.GetType()) {
        case rapidjson::kNullType:
          child_node->_value.Set<void*>(nullptr);
          break;
        case rapidjson::kFalseType:
          child_node->_value.Set<bool>(false);
          break;
        case rapidjson::kTrueType:
          child_node->_value.Set<bool>(true);
          break;
        case rapidjson::kObjectType:
          break;
        case rapidjson::kArrayType:
          break;
        case rapidjson::kStringType:
          child_node->_value.Set<std::string>(propnode.GetString());
          break;
        case rapidjson::kNumberType:
          child_node->_value.Set<double>(propnode.GetDouble());
          break;
        default:
          OrkAssert(false);
      }

      prop->deserialize(child_node);

    } else { // drop property, no longer registered
      printf("dropping property<%s>\n", propname);
    }
  }

  ///////////////////////////////////

  printf("instance_out<%p:%s>\n", instance_out.get(), uuidstr);
  return instance_out;
}

//////////////////////////////////////////////////////////////////////////////
/*
void JsonDeserializer::deserializeSharedObject(object_ptr_t& instance_out) {
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::beginCommand(Command& command) {
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::endCommand(const Command& command) {
}*/

//////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serialize
