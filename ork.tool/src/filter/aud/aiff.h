////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _AUDIO_DATAFILTER_AIFF_H
#define _AUDIO_DATAFILTER_AIFF_H

namespace ork { namespace tool {

///////////////////////////////////////////////////////////////////////////////

typedef class aiff_exp
{
	public: //
		
	const S16 *sounddata;
	U32 len;
	U32 loop_start;
	U32 loop_end;
	int isamplerate;
	bool mbLoopEnable;

	aiff_exp()
	{
		sounddata = 0;
		len = 0;
		loop_start = 0;
		loop_end = 0;
		isamplerate = 44100;
		mbLoopEnable = false;
	}

} Taiff_exp;

///////////////////////////////////////////////////////////////////////////////

typedef class Cpstring
{
	public: //
		
	U8 count;
	U8 tsize;
	U8 pad;

	STRING string;

	void set_string( STRING newstring );
	void write_to_file( FILE *fout );

} TCpstring;

///////////////////////////////////////////////////////////////////////////////

typedef class iff_chunk iff_chunk_t;

typedef class iff_chunk
{
	public: //
		
	U32 chunkID; // #define RIFFChunk::ChunkName()
	U32 size;
	void *data;
	orkvector< iff_chunk_t * > children_vect;

	virtual U32 calcsize( void );
	virtual void write_to_file( FILE *fout );

} iff_chunk_t;

///////////////////////////////////////////////////////////////////////////////

typedef class aiff_comm : public iff_chunk_t
{
	public: //

	S16 num_channels;
	U32	num_frames;
	S16 num_bits;
	double	samplerate;

	virtual U32 calcsize( void );
	virtual void write_to_file( FILE *fout );

	aiff_comm()
	{
		num_channels = 1;
		num_bits = 16;
		samplerate = 0.0;
		num_frames = 0;
	}

} Taiff_comm;

///////////////////////////////////////////////////////////////////////////////

typedef class aiff_ssnd : public iff_chunk_t
{
	public: //

	U32 offset;
	U32 blocksize;
	// sample[numsamps];

	virtual U32 calcsize( void );
	virtual void write_to_file( FILE *fout );

	Taiff_exp *export_sample;

	aiff_ssnd()
	{
		offset = 0;
		blocksize = 0;
		export_sample = 0;
	}

} Taiff_ssnd;

///////////////////////////////////////////////////////////////////////////////

typedef class aiff_marker
{
	public: //
		
	U16 mkrID;
	U32 pos;
	TCpstring name;

	U32 calcsize( void );

} Taiff_marker;

typedef class aiff_mark : public iff_chunk_t
{
	public: //

	//U16 num_markers;
	orkvector< Taiff_marker * > marker_vect;

	aiff_mark()
	{
	}

	virtual U32 calcsize( void );
	virtual void write_to_file( FILE *fout );
	
} Taiff_mark;

///////////////////////////////////////////////////////////////////////////////

#define NoLooping 0
#define ForwardLooping 1
#define ForwardBackwardLooping 2

typedef struct Saiff_loop
{
	U16 loop_mode;
	U16 mkrID_start;
	U16 mkrID_end;

} TSaiff_loop;

typedef class aiff_inst : public iff_chunk_t
{
	public: //

	U8 basenote;
	U8 detune;
	U8 lonote;
	U8 hinote;
	U8 lovel;
	U8 hivel;
	S16 gain;
	TSaiff_loop susloop;
	TSaiff_loop relloop;
	//int isamplerate;

	virtual U32 calcsize( void );
	virtual void write_to_file( FILE *fout );

	aiff_inst()
	{
		basenote = 60;
		detune = 0;
		lonote = 0;
		hinote = 127;
		lovel = 0;
		hivel = 127;
		gain = 0;
		susloop.loop_mode = NoLooping;
		susloop.mkrID_start = 0xffff;
		susloop.mkrID_end = 0xffff;
		relloop.loop_mode = NoLooping;
		relloop.mkrID_start = 0xffff;
		relloop.mkrID_end = 0xffff;
		
	}

} Taiff_inst;


void write_sample_aiff( const char * fname, Taiff_exp *exp );

///////////////////////////////////////////////////////////////////////////////

} }

#endif
