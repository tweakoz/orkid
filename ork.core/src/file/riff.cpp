////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/file/file.h>
#if 1

#include <ork/file/riff.h>
#include <ork/util/endian.h>

using ork::RIFFChunk;
using ork::RIFFFile;
using ork::RiffChunk2;
using ork::RiffFile2;

///////////////////////////////////////////////////////////////////////////////

U32 RIFFChunk::ChunkName( U8 a, U8 b, U8 c, U8 d )
{
	U32 uval = ((static_cast<U32>(a)&255))
				+ ((static_cast<U32>(b)&255)<<8)
				+ ((static_cast<U32>(c)&255)<<16)
				+ ((static_cast<U32>(d)&255)<<24);
	
#if defined( _OSX )
	RIFFFile::SwapBytes( reinterpret_cast<u8*>( & uval ), 4 );
#endif

	return uval;
}

///////////////////////////////////////////////////////////////////////////////

U32 RIFFChunk::ChunkName( const char *pCHKNAM )
{
	return ChunkName((unsigned char)pCHKNAM[0], (unsigned char)pCHKNAM[1], (unsigned char)pCHKNAM[2], (unsigned char)pCHKNAM[3]);
}

///////////////////////////////////////////////////////////////////////////////

U32 RIFFChunk::GetChunkLen( U32 val )
{
	U8 ch0, ch1, ch2, ch3;
	
	ch3 = (U8)(((U32)val&0xff000000)>>24);
	ch2 = (U8)(((U32)val&0x00ff0000)>>16);
	ch1 = (U8)(((U32)val&0x0000ff00)>>8);
	ch0 = (U8)(((U32)val&0x000000ff));
	U32 outval = U32((ch3<<24)+(ch2<<16)+(ch1<<8)+ch0);
	#if defined( _OSX )
	RIFFFile::SwapBytes( reinterpret_cast<u8*>( & outval ), 4 );
	#endif
	return outval;
}

////////////////////////////////////////////////////////////////////////////////

void RIFFChunk::PrintChunkID( U32 val )
{
	#if defined( _OSX )
	RIFFFile::SwapBytes( reinterpret_cast<u8*>( & val ), 4 );
	#endif
	U8 ch0, ch1, ch2, ch3;
	ch3 = (U8)(((U32)val&0xff000000)>>24);
	ch2 = (U8)(((U32)val&0x00ff0000)>>16);
	ch1 = (U8)(((U32)val&0x0000ff00)>>8);
	ch0 = (U8)(((U32)val&0x000000ff));
	orkmessageh( "chunkID: <%08x> <%c%c%c%c>\n", val, ch0, ch1, ch2, ch3 );
}
///////////////////////////////////////////////////////////////////////////////

U32 RIFFChunk::GetU32( U32 offset )
{
	U32 rval = 0;
	U8 *u8ptr = & chunkdata[ offset ];
	rval = U32(u8ptr[3]<<24);
	rval |= (u8ptr[2]<<16);
	rval |= (u8ptr[1]<<8);
	rval |= u8ptr[0];
	#if defined( _OSX )
	RIFFFile::SwapBytes( reinterpret_cast<u8*>( & rval ), 4 );
	#endif
	return rval;
}

////////////////////////////////////////////////////////////////////////////////

void RIFFChunk::DumpHeader( void )
{	PrintChunkID( chunkID );
	U8 ch0, ch1, ch2, ch3;
	ch3 = (U8)(((U32)chunklen&0xff000000)>>24);
	ch2 = (U8)(((U32)chunklen&0x00ff0000)>>16);
	ch1 = (U8)(((U32)chunklen&0x0000ff00)>>8);
	ch0 = (U8)(((U32)chunklen&0x000000ff));
	//orkmessageh( "chunkLen: 0x%x (%d) ((%c%c%c%c))\n", chunklen, chunklen, ch0, ch1, ch2, ch3 );
	//U32 chd0 = chunkdata[0];
	//U32 chd1 = chunkdata[1];
	//messageh( MSGPANE_DEBUG, "chunkdata0: 0x%x chunkdata1: 0x%x\n", chd0, chd1 );
}

////////////////////////////////////////////////////////////////////////////////

void RIFFChunk::DumpListHeader( void )
{	PrintChunkID( chunkID );
	PrintChunkID( subID );
}

////////////////////////////////////////////////////////////////////////////////

