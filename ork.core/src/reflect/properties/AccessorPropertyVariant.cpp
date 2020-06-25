////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/reflect/properties/AccessorVariant.h>
#include <ork/object/Object.h>

namespace ork { namespace reflect {

AccessorVariant::AccessorVariant(
		bool (Object::*ser)(ISerializer &) const,
		bool (Object::*deser)(IDeserializer &) )
	: mDeserialize(deser)
	, mSerialize(ser)
{

}

bool AccessorVariant::Deserialize(IDeserializer &deserializer, Object *object) const
{
	return (object->*mDeserialize)(deserializer);
}

bool AccessorVariant::Serialize(ISerializer &serializer, const Object *object) const
{
	return (object->*mSerialize)(serializer);
}

} }
