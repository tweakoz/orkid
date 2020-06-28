////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/IDeserializer.h>
#include <ork/stream/InputStreamBuffer.h>

#include <ork/orkstl.h>
#include <rapidjson/reader.h>
#include <rapidjson/document.h>

namespace ork { namespace reflect { namespace serialize {

class JsonDeserializer : public IDeserializer {
public:
  void deserializeTop(object_ptr_t&) override;

  JsonDeserializer(const std::string& jsondata);

  // void deserializeSharedObject(object_ptr_t&) override;
  // void deserializeObjectProperty(const ObjectProperty*, object_ptr_t) override;

  // void beginCommand(Command&) override;
  // void endCommand(const Command&) override;
  // void deserializeItem() override;

private:
  using allocator_t = rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>*;

  object_ptr_t _parseObjectNode(const rapidjson::Value& node);
  ////////////////////////////////////////////

  rapidjson::Document _document;
  allocator_t _allocator;
};

}}} // namespace ork::reflect::serialize
