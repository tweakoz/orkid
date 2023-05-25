////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/orkvector.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

///////////////////////////////////////////////////////////////////////////////

template<typename T>
class Array
{
public:

	Array();
	explicit Array(u32 size);
	Array(const Array& rhs);
	Array& operator=(const Array& rhs);

	T& operator[](size_t index);
	const T& operator[](size_t index) const;
	u32 size() const;

private:
	orkvector<T> mArray;
};

/// ////////////////////////////////////////////////////////////////////////////
/// tweaks fixedvector class
/// fixed capacity, variable sized stl compatible vector
/// It will only use memory that you explicitly give it.
/// If you instantiate one on the stack, it will only use stack memory
/// If you instantiate one on the heap, it will only use heap memory
/// Think of it as a bounds checked variable sized array
/// ////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
class fixedvector
{
public:

	typedef T value_type;
	typedef size_t size_type;
	typedef intptr_t difference_type;

	struct iterator_base
	{
		typedef std::random_access_iterator_tag iterator_category;
		typedef T value_type;
		typedef T* pointer;
		typedef const T* const_pointer;
		typedef T&       reference;
		typedef const T& const_reference;
		typedef std::ptrdiff_t difference_type;

		intptr_t mindex = 0;
		int mdirection = 0;

		iterator_base( intptr_t idx=-1, int idir=0 );
	};

	struct iterator : public iterator_base
	{
		typedef intptr_t iter_type;
		static const iter_type npos = -1;

		fixedvector*	mpfixedary = nullptr;
		iterator( intptr_t idx=npos, int idir=0, fixedvector* pfm=0);
		iterator(const iterator& oth);
		void operator = ( const iterator& oth );
		bool operator == (const iterator& oth ) const;
		bool operator != (const iterator& oth ) const;
		T* operator ->() const;
		T& operator *() const;
		iterator operator++();	// prefix
		iterator operator--(); // prefix
		iterator operator++(int i);	// postfix
		iterator operator--(int i); // prefix
		iterator operator+(intptr_t i) const;	// add
		iterator operator-(intptr_t i) const;	// sub
		iterator operator+=(intptr_t i);	// add
		iterator operator-=(intptr_t i);	// sub
		bool operator < ( const iterator& oth ) const;
		typename iterator_base::difference_type operator - ( const iterator& oth ) const;
	};
	struct const_iterator : public iterator_base
	{
		typedef intptr_t iter_type;
		static const iter_type npos = -1;

		const fixedvector*	mpfixedary = nullptr;
		const_iterator( intptr_t idx=npos, int idir=0, const fixedvector* pfm=0);
		const_iterator(const iterator& oth);
		const_iterator(const const_iterator& oth);
		void operator = ( const const_iterator& oth );
		bool operator == (const const_iterator& oth ) const;
		bool operator != (const const_iterator& oth ) const;
		const T* operator ->() const;
		const T& operator *() const;
		const_iterator operator++(); // prefix
		const_iterator operator--(); // prefix
		const_iterator operator++(int i); // postfix
		const_iterator operator--(int i); // prefix
		const_iterator operator+(intptr_t i) const;	// add
		const_iterator operator-(intptr_t i) const;	// sub
		const_iterator operator+=(intptr_t i);	// add
		const_iterator operator-=(intptr_t i);	// sub
		bool operator < ( const const_iterator& oth ) const;
		typename iterator_base::difference_type operator - ( const const_iterator& oth ) const;
	};

	fixedvector(size_t iinitialsize);
	fixedvector();
	fixedvector(const fixedvector& rhs);
	fixedvector& operator=(const fixedvector& rhs);

	T& operator[](size_t index);
	const T& operator[](size_t index) const;
	size_t size() const;
	void push_back( const T& val );
	void pop_back();
	const T* data() const { return mArray; }
    T* data() { return mArray; }
	iterator end();
	iterator begin();
	iterator rbegin();
	iterator rend();

	const_iterator begin() const;
	const_iterator end() const;

	iterator insert( const iterator& it, const T& value );
	void erase( const const_iterator& it );
	void reserve( size_t icount );
	void resize( size_t icount );

	void clear();

	T& create();

private:
	T mArray[kmax];
	size_t misize = 0;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
