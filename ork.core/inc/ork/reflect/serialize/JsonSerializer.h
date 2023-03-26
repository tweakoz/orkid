////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/ISerializer.h>
#include <ork/orkstl.h>
#include <ork/rtti/Category.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#include <rapidjson/document.h>

#pragma GCC diagnostic pop

namespace ork { namespace stream {
class IOutputStream;
}} // namespace ork::stream

namespace ork::reflect::serdes {

class JsonSerializer : public ISerializer {
public:
  JsonSerializer();
  ~JsonSerializer();

  node_ptr_t serializeContainerElement(node_ptr_t elemnode) override;
  node_ptr_t serializeObject(node_ptr_t objnode) override;
  void serializeLeaf(node_ptr_t leafnode) override;

  std::string output();

private:
  using allocator_t = rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>;

  void _serializeNamedItem(std::string name, const var_t&);
  node_ptr_t _createNode(std::string named, NodeType type);

  node_ptr_t pushNode(std::string named, NodeType type) override;
  void popNode() override;

  std::shared_ptr<allocator_t> _allocator;
  std::shared_ptr<rapidjson::Document> _document;
};
} // namespace ork::reflect::serdes
