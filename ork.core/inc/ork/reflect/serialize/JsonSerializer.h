////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/ISerializer.h>
#include <ork/orkstl.h>
#include <ork/rtti/Category.h>
#include <rapidjson/document.h>

namespace ork { namespace stream {
class IOutputStream;
}} // namespace ork::stream

namespace ork { namespace reflect { namespace serdes {

class JsonSerializer : public ISerializer {
public:
  JsonSerializer();
  ~JsonSerializer();

  node_ptr_t serializeMapElement(node_ptr_t elemnode) override;
  node_ptr_t serializeObject(node_ptr_t objnode) override;
  void serializeLeaf(node_ptr_t leafnode) override;

  std::string output();

private:
  using allocator_t = rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>*;

  void _serializeNamedItem(std::string name, const var_t&);
  node_ptr_t _createNode(std::string named);

  node_ptr_t pushNode(std::string named) override;
  void popNode() override;

  allocator_t _allocator;
  rapidjson::Document _document;
};
}}} // namespace ork::reflect::serdes
