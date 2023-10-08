////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/file.h>

namespace ork::scg {

#define CHUNKFOUND(val) (val != 0xffffffff)

struct CChunk;
struct ChunkGroup;
struct ChunkFile;
struct ChunkHeader;
struct ChunkData;
struct ChunkDataIterator;
struct ChunkReader;

typedef int ChunkHandle;

using chunkheader_ptr_t = std::shared_ptr<ChunkHeader>;
using chunkgroup_ptr_t = std::shared_ptr<ChunkGroup>;
using chunkdata_ptr_t = std::shared_ptr<ChunkData>;
using chunkdataiter_ptr_t = std::shared_ptr<ChunkDataIterator>;
using chunkfile_ptr_t = std::shared_ptr<ChunkFile>;
using chunkreader_ptr_t = std::shared_ptr<ChunkReader>;

using chunkdatasource_read_t = std::function<void(void*, size_t)>;
using chunkdatasource_seek_t = std::function<void(size_t)>;

///////////////////////////////////////////////////////////////////////////////

struct ChunkData{
    chunkdataiter_ptr_t iterator() const;
    size_t length() const;
   std::vector<uint8_t> _data;
   std::string _versionCode;
};

struct ChunkDataIterator{

    ChunkDataIterator(const ChunkData* cdata);

