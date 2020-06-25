////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IObjectMapProperty.h>
#include <ork/reflect/Command.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

class BidirectionalSerializer;
class Command;

class  AccessorMapPropertyVariantContext
{
public:
	AccessorMapPropertyVariantContext(BidirectionalSerializer &bidi);
	BidirectionalSerializer &Bidi();
	void BeginItem();
	void BeginValue();
	void EndItem();
private:
	BidirectionalSerializer &mBidi;
	Command mItemCommand;
	Command mAttributeCommand;
};

class  AccessorMapPropertyVariant : public IObjectMapProperty
{
	static void GetClassStatic(); // Kill inherited GetClassStatic()
public:

	typedef void (*SerializerCallbackItem)(
		AccessorMapPropertyVariantContext &ctx,
		BidirectionalSerializer &);

	static const int kDeserializeInsertItem = -1;

	AccessorMapPropertyVariant(
		bool (Object::*)(IDeserializer &, int, ISerializer &),
		bool (Object::*)(IDeserializer &, int, IDeserializer *) const,
		bool (Object::*)(AccessorMapPropertyVariantContext &) const
		);

private:
	bool (Object::*mReadItem)(IDeserializer &key, int, ISerializer &value);
	bool (Object::*mWriteItem)(IDeserializer &key, int, IDeserializer *value) const;
	bool (Object::*mMapSerialization)(AccessorMapPropertyVariantContext &) const;

    /*virtual*/ bool Deserialize(IDeserializer &serializer, Object *obj) const;
    /*virtual*/ bool Serialize(ISerializer &serializer, const Object *obj) const;
    /*virtual*/ bool DeserializeItem(IDeserializer *value, IDeserializer &key, int, Object *) const;
    /*virtual*/ bool SerializeItem(ISerializer &value, IDeserializer &key, int, const Object *) const;
};

} }

