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

  //_joprog.SetObject();
  //_joprog.AddMember("KRZ", _joprogroot, _document);
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
    case Command::EPROPERTY:
      break;
    case Command::EITEM:
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
      break;
    case Command::EITEM:
      break;
  }
  _currentCommand = _currentCommand->PreviousCommand();
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const char& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const short& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const int& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const long& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const float& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const double& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const bool& value) {
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
    ////////////////////////////////////
    // backreference
    ////////////////////////////////////
    if (it != _reftracker.end()) {
      //_write('B');
      //_write(uuids.c_str());
      auto node = pushObjectNode("backreference");
      // /node->_value = rapidjson::Value("yo");
      popNode();
    }
    ////////////////////////////////////
    // firstreference
    ////////////////////////////////////
    else {
      auto objclazz = instance->GetClass();
      auto category = rtti::downcast<rtti::Category*>(objclazz->GetClass());

      auto node = pushObjectNode("firstreference");
      // node->_value = rapidjson::Value("whatup");
      //_writeHeader('R', category->Name());
      //_write(uuids.c_str());
      category->serializeObject(*this, instance);
      //_writeFooter('r');
      popNode();
    }

  } else {
    auto node    = pushObjectNode("nil");
    node->_value = rapidjson::Value("nil");
    popNode();
  }
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::Hint(const PieceString&) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const PieceString& string) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serializeData(const uint8_t*, size_t) {
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serialize
