////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/reflect/properties/IArray.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/Command.h>

namespace ork { namespace reflect {

bool IArray::Deserialize(IDeserializer &deserializer, Object *obj) const
{
	bool result = true;
	int deser_count;

	Command command;

	if(deserializer.beginCommand(command))
	{
		if(command.Type() == Command::EATTRIBUTE && command.Name() == "size")
		{
			deserializer.Deserialize(deser_count);

			Resize(obj, size_t(deser_count));
		}
		else
		{
			deserializer.endCommand(command);
			return false;
		}

		if(false == deserializer.endCommand(command))
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	size_t count = Count(obj);

	for(size_t index = 0; index < count; index++)
	{
		Command item;
		if(false == deserializer.beginCommand(item))
			result = false;
		if(item.Type() != Command::EITEM)
			result = false;
		if(false == DeserializeItem(deserializer, obj, index))
			result = false;
		if(false == deserializer.endCommand(item))
			result = false;
	}

	return result;
}

bool IArray::Serialize(ISerializer &serializer, const Object *obj) const
{
	bool result = true;
	size_t count = Count(obj);

	Command command(Command::EATTRIBUTE, "size");

	if(false == serializer.beginCommand(command))
		result = false;
	if(false == serializer.Serialize(int(count)))
		result = false;
	if(false == serializer.endCommand(command))
		result = false;

	for(size_t index = 0; index < count; index++)
	{
		Command item(Command::EITEM);
		if(false == serializer.beginCommand(item))
			result = false;
		if(false == SerializeItem(serializer, obj, index))
			result = false;
		if(false == serializer.endCommand(item))
			result = false;
	}

	return result;
}

} }
