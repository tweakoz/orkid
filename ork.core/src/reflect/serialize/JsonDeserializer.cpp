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

struct JsonLeafNode {
  JsonLeafNode(const rapidjson::Value& jn)
      : _jsonnode(jn) {
  }
  const rapidjson::Value& _jsonnode;
};
struct JsonObjectNode {
  JsonObjectNode(const rapidjson::Value& jn)
      : _jsonobjectnode(jn) {
  }
  const rapidjson::Value& _jsonobjectnode;
  Value::ConstMemberIterator _iterator;
};
struct JsonArrayNode {
  JsonArrayNode(const rapidjson::Value& jn)
      : _jsonarraynode(jn) {
  }
  const rapidjson::Value& _jsonarraynode;
  Value::ConstValueIterator _iterator;
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
  auto dserjsonnode      = topnode->_impl.makeShared<JsonObjectNode>(rootnode);
  instance_out           = _parseObjectNode(topnode);
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserializeElement(node_ptr_t elemnode) {

  /////////////////////////////////////////////////////
  // map element
  /////////////////////////////////////////////////////
  if (elemnode->_impl.IsShared<JsonObjectNode>()) {
    auto implnode                   = elemnode->_impl.getShared<JsonObjectNode>();
    const rapidjson::Value& objnode = implnode->_jsonobjectnode;

    OrkAssert(implnode->_iterator != objnode.MemberEnd());

    printf("objnode<%s>\n", implnode->_iterator->name.GetString());

    implnode->_iterator++;

  }
  /////////////////////////////////////////////////////
  // array element
  /////////////////////////////////////////////////////
  else if (elemnode->_impl.IsShared<JsonArrayNode>()) {
    auto implnode                   = elemnode->_impl.getShared<JsonArrayNode>();
    const rapidjson::Value& arynode = implnode->_jsonarraynode;
    OrkAssert(implnode->_iterator != arynode.End());

    printf("arynode<%zu>\n", elemnode->_index);

    implnode->_iterator++;
  }
  /////////////////////////////////////////////////////
  // leaf node
  /////////////////////////////////////////////////////
  // else if (elemnode->_impl.IsShared<JsonLeafNode>()) {
  // auto implnode                = elemnode->_impl.getShared<JsonLeafNode>();
  // const rapidjson::Value& node = implnode->_jsonnode;
  //}
  /////////////////////////////////////////////////////
  // ???
  /////////////////////////////////////////////////////
  else {
    OrkAssert(false);
  }

  // OrkAssert(false);
}

//////////////////////////////////////////////////////////////////////////////

object_ptr_t JsonDeserializer::_parseObjectNode(IDeserializer::node_ptr_t dsernode) {

  const rapidjson::Value& objnode = dsernode->_impl.getShared<JsonObjectNode>()->_jsonobjectnode;
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

      auto jsonleafnode = child_node->_impl.makeShared<JsonLeafNode>(propnode);

      switch (propnode.GetType()) {
        case rapidjson::kObjectType: {
          child_node->_numchildren = propnode.MemberCount();
          auto jsonobjnode         = child_node->_impl.makeShared<JsonObjectNode>(propnode);
          jsonobjnode->_iterator   = propnode.MemberBegin();
          break;
        }
        case rapidjson::kArrayType: {
          child_node->_numchildren = propnode.MemberCount();
          auto jsonarynode         = child_node->_impl.makeShared<JsonArrayNode>(propnode);
          jsonarynode->_iterator   = propnode.Begin();
          break;
        }
        case rapidjson::kNullType:
          child_node->_value.Set<void*>(nullptr);
          break;
        case rapidjson::kFalseType:
          child_node->_value.Set<bool>(false);
          break;
        case rapidjson::kTrueType:
          child_node->_value.Set<bool>(true);
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
