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
ISerializer::node_ptr_t JsonSerializer::pushObjectNode(std::string named) {
  node_ptr_t n   = std::make_shared<Node>(); //)named, rapidjson::kObjectType);
  auto impl      = n->_impl.makeShared<JsonSerObjectNode>();
  n->_name       = named;
  n->_serializer = this;
  _nodestack.push(n);
  return n;
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::popNode() {
  auto n = topNode();
  _nodestack.pop();

  auto impl = n->_impl.getShared<JsonSerObjectNode>();
  rapidjson::Value key(n->_name.c_str(), *_allocator);
  if (_nodestack.empty()) {
    _document.AddMember(
        key, //
        impl->_jsonvalue,
        *_allocator);
  } else {
    auto topimpl = topNode()->_impl.getShared<JsonSerObjectNode>();
    topimpl->_jsonvalue.AddMember(
        key, //
        impl->_jsonvalue,
        *_allocator);
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
ISerializer::node_ptr_t JsonSerializer::serializeElement(ISerializer::node_ptr_t elemnode) {

  OrkAssert(elemnode->_instance);

  auto chnode       = pushObjectNode(elemnode->_key);
  chnode->_instance = elemnode->_instance;
  chnode->_parent   = elemnode;
  if (auto as_obj = elemnode->_value.TryAs<object_ptr_t>()) {
    chnode->_instance = as_obj.value();
    auto objnode      = serializeObject(chnode);
    OrkAssert(objnode);
    if (objnode)
      objnode->_parent = chnode;
  } else {
    chnode->_value = elemnode->_value;
    chnode->_name  = elemnode->_key;
    serializeLeaf(chnode);
  }

  popNode();
  return chnode;
}
void JsonSerializer::serializeLeaf(node_ptr_t leafnode) {
  auto implnode = leafnode->_impl.getShared<JsonSerObjectNode>();
  rapidjson::Value nameval(leafnode->_name.c_str(), *_allocator);
  if (auto as_bool = leafnode->_value.TryAs<bool>()) {
    rapidjson::Value boolval;
    boolval.SetBool(as_bool.value());
    implnode->_jsonvalue.AddMember(
        nameval, //
        boolval,
        *_allocator);
  } else if (auto as_int = leafnode->_value.TryAs<int>()) {
    rapidjson::Value intval;
    intval.SetInt(as_int.value());
    implnode->_jsonvalue.AddMember(
        nameval, //
        intval,
        *_allocator);
  } else if (auto as_uint32_t = leafnode->_value.TryAs<uint32_t>()) {
    rapidjson::Value uint32_tval(as_uint32_t.value());
    implnode->_jsonvalue.AddMember(
        nameval, //
        uint32_tval,
        *_allocator);
  } else if (auto as_size_t = leafnode->_value.TryAs<size_t>()) {
    rapidjson::Value size_tval(as_size_t.value());
    implnode->_jsonvalue.AddMember(
        nameval, //
        size_tval,
        *_allocator);
  } else if (auto as_float = leafnode->_value.TryAs<float>()) {
    rapidjson::Value floatval;
    floatval.SetFloat(as_float.value());
    implnode->_jsonvalue.AddMember(
        nameval, //
        floatval,
        *_allocator);
  } else if (auto as_double = leafnode->_value.TryAs<double>()) {
    rapidjson::Value doubleval;
    doubleval.SetDouble(as_double.value());
    implnode->_jsonvalue.AddMember(
        nameval, //
        doubleval,
        *_allocator);
  } else if (auto as_str = leafnode->_value.TryAs<std::string>()) {
    rapidjson::Value strval(as_str.value().c_str(), *_allocator);
    implnode->_jsonvalue.AddMember(
        nameval, //
        strval,
        *_allocator);
  } else if (auto as_nil = leafnode->_value.TryAs<void*>()) {
    //////////////////////////////////////////////////////////////////
    // if we get here we have an object property, but set to nullptr
    //  otherwise we would have went into serializeObject
    //////////////////////////////////////////////////////////////////
    auto parimplnode = leafnode->_impl.getShared<JsonSerObjectNode>();
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
ISerializer::node_ptr_t JsonSerializer::serializeObject(node_ptr_t parnode) {

  node_ptr_t onode;
  auto instance = parnode->_instance;
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

      onode            = pushObjectNode("object");
      onode->_parent   = parnode;
      onode->_instance = instance;
      auto oimplnode   = onode->_impl.getShared<JsonSerObjectNode>();
      auto classname   = objclazz->Name();
      rapidjson::Value classval(classname.c_str(), *_allocator);
      oimplnode->_jsonvalue.AddMember(
          "class", //
          classval,
          *_allocator);

      oimplnode->_jsonvalue.AddMember(
          "uuid", //
          uuidval,
          *_allocator);

      auto propsnode       = pushObjectNode("properties");
      propsnode->_instance = instance;
      propsnode->_parent   = onode;

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
