////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/object/ObjectCategory.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/rtti/downcast.h>

namespace ork { namespace object {

bool ObjectCategory::SerializeReference(reflect::ISerializer &serializer, const rtti::ICastable *value) const
{
	const Object *object = rtti::downcast<const Object *>(value);

	return object->Serialize(serializer);
}

bool ObjectCategory::DeserializeReference(reflect::IDeserializer &deserializer, rtti::ICastable *&value) const
{
	reflect::Command command;

	if(deserializer.BeginCommand(command))
	{
		if(command.Type() != reflect::Command::EOBJECT)
		{
			deserializer.EndCommand(command);
			return false;
		}

		Class *the_class = rtti::Class::FindClass(command.Name());

		if(the_class == NULL)
		{
			orkprintf("ERROR: could not find class<%s>\n", command.Name().c_str());
		}

		ObjectClass *clazz = rtti::downcast<ObjectClass *>(the_class);
		
		if(clazz == NULL)
		{
			orkprintf("not an object class<%s>\n", command.Name().c_str());
		}
		//OrkAssert(clazz);

		if( clazz )
		{
			value = clazz->CreateObject();

			Object *object = rtti::downcast<Object *>(value);

			if(false == object->Deserialize(deserializer))
			{
				deserializer.EndCommand(command);
				return false;
			}
		}
		else
		{
			value = NULL;
			//return deserializer.EndCommand(command);
			//exit(0);
		}
	}
	else
		return false;
	
	if(false == deserializer.EndCommand(command))
		return false;

	return true;
}

ObjectCategory::ObjectCategory(const rtti::RTTIData &data)
	: rtti::Category(data)
{}

} }