RIFFChunk::RIFFChunk()
{	chunklen=0;
	chunkdata=0;
	chunkID=0;
	subID = 0;
}

///////////////////////////////////////////////////////

RIFFChunk::~RIFFChunk()
{
}

///////////////////////////////////////////////////////////////////////////////

RIFFFile::RIFFFile() : // default constructor
	mpRawData(0),
	miRawDataLen(0)
{
}

///////////////////////////////////////////////////////////////////////////////

	RIFFFile::~RIFFFile()
{
	if( mpRawData )
	{
		free( mpRawData );
	}
}
////////////////////////////////////////////////////////////////////////////////

bool RIFFFile::OpenFile( std::string FileName )
{	
	miRawDataLen = 0;
	File RiffFile( FileName.c_str(), EFM_READ );
	RiffFile.Open();
	EFileErrCode eFileErr = RiffFile.GetLength( miRawDataLen );
	mpRawData = (U8*)malloc(U32(miRawDataLen));
	eFileErr = RiffFile.Read( mpRawData, miRawDataLen );
	eFileErr = RiffFile.Close();

	return (eFileErr == EFEC_FILE_OK);
}

///////////////////////////////////////////////////////////////////////////////

RIFFChunk::RIFFChunk( void* psubdata )
	: chunkdata(0)
	, chunklen(0)
{
	u8* pu8 = (u8*) psubdata;
	
	chunklen = *reinterpret_cast<int*>(pu8+0);
	chunkdata = (pu8+4);
}

////////////////////////////////////////////////////////////////////////////////

void RIFFFile::LoadChunks( void )
{	
	RIFFChunk* RootChunk = new RIFFChunk;

	RootChunk->chunkID = U32(BigEndianS32(&mpRawData[0], 4));
	RootChunk->chunklen = U32(LitEndianS32(&mpRawData[4], 4));
	RootChunk->chunkdata = (U8*)&mpRawData[8];
	RootChunk->DumpHeader();

	AddChunk( "ROOT", RootChunk );
	//root.get_sbfk();
}

////////////////////////////////////////////////////////////////////////////////

RIFFChunk* RIFFChunk::GetChunk( const char *pCHKNAM )
{
	RIFFChunk *pChunk = OrkSTXFindValFromKey( ChunkMap, (std::string) pCHKNAM, (RIFFChunk*) 0 );
	return pChunk;
}

////////////////////////////////////////////////////////////////////////////////

void RIFFChunk::AddChunk( const char*ChunkName, RIFFChunk *Chunk )
{
	U32 ChunkID = RIFFChunk::ChunkName( ChunkName );
	std::string scn = (std::string) ChunkName;
	OrkSTXMapInsert( ChunkMap, scn, Chunk );
	Chunks.push_back( Chunk );
}

////////////////////////////////////////////////////////////////////////////////

RIFFChunk* RIFFFile::GetChunk( const char *pCHKNAM )
{
	RIFFChunk *pChunk = OrkSTXFindValFromKey( ChunkMap, (std::string) pCHKNAM, (RIFFChunk*) 0 );
	return pChunk;
}

////////////////////////////////////////////////////////////////////////////////

void RIFFFile::AddChunk( const char*ChunkName, RIFFChunk *Chunk )
{
	U32 ChunkID = RIFFChunk::ChunkName( ChunkName );
	std::string scn = (std::string) ChunkName;
	OrkSTXMapInsert( ChunkMap, scn, Chunk );
	Chunks.push_back( Chunk );
}

////////////////////////////////////////////////////////////////////////////////

void RIFFFile::SwapBytes( U8* bytes, U32 len ) // inplace endian swap
{	U32 i = 0;	U32 j = 0;
	U8 tempd[8]; // up to 8 byte ints handled
	U8 *bp = reinterpret_cast<U8 *>(bytes);
	for( i=0, j=len-1; i<len; i++, j-- )	tempd[j] = bp[i];
	for( i=0; i<len; i++ ) bp[i] = tempd[i];
}

///////////////////////////////////////////////////////////////////////////////

S32 RIFFFile::ReadVarLenInt( U8 *buf, U32 *size )
{	*size = 0;
	U32 l = 0, i = 0;
	S32 rval = 0;
	do
	{	l = (U32) buf[i++];
		(*size)++;
		if( *size > 6 )fatalerror( "readVAR()", "unterminated variable integer" );
		else rval = S32((rval << 7) + (l & 0x7f));
	}
	while ((l & 0x80));
	return rval;
}

