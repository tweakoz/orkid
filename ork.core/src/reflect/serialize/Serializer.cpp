////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/Command.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/stream/IOutputStream.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>
#include <ork/object/Object.h>
#include <boost/uuid/uuid_io.hpp>
////////////////////////////////////////////////////////////////
namespace ork::reflect::serialize {
////////////////////////////////////////////////////////////////
/*void ISerializer::referenceObject(object_constptr_t as_object) {
  const auto& uuid  = as_object->_uuid;
  std::string uuids = boost::uuids::to_string(uuid);
  OrkAssert(_serialized.find(uuids) == _serialized.end());
  _serialized.insert(uuids);

  Command referenceAttributeCommand(Command::EATTRIBUTE, "id");
  beginCommand(referenceAttributeCommand);
  Serialize(PieceString(uuids.c_str()));
  endCommand(referenceAttributeCommand);
}*/
////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serialize
