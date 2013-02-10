////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/rtti/Category.h>

namespace ork { namespace rtti {

bool Category::SerializeReference(reflect::ISerializer &serializer, const ICastable *value) const
{
	return false;
}

bool Category::DeserializeReference(reflect::IDeserializer &deserializer, ICastable *&value) const
{
	return false;
}

} }

INSTANTIATE_TRANSPARENT_RTTI(ork::rtti::Category, "ClassCategory");
