////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/file/file.h>
#include <ork/math/audiomath.h>
#include <ork/lev2/aud/audiobank.h>
#include "soundfont.h"
#include "sf2tospu.h"
#include <ork/kernel/string/string.h>
#include <ork/file/riff.h>
#include <orktool/filter/filter.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////

int yo( RIFFChunk* parchunk, int ioffset, orkmultimap<U32,RIFFChunk*>& ChunkMap )
{
	U8 *parchkdata = parchunk->GetChunkData();
	RIFFChunk *nchnk = new RIFFChunk;
	nchnk->chunkID = parchunk->GetU32( ioffset );
	nchnk->chunklen = parchunk->GetU32( ioffset+4 );
	nchnk->chunkdata = & parchkdata[ ioffset+8 ];
	int rval = 8+nchnk->chunklen;

	ChunkMap.insert( std::make_pair(nchnk->chunkID,nchnk) );

	//RIFFChunk::PrintChunkID(nchnk->chunkID);
	return rval;
}

struct cue_point
{
	U32 uid;		//0x00 	4 	ID 	unique identification value
	U32 pop;		//0x04 	4 	Position 	play order position
	U32 dcid;		//0x08 	4 	Data Chunk ID 	RIFF ID of corresponding data chunk
	U32 cstart;		//0x0c 	4 	Chunk Start 	Byte Offset of Data Chunk *
	U32 blkstart;	//0x10 	4 	Block Start 	Byte Offset to sample of First Channel
	U32	smpoffset;	//0x14 	4 	Sample Offset 	Byte Offset to sample byte of First Channel
};

void GetSubChunks( RIFFChunk* basechunk, int iparlen, orkmultimap<U32,RIFFChunk*>& ChunkMap )
{
	//int ilen = basechunk->chunklen;
	int ioffset = 0;
	bool bdone = false;
	while( false == bdone )
	{	int iadd = yo( basechunk, ioffset, ChunkMap );
		ioffset += iadd;
		//orkprintf( "iadd<%d> ioffset<%d> iparlen<%d>\n", iadd, ioffset, iparlen );
		bdone = ((ioffset+4)>=iparlen);
	}
}

void GetSubListChunks( RIFFChunk* basechunk, int ioffset, orkmultimap<U32,RIFFChunk*>& ChunkMap )
{
	basechunk->chunkdata -= 4;
	basechunk->chunklen = *reinterpret_cast<u32*>( basechunk->chunkdata-8 );
	
	int iparlen = basechunk->chunklen;

	struct subyo
	{
		static int yo( RIFFChunk* parchunk, int ioffset, orkmultimap<U32,RIFFChunk*>& ChunkMap )
		{
			U8 *parchkdata = parchunk->GetChunkData();
			RIFFChunk *nchnk = new RIFFChunk;
			nchnk->chunkID = parchunk->GetU32( ioffset );

			bool islabl = nchnk->chunkID==RIFFChunk::ChunkName( 'l', 'a', 'b', 'l' );

			RIFFChunk::PrintChunkID(nchnk->chunkID);
			nchnk->chunklen = parchunk->GetU32( ioffset+4 );
			nchnk->chunkdata = & parchkdata[ ioffset+8 ];
			int rval = 8+nchnk->chunklen;

			if( islabl && (nchnk->chunklen&1)) rval+=1;

			ChunkMap.insert( std::make_pair(nchnk->chunkID,nchnk) );

			return rval;
		}
	};

	ioffset = 0;
	bool bdone = false;
	while( false == bdone )
	{	int iadd = subyo::yo( basechunk, ioffset, ChunkMap );
		ioffset += iadd;
		//orkprintf( "iadd<%d> ioffset<%d> iparlen<%d>\n", iadd, ioffset, iparlen );
		bdone = ((ioffset+4)>=iparlen);
	}
}

