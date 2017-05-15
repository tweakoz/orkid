////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/math/audiomath.h>
#include <ork/file/riff.h>
#include "sf2.h"

using namespace ork::audiomath;

////////////////////////////////////////////////////////////////////////////////

namespace ork { namespace sf2 {

U32 SoundFont::GetSBFK( CRIFFChunk *pROOT )
{

	U32 rval =0;
	auto nchnk = new CRIFFChunk;
	nchnk->chunkID = pROOT->GetU32( 0 );
	U8* chunkdata = pROOT->GetChunkData();
	nchnk->chunklen = CRIFFChunk::GetChunkLen( chunkdata[1] );
	nchnk->chunkdata = & pROOT->chunkdata[4];
	nchnk->DumpHeader();
	//children.push_back( nchnk );
	
	mDynamicChunks.push_back( nchnk );

	U32 offs = 0;
	U32 len = 0;
	
	len = GetINFOList( nchnk, 0 );
	len = GetSDTAList( nchnk, len );
	len = GetPDTAList( nchnk, len );
	
	return rval;
}

////////////////////////////////////////////////////////////////////////////////

U32 SoundFont::GetINFOChunk( CRIFFChunk *ParChunk, U32 offset )
{
	//printf( "get_chunk( 0x%x )\n", offset );
	U32 rval =0;
	U8 *parchkdata = ParChunk->GetChunkData();
	CRIFFChunk *nchnk = new CRIFFChunk;
	nchnk->chunkID = ParChunk->GetU32( offset );
	nchnk->chunklen = ParChunk->GetU32( offset+4 );
	nchnk->chunkdata = & parchkdata[ offset+8 ];
	//children.push_back( nchnk );
	rval = 8+nchnk->chunklen;

	mDynamicChunks.push_back( nchnk );


#if 1
	printf( "//////////////////////////\n" );
	if( nchnk->chunkID == CRIFFChunk::ChunkName( 'i', 'f', 'i', 'l' ) )
	{	printf( "ifil chunk found!\n" );
		U32 version = nchnk->GetU32( 0 );
		printf( "SoundFont Version: 0x%08x\n", version );
	}	
	else if( nchnk->chunkID == CRIFFChunk::ChunkName( 'i', 's', 'n', 'g' ) )
	{	printf( "isng chunk found!\n" );
		printf( "SoundEngine: %s\n", nchnk->chunkdata );
	}	
	else if( nchnk->chunkID == CRIFFChunk::ChunkName( 'I', 'N', 'A', 'M' ) )
	{	printf( "INAM chunk found!\n" );
		printf( "BANKName: %s\n", nchnk->chunkdata );
	}	
	else if( nchnk->chunkID == CRIFFChunk::ChunkName( 'I', 'S', 'F', 'T' ) )
	{	printf( "ISFT chunk found!\n" );
		printf( "Application: %s\n", nchnk->chunkdata );
	}	
	else if( nchnk->chunkID == CRIFFChunk::ChunkName( 'I', 'E', 'N', 'G' ) )
	{	printf( "IENG chunk found!\n" );
		printf( "Creator: %s\n", nchnk->chunkdata );
	}	
	else if( nchnk->chunkID == CRIFFChunk::ChunkName( 'I', 'C', 'O', 'P' ) )
	{	printf( "ICOP chunk found!\n" );
		printf( "Copyright: %s\n", nchnk->chunkdata );
	}	
	else if( nchnk->chunkID == CRIFFChunk::ChunkName( 'I', 'C', 'M', 'T' ) )
	{	printf( "ICMT chunk found!\n" );
		printf( "Comment: %s\n", nchnk->chunkdata );
	}	
	else
	{	printf( "UNKNOWN CHUNK!\n" );
		nchnk->DumpHeader();
	}
	printf( "//////////////////////////\n" );
#endif

	return rval;
}


////////////////////////////////////////////////////////////////////////////////

U32 SoundFont::GetINFOList( CRIFFChunk* ParChunk, U32 offset )
{
	//printf( "get_info_list()\n" );
	U32 rval =0;
	U8 *parchkdata = ParChunk->GetChunkData();
	CRIFFChunk *nchnk = new CRIFFChunk;
	nchnk->chunkID = ParChunk->GetU32( offset );
	nchnk->subID = ParChunk->GetU32( offset+8 );
	nchnk->chunklen = CRIFFChunk::GetChunkLen( ParChunk->GetU32( offset+4 ) );
	nchnk->chunkdata = & parchkdata[(3*4)];
	nchnk->DumpListHeader();

	mDynamicChunks.push_back( nchnk );

	list_info = nchnk;
	
	//children.push_back( nchnk );
	rval = 0;
	
	U32 i = 0;
	while( rval < (nchnk->chunklen-8) )
	{	rval += GetINFOChunk( nchnk, rval);
	}
	return (rval+12);
}

////////////////////////////////////////////////////////////////////////////////

U32 SoundFont::GetSDTAChunk( CRIFFChunk *ParChunk, U32 offset )
{
	//printf( "get_chunk( 0x%x )\n", offset );
	U32 rval =0;
	U8 *parchkdata = ParChunk->GetChunkData();
	auto nchnk = new CRIFFChunk;
	nchnk->chunkID = ParChunk->GetU32( offset );
	nchnk->chunklen = ParChunk->GetU32( offset+4 );
	nchnk->chunkdata = & parchkdata[ offset+8 ];
	//children.push_back( nchnk );
	rval = 8+nchnk->chunklen;

	mDynamicChunks.push_back( nchnk );

	printf( "//////////////////////////\n" );
	if( nchnk->chunkID == CRIFFChunk::ChunkName( 's', 'm', 'p', 'l' ) )
	{	printf( "smpl chunk found! (offset: 0x%x len: %d\n", offset, nchnk->chunklen );

		static int total = 0;
		total += nchnk->chunklen;

		assert(_chunkOfSampleData==nullptr);

		void* copy = malloc(nchnk->chunklen);
		memcpy( copy, (void*) & nchnk->chunkdata[ 2 ], nchnk->chunklen );
		_chunkOfSampleData = (S16 *) copy;
		printf( "_chunkOfSampleData<%p> _bankName<%s> total<%d MiB>\n", _chunkOfSampleData, _bankName.c_str(), total>>20 );
		int *pbdlen = const_cast<int*>(& _sampleDataNumSamples);
		*pbdlen = nchnk->chunklen;

	}	
	else
	{	printf( "//////////////////////////\n" );
		printf( "UNKNOWN CHUNK at offset 0x%x!\n", offset );
		nchnk->DumpHeader();
		printf( "//////////////////////////\n" );
	}

	return rval;
}

////////////////////////////////////////////////////////////////////////////////

U32 SoundFont::GetSDTAList( CRIFFChunk* ParChunk, U32 offset )
{
	//printf( "get_sdta_list( 0x%x )\n", offset );
	U32 rval =0;
	U8 *parchkdata = ParChunk->GetChunkData();

	CRIFFChunk *nchnk = new CRIFFChunk;
	nchnk->chunkID = ParChunk->GetU32( offset );
	nchnk->subID = ParChunk->GetU32( offset+8 );
	nchnk->chunklen = ParChunk->GetU32( offset+4 );
	nchnk->chunkdata = & parchkdata[(3*4)];
	nchnk->DumpListHeader();

	mDynamicChunks.push_back( nchnk );

	list_sdta = nchnk;

	rval = offset;
	
	U32 i = 0;
	while( rval < (nchnk->chunklen-8) )
	{	
		rval += GetSDTAChunk( nchnk, rval ); //->get_sdta_chunk(rval);
	}
	return rval+12;
}

////////////////////////////////////////////////////////////////////////////////

U32 SoundFont::GetPDTAList( CRIFFChunk* ParChunk, U32 offset )
{
	//printf( "get_pdta_list( 0x%x )\n", offset );
	U32 rval =0;
	U8 *parchkdata = ParChunk->GetChunkData();
	CRIFFChunk *nchnk = new CRIFFChunk;
	nchnk->chunkID = ParChunk->GetU32( offset );
	nchnk->subID = ParChunk->GetU32( offset+8 );
	nchnk->chunklen = CRIFFChunk::GetChunkLen( ParChunk->GetU32( offset+4 ) );
	nchnk->chunkdata = & parchkdata[(3*4)];
	nchnk->DumpListHeader();
	
	mDynamicChunks.push_back( nchnk );

	list_sdta = nchnk;
	
	rval = offset;
	
	U32 i = 0;
	//printf( "rval: %d clm8: %d\n", (rval-offset), (nchnk->chunklen-8) );
	while( (rval-offset) < (nchnk->chunklen-8) )
	{	rval += GetPDTAChunk( nchnk, rval );
	}
	
	//post_process();
	
	return rval+12;
}

////////////////////////////////////////////////////////////////////////////////

U32 SoundFont::GetPDTAChunk( CRIFFChunk* ParChunk, U32 offset )
{
	//printf( "get_chunk( 0x%x )\n", offset );
	U32 rval =0;
	U8 *parchkdata = ParChunk->GetChunkData();

	CRIFFChunk *nchnk = new CRIFFChunk;
	nchnk->chunkID = ParChunk->GetU32( offset );
	nchnk->chunklen = CRIFFChunk::GetChunkLen( ParChunk->GetU32( offset+4 ) );
	nchnk->chunkdata = & parchkdata[ offset+8 ];
	//children.push_back( nchnk );
	rval = 8+nchnk->chunklen;

	mDynamicChunks.push_back( nchnk );

	//printf( "//////////////////////////\n" );
	if( nchnk->chunkID == CRIFFChunk::ChunkName( 'p', 'h', 'd', 'r' ) )
	{	//printf( "phdr chunk found!\n" );
		U32 sizofpre = sizeof( Ssfontpreset );	
		U32 numpresets = (nchnk->chunklen / 38)-1;
		//printf( "chunklen: %d sizofpre: %d numpresets: %d\n", nchnk->chunklen, sizofpre, numpresets );

		for( U32 i=0; i<numpresets; i++ )
		{
			U32 pnum = 0xffffffff;
		
			Ssfontpreset *preset = (Ssfontpreset *) & nchnk->chunkdata[ (38*i) ];
			//SF2Presets.push_back( preset );

			AddProgram( preset );
		}

	}
	else if( nchnk->chunkID == CRIFFChunk::ChunkName( 's', 'h', 'd', 'r' ) )
	{	//printf( "//////////////////////////\n" );
		//printf( "shdr chunk found!\n" );
		sizofsmp = sizeof( Ssfontsample );	
		numsamples = (nchnk->chunklen / 46)-1;
		//printf( "chunklen: %d sizofsmp: %d numsamples: %d\n", nchnk->chunklen, sizofsmp, numsamples );

		for( U32 i=0; i<U32(numsamples); i++ )
		{
			auto osample = (Ssfontsample *) & nchnk->chunkdata[ (46*i) ];

			//printf( "inpsamp<i>\n");
			//printf( "  st<%d>\n", osample->dwStart);
			//printf( "  en<%d>\n", osample->dwEnd);
			//printf( "  lpst<%d>\n", osample->dwStartloop);
			//printf( "  lpen<%d>\n", osample->dwEndloop);
			//SF2Samples.push_back( osample );
			AddSample( osample );
		}
	}
	else if( nchnk->chunkID == CRIFFChunk::ChunkName( 'i', 'g', 'e', 'n' ) )
	{	//printf( "//////////////////////////\n" );
		//printf( "igen chunk found!\n" );
		U32 sizofign = sizeof( SSoundFontGenerator );	
		U32 numinstgens = (nchnk->chunklen / 4)-1;
		//printf( "chunklen: %d sizofign: %d numinstgens: %d\n", nchnk->chunklen, sizofign, numinstgens );

		for( U32 i=0; i<numinstgens; i++ )
		{
			SSoundFontGenerator *ign = (SSoundFontGenerator *) & nchnk->chunkdata[ (4*i) ];
			//SF2InstGens.push_back( (SSoundFontGenerator*)ign );
			AddInstrumentGen( ign );
			
		}
	}
	else if( nchnk->chunkID == CRIFFChunk::ChunkName( 'p', 'g', 'e', 'n' ) )
	{	//printf( "//////////////////////////\n" );
		//printf( "pgen chunk found!\n" );
		U32 sizofpgn = sizeof( SSoundFontGenerator );	
		U32 numpregens = (nchnk->chunklen / 4)-1;
		//printf( "chunklen: %d sizofpgn: %d numpregens: %d\n", nchnk->chunklen, sizofpgn, numpregens );

		for( U32 i=0; i<numpregens; i++ )
		{	SSoundFontGenerator *pgn = (SSoundFontGenerator *) & nchnk->chunkdata[ (4*i) ];
			//SF2PreGens.push_back( (SSoundFontGenerator*)pgn );
			//printf( "ign: %d	genID: 0x%x	genVAL: 0x%x\n", i, ign->gen_ID, ign->value );
			AddPresetGen( pgn );
		}
	}
	else if( nchnk->chunkID == CRIFFChunk::ChunkName( 'i', 'b', 'a', 'g' ) )
	{	//printf( "//////////////////////////\n" );
		//printf( "ibag chunk found!\n" );
		U32 sizofibg = sizeof( Ssfontinstbag );	
		U32 numinstbags = (nchnk->chunklen / 4)-1;
		//printf( "chunklen: %d sizofibg: %d numinstbags: %d\n", nchnk->chunklen, sizofibg, numinstbags );

		for( U32 i=0; i<numinstbags; i++ )
		{
			Ssfontinstbag *ibg = (Ssfontinstbag *) & nchnk->chunkdata[ (4*i) ];
			//SF2InstBags.push_back( ibg );

			//printf( "InstBag [%d] [ibagndx %d] [imodndx %d]\n", i, ibg->wInstGenNdx, ibg->wInstModNdx );

			AddInstrumentZone( ibg );

		}
	}
	else if( nchnk->chunkID == CRIFFChunk::ChunkName( 'i', 'n', 's', 't' ) )
	{	//printf( "//////////////////////////\n" );
		//printf( "inst chunk found!\n" );
		U32 sizofinst = sizeof( Ssfontinst );	
		U32 numinsts = (nchnk->chunklen / 22)-1;
		//printf( "chunklen: %d sizofinst: %d numinsts: %d\n", nchnk->chunklen, sizofinst, numinsts );

		for( U32 i=0; i<numinsts; i++ )
		{
			U32 j=i+1;
			Ssfontinst *inst = (Ssfontinst *) & nchnk->chunkdata[ (22*i) ];
			//SF2Instruments.push_back( inst );

			//printf( "Instrument [%d] %s :[InstGenBase %d]\n", i, inst->achInstName, inst->wInstBagNdx );
			
			AddInstrument( inst );

		}
	}
	else if( nchnk->chunkID == CRIFFChunk::ChunkName( 'p', 'b', 'a', 'g' ) )
	{	//printf( "//////////////////////////\n" );
		//printf( "pbag chunk found!\n" );
		U32 sizofpbg = sizeof( Ssfontprebag );	
		numprebags = (nchnk->chunklen / 4)-1;
		//printf( "chunklen: %d sizofpbg: %d numprebags: %d\n", nchnk->chunklen, sizofpbg, numprebags );

		for( U32 i=0; i<numprebags; i++ )
		{
			Ssfontprebag *pbg = (Ssfontprebag *) & nchnk->chunkdata[ (4*i) ];
			//SF2PreBags.push_back( pbg );
			//printf( "ibg: %d	genidx: 0x%x	modidx: 0x%x\n", i, ibg->wInstGenNdx, ibg->wInstModNdx );
			
			AddProgramZone( pbg );
			
		}
	}

	else
	{	//printf( "UNKNOWN CHUNK at offset 0x%x!\n", offset );
		nchnk->DumpHeader();
	}
	//printf( "//////////////////////////\n" );
	
	return rval;
}

} }

