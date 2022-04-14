////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/Command.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/stream/IOutputStream.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>
#include <ork/object/Object.h>
#include <boost/uuid/uuid_io.hpp>
////////////////////////////////////////////////////////////////
namespace ork::reflect::serdes {
////////////////////////////////////////////////////////////////
IDeserializer::~IDeserializer() {
}
////////////////////////////////////////////////////////////////
  node_ptr_t IDeserializer::pushNode(std::string named, NodeType type) {
    return nullptr;
  }
////////////////////////////////////////////////////////////////
  void IDeserializer::popNode() {
    return;
  }
////////////////////////////////////////////////////////////////

  node_ptr_t IDeserializer::deserializeObject(node_ptr_t) {
    return node_ptr_t(nullptr);
  }
////////////////////////////////////////////////////////////////
  node_ptr_t IDeserializer::deserializeElement(node_ptr_t elemnode) {
    return node_ptr_t(nullptr);
  }

////////////////////////////////////////////////////////////////
void IDeserializer::trackObject(
    boost::uuids::uuid id, //
    object_ptr_t instance) {
  std::string uuids = boost::uuids::to_string(id);
  auto it           = _reftracker.find(uuids);
  if( it != _reftracker.end() ){
    printf( "Duplicate UUID detected<%s>\n",uuids.c_str() );
  }
  OrkAssert(it == _reftracker.end());
  _reftracker[uuids] = instance;
} // namespace ork::reflect::serdesvoidIDeserializer::trackObject(boost::uuids::uuidid,object_ptr_tinstance)
////////////////////////////////////////////////////////////////
object_ptr_t IDeserializer::findTrackedObject(boost::uuids::uuid id) const {
  std::string uuids = boost::uuids::to_string(id);
  auto it           = _reftracker.find(uuids);
  OrkAssert(it != _reftracker.end());
  return it->second;
}
////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serdes