bool WavToMkr( const tokenlist& toklist )
{
	ork::tool::FilterOptMap	OptionsMap;
	OptionsMap.SetDefault( "-in", "yo");
	OptionsMap.SetDefault( "-out", "dude" );
	OptionsMap.SetOptions( toklist );

	std::string ttv_in = OptionsMap.GetOption( "-in" )->GetValue();
	std::string ttv_out = OptionsMap.GetOption( "-out" )->GetValue();

	ork::file::Path InPath( ttv_in.c_str() );
	ork::file::Path Outfile( ttv_out.c_str() );

	RIFFFile RiffFile;
	RiffFile.OpenFile( InPath.ToAbsolute().c_str() );
	RiffFile.LoadChunks();

	orkmultimap<U32,RIFFChunk*> ChunkMap;

	RIFFChunk* proot = RiffFile.GetChunk( "ROOT" );

	int irootlen = proot->chunklen;

	U32 rval =0;
	RIFFChunk *nchnk = new RIFFChunk;
	nchnk->chunkID = proot->GetU32( 0 );
	U8* chunkdata = proot->GetChunkData();
	nchnk->chunklen = RIFFChunk::GetChunkLen( chunkdata[1] );
	nchnk->chunkdata = & proot->chunkdata[4];
	nchnk->DumpHeader();

	GetSubChunks( nchnk, irootlen, ChunkMap );

	//////////////////////////////////////
	// get format
	//////////////////////////////////////

	int isamprate = 0;

	orkmultimap<U32,RIFFChunk*>::const_iterator itfmt = ChunkMap.find( RIFFChunk::ChunkName( 'f', 'm', 't', ' ' ) );
	if( itfmt != ChunkMap.end() )
	{
		RIFFChunk*	pfmtchunk = itfmt->second;
		u8* pdata = pfmtchunk->chunkdata;

		isamprate = *reinterpret_cast<int*>( pdata+0x04 );


	}

	//////////////////////////////////////
	// get labels
	//////////////////////////////////////
	
	orkvector<std::string> labels;

	orkmultimap<U32,RIFFChunk*>::const_iterator list_lower = ChunkMap.lower_bound( RIFFChunk::ChunkName( 'L', 'I', 'S', 'T' ) );
	orkmultimap<U32,RIFFChunk*>::const_iterator list_upper = ChunkMap.upper_bound( RIFFChunk::ChunkName( 'L', 'I', 'S', 'T' ) );

	for( orkmultimap<U32,RIFFChunk*>::const_iterator itlist=list_lower; itlist!=list_upper; itlist++ )
	{
		RIFFChunk*	plistchunk = itlist->second;

		orkmultimap<U32,RIFFChunk*> ListChunkMap;
		GetSubChunks( plistchunk, nchnk->chunklen, ListChunkMap );

		orkmultimap<U32,RIFFChunk*>::const_iterator itadtl = ListChunkMap.find( RIFFChunk::ChunkName( 'a', 'd', 't', 'l' ) );
		if( itadtl != ListChunkMap.end() )
		{
			RIFFChunk*	padtlchunk = itadtl->second;

			orkmultimap<U32,RIFFChunk*> AdtlChunkMap;
			GetSubListChunks( padtlchunk, 0, AdtlChunkMap );
					
			orkmultimap<U32,RIFFChunk*>::const_iterator labl_lower = AdtlChunkMap.lower_bound( RIFFChunk::ChunkName( 'l', 'a', 'b', 'l' ) );
			orkmultimap<U32,RIFFChunk*>::const_iterator labl_upper = AdtlChunkMap.upper_bound( RIFFChunk::ChunkName( 'l', 'a', 'b', 'l' ) );

			for( orkmultimap<U32,RIFFChunk*>::const_iterator itlab=labl_lower; itlab!=labl_upper; itlab++ )
			{
				RIFFChunk*	plablchunk = itlab->second;

				int ilen = plablchunk->chunklen;

				char* pdata = (char*) plablchunk->chunkdata;

				int ival = *reinterpret_cast<int*>(pdata+0);
				char* pstr = pdata+4;

				//orkprintf( "lablchunk ilen<%d> ival<%d> pstr<%s>\n", ilen, ival, pstr );

				labels.push_back( pstr );

			}
		}
	}

	//////////////////////////////////////
	// get cue points
	//////////////////////////////////////

	orkmultimap<U32,RIFFChunk*>::const_iterator it = ChunkMap.find( RIFFChunk::ChunkName( 'c', 'u', 'e', ' ' ) );

	orkmap<float,std::string> CueMap;
	if( it != ChunkMap.end() )
	{
		RIFFChunk*	pcuechunk = it->second;

		int ichunklen = pcuechunk->chunklen;
		u8* pdata = pcuechunk->chunkdata;

		int inumcuepoints = *reinterpret_cast<int*>(pdata+0);
		const cue_point* ppoints = reinterpret_cast<const cue_point*>(pdata+4);

		orkprintf( "numcuepoints<%d>\n", inumcuepoints );

		for( int ic=0; ic<inumcuepoints; ic++ )
		{
			const cue_point& cpoint = ppoints[ic];

			U32 dcid = cpoint.dcid;

			Char4 dcidch( dcid );

			const std::string& label = labels[ic];

			int isample = cpoint.smpoffset;

			float ftime = float(isample) / float(isamprate);

			orkprintf( "cuepoint<%d> label<%s> uid<%d> pop<%d> isample<%d> ftime<%f>\n", ic, label.c_str(), cpoint.uid, cpoint.pop, isample, ftime );

			CueMap[ftime] = label;
			
		}
	}

	File ofile( Outfile, ork::EFM_WRITE );

	int icount = (int) CueMap.size();
	ofile.Write( & icount, sizeof(icount) );
	for( orkmap<float,std::string>::const_iterator it =CueMap.begin(); it!=CueMap.end(); it++ )
	{
		float ftime = it->first;
		const std::string& name = it->second;
		int istrlen = (int) strlen(name.c_str());
		ofile.Write( & ftime, sizeof(ftime) );
		ofile.Write( & istrlen, sizeof(istrlen) );
		ofile.Write( name.c_str(), istrlen );
	}
	ofile.Close();
	return true;
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
