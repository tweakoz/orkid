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

namespace ork::reflect::serdes {

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
      : _jsonarray(jn) {
  }
  const rapidjson::Value& _jsonarray;
  Value::ConstValueIterator _iterator;
};

//////////////////////////////////////////////////////////////////////////////

JsonDeserializer::JsonDeserializer(const std::string& jsondata)
    : _document() {
  _allocator = &_document.GetAllocator();
  _document.Parse(jsondata.c_str());
  bool is_object = _document.IsObject();
  bool has_top   = _document.HasMember("root");
  OrkAssert(is_object);
  OrkAssert(has_top);
}

//////////////////////////////////////////////////////////////////////////////

node_ptr_t JsonDeserializer::pushNode(std::string named, NodeType type) {
  node_ptr_t n     = std::make_shared<Node>();
  n->_name         = named;
  n->_deserializer = this;
  n->_type         = type;
  switch (type) {
    case NodeType::MAP:
      OrkAssert(false);
      break;
    case NodeType::MAP_ELEMENT_LEAF:
    case NodeType::MAP_ELEMENT_OBJECT:
      n->_impl.makeShared<JsonObjectNode>(rapidjson::Value());
      break;
    case NodeType::ARRAY:
      OrkAssert(false);
      break;
    case NodeType::ARRAY_ELEMENT_LEAF:
    case NodeType::ARRAY_ELEMENT_OBJECT:
      n->_impl.makeShared<JsonArrayNode>(rapidjson::Value());
      break;
    case NodeType::OBJECT:
      n->_impl.makeShared<JsonObjectNode>(rapidjson::Value());
      break;
    case NodeType::LEAF:
      n->_impl.makeShared<JsonLeafNode>(rapidjson::Value());
      break;
    case NodeType::UNKNOWN:
      OrkAssert(false);
      break;
  }
  _nodestack.push(n);
  return n;
}
void JsonDeserializer::popNode() {
  _nodestack.pop();
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserializeTop(object_ptr_t& instance_out) {
  const auto& rootnode        = _document["root"];
  auto topnode                = this->pushNode("root", NodeType::OBJECT);
  auto child_node             = _parseSubNode(topnode, rootnode);
  instance_out                = child_node->_deser_instance;
  auto instance_out_classname = instance_out->GetClass()->Name();
  std::string uuids           = boost::uuids::to_string(instance_out->_uuid);

  if (0)
    printf(
        "top instance<%p> class<%s> uuid<%s>\n", //
        instance_out.get(),
        instance_out_classname.c_str(),
        uuids.c_str());

  this->popNode();
}

//////////////////////////////////////////////////////////////////////////////

node_ptr_t JsonDeserializer::deserializeObject(node_ptr_t parnode) {
  auto topnode = pushNode("object", NodeType::OBJECT);

  if (parnode->_impl.IsShared<JsonLeafNode>()) {
    OrkAssert(parnode->_value.Get<std::string>() == "nil");
    return nullptr;
  } else {
    auto implnode       = parnode->_impl.getShared<JsonObjectNode>();
    const auto& jsonval = implnode->_jsonobjectnode;
    OrkAssert(jsonval.IsObject());
    OrkAssert(jsonval.HasMember("object"));
    auto child_node   = _parseSubNode(topnode, jsonval);
    auto instance_out = child_node->_deser_instance;
    popNode();
    return child_node;
  }
}

//////////////////////////////////////////////////////////////////////////////

serdes::node_ptr_t JsonDeserializer::_parseSubNode(
    serdes::node_ptr_t parentnode, //
    const rapidjson::Value& subvalue) {

  auto child_node             = pushNode("", NodeType::OBJECT);
  child_node->_parent         = parentnode;
  child_node->_property       = parentnode->_property;
  child_node->_deser_instance = parentnode->_deser_instance;

  switch (subvalue.GetType()) {
    case rapidjson::kObjectType: {
      if (subvalue.HasMember("object")) {
        const auto& jsonobjnode     = subvalue["object"];
        auto implnode               = child_node->_impl.makeShared<JsonObjectNode>(jsonobjnode);
        auto instance_out           = _parseObjectNode(child_node);
        auto instance_out_classname = instance_out->GetClass()->Name();
        child_node->_deser_instance = instance_out;
        std::string uuids           = boost::uuids::to_string(instance_out->_uuid);
        child_node->_value.Set<object_ptr_t>(instance_out);
        trackObject(instance_out->_uuid, instance_out);
        if (0)
          printf(
              "instance<%p> class<%s> uuid<%s>\n", //
              instance_out.get(),
              instance_out_classname.c_str(),
              uuids.c_str());
      } else if (subvalue.HasMember("object-ref")) {
        const auto& jsonobjnode = subvalue["object-ref"];
        bool has_uuid           = jsonobjnode.HasMember("uuid-ref");
        OrkAssert(has_uuid);
        auto uuidstr = jsonobjnode["uuid-ref"].GetString();
        boost::uuids::string_generator gen;
        auto uuid                   = gen(uuidstr);
        auto instance_out           = findTrackedObject(uuid);
        child_node->_deser_instance = instance_out;
        child_node->_value.Set<object_ptr_t>(instance_out);
      } else {
        OrkAssert(false);
      }
      break;
    }
    case rapidjson::kArrayType: {
      OrkAssert(false);
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
      child_node->_value.Set<std::string>(subvalue.GetString());
      // printf("gotstr<%s>\n", child_node->_value.Get<std::string>().c_str());
      break;
    case rapidjson::kNumberType:
      child_node->_value.Set<double>(subvalue.GetDouble());
      break;
    default:
      OrkAssert(false);
  }
  popNode();
  return child_node;
}

//////////////////////////////////////////////////////////////////////////////

serdes::node_ptr_t JsonDeserializer::deserializeElement(node_ptr_t elemnode) {
  node_ptr_t childnode;
  /////////////////////////////////////////////////////
  // map element
  /////////////////////////////////////////////////////
  switch (elemnode->_type) {
    case NodeType::MAP_ELEMENT_LEAF:
    case NodeType::MAP_ELEMENT_OBJECT: {
      auto mapnode                    = elemnode->_parent;
      auto mapimplnode                = mapnode->_impl.getShared<JsonObjectNode>();
      const rapidjson::Value& objnode = mapimplnode->_jsonobjectnode;
      OrkAssert(mapimplnode->_iterator != objnode.MemberEnd());
      // printf("mapnode key<%s>\n", mapimplnode->_iterator->name.GetString());
      const auto& childvalue = mapimplnode->_iterator->value;
      childnode              = _parseSubNode(elemnode, childvalue);
      childnode->_key        = mapimplnode->_iterator->name.GetString();
      mapimplnode->_iterator++;
    } break;
    case NodeType::ARRAY_ELEMENT_LEAF:
    case NodeType::ARRAY_ELEMENT_OBJECT: {
      auto arynode = elemnode->_parent;

      auto aryimplnode               = arynode->_impl.getShared<JsonArrayNode>();
      const rapidjson::Value& jarray = aryimplnode->_jsonarray;
      OrkAssert(aryimplnode->_iterator != jarray.End());
      // printf("array element<%zu>\n", arynode->_index);
      const auto& childjsonvalue = *aryimplnode->_iterator;
      childnode                  = _parseSubNode(elemnode, childjsonvalue);
      aryimplnode->_iterator++;
    } break;
    default:
      OrkAssert(false);
      break;
  }
  return childnode;
  // OrkAssert(false);
}

//////////////////////////////////////////////////////////////////////////////

object_ptr_t JsonDeserializer::_parseObjectNode(serdes::node_ptr_t dsernode) {

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

  instance_out->preDeserialize(*this);

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
    auto prop     = description.property(propname);

    if (prop) {
      // printf("found propname<%s> prop<%p>\n", propname, prop);
      dsernode->_property = prop;
      auto child_node     = std::make_shared<Node>();

      child_node->_parent         = dsernode;
      child_node->_property       = prop;
      child_node->_deserializer   = this;
      child_node->_deser_instance = instance_out;
      child_node->_impl.makeShared<JsonLeafNode>(propnode);

      switch (propnode.GetType()) {
        case rapidjson::kObjectType: {
          child_node->_numchildren = propnode.MemberCount();
          auto jsonobjnode         = child_node->_impl.makeShared<JsonObjectNode>(propnode);
          jsonobjnode->_iterator   = propnode.MemberBegin();
          break;
        }
        case rapidjson::kArrayType: {
          child_node->_numchildren = propnode.Size();
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
  instance_out->postDeserialize(*this);

  ///////////////////////////////////
  return instance_out;
}

//////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serdes
