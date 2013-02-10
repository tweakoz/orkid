////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>

#include <ork/file/riff.h>
#include <orktool/filter/filter.h>
#include "soundfont.h"
#include "aiff.h"
//#include <audio/datafilter/sf2tospu.h>

///////////////////////////////////////////////////////////////////////////////

bool InitEncVagDLL( void );

#ifndef HUGE_VAL
# define HUGE_VAL HUGE
#endif /*HUGE_VAL*/

# define FloatToUnsigned(f) ((u32)(((s32)(f - 2147483648.0)) + 2147483647L) + 1)

struct extended_fp80
{
	char buffer[10];
};

static void ConvertToIeeeExtended(double num, extended_fp80& fp80)
{
    int    sign;
    int expon;
    double fMant, fsMant;
    u32 hiMant, loMant;

    if (num < 0) {
        sign = 0x8000;
        num *= -1;
    } else {
        sign = 0;
    }

    if (num == 0) {
        expon = 0; hiMant = 0; loMant = 0;
    }
    else {
        fMant = frexp(num, &expon);
        if ((expon > 16384) || !(fMant < 1)) {    /* Infinity or NaN */
            expon = sign|0x7FFF; hiMant = 0; loMant = 0; /* infinity */
        }
        else {    /* Finite */
            expon += 16382;
            if (expon < 0) {    /* denormalized */
                fMant = ldexp(fMant, expon);
                expon = 0;
            }
            expon |= sign;
            fMant = ldexp(fMant, 32);          
            fsMant = floor(fMant); 
            hiMant = FloatToUnsigned(fsMant);
            fMant = ldexp(fMant - fsMant, 32); 
            fsMant = floor(fMant); 
            loMant = FloatToUnsigned(fsMant);
        }
    }
    
    fp80.buffer[0] = char(expon >> 8);
    fp80.buffer[1] = char(expon);
    fp80.buffer[2] = char(hiMant >> 24);
    fp80.buffer[3] = char(hiMant >> 16);
    fp80.buffer[4] = char(hiMant >> 8);
    fp80.buffer[5] = char(hiMant);
    fp80.buffer[6] = char(loMant >> 24);
    fp80.buffer[7] = char(loMant >> 16);
    fp80.buffer[8] = char(loMant >> 8);
    fp80.buffer[9] = char(loMant);
}

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace tool {

void swapbytes( ADDRESS bytes, U32 len ) // inplace endian swap
{
	static const int kmaxlen = 32;
	OrkAssert( len<kmaxlen );

	U32 i = 0;        U32 j = 0;
    U8 tempd[kmaxlen]; // up to kmaxlen byte ints handled
    U8 *bp = reinterpret_cast<U8 *>(bytes);
    for( i=0, j=len-1; i<len; i++, j-- )        tempd[j] = bp[i];
    for( i=0; i<len; i++ ) bp[i] = tempd[i];
}

void hal_write_F32( FILE *fout, F32 outv )
{
	U32 size = sizeof( F32 );
	swapbytes( (ADDRESS) & outv, size );
	fwrite( (void *) & outv, 1, size, fout );
}
void hal_write_F80( FILE *fout, double outv )
{
	extended_fp80 fp80;
	ConvertToIeeeExtended( outv, fp80 );
	U32 size = sizeof( fp80 );
	OrkAssert( size == 10 );
	//swapbytes( (ADDRESS) & fp80, size );
	fwrite( (void *) & fp80.buffer[0], 1, size, fout );
}

void hal_write_U32( FILE *fout, U32 outv )
{
	U32 size = sizeof( U32 );
	swapbytes( (ADDRESS) & outv, size );
	fwrite( (void *) & outv, 1, size, fout );
}

void write_U32( FILE *fout, U32 outv )
{
	U32 size = sizeof( U32 );
	fwrite( (void *) & outv, 1, size, fout );
}

void hal_write_U16( FILE *fout, U16 outv )
{
	U32 size = sizeof( U16 );
	swapbytes( (ADDRESS) & outv, size );
	fwrite( (void *) & outv, 1, size, fout );
}

void hal_write_U8( FILE *fout, U8 outv )
{
	U32 size = sizeof( U8 );
	swapbytes( (ADDRESS) & outv, size );
	fwrite( (void *) & outv, 1, size, fout );
}

void hal_write_S32( FILE *fout, S32 outv )
{
	U32 size = sizeof( S32 );
	swapbytes( (ADDRESS) & outv, size );
	fwrite( (void *) & outv, 1, size, fout );
}

void hal_write_S16( FILE *fout, S16 outv )
{
	U32 size = sizeof( U16 );
	swapbytes( (ADDRESS) & outv, size );
	fwrite( (void *) & outv, 1, size, fout );
}

void hal_write_S8( FILE *fout, S8 outv )
{
	U32 size = sizeof( S8 );
	swapbytes( (ADDRESS) & outv, size );
	fwrite( (void *) & outv, 1, size, fout );
}

///////////////////////////////////////////////////////////////////////////////

void Cpstring::set_string( STRING newstring )
{
	tsize = (U8) strlen( (char *) newstring );
	pad = (tsize&1); // pad if odd
	count = tsize + pad;
	string = (STRING) _strdup( (char *) newstring );
	printf( "setstring %s tsize %d pad %d count %d\n", newstring, tsize, pad, count );
}

void Cpstring::write_to_file( FILE *fout )
{
	U8 zero = 0;
	
	fwrite( (void *) & tsize, 1, 1, fout );
	fwrite( (void *) string, 1, tsize+1, fout );
	if( pad )
		fwrite( (void *) & zero, 1, 1, fout );
	
}

///////////////////////////////////////////////////////////////////////////////

U32 Ciffchunk::calcsize( void )
{
	size = 0;
	size_t numchildren = children_vect.size();
	
	for( size_t i=0; i<numchildren; i++ )
	{	size += children_vect[i]->calcsize();
	}

	size += 4;

	return size;
}

void Ciffchunk::write_to_file( FILE *fout )
{
	write_U32( fout, CRIFFChunk::ChunkName( 'F', 'O', 'R', 'M' ) );
	hal_write_U32( fout, size );
	write_U32( fout, CRIFFChunk::ChunkName( 'A', 'I', 'F', 'F' ) );
	
	size_t numchildren = children_vect.size();
	for( size_t i=0; i<numchildren; i++ )
	{	children_vect[i]->write_to_file( fout );
	}
	
}

///////////////////////////////////////////////////////////////////////////////

U32 Caiff_comm::calcsize( void )
{
	size = 18; // 0x12
	return size;
}

void Caiff_comm::write_to_file( FILE *fout )
{
	write_U32( fout, chunkID );
	hal_write_U32( fout, size );
	
	hal_write_S16( fout, this->num_channels );
	hal_write_U32( fout, this->num_frames );
	hal_write_S16( fout, this->num_bits );
	hal_write_F80( fout, this->samplerate );
	//hal_write_U32( fout, 0 );
	//hal_write_U16( fout, 0 );
}

///////////////////////////////////////////////////////////////////////////////

U32 Caiff_ssnd::calcsize( void )
{
	U32 numsamps = export_sample->len;
	size = 8 + (numsamps*2);
	return size;
}

void Caiff_ssnd::write_to_file( FILE *fout )
{
	write_U32( fout, chunkID );
	hal_write_U32( fout, size );

	hal_write_U32( fout, this->offset );
	hal_write_U32( fout, this->blocksize );

	for( U32 i=0; i<export_sample->len; i++ )
	{
		S16 sample = export_sample->sounddata[i];
		hal_write_S16( fout, sample );
	}
}

///////////////////////////////////////////////////////////////////////////////

U32 TCaiff_marker::calcsize( void )
{
	U32 rval = 6 + name.count+1+1; // strlen + pad + size + zero
	return rval;
}

U32 Caiff_mark::calcsize( void )
{
	size_t nummarkers = marker_vect.size();
	size = 2;

	for( U32 i=0; i<nummarkers; i++ )
	{
		TCaiff_marker *marker = marker_vect[i];
		size += marker->calcsize();
	}
	return size;
}

void Caiff_mark::write_to_file( FILE *fout )
{
	write_U32( fout, chunkID );
	hal_write_U32( fout, size );

	size_t nummarkers = marker_vect.size();

	hal_write_U16( fout, u16(nummarkers) );

	for( U32 i=0; i<nummarkers; i++ )
	{	TCaiff_marker *marker = marker_vect[i];
		hal_write_U16( fout, marker->mkrID );
		hal_write_U32( fout, marker->pos );
		marker->name.write_to_file( fout );
	}
}

///////////////////////////////////////////////////////////////////////////////

U32 Caiff_inst::calcsize( void )
{
	size	= 6; // basenote detune lonote hinote lovel hivel
	size	+= 2; // gain
	size	+= sizeof( TSaiff_loop ); // susloop
	size	+= sizeof( TSaiff_loop ); // relloop
	
	return size;
}

void Caiff_inst::write_to_file( FILE *fout )
{
	write_U32( fout, chunkID );
	hal_write_U32( fout, size );

	hal_write_U8( fout, this->basenote );
	hal_write_U8( fout, this->detune );
	hal_write_U8( fout, this->lonote );
	hal_write_U8( fout, this->hinote );
	hal_write_U8( fout, this->lovel );
	hal_write_U8( fout, this->hivel );

	hal_write_S16( fout, this->gain );

	hal_write_U16( fout, this->susloop.loop_mode );
	hal_write_U16( fout, this->susloop.mkrID_start );
	hal_write_U16( fout, this->susloop.mkrID_end );

	hal_write_U16( fout, this->relloop.loop_mode );
	hal_write_U16( fout, this->relloop.mkrID_start );
	hal_write_U16( fout, this->relloop.mkrID_end );
}
	
///////////////////////////////////////////////////////////////////////////////

TCiffchunk aiff_main_chunk;
TCaiff_comm aiff_comm_chunk;
TCaiff_ssnd aiff_ssnd_chunk;
TCaiff_mark aiff_mark_chunk;
TCaiff_inst aiff_inst_chunk;
TCaiff_marker start_marker;
TCaiff_marker end_marker;
	
void init_aiff_exporter( void )
{
	aiff_main_chunk.children_vect.push_back( & aiff_comm_chunk );
	aiff_main_chunk.children_vect.push_back( & aiff_mark_chunk );
	aiff_main_chunk.children_vect.push_back( & aiff_inst_chunk );
	aiff_main_chunk.children_vect.push_back( & aiff_ssnd_chunk );

	aiff_main_chunk.chunkID = CRIFFChunk::ChunkName( 'A', 'I', 'F', 'F' );
	aiff_comm_chunk.chunkID = CRIFFChunk::ChunkName( 'C', 'O', 'M', 'M' );
	aiff_ssnd_chunk.chunkID = CRIFFChunk::ChunkName( 'S', 'S', 'N', 'D' );
	aiff_mark_chunk.chunkID = CRIFFChunk::ChunkName( 'M', 'A', 'R', 'K' );
	aiff_inst_chunk.chunkID = CRIFFChunk::ChunkName( 'I', 'N', 'S', 'T' );

	aiff_mark_chunk.marker_vect.push_back( & start_marker );
	aiff_mark_chunk.marker_vect.push_back( & end_marker );

	start_marker.mkrID = 1;
	start_marker.name.set_string( "Loop Start" );

	end_marker.mkrID = 2;
	end_marker.name.set_string( "Loop End" );

	aiff_inst_chunk.susloop.loop_mode = NoLooping;
	aiff_inst_chunk.susloop.mkrID_start = 1;
	aiff_inst_chunk.susloop.mkrID_end = 2; 

}

void write_sample_aiff( const char * fname, TCaiff_exp *exp )
{
	aiff_ssnd_chunk.export_sample = exp;
	aiff_ssnd_chunk.offset = 0;
	aiff_ssnd_chunk.blocksize = 0;
	aiff_comm_chunk.num_frames = exp->len;
	aiff_comm_chunk.samplerate = float(exp->isamplerate);
	aiff_inst_chunk.susloop.loop_mode = exp->mbLoopEnable ? ForwardLooping : NoLooping;

	start_marker.pos = exp->loop_start;
	end_marker.pos = exp->loop_end;

	U32 size = aiff_main_chunk.calcsize();	

	FILE *fout = fopen( fname, "wb" );

	if( fout != NULL )
	{
		aiff_main_chunk.write_to_file( fout );
		fclose( fout );
	}
	else
	{	printf( "ERROR: cannot open %s\n", fname );
	}

}

/*
struct VAGheader {
	unsigned long		format;		// always 'VAGp' for identifying
	unsigned long		ver;		// format version (2)
	unsigned long		ssa;		// Source Start Address, always 0 (reserved for VAB format) 
	unsigned long		size;		// Sound Data Size in byte 

	unsigned long		fs;			// sampling frequency, 44100(>pt1000), 32000(>pt), 22000(>pt0800)... 
	unsigned short		volL;		// base volume for Left channel 
	unsigned short		volR;		// base volume for Right channel 
	unsigned short		pitch;		// base pitch (includes fs modulation)
	unsigned short		ADSR1;		// base ADSR1 (see SPU manual) 
	unsigned short		ADSR2;		// base ADSR2 (see SPU manual) 
	unsigned short		reserved;	// not in use 

	char				name[16];
};
*/

} }
