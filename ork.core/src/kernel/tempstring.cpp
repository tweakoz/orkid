///////////////////////////////////////////////////////////////////////////////
// Orkid2
// Copyright 1996-2020, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid2/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <functional>
#include <ork/kernel/tempstring.h>
#include <ork/kernel/tempstring.hpp>
#include <ork/kernel/Array.h>

namespace ork {


size_t FixedStringBase::length() const
{
    return int(strlen( c_str() ));
}

FixedStringBase::FixedStringBase()
    : mLength(0)
{

}

FixedStringBase::HashType FixedStringBase::hash() const
{
    size_t rval = 5381;
    const char* pbas = c_str();
    for( size_t i=0; i<mLength; i++ )
        rval = ((rval << 5) + rval) + size_t(pbas[i]); /* hash * 33 + c */
    return rval;
}


///////////////////////////////////////////////////////////////////////////////

bool const_string::operator==(const const_string &other) const
{
	return strcmp(mpstr,other.mpstr)==0;
}
///////////////////////////////////////////////////////////////////////////////

template class FixedString<4>;
template class FixedString<8>;
template class FixedString<16>;
template class FixedString<24>;
template class FixedString<32>;
template class FixedString<64>;
template class FixedString<96>;
template class FixedString<128>;
template class FixedString<256>;
template class FixedString<1024>;
template class FixedString<2048>;
template class FixedString<4096>;
template class FixedString<8192>;
template class FixedString<16384>;
template class FixedString<65536>;


/*template struct FixedString<16>::iterator;
template struct FixedString<24>::iterator;
template struct FixedString<32>::iterator;
template struct FixedString<64>::iterator;
template struct FixedString<96>::iterator;
template struct FixedString<128>::iterator;
template struct FixedString<256>::iterator;
template struct FixedString<1024>::iterator;

template struct FixedString<16>::const_iterator;
template struct FixedString<24>::const_iterator;
template struct FixedString<32>::const_iterator;
template struct FixedString<64>::const_iterator;
template struct FixedString<96>::const_iterator;
template struct FixedString<128>::const_iterator;
template struct FixedString<256>::const_iterator;
template struct FixedString<1024>::const_iterator;
*/

///////////////////////////////////////////////////////////////////////////////

void Char4::SetCString( const char *str )
{
	muVal32 = 0;
	int islen = int(strlen(str));
	OrkAssert( islen <=4 );
	for( int i=0; i<islen; i++ )
	{	mCharMems[i] = str[i];
	}
}

///////////////////////////////////////////////////////////////////////////////

void Char8::SetCString( const char *str )
{
	muVal64 = 0;
	int islen = int(strlen(str));
	OrkAssert( islen <=8 );
	if( islen>8 ) islen = 8;
	for( int i=0; i<islen; i++ )
	{	mCharMems[i] = str[i];
	}
}

///////////////////////////////////////////////////////////////////////////////

}
