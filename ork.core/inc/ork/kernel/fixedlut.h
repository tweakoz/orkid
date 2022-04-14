////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


// Fixed capacity Look Up Table

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <utility>
#include <ork/orkstl.h>
#include <ork/kernel/orkalloc.h>
#include <ork/kernel/Array.h>
#include <ork/kernel/orklut.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork{
///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, int kmax >
class fixedlut : private fixedvector< std::pair<K,V>, kmax >
{
	EOrkLutKeyPolicy meKeyPolicy;

public:

	static const int kimax = kmax;

	typedef fixedvector< std::pair<K,V>, kmax > BaseType;

	fixedlut(EOrkLutKeyPolicy ekeypolicy = EKEYPOLICY_LUT) : fixedvector< std::pair<K,V>, kmax >(), meKeyPolicy(ekeypolicy) {}

	typedef typename BaseType::iterator iterator;
	typedef typename BaseType::const_iterator const_iterator;

	typedef K key_type;
	typedef V mapped_type;
	typedef typename BaseType::value_type value_type;
	typedef typename BaseType::size_type size_type;
	typedef typename BaseType::difference_type difference_type;

	// ork::_vector_iterator_base<Data,Traverse,Const> ork::orklut<K,V>::BinarySearch(const K &,ork::_vector_iterator_base<Data,Traverse,Const> &) const

	EOrkLutKeyPolicy GetKeyPolicy() const { return meKeyPolicy; }
	void SetKeyPolicy(EOrkLutKeyPolicy policy) { meKeyPolicy = policy; }

	//iterator BinarySearchNonConst( const K & Testkey, iterator & insertion_point );
	//const_iterator BinarySearch( const K & Testkey, const_iterator & insertion_point ) const;
	const_iterator LowerBound( const K & Testkey ) const;
	const_iterator UpperBound( const K & Testkey ) const;
	iterator LowerBound( const K & Testkey );
	iterator UpperBound( const K & Testkey );

	using BaseType::begin;
	using BaseType::end;
	using BaseType::rbegin;
	using BaseType::rend;
	using BaseType::clear;
	using BaseType::size;
	using BaseType::reserve;

	iterator AddSorted( const K & key, const V & val );
	iterator Replace( const K & key, const V & val );
	void insert( const value_type &kv) { AddSorted(kv.first, kv.second); };
	const_iterator find( const K & key ) const;
	iterator find( const K & key ); 
	void RemoveItem( const iterator & it );
	void erase( const iterator & it ) { return RemoveItem(it); }

	const value_type &GetItemAtIndex( int idx ) const { return (*this)[size_type(idx)]; }
	value_type &RefIndex( int idx ) { return (*this)[size_type(idx)]; }
};

///////////////////////////////////////////////////////////////////////////////
};
///////////////////////////////////////////////////////////////////////////////