////////////////////////////////////////////////////////////////////////////////

S32 RIFFFile::BigEndianS32( U8 *pD, U32 size ) // get from bigendian source
{	S32 rval = 0;
	U32 shift = (size-1)*8;
	for( U32 i=0; i<size; i++, shift-=8 )
	{	rval += (S32) (*pD++)<<shift;
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

S32 RIFFFile::LitEndianS32( U8 *pD, U32 size ) // get from littleendian source
{	S32 rval = 0;
	U32 shift = 0;
	for( U32 i=0; i<size; i++, shift+=8 )
	{	rval += (S32) (*pD++)<<shift;
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

RiffChunk2::RiffChunk2()
	: chunktype( 0 )	
	, chunklength( 0 )	
	, data( 0 )	
{
}

///////////////////////////////////////////////////////////////////////////////

RiffFile2::RiffFile2()
	: mpAllocData(0)
{
}

///////////////////////////////////////////////////////////////////////////////

RiffFile2::~RiffFile2()
{
	if( mpAllocData )
		delete[] mpAllocData;
	mpAllocData = 0;
}

///////////////////////////////////////////////////////////////////////////////

struct Item
{
	char*	mpBase;
	int		miLength;
};

void RiffFile2::Open( const ork::file::Path& pth )
{
	///////////////////////////////////////////
	// Read Data
	///////////////////////////////////////////

	ork::File File( pth, ork::EFM_READ );
	size_t isiz = 0;
	File.GetLength( isiz );
	mpAllocData = new char[ isiz ];
	File.Read( mpAllocData, isiz );
	File.Close();

	///////////////////////////////////////////
	// Read ROOT
	///////////////////////////////////////////
	char rootname[5] = { 0, 0, 0, 0, 0 };
	int rootlength;

	memcpy( (void*) & rootname, mpAllocData+0, 4 );
	memcpy( (void*) & rootlength, mpAllocData+4, 4 ); 
	
	if( 0 == strcmp(rootname,"RIFF") ) // Root Chunk
	{
		int ifiletype;
		memcpy( (void*) & ifiletype, mpAllocData+8, 4 );
		char filetypename[5] = { 0, 0, 0, 0, 0 };
		memcpy( filetypename, &ifiletype, 4 );
		orkprintf( "filetype %s\n", filetypename );
	}
	else
	{
		OrkAssert(false);
	}

	///////////////////////////////////////////
	// Parse Chunks
	///////////////////////////////////////////

	int chunknum = 0;

	orkprintf( "getchunks()\n" );

	orkstack<Item> ItemStack;

	Item RootItem;
	RootItem.mpBase = mpAllocData+12;
	RootItem.miLength = (int)isiz-12;

	ItemStack.push(RootItem);

	while( ItemStack.empty() == false )
	{
		const Item item = ItemStack.top();
		ItemStack.pop();

		int ipos = 0;

		while( ipos < item.miLength )
		{
			/////////////////////////////////
			// read chunk header
		
			char chunkname[5] = { 0, 0, 0, 0, 0 };
			int chunktype;
			int chunklength;

			memcpy( (void*) & chunktype, item.mpBase+ipos, 4 ); ipos+=4;
			memcpy( (void*) & chunklength, item.mpBase+ipos, 4 ); ipos+=4;
			
			swapbytes_from_little( chunklength );

			orkprintf( "chunktype <%08x> chunklen %08x\n", chunktype, chunklength );

			#if ! defined( _OSX )
			//RIFFFile::SwapBytes( reinterpret_cast<U8 *>( & chunklength ), 4 );
			#endif

			int bytesleft = chunklength;

			memcpy( chunkname, &chunktype, 4 );

			orkprintf( "chunktype %s chunklen %08x\n", chunkname, chunklength );

			/////////////////////////////////
			// read chunk data

			if( chunklength != -1 )									// we have a good chunk
			{	
				RiffChunk2 *pChunk = new RiffChunk2;

				Chunks.push_back( pChunk );

				pChunk->data = item.mpBase+ipos;
				pChunk->chunktype = chunktype;
				pChunk->chunklength = chunklength;


				memset( pChunk->chunkname, 0, 5 );
				memcpy( pChunk->chunkname, chunkname, 4 );
				
				ipos += chunklength;
			}

			/////////////////////////////////

			chunknum++;
			
		}
	}

	int numchunks = chunknum-1;	

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#endif
