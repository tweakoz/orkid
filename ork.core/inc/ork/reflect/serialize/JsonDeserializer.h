////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/IDeserializer.h>
#include <ork/stream/InputStreamBuffer.h>

#include <ork/orkstl.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#include <rapidjson/reader.h>
#include <rapidjson/document.h>

#pragma GCC diagnostic pop

namespace ork { namespace reflect { namespace serdes {

class JsonDeserializer : public IDeserializer {
public:
  void deserializeTop(object_ptr_t&) override;
  node_ptr_t deserializeElement(node_ptr_t elemnode) override;
  node_ptr_t deserializeObject(node_ptr_t) override;

  JsonDeserializer(const std::string& jsondata);

private:
  using allocator_t = rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>*;
  node_ptr_t pushNode(std::string named, NodeType type) override;
  void popNode() override;

  object_ptr_t _parseObjectNode(serdes::node_ptr_t dsernode);
  node_ptr_t _parseSubNode(
      serdes::node_ptr_t parentnode, //
      const rapidjson::Value& subvalue);

  ////////////////////////////////////////////

  rapidjson::Document _document;

};

}}} // namespace ork::reflect::serdes
