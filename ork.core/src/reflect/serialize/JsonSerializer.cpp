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

namespace ork::reflect::serialize {
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
JsonSerializer::node_ptr_t JsonSerializer::pushObjectNode(std::string named) {
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
JsonSerializer::node_ptr_t JsonSerializer::topNode() {
  node_ptr_t n;
  if (not _nodestack.empty()) {
    n = _nodestack.top();
  }
  return n;
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
ISerializer::node_ptr_t JsonSerializer::serializeTop(object_constptr_t instance) {
  _topnode            = pushObjectNode("top");
  _topnode->_instance = instance;
  auto objnode        = serializeObject(_topnode);
  return _topnode;
}
////////////////////////////////////////////////////////////////////////////////
ISerializer::node_ptr_t JsonSerializer::serializeElement(ISerializer::node_ptr_t elemnode) {

  OrkAssert(elemnode->_instance);

  auto chnode       = pushObjectNode(elemnode->_key);
  chnode->_instance = elemnode->_instance;
  chnode->_parent   = elemnode;
  if (auto as_obj = elemnode->_value.TryAs<object_ptr_t>()) {
    auto objnode = serializeObject(chnode);
    if (objnode)
      objnode->_parent = chnode;
  }

  // chnode->_parent = elemnode;
  popNode();
  return chnode;
}
////////////////////////////////////////////////////////////////////////////////
/*
void JsonSerializer::_serializeNamedItem(
    std::string named, //
    const hintvar_t& value) {
  rapidjson::Value nameval(named.c_str(), *_allocator);
  if (auto as_piecestr = value.TryAs<PieceString>()) {
    rapidjson::Value strval(as_piecestr.value().c_str(), *_allocator);
    topNode()->_value.AddMember(
        nameval, //
        strval,
        *_allocator);
  } else if (auto as_int = value.TryAs<int>()) {
    rapidjson::Value intval;
    intval.SetInt(as_int.value());
    topNode()->_value.AddMember(
        nameval, //
        intval,
        *_allocator);
  } else if (auto as_bool = value.TryAs<bool>()) {
    rapidjson::Value boolval;
    boolval.SetBool(as_bool.value());
    topNode()->_value.AddMember(
        nameval, //
        boolval,
        *_allocator);
  } else if (auto as_object = value.TryAs<object_constptr_t>()) {
    serializeSharedObject(as_object.value());
  } else if (auto as_object = value.TryAs<object_ptr_t>()) {
    serializeSharedObject(as_object.value());
  } else {
    OrkAssert(false);
  }
}
void JsonSerializer::serializeElement(const hintvar_t& value) {
  _serializeNamedItem("item", value);
}
*/
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

      for (auto prop_item : desc.Properties()) {

        auto propname       = prop_item.first;
        auto property       = prop_item.second;
        auto propnode       = pushObjectNode(propname.c_str());
        propnode->_property = property;
        propnode->_instance = instance;
        propnode->_parent   = propsnode;
        property->serialize(propnode);
        popNode();
      }

      // Object::xxxSerializeShared(instance, *this);

      popNode(); // pop "properties"

      popNode(); // pop "object"
    }
    ////////////////////////////////////
    // backreference
    ////////////////////////////////////
    else {
      /*topNode()->_value.AddMember(
          "backreference", //
          uuidval,
          *_allocator);*/
    }

  } else {
    /*topNode()->_value.AddMember(
        "nil", //
        "nil",
        *_allocator);*/
  }
  return onode;
} /*
 ////////////////////////////////////////////////////////////////////////////////
 void JsonSerializer::Hint(const PieceString& name, hintvar_t val) {

   if (name == "MultiIndex") {
     _multiindex = val.Get<int>();
   } else if (name == "map_key") {
     _mapkey = val;
   } else if (name == "map_value") {
     auto kasstr = _mapkey.Get<std::string>();
     _serializeNamedItem(kasstr, val);
   } else if (auto as_str = val.TryAs<std::string>()) {
     rapidjson::Value nameval(name.c_str(), *_allocator);
     rapidjson::Value valval(as_str.value().c_str(), *_allocator);
     topNode()->_value.AddMember(
         nameval, //
         valval,
         *_allocator);
   }
 }
 ////////////////////////////////////////////////////////////////////////////////
 void JsonSerializer::serializeData(const uint8_t*, size_t) {
 }*/
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serialize
