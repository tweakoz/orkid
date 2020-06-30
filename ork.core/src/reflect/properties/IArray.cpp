///////////////////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/reflect/properties/IArray.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/Command.h>

namespace ork::reflect {
///////////////////////////////////////////////////////////////////////////////

void IArray::deserialize(serdes::node_ptr_t desernode) const {

  /*int deser_count;
  deserializer.deserialize(deser_count);
  resize(obj, size_t(deser_count));

  deserializer.endCommand(command);

  size_t numitems = count(obj);

  for (size_t index = 0; index < numitems; index++) {
    Command item;
    deserializer.beginCommand(item);
    OrkAssert(item.Type() == Command::ELEMENT);
    deserializeElement(deserializer, obj, index);
    deserializer.endCommand(item);
  }*/
}

///////////////////////////////////////////////////////////////////////////////

void IArray::serialize(serdes::node_ptr_t sernode) const {
  size_t numitems = count(sernode->_ser_instance);
  for (size_t index = 0; index < numitems; index++) {
    // Command item(Command::ELEMENT);
    // serializer.beginCommand(item);
    // serializeElement(serializer, obj, index);
    // serializer.endCommand(item);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
