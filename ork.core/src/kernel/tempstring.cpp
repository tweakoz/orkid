///////////////////////////////////////////////////////////////////////////////
// Orkid2
// Copyright 1996-2010, Michael T. Mayers
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

FixedStringBase::HashType FixedStringBase::Hash() const
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

bool ork_cstr_replace(	const char *src,
						const char *from,
						const char *to,
						char* dest,
						const size_t idestlen,
						ork_cstr_replace_pred pred
						)
{
    const int nummarkers = 8;

    size_t isrclen = strlen(src);
    size_t ifromlen = strlen(from);
    size_t itolen = strlen(to);

    bool bdone = false;
    bool brval = true;
    const char* src_marker = src;
    const char* src_end = src+isrclen;
    char* dst_marker = dest;

    //src: whatupyodiggittyyoyo
    //from: yo
    //to: damn


    while( false==bdone )
    {
        const char* search = strstr( src_marker, from );

        //printf( "search<%s> src_marker<%s> from<%s> to<%s> src<%s> dest<%s>\n", search, src_marker, from, to, src, dest );
        //search<yodiggittyyoyo> src_marker<whatupyodiggittyyoyo> dest<>
        //search<yoyo> src_marker<diggittyyoyo> dest<whatupdamn>

        /////////////////////////////////////
        // copy [src_marker..search] ->output
        /////////////////////////////////////
        if( (search!=0) && (search >= src_marker) )
        {
            size_t ilen = size_t(search-src_marker);
            OrkAssert( (dst_marker+ilen) <= (dest+idestlen) );
            strncpy( dst_marker, src_marker, ilen );
            dst_marker += ilen;
            src_marker += ilen;
        }
        /////////////////////////////////////
        // copy "to" -> output, advance input by ifromlen
        /////////////////////////////////////
        if( (search!=0) )
        {
            bool doit = true;
            if( pred )
            {
                doit = pred( src, src_marker, isrclen );
            }

            if( doit )
            {
                OrkAssert( (dst_marker+itolen) <= (dest+idestlen) );
                strncpy( dst_marker, to, itolen );
                dst_marker += itolen;
                src_marker += ifromlen;
            }
            else
            {
                OrkAssert( dst_marker<(dest+idestlen) );
                dst_marker[0] = src_marker[0];
                dst_marker ++;
                src_marker ++;
            }

        }

        /////////////////////////////////////
        // copy [mkr..end] -> output
        /////////////////////////////////////
        else
        {
                size_t ilen = isrclen - (src_marker-src);
                strncpy( dst_marker, src_marker, ilen );
                dst_marker += ilen;
                src_marker += ilen;
        }
       bdone = (src_marker>=src_end);
    }

    dst_marker[0] = 0;
    dst_marker++;

    size_t inewlen = size_t(dst_marker-dest);
    OrkAssert( inewlen<idestlen);
    return brval;
}

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