    template <typename T> T readItem() {
        T rval;
        memcpy((void*) &rval, _chunkdata->_data.data() + _offset, sizeof(T));
        _offset += sizeof(T);
        return rval;
    }
    size_t _offset = 0;
    const ChunkData* _chunkdata;

};

///////////////////////////////////////////////////////////////////////////////

struct ChunkHeader {	
	uint32_t chunktypeID;	//		what type of chunk?
	uint32_t chunkID;		//		which instance of this type of chunk?
	uint32_t version;		//		version code ?
	uint32_t offset;		//		offset from base of chunkgroup
	uint32_t chunklen;		//		length of chunk
};

///////////////////////////////////////////////////////////////////////////////

struct ChunkReader{
    virtual ~ChunkReader() {}
    virtual void seek(size_t seekpos) = 0;
    virtual void read( void *where, size_t howmany ) = 0;
    template <typename T> T readItem() {
        T rval;
        read((void*) &rval, sizeof(T));
        return rval;
    }
    size_t _offset = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct ChunkReaderFile : public ChunkReader {
    file_ptr_t _ork_file;
    void seek(size_t seekpos) final;
    void read( void *where, size_t howmany ) final;
};

///////////////////////////////////////////////////////////////////////////////

struct ChunkSubReader : public ChunkReader{
    ChunkSubReader(chunkdata_ptr_t chdata);
    chunkdata_ptr_t _chunkdata;
    void seek(size_t seekpos) final;
    void read( void *where, size_t howmany ) final;
};

///////////////////////////////////////////////////////////////////////////////

struct ChunkGroup {
    std::string _name;
    chunkreader_ptr_t _reader;
	uint32_t numchunks;
	uint32_t _offsetbase = 0;
    void dump_headers(std::string headers_name);
    chunkdata_ptr_t load( chunkheader_ptr_t chunkhdr );			// load chunk data
    chunkheader_ptr_t getChunkOfType( std::string chunkID ) const;
    std::vector<chunkheader_ptr_t> chunksOfType( std::string chunkID ) const;
	std::vector<chunkheader_ptr_t> _chunk_headers; //	fake array chunkoffsets[0]
};



struct ChunkFile {
    ChunkFile(file::Path path);
    chunkgroup_ptr_t _loadSubHeader( size_t seekoffset );				// get nested header
    chunkgroup_ptr_t _loadSubHeader( chunkdata_ptr_t parent );	        // get nested header
	file_ptr_t _ork_file;
    chunkreader_ptr_t _top_reader;
	uint32_t chunkfileID;
	uint32_t filetypeID;
	chunkgroup_ptr_t _top_chunkgroup;
};


uint32_t			chunk_genchunkID( const char *idstr );											// gen CHUNKID from ascii chars
const char*			chunk_genchunkIDSTRING( uint32_t chID );


///////////////////////////////////////////////////////////////////////////////
//
// perhaps place chunkheads in contiguous space (so its cached) and then followed by contiguous data
//        this allows loader to treat allocs per chunk ( eg texfile is non resident )
//
////////////////////////
//
//        Chunk File Header
//
//        uint32_t chunkfileID        // always 'CHKF'
//        uint32_t filetypeID        // user defined
//
////////////////////////
//
//        Chunk Group Header ( may be nested )
//
//        uint32_t numchunks
//
///////// chunkIDX 0 Header
//
//        uint32_t        chunktypeID        // what type of chunk?
//        uint32_t        chunkID                // which instance of this type of chunk?
//        uint32_t chunklen
//        uint32_t offset
//
///////// chunkIDX 1 Header
//
//        uint32_t        chunktypeID
//        uint32_t        chunkID
//        uint32_t chunklen
//        uint32_t offset
//
///////// chunkIDX n Header
//
//        uint32_t        chunktypeID
//        uint32_t        chunkID
//        uint32_t chunklen
//        uint32_t offset
//
///////// chunk IDX 0..n Data
//
//        char data 0 [ chunklen ]
//        char data 1 [ chunklen ]
//        char data n [ chunklen ]
//
//
/////////
//
//        *note chunks can be nested if desired
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//        example::
//        MAP FILE ( filetypeID 'MAP_' )
//
//        chunkTID                'MAP_':        map
//                chunkTID        'HEAD':        map header
//                chunkTID        'DATA':        map data
//                chunkTID        'ACTI':        map actor instancing / placement / attributes
//                chunkTID        'PATH':        actor path ( shared by multiple actors? )
//                chunkTID        'BBOX':        enemy bounding box
//                chunkTID        'NAME':        cellnames
//
//        chunkTID                'ACTR':        map
//                chunkTID        'GEOM':        actor psx geometry         (.msh file)
//                chunkTID        'ATEX':        actor textures             (.tex file)
//                chunkTID        'AANI':        actor binary anim          (.enm file)
//
//        chunkTID                'CELS':        map
//                chunkTID        'GEOM':        cellset psx geometry       (.cel file)
//                chunkTID        'CTEX':        cellset textures           (.tex file)
//                chunkTID        'CANI':        cell animation             (.can file)
//
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//        C++ chunkwriter code

/*struct CChunk
{
    public: //
    vector< char > data_vect;
    uint32_t chunktypeID;
    uint32_t chunkID;
    uint32_t version_code;
    uint32_t offset_base;							// what is the base offset of this chunks data? (relative to beginning of file)
    uint32_t offset_current;							// what is the chunks current data length? (actually same as data_vect.size())
	mutable int	miReadIndex;

    void WriteData( FILE *fout ) const;
    uint32_t  AddData( const char *data, int len );
	void GetData( char *data, int len ) const;
    void AddSubChunkData( Cchunkgroup *chgrp );		// add(inline) data from sub chunk into this chunk
    void data_pad( void );
    void data_dump( void );
	int GetChunkLen( void ) const { return data_vect.size(); }

	CChunk();
	CChunk( Schunkgroup *chgrp, ChunkHandle chunkIDX );

	template<typename T> uint32_t AddItem( const T & val )
	{
		int ilen = sizeof( T );
		return AddData( (const char*) & val, ilen );
	}

	template<typename T> void GetItem( T & val ) const
	{
		int ilen = sizeof( T );

		char *pdata = (char*) & val;

		for( int i=0; i<ilen; i++ )
		{
			pdata[i] = data_vect[ miReadIndex ];
			miReadIndex++;
		}
	}

};

///////////////////////////////////////////////////////////////////////////////

struct Cchunkgroup
{
    public: //

    std::vector< chunk_ptr_t >	chunk_vect;
    uint32_t							chunkgroup_length;		// what is the length of this chunkgroup in bytes (include Schunkgroup header)
														//        only valid after calc_offsets()

    void data_write( FILE *fout );
    void calc_offsets( void );
    Cchunkgroup();
    ~Cchunkgroup();
    void write_chunk_file( std::string fname, uint32_t filetypeID );
    chunk_ptr_t newchunk( char *chtID, char *chID, char *verID );
    chunk_ptr_t newchunk( char *chtID, uint32_t chID, char *verID );

};

///////////////////////////////////////////////////////////////////////////////

struct CChunkDataLoader
{
	public: //

	chunkfile_ptr_t		mpchfil;
	Schunkgroup*	mpchgrp;
	chunkheader_ptr_t		mpchunkhdr;
	int				micuroffset;
	int				michunklen;

	//////////////////////////

	CChunkDataLoader( Schunkgroup *chgrp, chunkheader_ptr_t chunkhdr )
		: mpchgrp( chgrp )
		, mpchunkhdr( chunkhdr )
		, mpchfil( chgrp->chfil )
		, micuroffset(0)
		, michunklen( mpchunkhdr->chunklen )
	{
        uint32_t offset = mpchunkhdr->offset + mpchgrp->offsetbase;
		chunk_fseek( mpchfil->_ork_file, offset );
	}

	//////////////////////////

	void LoadData( void *pWhere, int ilen )
	{
		OrkAssert( (micuroffset+ilen) <= michunklen );
		chunk_fread( mpchfil->_ork_file, pWhere, ilen );
		micuroffset += ilen;
	}

	template<typename T> void LoadItem( T& ObjRef )
	{
		LoadData( reinterpret_cast<void*>( & ObjRef ), sizeof(T) );
	}

};
*/
///////////////////////////////////////////////////////////////////////////////

} //namespace ork::scg {

