////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/reflect/AccessorObjectPropertyVariant.h>
#include <ork/object/Object.h>

namespace ork { namespace reflect {

AccessorObjectPropertyVariant::AccessorObjectPropertyVariant(
		bool (Object::*ser)(ISerializer &) const,
		bool (Object::*deser)(IDeserializer &) )
	: mDeserialize(deser)
	, mSerialize(ser)
{

}

bool AccessorObjectPropertyVariant::Deserialize(IDeserializer &deserializer, Object *object) const
{
	return (object->*mDeserialize)(deserializer);
}

bool AccessorObjectPropertyVariant::Serialize(ISerializer &serializer, const Object *object) const
{
	return (object->*mSerialize)(serializer);
}

} }
