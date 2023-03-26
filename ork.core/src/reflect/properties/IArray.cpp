///////////////////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/reflect/properties/IArray.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/Command.h>

namespace ork::reflect {
///////////////////////////////////////////////////////////////////////////////

void IArray::deserialize(serdes::node_ptr_t arynode) const {
  auto deserializer  = arynode->_deserializer;
  auto instance      = arynode->_deser_instance;
  size_t numelements = arynode->_numchildren;
  resize(instance, numelements);
  for (size_t i = 0; i < numelements; i++) {
    arynode->_index = i;
    this->deserializeElement(arynode);
  }
}

///////////////////////////////////////////////////////////////////////////////

void IArray::serialize(serdes::node_ptr_t sernode) const {
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto arynode           = serializer->pushNode(_name, serdes::NodeType::ARRAY);
  arynode->_parent       = sernode;
  arynode->_ser_instance = instance;
  int numelements        = count(instance);
  for (size_t index = 0; index < numelements; index++) {
    auto elemnode           = serializer->pushNode("aryelem", serdes::NodeType::ARRAY_ELEMENT_LEAF);
    elemnode->_index        = index;
    elemnode->_parent       = arynode;
    elemnode->_ser_instance = instance;
    elemnode->_serializer   = serializer;
    serializeElement(elemnode);
    serializer->popNode(); // pop elemnode
  }
  serializer->popNode(); // pop mapnode
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
