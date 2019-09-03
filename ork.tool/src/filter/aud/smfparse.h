////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _SMFPARSE_H
#define _SMFPARSE_H

///////////////////////////////////////////////////////////////////////////////

#define MAXCHUNKLENGTH	1048576

namespace ork { namespace tool {

class smfparse_t

{	public: // data

	ifstream *ifile;
	char HABname[256];
	char vbname[256];
	char dbgname[256];
	char dmpname[256];
		
	public: // methods

	smfparse_t( void ); // default constructor
	bool openfilename( STRING fname );
	void getchunks( void );

};

///////////////////////////////////////////////////////////////////////////////

class Criffchunk

{	public: // data

	U32 chunktype;
	S32 chunklength;
	U8	*data;
	char chunkname[5];

	public: // methods

	Criffchunk(); // default constructor

};

///////////////////////////////////////////////////////////////////////////////

class SMFParser

{
	///////////////////////////////////
	public: // data
	///////////////////////////////////

	vector<Criffchunk*>	Chunks;
	CAudioSequence mSequence;
	MidiTimeStamp LastTempoChangeTime;
	F32		 LastTempoChange;
	File mInFile;
	RIFFFile mRiffFile;
	U32 bytesleft;
	U32 numchunks;
	bool done;
	F32 fbpm,	ftpq;
	int bpm,	ticksperquarter;
	U32 tempo;
	int miTimeSigNum;
	int miTimeSigDen;

	///////////////////////////////////
	public: // methods
	///////////////////////////////////

	SMFParser( void ); // default constructor
	bool openfilename( string fname );
	void close( void );
	void getchunks( void );
	void parsetrack( U32 trackchunk );
	void parsemetaevent( CAudioSequenceEvent *ev );
	void parsetracks( void );
	MidiTimeStamp ConvertMidiTime( int absticks ); // we are assuming 4/4 for now
	static U32 readintVAR( U8 *buf, int *size);

};

///////////////////////////////////////////////////////////////////////////////

class SMF2MMTFilter // standard midifile -> orkid midi trax file
{
	public: //

	////////////////////////////////////

	static void ClassInit( CClass *pClass );
	SMF2MMTFilter( CClass *pClass );
	static string GetClassName( void ) { return string("SMF2MMTFilter"); }

	////////////////////////////////////

	static bool ConvertAsset( CObject *pOBJ, AssetFilterContext*pEV );

	////////////////////////////////////

};

} } 

#endif // _PSXMUS_H



















