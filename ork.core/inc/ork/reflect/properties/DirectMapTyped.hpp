////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/properties/DirectMapTyped.h>
#include <ork/reflect/IObjectMapPropertyType.hpp>
#include <ork/kernel/core_interface.h>

namespace ork { namespace reflect {

template < typename kt, typename vt >
bool IsMultiMapDeducer( const std::map<kt,vt>& map ) { return false; }

template < typename kt, typename vt >
bool IsMultiMapDeducer( const std::multimap<kt,vt>& map ) { return true; }

template < typename kt, typename vt >
bool IsMultiMapDeducer( const ork::orklut<kt,vt>& map )
{
	return map.GetKeyPolicy()==ork::EKEYPOLICY_MULTILUT;
}

template<typename MapType>
bool DirectMapPropertyType<MapType>::IsMultiMap(const Object* obj) const
{
	return IsMultiMapDeducer(GetMap(obj));
}

template<typename MapType>
DirectMapPropertyType<MapType>::DirectMapPropertyType( MapType Object::*prop )
	: mProperty(prop)
{}

template<typename MapType>
MapType& DirectMapPropertyType<MapType>::GetMap( Object* object ) const
{
	return object->*mProperty;
}

template<typename MapType>
const MapType& DirectMapPropertyType<MapType>::GetMap( const Object* object ) const
{
	return object->*mProperty;
}

template<typename MapType>
bool DirectMapPropertyType<MapType>::EraseItem(
	Object *object,
	const typename DirectMapPropertyType<MapType>::KeyType &key,
	int multi_index ) const
{
	MapType &map = object->*mProperty;
	typename MapType::iterator it = map.find(key);

	if( it != map.end() )
	{
		while(multi_index > 0)
		{
			it++;
			multi_index--;
		}

		OrkAssert( it != map.end() );
		OrkAssert( (*it).first == key );

		map.erase( it );
	}

	return true;
}

template<typename MapType>
bool DirectMapPropertyType<MapType>::ReadItem(
	const Object *object,
	const typename DirectMapPropertyType<MapType>::KeyType &key,
	int multi_index,
	typename DirectMapPropertyType<MapType>::ValueType &value) const
{
	const MapType &map = object->*mProperty;
	typename MapType::const_iterator it = map.find(key);

	if( it==map.end() ) return false;

	while(multi_index > 0)
	{
		it++;
		multi_index--;
	}

	value = it->second;

	return true;
}

template<typename MapType>
bool DirectMapPropertyType<MapType>::WriteItem(
	Object *object,
	const typename DirectMapPropertyType<MapType>::KeyType &key,
	int multi_index,
	const typename DirectMapPropertyType<MapType>::ValueType *value) const
{	MapType &map = object->*mProperty;
	const int orig_multi_index = multi_index;
	if(multi_index == IObjectMapProperty::kDeserializeInsertItem)
	{	OrkAssert(value);
		map.insert(std::make_pair(key, *value)); 
	}
	else 
	{	typename MapType::iterator it = map.find(key);
		while(multi_index > 0)
		{	it++;
			multi_index--;
		}
		if(value)
		{	it->second = *value;
		}
		else
		{	typename DirectMapPropertyType<MapType>::ValueType val2erase = it->second;
			ItemRemovalEvent ev;
			ev.mProperty = this;
			ev.miMultiIndex = orig_multi_index;
			ev.mKey.Set(key);
			ev.mOldValue.Set(val2erase);
			ork::Object* pevl = static_cast<ork::Object*>( object );
			pevl->Notify(&ev);
			map.erase(it);
		}
	}
	return true;
}

template<typename MapType>
bool DirectMapPropertyType<MapType>::MapSerialization(
		typename DirectMapPropertyType<MapType>::ItemSerializeFunction serialization_func,
		BidirectionalSerializer &bidi,
		const Object *serialize_object) const
{

	if(bidi.Serializing())
	{
		const MapType &map = serialize_object->*mProperty;

		//const KeyType *last_key = NULL;
	
		typename MapType::const_iterator itprev;

		int imultiindex = 0;

		for(typename MapType::const_iterator it = map.begin(); it != map.end(); it++)
		{
			KeyType key = it->first;

			
			if( it != map.begin() )
			{
				const KeyType& ka = itprev->first;
				const KeyType& kb = it->first;
				
				if( ka==kb )
				{
					imultiindex++;
				}
				else
				{
					imultiindex=0;
				}
			}

			itprev = it;

			///////////////////////////////////////////////////
			// multi index hint
			///////////////////////////////////////////////////

			bidi.Serializer()->Hint( "MultiIndex", imultiindex );

			///////////////////////////////////////////////////

			ValueType value = it->second;

			(*serialization_func)(bidi, key, value);
		}
	}
	
	return true;
}

template<typename MapType>
bool DirectMapPropertyType<MapType>::GetKey(const Object *pser, int idx, KeyType &kt) const
{
	const MapType &map = pser->*mProperty;
	OrkAssert( idx < int(map.size()) );
	typename MapType::const_iterator it = map.begin();
	for( int i=0; i<idx; i++ ) it++;
	kt = (*it).first;
	return true;
}
template<typename MapType>
bool DirectMapPropertyType<MapType>::GetVal(const Object *pser, const KeyType &k, ValueType &v) const
{
	const MapType &map = pser->*mProperty;
	typename MapType::const_iterator it = map.find(k);
	if( it == map.end() ) return false;
	v = (*it).second;
	return true;
}


} }
