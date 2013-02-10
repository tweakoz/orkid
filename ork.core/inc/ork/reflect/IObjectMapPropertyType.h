////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IObjectMapProperty.h>
#include <ork/reflect/BidirectionalSerializer.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

template<typename KeyType, typename ValueType>
class  IObjectMapPropertyType : public IObjectMapProperty
{
	//DECLARE_TRANSPARENT_TEMPLATE_CASTABLE(DirectObjectPropertyType<T>, IObjectPropertyType<T>)
	DECLARE_TRANSPARENT_TEMPLATE_CASTABLE(IObjectMapPropertyType, IObjectMapProperty)
	//static void GetClassStatic(); // Kill inherited GetClassStatic()

public:
	typedef bool (*ItemSerializeFunction)(BidirectionalSerializer &, KeyType &, ValueType &);
    /*virtual*/ bool DeserializeItem(IDeserializer *value, IDeserializer &key, int, Object *) const;
    /*virtual*/ bool SerializeItem(ISerializer &value, IDeserializer &key, int, const Object *) const;

protected:
	virtual bool GetKey(const Object *, int idx, KeyType &) const = 0;
	virtual bool GetVal(const Object *, const KeyType &k, ValueType &v) const = 0;
	virtual bool ReadItem(const Object *, const KeyType &, int, ValueType &) const = 0;
	virtual bool WriteItem(Object *, const KeyType &, int, const ValueType *) const = 0;
	virtual bool MapSerialization(
		ItemSerializeFunction,
		BidirectionalSerializer &,
		const Object *) const = 0;

	IObjectMapPropertyType() 
		: IObjectMapProperty()
	{
	}

private:
	static bool DoDeserialize(BidirectionalSerializer &, KeyType &, ValueType &);
	static bool DoSerialize(BidirectionalSerializer &, KeyType &, ValueType &);
    /*virtual*/ bool Deserialize(IDeserializer &, Object *) const;
    /*virtual*/ bool Serialize(ISerializer &, const Object *) const;
	//virtual void DelItem( Object *, const PropTypeString& keystring, int imultiindex ) const;
	//virtual void AddItem( Object *, const PropTypeString& keystring, const PropTypeString& valstring ) const;
	//virtual void SetItem( Object *, const PropTypeString& keystring, int imultiindex, const PropTypeString& valstring ) const;
	//virtual void GetItem( const Object *, const PropTypeString& keystring, int idx, PropTypeString& valstring ) const;
};

} }
