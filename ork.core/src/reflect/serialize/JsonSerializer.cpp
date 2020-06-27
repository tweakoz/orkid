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
////////////////////////////////////////////////////////////////////////////////
JsonSerializer::JsonSerializer(stream::IOutputStream& stream)
    : mStream(stream)
    , _document() {
  _allocator = &_document.GetAllocator();
  _document.SetObject();

  _objects = pushObjectNode("objects");
}
////////////////////////////////////////////////////////////////////////////////
JsonSerializer::~JsonSerializer() {
}
////////////////////////////////////////////////////////////////////////////////
JsonSerializer::node_t JsonSerializer::pushObjectNode(std::string named) {
  node_t n = std::make_shared<Node>(named, rapidjson::kObjectType);
  _nodestack.push(n);
  return n;
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::popNode() {
  auto n = topNode();
  _nodestack.pop();
  rapidjson::Value key(n->_name.c_str(), *_allocator);
  if (_nodestack.empty()) {
    _document.AddMember(
        key, //
        n->_value,
        *_allocator);
  } else {
    topNode()->_value.AddMember(
        key, //
        n->_value,
        *_allocator);
  }
}
////////////////////////////////////////////////////////////////////////////////
JsonSerializer::node_t JsonSerializer::topNode() {
  node_t n;
  if (not _nodestack.empty()) {
    n = _nodestack.top();
  }
  return n;
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::finalize() {

  popNode(); // pop objects

  rapidjson::StringBuffer strbuf;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
  writer.SetIndent(' ', 1);
  _document.Accept(writer);

  auto outstr = strbuf.GetString();
  mStream.Write((const uint8_t*)outstr, strlen(outstr));
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::beginCommand(const Command& command) {
  switch (command.Type()) {
    case Command::EOBJECT:
      break;
    case Command::EATTRIBUTE:
      break;
    case Command::EPROPERTY: {
      auto propname = command.Name();
      auto node     = pushObjectNode(propname.c_str());
      break;
    }
    case Command::EITEM:
      // auto node = pushObjectNode("item");
      break;
  }

  command.PreviousCommand() = _currentCommand;
  _currentCommand           = &command;
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::endCommand(const Command& command) {
  OrkAssert(_currentCommand == &command);
  switch (command.Type()) {
    case Command::EOBJECT:
      break;
    case Command::EATTRIBUTE:
      break;
    case Command::EPROPERTY:
      popNode();
      break;
    case Command::EITEM:
      // popNode();
      break;
  }
  _currentCommand = _currentCommand->PreviousCommand();
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serializeItem(const hintvar_t& value) {
  if (auto as_piecestr = value.TryAs<PieceString>()) {
    rapidjson::Value strval(as_piecestr.value().c_str(), *_allocator);
    topNode()->_value.AddMember(
        "str", //
        strval,
        *_allocator);
  } else if (auto as_int = value.TryAs<int>()) {
    rapidjson::Value intval;
    intval.SetInt(as_int.value());
    topNode()->_value.AddMember(
        "int", //
        intval,
        *_allocator);
  } else if (auto as_bool = value.TryAs<bool>()) {
    rapidjson::Value boolval;
    boolval.SetBool(as_bool.value());
    topNode()->_value.AddMember(
        "bool", //
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
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serializeObjectProperty(
    const ObjectProperty* prop, //
    object_constptr_t instance) {

  prop->serialize(*this, instance);
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serializeSharedObject(object_constptr_t instance) {
  if (instance) {
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
      auto category = rtti::downcast<rtti::Category*>(objclazz->GetClass());

      auto node = pushObjectNode("object");

      auto classname = objclazz->Name();
      rapidjson::Value classval(classname.c_str(), *_allocator);
      node->_value.AddMember(
          "class", //
          classval,
          *_allocator);

      node->_value.AddMember(
          "uuid", //
          uuidval,
          *_allocator);

      auto propnode = pushObjectNode("properties");

      category->serializeObject(*this, instance);
      popNode();

      popNode();
    }
    ////////////////////////////////////
    // backreference
    ////////////////////////////////////
    else {
      topNode()->_value.AddMember(
          "backreference", //
          uuidval,
          *_allocator);
    }

  } else {
    topNode()->_value.AddMember(
        "nil", //
        "nil",
        *_allocator);
  }
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::Hint(const PieceString& name, hintvar_t val) {

  if (name == "MultiIndex") {
    _multiindex = val.Get<int>();
  } else if (name == "map_key") {
    _mapkey = val;
  } else if (name == "map_value") {
    auto kasstr = _mapkey.Get<std::string>();
    rapidjson::Value keyval(kasstr.c_str(), *_allocator);
    pushObjectNode(kasstr.c_str());
    serializeItem(val);
    popNode();
    // rapidjson::Value valval(as_str.value().c_str(), *_allocator);
    // topNode()->_value.AddMember(
    //  nameval, //
    // valval,
    //*_allocator);

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
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serialize
