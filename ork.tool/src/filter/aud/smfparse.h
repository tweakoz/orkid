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

class TCsmfparse

{	public: // data

	ifstream *ifile;
	char HABname[256];
	char vbname[256];
	char dbgname[256];
	char dmpname[256];
		
	public: // methods

	TCsmfparse( void ); // default constructor
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

class CSMFParser

{
	///////////////////////////////////
	public: // data
	///////////////////////////////////

	vector<Criffchunk*>	Chunks;
	CAudioSequence mSequence;
	MidiTimeStamp LastTempoChangeTime;
	F32		 LastTempoChange;
	CFile mInFile;
	CRIFFFile mRiffFile;
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

	CSMFParser( void ); // default constructor
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

class CSMF2MMTFilter // standard midifile -> orkid midi trax file
{
	public: //

	////////////////////////////////////

	static void ClassInit( CClass *pClass );
	CSMF2MMTFilter( CClass *pClass );
	static string GetClassName( void ) { return string("CSMF2MMTFilter"); }

	////////////////////////////////////

	static bool ConvertAsset( CObject *pOBJ, CAssetFilterContext*pEV );

	////////////////////////////////////

};

} } 

#endif // _PSXMUS_H



















