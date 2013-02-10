////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <ork/util/endian.h>
#include <ork/stream/IOutputStream.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector4.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector2.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/fixedlut.hpp>
#include <ork/kernel/string/PoolString.h>

///
namespace ork { 
PoolString AddPooledString(const PieceString &ps);
PoolString AddPooledLiteral(const ConstString &cs);
PoolString FindPooledString(const PieceString &ps);

namespace chunkfile {
///

template <typename T> void OutputStream::AddItem( const T& data )
{
    T temp = data;
    temp.EndianSwap();
    Write( (unsigned char*) & temp, sizeof(temp) );		
}
///////////////////////////////////////////////////////////////////////////////
template< typename T > void InputStream::GetItem( T &item )
{
    int isize = sizeof( T );
    OrkAssert((midx+isize)<=milength);
    const char *pchbase = (const char*) mpbase;
    T* pt = (T*) & pchbase[ midx ];
    item = *pt;
    midx += isize;
}
///////////////////////////////////////////////////////////////////////////////
template< typename T > void InputStream::RefItem( T* &item )
{
    int isize = sizeof( T );
    int ileft = milength - midx;
    OrkAssert((midx+isize)<=milength);
    const char *pchbase = (const char*) mpbase;
    item = (T*) & pchbase[ midx ];
    midx += isize;
}
///////////////////////////////////////////////////////////////////////////////
template <typename Allocator>
Reader<Allocator>::~Reader()
{
    for( typename StreamLut::const_iterator it=mInputStreams.begin(); it!=mInputStreams.end(); it++ )
    {
        const PoolString& pname = it->first;
        InputStream* stream = it->second;
        if( stream->GetLength() )
        {
            mAllocator.done(pname.c_str(),stream->GetDataAt(0));
        }
    }
}
///////////////////////////////////////////////////////////////////////////////
template <typename Allocator> 
Reader<Allocator>::Reader( const file::Path& inpath, const char* ptype )
    : mpstrtab( 0 )
    , mistrtablen( 0 )
    , mbOk( false )
{
    const Char4 good_chunk_magic("chkf");
    OrkHeapCheck();
    if( CFileEnv::GetRef().DoesFileExist( inpath ) )
    {
        ork::CFile inputfile( inpath, ork::EFM_READ );
        OrkHeapCheck();

        ///////////////////////////
        Char4 bad_chunk_magic;
        inputfile.Read( & bad_chunk_magic, sizeof(bad_chunk_magic) );
        OrkAssert( bad_chunk_magic==good_chunk_magic );
        OrkHeapCheck();
        ///////////////////////////
        inputfile.Read( & mistrtablen, sizeof(mistrtablen) );
        char* pst = new char[ mistrtablen ];
        inputfile.Read( pst, mistrtablen );
        mpstrtab = pst;
        OrkHeapCheck();
        ///////////////////////////
        int32_t ifiletype = 0;
        inputfile.Read( & ifiletype, sizeof(ifiletype) );
        const char* pthistype = mpstrtab+ifiletype;
        OrkAssert( 0 == strcmp(pthistype,ptype) );
        OrkHeapCheck();
        ///////////////////////////
        int32_t inumchunks = 0;
        inputfile.Read( & inumchunks, sizeof(inumchunks) );
        mbOk = true;
        ///////////////////////////
        for( int ic=0; ic<inumchunks; ic++ )
        {
            int32_t ichunkid, ioffset, ichunklen;
            inputfile.Read( & ichunkid, sizeof(ichunkid) );
            inputfile.Read( & ioffset, sizeof(ioffset) );
            inputfile.Read( & ichunklen, sizeof(ichunklen) );
            PoolString psname = AddPooledString( GetString(ichunkid) );
            InputStream* stream = & mStreamBank[ic];
            OrkHeapCheck();
            if( ichunklen )
            {
                void* pdata = mAllocator.alloc( psname.c_str(), ichunklen );
                OrkAssert( pdata != 0 );
                new ( stream ) InputStream( pdata, ichunklen );
                mInputStreams.AddSorted(psname,stream);
            }
            else
            {
                new ( stream ) InputStream( 0, 0 );
                mInputStreams.AddSorted(psname,stream);
            }
            OrkHeapCheck();
        }
        ///////////////////////////
        for( int ic=0; ic<inumchunks; ic++ )
        {
            if( mStreamBank[ic].GetLength() )
            {
                inputfile.Read( mStreamBank[ic].GetDataAt(0), mStreamBank[ic].GetLength() );
            }
        }
        ///////////////////////////
    }
}
////////////////////////////////////////////////////////////////////////////////////
template <typename Allocator> InputStream* Reader<Allocator>::GetStream( const char* streamname )
{
    PoolString ps = ork::AddPooledString(streamname);
    typename StreamLut::const_iterator it=mInputStreams.find(ps);
    return (it==mInputStreams.end()) ? 0 : it->second;
}
////////////////////////////////////////////////////////////////////////////////////
template <typename Allocator> const char* Reader<Allocator>::GetString( int index )
{
    OrkAssert( index<mistrtablen );
    OrkAssert( mpstrtab );
    return mpstrtab+index;
}

///
}}
///
