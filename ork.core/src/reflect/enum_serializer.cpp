////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/enum_serializer.inl>

namespace ork::reflect::serdes {

enumregistrar_ptr_t EnumRegistrar::instance() {
  static enumregistrar_ptr_t _inst = std::make_shared<EnumRegistrar>();
  return _inst;
}

} // namespace ork::reflect::serdes
