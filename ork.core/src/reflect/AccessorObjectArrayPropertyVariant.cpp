////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/reflect/AccessorObjectArrayPropertyVariant.h>
#include <ork/object/Object.h>

namespace ork { namespace reflect {

AccessorObjectArrayPropertyVariant::AccessorObjectArrayPropertyVariant(
	bool (Object::*serialize_item)(ISerializer &, size_t) const,
	bool (Object::*deserialize_item)(IDeserializer &, size_t),
	size_t (Object::*count)() const,
	bool (Object::*resize)(size_t))
	: mSerializeItem(serialize_item)
	, mDeserializeItem(deserialize_item)
	, mCount(count)
	, mResize(resize)
{}

bool AccessorObjectArrayPropertyVariant::DeserializeItem(
	IDeserializer &deserializer, Object *object, size_t index) const
{
	return (object->*mDeserializeItem)(deserializer, index);
}

bool AccessorObjectArrayPropertyVariant::SerializeItem(
	ISerializer &serializer, const Object *object, size_t index) const
{
	return (object->*mSerializeItem)(serializer, index);
}

size_t AccessorObjectArrayPropertyVariant::Count( const Object *object ) const
{
	return (object->*mCount)();
}

bool AccessorObjectArrayPropertyVariant::Resize( Object *object, size_t size ) const
{
	return (object->*mResize)(size);
}

} }
