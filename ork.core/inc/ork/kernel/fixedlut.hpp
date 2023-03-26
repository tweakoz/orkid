////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/fixedlut.h>
#include <ork/kernel/Array.hpp>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, int kmax >
typename fixedlut<K,V,kmax>::const_iterator fixedlut<K,V,kmax>::LowerBound( const K &key ) const
{
	value_type ComparePair( key,V() );
	const_iterator WhereIt = std::lower_bound( begin(), end(), ComparePair, LutComparator<K,V>() );
	return WhereIt;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, int kmax >
typename fixedlut<K,V,kmax>::const_iterator fixedlut<K,V,kmax>::UpperBound( const K &key ) const
{
	value_type ComparePair( key,V() );
	const_iterator WhereIt = std::upper_bound( begin(), end(), ComparePair, LutComparator<K,V>() );
	return WhereIt;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, int kmax >
typename fixedlut<K,V,kmax>::iterator fixedlut<K,V,kmax>::LowerBound( const K &key )
{
	value_type ComparePair( key,V() );
	iterator WhereIt = std::lower_bound( begin(), end(), ComparePair, LutComparator<K,V>() );
	return WhereIt;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, int kmax >
typename fixedlut<K,V,kmax>::iterator fixedlut<K,V,kmax>::UpperBound( const K &key )
{
	value_type ComparePair( key,V() );
	iterator WhereIt = std::upper_bound( begin(), end(), ComparePair, LutComparator<K,V>() );
	return WhereIt;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, int kmax >
typename fixedlut<K,V,kmax>::iterator fixedlut<K,V,kmax>::AddSorted( const K &key, const V &val )
{

	value_type InsertPair( key,val );

	OrkAssertI( (find(key)==end()) || (meKeyPolicy==EKEYPOLICY_MULTILUT),
				"lut is a singlekey lut and can not have multiple items with the same key, use EKEYPOLICY_MULTILUT if you want a multilut"
		      );

	iterator WhereIt = UpperBound( key );

	if( WhereIt == end() ) // at the end
	{
		difference_type iter = difference_type(size());
		BaseType::push_back( InsertPair );
		return end()-1;
	}
	else // before WhereIt
	{
		return BaseType::insert( WhereIt, InsertPair );
		//return WhereIt;
	}
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, int kmax >
typename fixedlut<K,V,kmax>::iterator fixedlut<K,V,kmax>::Replace( const K &key, const V &val )
{

	value_type InsertPair( key,val );

	iterator WhereIt = find(key);

	if( WhereIt == end() ) // at the end
	{
		WhereIt = AddSorted(key,val);
	}
	else 
	{
		WhereIt->second = val;
	}
	return WhereIt;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, int kmax >
typename fixedlut<K,V,kmax>::const_iterator fixedlut<K,V,kmax>::find( const K & key ) const
{
	const_iterator it = LowerBound( key ); // it.first >= key

	if( it != end() )
	{
		if( it->first != key ) // key not present
		{
			it = end();
		}
	}

	return it;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, int kmax >
typename fixedlut<K,V,kmax>::iterator fixedlut<K,V,kmax>::find( const K & key )
{
	iterator it = LowerBound( key ); // it.first >= key

	if( it != end() )
	{
		if( it->first != key ) // key not present
		{
			it = end();
		}
	}
	return it;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, int kmax >
void fixedlut<K,V,kmax>::RemoveItem( typename fixedlut<K,V,kmax>::iterator const & it )
{
	BaseType::erase( it );
}

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
