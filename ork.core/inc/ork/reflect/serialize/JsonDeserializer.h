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
  node_ptr_t deserializeElement(node_ptr_t elemnode) override;

  JsonDeserializer(const std::string& jsondata);

private:
  using allocator_t = rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>*;

  object_ptr_t _parseObjectNode(IDeserializer::node_ptr_t dsernode);
  node_ptr_t _parseSubNode(
      IDeserializer::node_ptr_t parentnode, //
      const rapidjson::Value& subvalue);

  ////////////////////////////////////////////

  rapidjson::Document _document;
  allocator_t _allocator;
};

}}} // namespace ork::reflect::serialize
