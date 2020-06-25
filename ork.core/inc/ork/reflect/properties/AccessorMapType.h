////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/properties/ITypedMap.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

class BidirectionalSerializer;

template<typename KeyType, typename ValueType>
class  AccessorMapType : public ITypedMap<KeyType, ValueType>
{
public:
	typedef typename ITypedMap<KeyType, ValueType>::ItemSerializeFunction
		ItemSerializeFunction;

	AccessorMapType(
		bool (Object::*getter)(const KeyType &, int, ValueType &) const,
		void (Object::*setter)(const KeyType &, int, const ValueType &),
		void (Object::*eraser)(const KeyType &, int),
		void (Object::*serializer)(ItemSerializeFunction, BidirectionalSerializer &) const
		);

private:
	/*virtual*/ bool ReadItem(const Object *, const KeyType &, int, ValueType &) const;
	/*virtual*/ bool WriteItem(Object *, const KeyType &, int, const ValueType *) const;
	/*virtual*/ bool MapSerialization(
		ItemSerializeFunction,
		BidirectionalSerializer &,
		const Object *) const;

	bool (Object::*mGetter)(const KeyType &, int, ValueType &) const;
	void (Object::*mSetter)(const KeyType &, int, const ValueType &);
	void (Object::*mEraser)(const KeyType &, int);
	void (Object::*mSerializer)(ItemSerializeFunction, BidirectionalSerializer &) const;
};

} }

