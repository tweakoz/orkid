////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/reflect/AccessorObjectArrayPropertyObject.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/ISerializer.h>
#include <ork/object/Object.h>

namespace ork { namespace reflect {

AccessorObjectArrayPropertyObject::AccessorObjectArrayPropertyObject(
    Object *(Object::*accessor)(size_t),
    size_t (Object::*counter)() const,
	void (Object::*resizer)(size_t))
	: mAccessor(accessor)
	, mCounter(counter)
	, mResizer(resizer)
{
}

Object *AccessorObjectArrayPropertyObject::AccessObject(Object *object, size_t index) const
{
	return (object->*mAccessor)(index);
}

const Object *AccessorObjectArrayPropertyObject::AccessObject(const Object *object, size_t index) const
{
	return (const_cast<Object *>(object)->*mAccessor)(index);
}

size_t AccessorObjectArrayPropertyObject::Count(const Object *object) const
{
	return (object->*mCounter)();
}

bool AccessorObjectArrayPropertyObject::DeserializeItem(
	IDeserializer &deserializer, Object *object, size_t index) const
{
	Command object_command;

	if(false == deserializer.BeginCommand(object_command))
		return false;

	Object *value = AccessObject(object, index);

	if(object_command.Type() != Command::EOBJECT 
		|| NULL == value 
		|| object_command.Name() != value->GetClass()->Name())
	{
		deserializer.EndCommand(object_command);
		return false;
	}

	if(false == value->Deserialize(deserializer))
		return false;

	if(false == deserializer.EndCommand(object_command))
		return false;

	return true;
}

bool AccessorObjectArrayPropertyObject::SerializeItem(
	ISerializer &serializer, const Object *object, size_t index) const
{
	return AccessObject(object, index)->Serialize(serializer);
}

bool AccessorObjectArrayPropertyObject::Resize(Object *obj, size_t size) const
{
	if(mResizer != 0)
	{
		(obj->*mResizer)(size);
		return true;
	}
	else
	{
		return size == Count(obj);
	}
}


bool AccessorObjectArrayPropertyObject::Deserialize( ork::reflect::IDeserializer &, ork::Object * ) const
{
	OrkAssert(false);
	return false;
}
bool AccessorObjectArrayPropertyObject::Serialize( ork::reflect::ISerializer &, ork::Object const * ) const
{
	OrkAssert(false);
	return false;
}

} }
