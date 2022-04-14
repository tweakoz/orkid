////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/orkstl.h>
#include <ork/file/file.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork{
///////////////////////////////////////////////////////////////////////////////

class RIFFChunk
{
	public: //
	
	U32 chunkID;
	U32 subID;
	U32 chunklen;
	U8 *chunkdata;
	orkvector< RIFFChunk* >		Chunks;
	orkmap< std::string, RIFFChunk* >	ChunkMap;

	///////////////////////////////////////////////////////

	U32 GetU32( U32 offset );
	void DumpHeader( void );
	void DumpListHeader( void );
	U8 * GetChunkData( void ) { return chunkdata; }
	
	static U32 ChunkName( U8 a, U8 b, U8 c, U8 d );
	static U32 ChunkName( const char *pCHKNAM );
	static U32 GetChunkLen( U32 val );
	static void PrintChunkID( U32 val );
	RIFFChunk* GetChunk( const char* pCHKNAM );
	void AddChunk( const char*ChunkName, RIFFChunk * );

	///////////////////////////////////////////////////////

	RIFFChunk();
	~RIFFChunk();

	RIFFChunk( void* psubdata );

};

///////////////////////////////////////////////////////////////////////////////

struct RiffChunk2

{	public: // data

	U32			chunktype;
	S32			chunklength;
	const char*	data;
	char		chunkname[5];

	public: // methods

	RiffChunk2(); 

};

///////////////////////////////////////////////////////////////////////////////

struct RiffFile2
{
	RiffFile2();
	~RiffFile2();
	vector<RiffChunk2*>	Chunks;
	void Open( const ork::file::Path& pth );
	char*	mpAllocData;
};


///////////////////////////////////////////////////////////////////////////////

class RIFFFile
{	
	public: // methods

	orkvector< RIFFChunk* >		Chunks;
	orkmap< std::string, RIFFChunk* >	ChunkMap;
	U8*							mpRawData;
	size_t						miRawDataLen;

	RIFFFile( void ); // default constructor
	~RIFFFile();
	
	bool OpenFile( std::string fname );

	void LoadChunks( void );
	S32 LitEndianS32( U8 *pD, U32 size ); // get from bigendian source
	S32 BigEndianS32( U8 *pD, U32 size ); // get from bigendian source
	S32 ReadVarLenInt( U8 *buf, U32 *size );
	static void SwapBytes( U8* bytes, U32 len ); // inplace endian swap
	RIFFChunk* GetChunk( const char* pCHKNAM );
	void AddChunk( const char*ChunkName, RIFFChunk * );
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////


