////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/enum_serializer.inl>

namespace ork::reflect::serdes {

const char* DoSerializeEnum(int value, EnumNameMap* enum_map, BidirectionalSerializer& bidi) {
  ISerializer* pser = bidi.Serializer();

  // pser->Hint( "EnumMap", reinterpret_cast<intptr_t>(enum_map) );

  for (int i = 0; enum_map[i].name; i++) {
    if (value == enum_map[i].value) {
      return enum_map[i].name;
    }
  }

  bidi.Fail();

  return "<<BAD_ENUM>>";
}

int DoDeserializeEnum(const ConstString& name, EnumNameMap* enum_map, BidirectionalSerializer& bidi) {
  // IDeserializer *pser = bidi.Deserializer();
  // pser->Hint( "EnumMap", reinterpret_cast<int>(enum_map) );

  for (int i = 0; enum_map[i].name; i++) {
    if (name == enum_map[i].name) {
      return enum_map[i].value;
    }
  }

  bidi.Fail();

  return -1;
}

} // namespace ork::reflect::serdes
