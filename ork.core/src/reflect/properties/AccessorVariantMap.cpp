////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/reflect/properties/AccessorMapPropertyVariant.h>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork { namespace reflect {


#if 0 // don't allow instatiations yet, this class isn't finished!!!
AccessorMapPropertyVariant::AccessorMapPropertyVariant(
	bool (Object::*read)(IDeserializer &, int, ISerializer &),
	bool (Object::*write)(IDeserializer &, int, IDeserializer *) const,
	bool (Object::*map)(AccessorMapPropertyVariantContext &) const)
	: mReadItem(read)
	, mWriteItem(write)
	, mMapSerialization(map)
{}

bool AccessorMapPropertyVariant::Deserialize(IDeserializer &deserializer, Object *object) const
{
	BidirectionalSerializer bidi(deserializer);

	KeyType key;
	ValueType value;

	while(DoDeserialize(bidi, key, value))
	{
		WriteItem(object, key, -1, &value);
	}

	return bidi.Succeeded();

	return false;
}

bool AccessorMapPropertyVariant::Serialize(ISerializer &serializer, const Object *object) const
{
	BidirectionalSerializer bidi(serializer);
	AccessorMapPropertyVariantContext ctx(bidi);

	return (object->*mMapSerialization)(ctx);
}

bool AccessorMapPropertyVariant::DeserializeItem(IDeserializer *value, IDeserializer &key, int, Object *) const
{
	return false;
}

bool AccessorMapPropertyVariant::SerializeItem(ISerializer &value, IDeserializer &key, int, const Object *) const
{
	return false;
}

AccessorMapPropertyVariantContext::AccessorMapPropertyVariantContext(BidirectionalSerializer &bidi)
	: mBidi(bidi)
{}

BidirectionalSerializer &AccessorMapPropertyVariantContext::Bidi()
{
	return mBidi;
}

void AccessorMapPropertyVariantContext::BeginItem()
{
	mItemCommand.Setup(Command::EITEM);

	if(false == mBidi.Serializer()->BeginCommand(mItemCommand))
		mBidi.Fail();

	mAttributeCommand.Setup(Command::EATTRIBUTE, "key");

	if(false == mBidi.Serializer()->BeginCommand(mAttributeCommand))
		mBidi.Fail();
}

void AccessorMapPropertyVariantContext::BeginValue()
{
	if(false == mBidi.Serializer()->EndCommand(mAttributeCommand))
		mBidi.Fail();
}

void AccessorMapPropertyVariantContext::EndItem()
{
	if(false == mBidi.Serializer()->EndCommand(mItemCommand))
		mBidi.Fail();
}
#endif

} }
