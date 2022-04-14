////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/Array.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

///////////////////////////////////////////////////////////////////////////////

template<typename T>
Array<T>::Array()
	: mArray()
{
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
Array<T>::Array(u32 size)
	: mArray(size)
{

}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
Array<T>::Array(const Array<T>& rhs)
	: mArray(rhs.size())
{
	for(u32 i = 0; i < size(); ++i)
		mArray[i] = rhs[i];
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
Array<T>& Array<T>::operator=(const Array<T>& rhs)
{
	mArray.clear();
	mArray.resize(rhs.size());
	for(u32 i = 0; i < size(); ++i)
		mArray[i] = rhs[i];

	return *this;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
T& Array<T>::operator[](size_t index)
{
	return mArray[index];
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
const T& Array<T>::operator[](size_t index) const
{
	return mArray[index];
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
u32 Array<T>::size() const
{
	return mArray.size();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
fixedvector<T,kmax>::fixedvector(size_t iinitialsize)
	: misize( iinitialsize )
{
	OrkAssert( iinitialsize<=kmax );
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
fixedvector<T,kmax>::fixedvector()
	: misize( 0 )
{
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
fixedvector<T,kmax>::fixedvector(const fixedvector<T,kmax>& rhs)
	: misize( rhs.size() )
{
	for(u32 i = 0; i < size(); ++i)
		mArray[i] = rhs.mArray[i];
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
fixedvector<T,kmax>& fixedvector<T,kmax>::operator=(const fixedvector<T,kmax>& rhs)
{
	misize = 0;
	//mArray.clear();
	//mArray.resize(rhs.size());
	for(u32 i = 0; i < size(); ++i)
		mArray[i] = rhs[i];

	return *this;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
T& fixedvector<T,kmax>::operator[](size_t index)
{
	OrkAssert(index>=0);
	OrkAssert(index<misize);
	return mArray[index];
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
const T& fixedvector<T,kmax>::operator[](size_t index) const
{
	OrkAssert(index>=0);
	OrkAssert(index<misize);
	return mArray[index];
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
size_t fixedvector<T,kmax>::size() const
{
	return misize;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
void fixedvector<T,kmax>::push_back( const T& val )
{
	OrkAssert( (misize+1)<=kmax );
	mArray[ misize ] = val;
	misize++;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
void fixedvector<T,kmax>::pop_back()
{
	OrkAssert( misize>0 );
	misize--;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::end()
{
	return iterator(iterator::npos,+1,this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::begin()
{
	int idx = (misize>0) ? 0 : iterator::npos;
	return iterator(idx,+1,this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::const_iterator fixedvector<T,kmax>::end() const
{
	return const_iterator(const_iterator::npos,+1,this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::const_iterator fixedvector<T,kmax>::begin() const
{
	int idx = (misize>0) ? 0 : const_iterator::npos;
	return const_iterator(idx,+1,this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::rbegin()
{
	int idx = (misize>0) ? 0 : iterator::npos;
	return iterator(idx,-1,this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::rend()
{
	return iterator(iterator::npos,-1,this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
void fixedvector<T,kmax>::erase( const typename fixedvector<T,kmax>::const_iterator& it )
{
	OrkAssert(it.mindex != const_iterator::npos);
	OrkAssert(it.mindex < (typename iterator::iter_type)misize);
	OrkAssert( it.mpfixedary == this );

	misize--;

	for( size_t ic=size_t(it.mindex); ic<misize; ic++ )
	{
		mArray[ic] = mArray[ic+1];
	}
	/////////////////////////////////////
	// optional clear the tail item
	/////////////////////////////////////
	if( misize < kmax )
	{
		mArray[misize] = T();
	}
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::insert( const iterator& it, const T& value )
{
	intptr_t index = it.mindex;
	typename iterator::iter_type isize = typename iterator::iter_type(misize);

	OrkAssert( index>=0 );
	OrkAssert( index<isize );
	OrkAssert( (isize+1)<=kmax );

	for( typename iterator::iter_type ic=(isize-1); ic>=index; ic-- )
	{
		mArray[ic+1] = mArray[ic];
	}
	mArray[index] = value;
	misize++;

	typename fixedvector<T,kmax>::iterator ret = it;


	return ret;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
void fixedvector<T,kmax>::clear()
{
	misize = 0;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
void fixedvector<T,kmax>::reserve( size_t icount )
{
	OrkAssert( kmax==icount );
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
void fixedvector<T,kmax>::resize( size_t icount )
{
	misize = icount;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
T& fixedvector<T,kmax>::create()
{
	OrkAssert( (misize+1) < kmax );
	T& rval = mArray[misize++];
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
bool fixedvector<T,kmax>::iterator::operator == (const iterator& oth ) const
{	OrkAssert( oth.mpfixedary == this->mpfixedary );
	OrkAssert( oth.mdirection == this->mdirection );
	if( oth.mindex == this->mindex )
	{	return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
bool fixedvector<T,kmax>::iterator::operator != (const iterator& oth ) const
{	return ! operator == ( oth );
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
T* fixedvector<T,kmax>::iterator::operator ->() const
{
	OrkAssert( mpfixedary != 0 );
	size_t isize = mpfixedary->size();
	OrkAssert( this->mindex >= 0 );
	OrkAssert( this->mindex < iter_type(isize) );
	typename fixedvector<T,kmax>::value_type* p0 =
		(this->mdirection>0) ? &(*mpfixedary)[this->mindex] : &(*mpfixedary)[(isize-1)-this->mindex];
	return p0;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
T& fixedvector<T,kmax>::iterator::operator *() const
{
	return * operator ->();
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::iterator::operator--() // prefix
{
	OrkAssert( mpfixedary );
	size_t isize = mpfixedary->size();
	this->mindex--;
	if( this->mindex < 0 )
	{
		this->mindex = npos;
	}
	return typename fixedvector<T,kmax>::iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::iterator::operator++() // prefix
{
	OrkAssert( mpfixedary );
	iter_type isize = iter_type(mpfixedary->size());
	this->mindex++;
	if( this->mindex >= isize )
	{
		this->mindex = npos;
	}
	return typename fixedvector<T,kmax>::iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::iterator::operator--(int i) // postfix
{
	OrkAssert( mpfixedary );
	iterator temp( *this );
	size_t isize = mpfixedary->size();
	this->mindex--;
	if( this->mindex < 0 )
	{
		this->mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::iterator::operator++(int i) // postfix
{
	OrkAssert( mpfixedary );
	iterator temp( *this );
	iter_type isize = iter_type(mpfixedary->size());
	this->mindex++;
	if( this->mindex >= isize )
	{
		this->mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::iterator::operator+(intptr_t i) const// add
{
	OrkAssert( mpfixedary );
	iterator temp( *this );
	iter_type isize = iter_type(mpfixedary->size());
	temp.mindex+=i;
	if( temp.mindex >= isize )
	{
		temp.mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::iterator::operator-(intptr_t i) const// sub
{
	OrkAssert( mpfixedary );
	iterator temp( *this );
	iter_type isize = iter_type(mpfixedary->size());
	if( temp.mindex >= isize )
	{
		temp.mindex = npos;
	}
	else if( temp.mindex==npos && (i<=isize) )
	{
		temp.mindex = intptr_t(isize)-i;
	}
	else if( temp.mindex < 0 )
	{
		temp.mindex = npos;
	}
	else
	{
		temp.mindex-=i;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::iterator::operator+=(intptr_t i) // add
{
	OrkAssert( mpfixedary );
	iter_type isize = iter_type(mpfixedary->size());
	this->mindex+=i;
	if( this->mindex >= isize )
	{
		this->mindex = npos;
	}
	return typename fixedvector<T,kmax>::iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator fixedvector<T,kmax>::iterator::operator-=(intptr_t i) // sub
{
	OrkAssert( mpfixedary );
	iter_type isize = iter_type(mpfixedary->size());
	this->mindex-=i;
	if( this->mindex >= isize )
	{
		this->mindex = npos;
	}
	if( this->mindex < 0 )
	{
		this->mindex = npos;
	}
	return typename fixedvector<T,kmax>::iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
bool fixedvector<T,kmax>::iterator::operator < ( const iterator& oth ) const
{
	OrkAssert( oth.mpfixedary == mpfixedary );
	OrkAssert( oth.mdirection == this->mdirection );
	return oth.mindex < this->mindex;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
void fixedvector<T,kmax>::iterator::operator = ( const iterator& oth )
{
	mpfixedary = oth.mpfixedary;
	this->mindex = oth.mindex;
	this->mdirection = oth.mdirection;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator_base::difference_type fixedvector<T,kmax>::iterator::operator - ( const iterator& oth ) const
{
	OrkAssert( mpfixedary );
	OrkAssert( oth.mpfixedary );
	OrkAssert( mpfixedary==oth.mpfixedary );
	OrkAssert( oth.mdirection == this->mdirection );
	OrkAssert( this->mindex < int(mpfixedary->size()) );
	OrkAssert( oth.mindex < int(oth.mpfixedary->size()) );
	typename fixedvector<T,kmax>::iterator_base::difference_type defval = (this->mindex-oth.mindex);
	if( this->mindex==npos && oth.mindex>=0 )
	{
		defval = mpfixedary->size()-oth.mindex;
	}
	return defval;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
bool fixedvector<T,kmax>::const_iterator::operator == (const const_iterator& oth ) const
{	OrkAssert( oth.mpfixedary == mpfixedary );
	OrkAssert( oth.mdirection == this->mdirection );
	if( oth.mindex == this->mindex )
	{	return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
bool fixedvector<T,kmax>::const_iterator::operator != (const const_iterator& oth ) const
{	return ! operator == ( oth );
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
const T* fixedvector<T,kmax>::const_iterator::operator ->() const
{
	//printf( "mpfixedary<%p>\n", (void*) mpfixedary );
	fflush(stdout);
	OrkAssert( mpfixedary != 0 );
	iter_type isize = iter_type(mpfixedary->size());
	//printf( "mpfixedary<%p> isize<%d> this->mindex<%d>\n", (void*) mpfixedary, (int) isize, (int) this->mindex );
	fflush(stdout);
	OrkAssert( this->mindex >= 0 );
	OrkAssert( this->mindex < isize );

	const T* p0 = (this->mdirection>0)
				? & mpfixedary->operator[](this->mindex)
				: & mpfixedary->operator[]((isize-1)-this->mindex);

	return p0;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
const T& fixedvector<T,kmax>::const_iterator::operator *() const
{
	return * operator ->();
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::const_iterator fixedvector<T,kmax>::const_iterator::operator++() // prefix
{
	OrkAssert( mpfixedary );
	iter_type isize = iter_type(mpfixedary->size());
	this->mindex++;
	if( this->mindex >= isize )
	{
		this->mindex = npos;
	}
	return typename fixedvector<T,kmax>::const_iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::const_iterator fixedvector<T,kmax>::const_iterator::operator--() // prefix
{
	OrkAssert( mpfixedary );
	size_t isize = mpfixedary->size();
	this->mindex--;
	if( this->mindex < 0 )
	{
		this->mindex = npos;
	}
	return typename fixedvector<T,kmax>::const_iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::const_iterator fixedvector<T,kmax>::const_iterator::operator++(int i) // postfix
{
	OrkAssert( mpfixedary );
	const_iterator temp( *this );
	iter_type isize = iter_type(mpfixedary->size());
	this->mindex++;
	if( this->mindex >= isize )
	{
		this->mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::const_iterator fixedvector<T,kmax>::const_iterator::operator--(int i) // postfix
{
	OrkAssert( mpfixedary );
	const_iterator temp( *this );
	size_t isize = mpfixedary->size();
	this->mindex--;
	if( this->mindex < 0 )
	{
		this->mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::const_iterator fixedvector<T,kmax>::const_iterator::operator+(intptr_t i) const // add
{
	OrkAssert( mpfixedary );
	const_iterator temp( *this );
	iter_type isize = iter_type(temp.mpfixedary->size());
	temp.mindex+=i;
	if( temp.mindex >= isize )
	{
		temp.mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::const_iterator fixedvector<T,kmax>::const_iterator::operator-(intptr_t i) const // add
{
	OrkAssert( mpfixedary );
	const_iterator temp( *this );
	iter_type isize = iter_type(temp.mpfixedary->size());
	temp.mindex-=i;
	if( temp.mindex >= isize )
	{
		temp.mindex = npos;
	}
	if( temp.mindex < 0 )
	{
		temp.mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::const_iterator fixedvector<T,kmax>::const_iterator::operator+=(intptr_t i) // add
{
	OrkAssert( mpfixedary );
	iter_type isize = iter_type(mpfixedary->size());
	this->mindex+=i;
	if( this->mindex >= isize )
	{
		this->mindex = npos;
	}
	return typename fixedvector<T,kmax>::const_iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::const_iterator fixedvector<T,kmax>::const_iterator::operator-=(intptr_t i) // sub
{
	OrkAssert( mpfixedary );
	iter_type isize = iter_type(mpfixedary->size());
	this->mindex-=i;
	if( this->mindex >= isize )
	{
		this->mindex = npos;
	}
	if( this->mindex < 0 )
	{
		this->mindex = npos;
	}
	return typename fixedvector<T,kmax>::const_iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
bool fixedvector<T,kmax>::const_iterator::operator < ( const const_iterator& oth ) const
{
	OrkAssert( oth.mpfixedary == mpfixedary );
	OrkAssert( oth.mdirection == this->mdirection );
	return oth.mindex < this->mindex;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
void fixedvector<T,kmax>::const_iterator::operator = ( const const_iterator& oth )
{
	mpfixedary = oth.mpfixedary;
	this->mindex = oth.mindex;
	this->mdirection = oth.mdirection;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
typename fixedvector<T,kmax>::iterator_base::difference_type fixedvector<T,kmax>::const_iterator::operator - ( const const_iterator& oth ) const
{
	OrkAssert( mpfixedary );
	if( 0 != oth.mpfixedary )
	{
		OrkAssert( mpfixedary==oth.mpfixedary );
	}
	OrkAssert( oth.mdirection == this->mdirection );
	OrkAssert( this->mindex < int(mpfixedary->size()) );
	OrkAssert( oth.mindex < int(oth.mpfixedary->size()) );
	typename fixedvector<T,kmax>::iterator_base::difference_type defval = 0;
	if( this->mindex==npos )
	{
		iter_type icnt = iter_type(mpfixedary->size());
		defval = (icnt-oth.mindex);
		if( defval>icnt ) defval = icnt;
	}
	else if( oth.mindex==npos )
	{
		iter_type icnt = iter_type(mpfixedary->size());
		defval = (this->mindex-icnt);
	}
	else
	{
		defval = (this->mindex-oth.mindex);
	}
	return defval;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
fixedvector<T,kmax>::iterator_base::iterator_base( intptr_t idx, int idir )
	: mindex( idx )
	, mdirection(idir)
{

}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
fixedvector<T,kmax>::iterator::iterator( intptr_t idx, int idir, fixedvector* pfm)
	: iterator_base(idx,idir)
	, mpfixedary(pfm)
{
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
fixedvector<T,kmax>::const_iterator::const_iterator( intptr_t idx, int idir, const fixedvector* pfm)
	: iterator_base(idx,idir)
	, mpfixedary(pfm)
{
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
fixedvector<T,kmax>::const_iterator::const_iterator(const iterator& oth)
	: iterator_base(oth.mindex,oth.mdirection)
	, mpfixedary(oth.mpfixedary)
{
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
fixedvector<T,kmax>::const_iterator::const_iterator(const const_iterator& oth)
	: iterator_base(oth.mindex,oth.mdirection)
	, mpfixedary(oth.mpfixedary)
{
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, int kmax>
fixedvector<T,kmax>::iterator::iterator(const iterator& oth)
	: iterator_base(oth.mindex,oth.mdirection)
	, mpfixedary(oth.mpfixedary)
{
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
