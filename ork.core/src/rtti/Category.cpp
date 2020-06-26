////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/rtti/Category.h>

namespace ork { namespace rtti {

void Category::serializeObject(reflect::ISerializer& serializer, object_constptr_t value) const {
}

void Category::deserializeObject(reflect::IDeserializer& deserializer, object_ptr_t& value) const {
}

}} // namespace ork::rtti

INSTANTIATE_TRANSPARENT_RTTI(ork::rtti::Category, "ClassCategory");
