////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/reflect/Command.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/stream/IOutputStream.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>
#include <ork/kernel/string/string.h>
#include <ork/object/Object.h>
#include <boost/uuid/uuid_io.hpp>
#include <cstring>
#include <rapidjson/writer.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>

namespace ork::reflect::serdes {
struct JsonSerObjectNode {
  JsonSerObjectNode() {
    _jsonvalue.SetObject();
  }
  rapidjson::Value _jsonvalue;
};
////////////////////////////////////////////////////////////////////////////////
JsonSerializer::JsonSerializer()
    : _document() {
  _allocator = &_document.GetAllocator();
  _document.SetObject();
}
////////////////////////////////////////////////////////////////////////////////
JsonSerializer::~JsonSerializer() {
}
////////////////////////////////////////////////////////////////////////////////
node_ptr_t JsonSerializer::_createNode(std::string named, NodeType type) {
  node_ptr_t n   = std::make_shared<Node>(); //)named, rapidjson::kObjectType);
  auto impl      = n->_impl.makeShared<JsonSerObjectNode>();
  n->_name       = named;
  n->_type       = type;
  n->_serializer = this;
  return n;
}
////////////////////////////////////////////////////////////////////////////////
node_ptr_t JsonSerializer::pushNode(std::string named, NodeType type) {
  node_ptr_t n = _createNode(named, type);
  _nodestack.push(n);
  return n;
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::popNode() {
  auto n = topNode();
  _nodestack.pop();

  switch (n->_type) {
    case NodeType::OBJECT:
    case NodeType::PROPERTIES:
    case NodeType::MAP:
    case NodeType::ARRAY:
    case NodeType::LEAF: {
      rapidjson::Value key(n->_name.c_str(), *_allocator);
      auto impl = n->_impl.getShared<JsonSerObjectNode>();
      if (_nodestack.empty()) {
        _document.AddMember(
            key, //
            impl->_jsonvalue,
            *_allocator);
        break;
      } else {
        auto topimpl = topNode()->_impl.getShared<JsonSerObjectNode>();
        topimpl->_jsonvalue.AddMember(
            key, //
            impl->_jsonvalue,
            *_allocator);
        break;
      }
    }
    case NodeType::MAP_ELEMENT:
    case NodeType::ARRAY_ELEMENT:
      break;
    default:
      OrkAssert(false);
      break;
  }
}
////////////////////////////////////////////////////////////////////////////////
std::string JsonSerializer::output() {

  popNode(); // pop objects

  rapidjson::StringBuffer strbuf;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
  writer.SetIndent(' ', 1);
  _document.Accept(writer);

  return strbuf.GetString();
}
////////////////////////////////////////////////////////////////////////////////
node_ptr_t JsonSerializer::serializeMapElement(node_ptr_t elemnode) {

  OrkAssert(elemnode->_ser_instance);

  if (auto as_obj = elemnode->_value.TryAs<object_ptr_t>()) {
    elemnode->_ser_instance = as_obj.value();
    elemnode->_type         = NodeType::OBJECT;
    auto objnode            = serializeObject(elemnode);
    OrkAssert(objnode);
    objnode->_parent = elemnode;
    // popNode(); // pop objnode
  } else {
    elemnode->_name = elemnode->_key;
    /////////////////////////////////
    // put leafnodes under mapnode
    /////////////////////////////////
    auto mapnode    = elemnode->_parent;
    elemnode->_impl = mapnode->_impl;
    /////////////////////////////////
    serializeLeaf(elemnode);
  }

  return elemnode;
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serializeLeaf(node_ptr_t leafnode) {
  auto parimplnode = leafnode->_impl.getShared<JsonSerObjectNode>();
  rapidjson::Value nameval(leafnode->_name.c_str(), *_allocator);
  if (auto as_bool = leafnode->_value.TryAs<bool>()) {
    rapidjson::Value boolval;
    boolval.SetBool(as_bool.value());
    parimplnode->_jsonvalue.AddMember(
        nameval, //
        boolval,
        *_allocator);
  } else if (auto as_int = leafnode->_value.TryAs<int>()) {
    rapidjson::Value intval;
    intval.SetInt(as_int.value());
    parimplnode->_jsonvalue.AddMember(
        nameval, //
        intval,
        *_allocator);
  } else if (auto as_uint = leafnode->_value.TryAs<unsigned int>()) {
    rapidjson::Value uintval(uint32_t(as_uint.value()));
    parimplnode->_jsonvalue.AddMember(
        nameval, //
        uintval,
        *_allocator);
  } else if (auto as_ulong = leafnode->_value.TryAs<unsigned long>()) {
    rapidjson::Value ulongval(uint64_t(as_ulong.value()));
    parimplnode->_jsonvalue.AddMember(
        nameval, //
        ulongval,
        *_allocator);
  } else if (auto as_float = leafnode->_value.TryAs<float>()) {
    rapidjson::Value floatval;
    floatval.SetFloat(as_float.value());
    parimplnode->_jsonvalue.AddMember(
        nameval, //
        floatval,
        *_allocator);
  } else if (auto as_double = leafnode->_value.TryAs<double>()) {
    rapidjson::Value doubleval;
    doubleval.SetDouble(as_double.value());
    parimplnode->_jsonvalue.AddMember(
        nameval, //
        doubleval,
        *_allocator);
  } else if (auto as_str = leafnode->_value.TryAs<std::string>()) {
    rapidjson::Value strval(as_str.value().c_str(), *_allocator);
    parimplnode->_jsonvalue.AddMember(
        nameval, //
        strval,
        *_allocator);
  } else if (auto as_nil = leafnode->_value.TryAs<void*>()) {
    //////////////////////////////////////////////////////////////////
    // if we get here we have an object property, but set to nullptr
    //  otherwise we would have went into serializeObject
    //////////////////////////////////////////////////////////////////
    // auto parimplnode = leafnode->_impl.getShared<JsonSerObjectNode>();
    OrkAssert(as_nil.value() == nullptr);
    parimplnode->_jsonvalue.AddMember(
        nameval, //
        "nil",
        *_allocator);
  } else {
    OrkAssert(false);
  }
}
////////////////////////////////////////////////////////////////////////////////
node_ptr_t JsonSerializer::serializeObject(node_ptr_t parnode) {

  node_ptr_t onode;
  auto instance = parnode->_ser_instance;
  if (instance) {
    auto parimplnode  = parnode->_impl.getShared<JsonSerObjectNode>();
    const auto& uuid  = instance->_uuid;
    std::string uuids = boost::uuids::to_string(uuid);
    auto it           = _reftracker.find(uuids);
    rapidjson::Value uuidval(uuids.c_str(), *_allocator);
    ////////////////////////////////////
    // firstreference
    ////////////////////////////////////
    if (it == _reftracker.end()) {
      _reftracker.insert(uuids);

      auto objclazz = instance->GetClass();
      auto& desc    = objclazz->Description();

      onode                = pushNode("object", NodeType::OBJECT);
      onode->_parent       = parnode;
      onode->_ser_instance = instance;
      auto oimplnode       = onode->_impl.getShared<JsonSerObjectNode>();
      auto classname       = objclazz->Name();
      rapidjson::Value classval(classname.c_str(), *_allocator);
      oimplnode->_jsonvalue.AddMember(
          "class", //
          classval,
          *_allocator);

      oimplnode->_jsonvalue.AddMember(
          "uuid", //
          uuidval,
          *_allocator);

      auto propsnode           = pushNode("properties", NodeType::PROPERTIES);
      propsnode->_ser_instance = instance;
      propsnode->_parent       = onode;

      for (auto prop_item : desc.properties()) {
        auto propname        = prop_item.first;
        auto property        = prop_item.second;
        propsnode->_property = property;
        propsnode->_name     = propname.c_str();
        property->serialize(propsnode);
      }
      propsnode->_name = "properties";

      popNode(); // pop "properties"
      popNode(); // pop "object"
    }
    ////////////////////////////////////
    // backreference
    ////////////////////////////////////
    else {
      OrkAssert(false);
      /*topNode()->_value.AddMember(
          "backreference", //
          uuidval,
          *_allocator);*/
    }

  } else {
    auto parimplnode = parnode->_impl.getShared<JsonSerObjectNode>();
    parimplnode->_jsonvalue.AddMember(
        "object", //
        "nil",
        *_allocator);
  }
  return onode;
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serdes
