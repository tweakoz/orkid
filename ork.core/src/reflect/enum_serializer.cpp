////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/enum_serializer.inl>

namespace ork::reflect::serdes {

enumregistrar_ptr_t EnumRegistrar::instance() {
  static enumregistrar_ptr_t _inst = std::make_shared<EnumRegistrar>();
  return _inst;
}

} // namespace ork::reflect::serdes
