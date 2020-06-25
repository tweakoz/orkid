////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IObjectMapPropertyType.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

template<typename MapType>
class  DirectMapPropertyType 
	: public IObjectMapPropertyType<typename MapType::key_type, typename MapType::mapped_type>
{
public:
	typedef typename MapType::key_type KeyType;
	typedef typename MapType::mapped_type ValueType;
	typedef typename IObjectMapPropertyType<KeyType, ValueType>::ItemSerializeFunction ItemSerializeFunction;

	DirectMapPropertyType(MapType Object::*);
	
	MapType& GetMap( Object* obj ) const;
	const MapType& GetMap( const Object* obj ) const;

	virtual bool IsMultiMap(const Object* obj) const;

protected:
	/*virtual*/ bool ReadItem(const Object *, const KeyType &, int, ValueType &) const;
	/*virtual*/ bool WriteItem(Object *, const KeyType &, int, const ValueType *) const;
	            bool EraseItem(Object *, const KeyType &, int ) const;
	/*virtual*/ bool MapSerialization(
		ItemSerializeFunction,
		BidirectionalSerializer &,
		const Object *) const;

	/*virtual*/ int GetSize(const Object* obj) const { return int(GetMap(obj).size()); }
	/*virtual*/ bool GetKey(const Object *, int idx, KeyType &) const;
	/*virtual*/ bool GetVal(const Object *, const KeyType &k, ValueType &v) const;

private:
	MapType Object::*mProperty;
};

} }

