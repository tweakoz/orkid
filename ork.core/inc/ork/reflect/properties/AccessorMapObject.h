////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/properties/IObjectMap.h>

namespace ork { namespace reflect {

template<typename KeyType>
class AccessorMapObject : public IObjectMap
{
public:
	typedef void (*SerializationFunction)(BidirectionalSerializer &, const KeyType &, const Object *);
	
	AccessorMapObject(
		const Object *(Object::*get)(const KeyType &, int) const,
		Object *(Object::*access)(const KeyType &, int),
		void (Object::*erase)(const KeyType &, int),
		void (Object::*mSerializer)(SerializationFunction, BidirectionalSerializer &) const
		);
private:
	/*virtual*/ Object *AccessItem(IDeserializer &key, int, Object *) const;
    /*virtual*/ const Object *AccessItem(IDeserializer &key, int, const Object *) const;

	/*virtual*/ bool DeserializeItem(IDeserializer *value, IDeserializer &key, int, Object *) const;
    /*virtual*/ bool SerializeItem(ISerializer &value, IDeserializer &key, int, const Object *) const;
    
	/*virtual*/ bool Deserialize(IDeserializer &serializer, Object *obj) const;
    /*virtual*/ bool Serialize(ISerializer &serializer, const Object *obj) const;

	static void DoSerialize(BidirectionalSerializer &bidi, const KeyType &key, const Object *value);

	const Object *(Object::*mGetter)(const KeyType &, int) const;
	Object *(Object::*mAccessor)(const KeyType &, int);
	void (Object::*mEraser)(const KeyType &, int);
	void (Object::*mSerializer)(SerializationFunction, BidirectionalSerializer &) const;
};

} }

