///////////////////////////////////////////////////////////////////////////////
// Orkid2
// Copyright 1996-2010, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid2/license.html
///////////////////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/string/MutableString.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

//template <unsigned int kmaxlen>
//const unsigned int FixedString<kmaxlen>::kMAXLEN = kmaxlen;

///////////////////////////////////////////////////////////////////////////////

inline const char* strrstr(const char* s1, const char* s2)
{
    if (*s2 == '\0') return((char *)s1);

    const char* ps1 = s1 + strlen(s1);

    while(ps1 != s1)
    {
        --ps1;

        const char* psc1;
        const char* sc2;
        for( psc1 = ps1, sc2 = s2; ; )
        {
            if (*(psc1++) != *(sc2++))
            {
                break;
            }
            else if (*sc2 == '\0')
            {
                return ((char *)ps1);
            }
        }
    }
    return ((char *)NULL);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::recalclen() 
{
	mLength = length();
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::SetChar( size_t index, char ch )
{
	OrkAssert( index < kmaxlen );
	buffer[ index ] = ch;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::set( const char* pstr )
{
	if( pstr )
	{
		int len = int(strlen( pstr ));
		if( len >= kmaxlen )
		{
			orkprintf( "uhoh, <%s>\n, a string %d chars in length is being put into a FixedString<%d>\n", pstr, len, int(kmaxlen) );
			OrkAssert( false );
		}
		strncpy( buffer, pstr, kmaxlen );
	}
	else
	{
		buffer[0] = 0;
	}
	recalclen();
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::set( const char* pstr, size_t len )
{
	if( pstr )
	{
		if( len >= kmaxlen )
		{
			orkprintf( "uhoh, <%s>\n, a string %d chars in length is being put into a FixedString<%d>\n", pstr, len, int(kmaxlen) );
			OrkAssert( false );
		}
		::strncpy(buffer, pstr, kmaxlen);
	}
	else
	{
		buffer[0] = 0;
	}
	recalclen();
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::append( const char*src, size_t ilen )
{
	size_t icurlen = strlen(buffer);
	OrkAssert( icurlen+ilen < kmaxlen );
	strncpy( & buffer[ icurlen ], src, ilen );
	buffer[ icurlen+ilen ] = 0;
	recalclen();
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::format( const char*fmt, ... )
{
	va_list argp;
	va_start(argp, fmt);
	vsnprintf( & buffer[0], kmaxlen, fmt, argp);
	va_end(argp);
	recalclen();
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::setempty()
{
	buffer[0] = 0;
	buffer[1] = 0;
	mLength = 0;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::operator += ( const FixedStringBase& oth )
{
	FixedString<kmaxlen> tmp = *this;
	this->format( "%s%s", tmp.c_str(), oth.c_str() );
	recalclen();
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::operator += ( const char* oth )
{
	FixedString<kmaxlen> tmp = *this;
	this->format( "%s%s", tmp.c_str(), oth );
	recalclen();
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen> FixedString<kmaxlen>::operator + ( const FixedString& oth ) const
{
	FixedString<kmaxlen> tmp;
	tmp.format( "%s%s", c_str(), oth.c_str() );
	//recalclen();
	return tmp;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::operator = ( const FixedString& oth )
{
	strcpy( buffer, oth.buffer );
	recalclen();
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::operator == ( const FixedString& oth ) const
{
	return (0 == strcmp( c_str(), oth.c_str() ));
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::operator == ( const char* oth ) const
{
	return (0 == strcmp( c_str(), oth ));
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::operator < ( const FixedString& oth ) const
{
	return (0 > strcmp( c_str(), oth.c_str() ));
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::operator != ( const FixedString& oth ) const
{
	return (0 != strcmp( c_str(), oth.c_str() ));
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::empty() const
{
	return length() == 0;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen>::FixedString()
{
	setempty();
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen>::FixedString(const char*pstr)
{
	set( pstr );
	recalclen();
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen>::FixedString(const char* pstr, size_t len)
{
	set( pstr, len );
	recalclen();
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::replace( const char* src, const char* from, const char* to, ork_cstr_replace_pred pred )
{
	bool bok = ork_cstr_replace( src, from, to, buffer, kmaxlen,pred);
	recalclen();
	return bok;
}

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::replace_in_place( const char* from, const char* to, ork_cstr_replace_pred pred )
{
	FixedString<kmaxlen> tmp;
	bool bok = tmp.replace(this->c_str(), from,to, pred );
	if( bok )
		this->set(tmp.c_str());
	return bok;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::replace( const char* src, const char from, const char to, ork_cstr_replace_pred pred )
{
	size_t ilen = strlen(src);
	OrkAssert(ilen<kmaxlen);
	for( size_t i=0; i<ilen; i++ )
	{
		char ch = src[i];
		if( ch == from ) ch = to;
		buffer[i] = ch;
	}
	recalclen();
	return true;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
int FixedString<kmaxlen>::FastStrCmp( const FixedString& a, const FixedString& b )
{
	return strcmp( a.c_str(), b.c_str() );
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen>::operator MutableString()
{
	return MutableString(&buffer[0], kmaxlen-1, mLength);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
char FixedString<kmaxlen>::operator[] ( const size_type& i ) const
{
	OrkAssert( i < size_type(mLength) );
	OrkAssert( i < kmaxlen );

	return buffer[i];
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
char& FixedString<kmaxlen>::operator[] ( const size_type& i )
{
	OrkAssert( i < size_type(mLength) );
	OrkAssert( i < kmaxlen );

	return buffer[i];
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen> FixedString<kmaxlen>::substr( size_type first, size_type length ) const
{
	FixedString ret;
	size_type last = ( length==npos ) ? this->length() : (first+length);

	size_type count = (last-first);
	if( count > 0 )
	{
		OrkAssert( count < size_type(kmaxlen) );
		strncpy( ret.buffer, c_str()+first, count );
		ret.SetChar(count,0);
	}
	ret.recalclen();
	return ret;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::size_type FixedString<kmaxlen>::cue_to_char( char cch, size_t start ) const
{	size_type slen = size();
	bool found = false;
	size_type idx = size_type(start);
	size_type rval = npos;
	for(size_type i = idx; ((i<slen)&&(found==false)); i++)
	{
		if(cch == buffer[i])
		{
			found = true;
			rval = i;
		}
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::size_type FixedString<kmaxlen>::find_first_of( const char* srch ) const
{
	size_type rval = npos;

	const char* pfound = strstr( buffer, srch );
	
	if( pfound )
	{
		rval = (pfound-buffer);
	}

	return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::size_type FixedString<kmaxlen>::find_last_of( const char* srch ) const
{
	size_type rval = npos;

	const char* pfound = strrstr( buffer, srch );
	
	if( pfound )
	{
		rval = (pfound-buffer);
	}

	return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::size_type FixedString<kmaxlen>::find( const char* srch, size_t pos ) const
{
	size_type rval = npos;

	const char* pfound = strstr( buffer+pos, srch );
	
	if( pfound )
	{
		rval = (pfound-buffer);
	}

	return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::size_type FixedString<kmaxlen>::find( const FixedString& s, size_t pos ) const
{
	return find( s.c_str(), pos );
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen>& FixedString<kmaxlen>::replace( size_type pos1, size_type n1, const FixedString& str )
{

	size_t inlen = str.length();
	size_t iolen = n1;
	intptr_t isizediff = iolen-inlen;

	OrkAssert( pos1+inlen < kmaxlen );
	memcpy( buffer+pos1, str.c_str(), inlen );

	if( isizediff > 0 )
	{
		intptr_t idst = pos1+inlen;
		intptr_t isrc = pos1+inlen+isizediff;
		intptr_t ilen = (mLength-isrc)+1;

		OrkAssert( idst+ilen < kmaxlen );
		memcpy( buffer+idst, c_str()+isrc, ilen );
	}

	recalclen();
	
	return *this;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator FixedString<kmaxlen>::begin()
{
	size_t idx = (size()>0) ? 0 : npos;
	return iterator(idx,+1,this);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator FixedString<kmaxlen>::end()
{
	return iterator(npos,+1,this);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator FixedString<kmaxlen>::begin() const
{
	size_t idx = (size()>0) ? 0 : npos;
	return const_iterator(idx,+1,this);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator FixedString<kmaxlen>::end() const
{
	return const_iterator(npos,+1,this);
}

///////////////////////////////////////////////////////////////////////////////
// FixedString<kmaxlen>::iterator_base
///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen>::iterator_base::iterator_base( size_t idx, int idir )
	: mindex( idx )
	, mdirection(idir)
{

}

///////////////////////////////////////////////////////////////////////////////
// FixedString<kmaxlen>::iterator
///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen>::iterator::iterator( size_t idx, int idir, FixedString* pfm)
	: mIteratorBase(idx,idir)
	, mpString(pfm)
{
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen>::iterator::iterator(const iterator& oth)
	: mIteratorBase(oth.mIteratorBase.mindex,oth.mIteratorBase.mdirection)
	, mpString(oth.mpString)
{
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::iterator::operator==(const iterator& oth ) const
{	if( oth.mpString != mpString )
		return false;
	if( oth.mIteratorBase.mdirection != mIteratorBase.mdirection  )
		return false;
	if( oth.mIteratorBase.mindex == mIteratorBase.mindex )
	{	return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::iterator::operator!=(const iterator& oth ) const
{	return ! operator == ( oth );
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator::pointer FixedString<kmaxlen>::iterator::operator->() const
{
	OrkAssert( mpString != 0 );
	size_t isize = mpString->size();
	OrkAssert( mIteratorBase.mindex >= 0 );
	OrkAssert( mIteratorBase.mindex < isize );
	OrkAssert( mIteratorBase.mindex < kmaxlen );
	typename FixedString<kmaxlen>::iterator::value_type* p0 = 
		(mIteratorBase.mdirection>0) ? &mpString->buffer[mIteratorBase.mindex] : &mpString->buffer[(isize-1)-mIteratorBase.mindex];
	return p0;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator::reference FixedString<kmaxlen>::iterator::operator *() const
{
	return *(this->operator->());
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator FixedString<kmaxlen>::iterator::operator--() // prefix
{
	OrkAssert( mpString );
	size_t isize = mpString->size();
	mIteratorBase.mindex--;
	if( mIteratorBase.mindex >= isize )
	{
		mIteratorBase.mindex = npos;
	}
	return typename FixedString<kmaxlen>::iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator FixedString<kmaxlen>::iterator::operator++() // prefix
{
	OrkAssert( mpString );
	size_t isize = mpString->size();
	mIteratorBase.mindex++;
	if( mIteratorBase.mindex >= isize )
	{
		mIteratorBase.mindex = npos;
	}
	return typename FixedString<kmaxlen>::iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator FixedString<kmaxlen>::iterator::operator--(int i) // postfix
{
	OrkAssert( mpString );
	iterator temp( *this );
	size_t isize = mpString->size();
	mIteratorBase.mindex--;
	if( mIteratorBase.mindex >= isize )
	{
		mIteratorBase.mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator FixedString<kmaxlen>::iterator::operator++(int i) // postfix
{
	OrkAssert( mpString );
	iterator temp( *this );
	size_t isize = mpString->size();
	mIteratorBase.mindex++;
	if( mIteratorBase.mindex >= isize )
	{
		mIteratorBase.mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator FixedString<kmaxlen>::iterator::operator+(int i) const// add
{
	OrkAssert( mpString );
	iterator temp( *this );
	size_t isize = mpString->size();
	temp.mIteratorBase.mindex+=i;
	if( temp.mIteratorBase.mindex >= isize )
	{
		temp.mIteratorBase.mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator FixedString<kmaxlen>::iterator::operator-(int i) const// sub
{
	OrkAssert( mpString );
	iterator temp( *this );
	size_t isize = mpString->size();
	temp.mIteratorBase.mindex-=i;
	if( temp.mIteratorBase.mindex >= isize )
	{
		temp.mIteratorBase.mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator FixedString<kmaxlen>::iterator::operator+=(int i) // add
{
	OrkAssert( mpString );
	size_t isize = mpString->size();
	mIteratorBase.mindex+=i;
	if( mIteratorBase.mindex >= isize )
	{
		mIteratorBase.mindex = npos;
	}
	return typename FixedString<kmaxlen>::iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator FixedString<kmaxlen>::iterator::operator-=(int i) // sub
{
	OrkAssert( mpString );
	size_t isize = mpString->size();
	mIteratorBase.mindex-=i;
	if( mIteratorBase.mindex >= isize )
	{
		mIteratorBase.mindex = npos;
	}
	return typename FixedString<kmaxlen>::iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::iterator::operator < ( const iterator& oth ) const
{
	OrkAssert( oth.mpString == mpString );
	OrkAssert( oth.mIteratorBase.mdirection == mIteratorBase.mdirection );

	bool othNPOS = ( oth.mIteratorBase.mindex == npos );
	bool thsNPOS = ( mIteratorBase.mindex == npos );

	int index = int(othNPOS)+(int(thsNPOS)<<1);
	bool btable[4] = 
	{
		oth.mIteratorBase.mindex < mIteratorBase.mindex,// 0==neither
		true, // 1==othNPOS
		false, // 2==thsNPOS
		false // 3==BOTH
	};

	return btable[index];
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::iterator::operator = ( const iterator& oth )
{
	mpString = oth.mpString;
	mIteratorBase.mindex = oth.mIteratorBase.mindex;
	mIteratorBase.mdirection = oth.mIteratorBase.mdirection;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::iterator::difference_type FixedString<kmaxlen>::iterator::operator - ( const iterator& oth ) const
{
	OrkAssert( mpString );
	OrkAssert( oth.mpString );
	OrkAssert( mpString==oth.mpString );
	OrkAssert( oth.mIteratorBase.mdirection == mIteratorBase.mdirection );
	OrkAssert( mIteratorBase.mindex < mpString->size() || mIteratorBase.mindex==npos );
	OrkAssert( oth.mIteratorBase.mindex < oth.mpString->size() || oth.mIteratorBase.mindex==npos );
	typename FixedString<kmaxlen>::iterator::difference_type defval = (mIteratorBase.mindex-oth.mIteratorBase.mindex);
	size_t othsize = oth.mpString->size();
	if( mIteratorBase.mindex==npos && oth.mIteratorBase.mindex<othsize )
	{
		defval = mpString->size()-oth.mIteratorBase.mindex;
	}
	return defval;
}

///////////////////////////////////////////////////////////////////////////////
// FixedString<kmaxlen>::const_iterator
///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen>::const_iterator::const_iterator( size_t idx, int idir, const FixedString* pfm)
	: mIteratorBase(idx,idir)
	, mpString(pfm)
{
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen>::const_iterator::const_iterator(const iterator& oth)
	: mIteratorBase(oth.mIteratorBase.mindex,oth.mIteratorBase.mdirection)
	, mpString(oth.mpString)
{
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
FixedString<kmaxlen>::const_iterator::const_iterator(const const_iterator& oth)
	: mIteratorBase(oth.mIteratorBase.mindex,oth.mIteratorBase.mdirection)
	, mpString(oth.mpString)
{
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::const_iterator::operator == (const const_iterator& oth ) const
{	OrkAssert( oth.mpString == mpString );
	OrkAssert( oth.mIteratorBase.mdirection == mIteratorBase.mdirection );
	if( oth.mIteratorBase.mindex == mIteratorBase.mindex )
	{	return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::const_iterator::operator != (const const_iterator& oth ) const
{	return ! operator == ( oth );
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator::const_pointer FixedString<kmaxlen>::const_iterator::operator ->() const
{
	OrkAssert( mpString != 0 );
	size_t isize = mpString->size();
	OrkAssert( mIteratorBase.mindex >= 0 );
	OrkAssert( mIteratorBase.mindex < isize );
	const typename FixedString<kmaxlen>::iterator::value_type* p0 = 
		(mIteratorBase.mdirection>0) ? &mpString->c_str()[mIteratorBase.mindex] : &mpString->c_str()[(isize-1)-mIteratorBase.mindex];
	return p0;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator::const_reference FixedString<kmaxlen>::const_iterator::operator *() const
{
	return *(this->operator->());
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator FixedString<kmaxlen>::const_iterator::operator++() // prefix
{
	OrkAssert( mpString );
	size_t isize = mpString->size();
	mIteratorBase.mindex++;
	if( mIteratorBase.mindex >= isize )
	{
		mIteratorBase.mindex = npos;
	}
	return typename FixedString<kmaxlen>::const_iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator FixedString<kmaxlen>::const_iterator::operator--() // prefix
{
	OrkAssert( mpString );
	size_t isize = mpString->size();
	mIteratorBase.mindex--;
	if( mIteratorBase.mindex >= isize )
	{
		mIteratorBase.mindex = npos;
	}
	return typename FixedString<kmaxlen>::const_iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator FixedString<kmaxlen>::const_iterator::operator++(int i) // postfix
{
	OrkAssert( mpString );
	const_iterator temp( *this );
	size_t isize = mpString->size();
	mIteratorBase.mindex++;
	if( mIteratorBase.mindex >= isize )
	{
		mIteratorBase.mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator FixedString<kmaxlen>::const_iterator::operator--(int i) // postfix
{
	OrkAssert( mpString );
	const_iterator temp( *this );
	size_t isize = mpString->size();
	mIteratorBase.mindex--;
	if( mIteratorBase.mindex >= isize )
	{
		mIteratorBase.mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator FixedString<kmaxlen>::const_iterator::operator+(int i) const // add
{
	OrkAssert( mpString );
	const_iterator temp( *this );
	size_t isize = temp.mpString->size();
	temp.mIteratorBase.mindex+=i;
	if( temp.mIteratorBase.mindex >= isize )
	{
		temp.mIteratorBase.mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator FixedString<kmaxlen>::const_iterator::operator-(int i) const // add
{
	OrkAssert( mpString );
	const_iterator temp( *this );
	size_t isize = temp.mpString->size();
	temp.mIteratorBase.mindex-=i;
	if( temp.mIteratorBase.mindex >= isize )
	{
		temp.mIteratorBase.mindex = npos;
	}
	return temp;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator FixedString<kmaxlen>::const_iterator::operator+=(int i) // add
{
	OrkAssert( mpString );
	size_t isize = mpString->size();
	mIteratorBase.mindex+=i;
	if( mIteratorBase.mindex >= isize )
	{
		mIteratorBase.mindex = npos;
	}
	return typename FixedString<kmaxlen>::const_iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator FixedString<kmaxlen>::const_iterator::operator-=(int i) // sub
{
	OrkAssert( mpString );
	size_t isize = mpString->size();
	mIteratorBase.mindex-=i;
	if( mIteratorBase.mindex >= isize )
	{
		mIteratorBase.mindex = npos;
	}
	return typename FixedString<kmaxlen>::const_iterator(*this);
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
bool FixedString<kmaxlen>::const_iterator::operator < ( const const_iterator& oth ) const
{
	OrkAssert( oth.mpString == mpString );
	OrkAssert( oth.mIteratorBase.mdirection == mIteratorBase.mdirection );

	bool othNPOS = ( oth.mIteratorBase.mindex == npos );
	bool thsNPOS = ( mIteratorBase.mindex == npos );

	int index = int(othNPOS)+(int(thsNPOS)<<1);
	bool btable[4] = 
	{
		oth.mIteratorBase.mindex < mIteratorBase.mindex,// 0==neither
		true, // 1==othNPOS
		false, // 2==thsNPOS
		false // 3==BOTH
	};

	return btable[index];
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::const_iterator::operator = ( const const_iterator& oth )
{
	mpString = oth.mpString;
	mIteratorBase.mindex = oth.mIteratorBase.mindex;
	mIteratorBase.mdirection = oth.mIteratorBase.mdirection;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
typename FixedString<kmaxlen>::const_iterator::difference_type FixedString<kmaxlen>::const_iterator::operator - ( const typename FixedString<kmaxlen>::const_iterator& oth ) const
{
	OrkAssert( mpString );
	if( 0 != oth.mpString )
	{
		OrkAssert( mpString==oth.mpString );
	}
	OrkAssert( oth.mIteratorBase.mdirection == mIteratorBase.mdirection );
	OrkAssert( mIteratorBase.mindex < mpString->size() || mIteratorBase.mindex == npos );
	OrkAssert( oth.mIteratorBase.mindex < oth.mpString->size() || oth.mIteratorBase.mindex == npos );
	typename FixedString<kmaxlen>::const_iterator::difference_type defval = (mIteratorBase.mindex-oth.mIteratorBase.mindex);
	return defval;
}

///////////////////////////////////////////////////////////////////////////////

template <unsigned int kmaxlen>
void FixedString<kmaxlen>::resize( size_t n, char c )
{
	OrkAssert( n>=0 );
	OrkAssert( n<(kmaxlen-2) );

	if( n > mLength )
	{	for( size_t i=mLength; i<n; i++ )
		{
			buffer[i+1] = c;			
		}
		buffer[n+1] = 0;
		mLength = n;
	}
	else if( n < mLength )
	{
		buffer[n+1] = 0;
		mLength = n;
	}
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
