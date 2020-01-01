////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/riff.h>

namespace ork::audio::singularity {
    struct VastObjectsDB;
}

namespace ork::audio::singularity::sf2 {


////////////////////////////////////////////////////////////////////////////////

enum ESF2Generators
{
	//ESF2GEN_SAMPLE_START_OFFSET = 0,	// offset in samples beyond start sample header
	//ESF2GEN_SAMPLE_END_OFFSET = 1,		// offset in samples beyond start sample header

	ESF2GEN_MODENV_TO_PITCH = 7,		// mod env to pitch (cents)
	ESF2GEN_MODENV_TO_FC = 11,			// mod env to filter cutoff (cents)

	ESF2GEN_BASE_FILTER_CUTOFF = 8,		// filter cutoff base freq (absolute cents)
	ESF2GEN_BASE_FILTER_Q = 9,			// filter Q base (DC gain in centibels)

	ESF2GEN_ZONE_PAN = 17,				// .1% units , range == -50% .. 50%

	ESF2GEN_MODLFO_DELAY = 21,			// mod lfo start delay (abs timecents) 0==1 second
	ESF2GEN_MODLFO_FREQ = 22,			// mod lfo freq (abs cents) 0==8.167 hz
	ESF2GEN_MODLFO_TO_PITCH = 5,		// mod lfo to pitch (cents)
	ESF2GEN_MODLFO_TO_FC = 10,			// mod lfo to filter cutoff (cents)
	ESF2GEN_MODLFO_TO_AMP = 13,			// mod lfo to amp (centibels)

	ESF2GEN_VIBLFO_DELAY = 23,			// mod lfo start delay (abs timecents) 0==1 second
	ESF2GEN_VIBLFO_FREQ = 24,			// mod lfo freq (abs cents) 0==8.167 hz
	ESF2GEN_VIBLFO_TO_PITCH = 6,		// vib lfo to pitch (cents)

	ESF2GEN_MODENV_DELAY = 25,			// delay(timecents) between keyon and attack phase of vol env.
	ESF2GEN_MODENV_ATTACK = 26,			// attack(timecents)
	ESF2GEN_MODENV_HOLD = 27,			// hold time(timecents)
	ESF2GEN_MODENV_DECAY = 28,			// decay time(timecents)
	ESF2GEN_MODENV_SUSTAIN = 29,		// sustain level(centibels)
	ESF2GEN_MODENV_RELEASE = 30,		// release_time(timecents)
	ESF2GEN_MODENV_KEYNUMTOHOLD = 31,	// keynum->+holdtime (timecents)
	ESF2GEN_MODENV_KEYNUMTODECAY = 32,	// keynum->+decaytime (timecents)

	ESF2GEN_AMPENV_DELAY = 33,			// delay(timecents) between keyon and attack phase of vol env.
	ESF2GEN_AMPENV_ATTACK = 34,			// attack(timecents)
	ESF2GEN_AMPENV_HOLD = 35,			// hold time(timecents)
	ESF2GEN_AMPENV_DECAY = 36,			// decay time(timecents)
	ESF2GEN_AMPENV_SUSTAIN = 37,		// sustain level(centibels)
	ESF2GEN_AMPENV_RELEASE = 38,		// release_time(timecents)
	ESF2GEN_AMPENV_KEYNUMTOHOLD = 39,	// keynum->+holdtime (timecents)
	ESF2GEN_AMPENV_KEYNUMTODECAY = 40,	// keynum->+decaytime (timecents)

	ESF2GEN_INSTRUMENT_ID = 41,			// instrument index

	ESF2GEN_KEYRANGE = 43,
	ESF2GEN_VELRANGE = 44,

	ESF2GEN_KEYNUM_OVERRIDE = 46,		// override keynum (instrument level only)
	ESF2GEN_VELOCITY_OVERRIDE = 47,		// override velocity (instrument level only)

	ESF2GEN_BASE_ATTENUATION = 48,		// base attenuation in centibels, 0==no atten, 60==-6dB

	ESF2GEN_TUNE_SEMIS = 51,			// pitch offset coarse (semitones)
	ESF2GEN_TUNE_CENTS = 52,			// pitch offset fine (cents)

	ESF2GEN_SAMPLE_ID = 53,
	ESF2GEN_SAMPLE_LOOPMODE = 54,		// 0==no loop, 1==continuous loop, 2==no loop, 3==gated loop

	ESF2GEN_TUNE_SCALE = 56,			// (cents/key) MIDI keynum -> pitch mod

	ESF2GEN_MUTEX_GROUP = 57,			// mutual exclusion group ID (instrument only)

	ESF2GEN_OVERRIDEROOTKEY = 58,
};


////////////////////////////////////////////////////////////////////////////////

struct sfontpreset
{
	char achPresetName[20];	// 20
	S16 wPreset;			// 22
	S16 wBank;				// 24
	S16 wPresetBagNdx;		// 26
	S32 dwLibrary;			// 30
	S32 dwGenre;			// 34
	S32 dwMorphology;		// 38

	sfontpreset()
		: wPreset(0)
		, wBank(0)
		, wPresetBagNdx(0)
		, dwLibrary(0)
		, dwGenre(0)
		, dwMorphology(0)
	{
		for( int i=0; i<20; i++ )
		{
			achPresetName[i] = 0;
		}
	}

	std::string GetName( void ) const {
		char name[21];
		for( int i=0; i<20; i++ )
			name[i] = achPresetName[i];
		name[20] = 0;
		auto strname =  (std::string) & name[0];
		//printf( "prst name<%s>\n", strname.c_str() );
		return strname;
	}
};

////////////////////////////////////////////////////////////////////////////////

struct sfontinst
{
	char achInstName[20];
	U16 wInstBagNdx;

	sfontinst()
		: wInstBagNdx(0)
	{
		for( int i=0; i<21; i++ )
		{
			achInstName[i] = 0;
		}
	}

	std::string GetName( void ) const {
		char name[21];
		for( int i=0; i<20; i++ )
			name[i] = achInstName[i];
		name[20] = 0;
		auto strname =  (std::string) & name[0];
		//printf( "inst name<%s>\n", strname.c_str() );
		return strname;
	}


};

////////////////////////////////////////////////////////////////////////////////

struct sfontsample
{
	U8 achSampleName[20];	// 20
	S32 dwStart;			// 24
	S32 dwEnd;
	S32 dwStartloop;
	S32 dwEndloop;
	S32 dwSampleRate;		// 40
	U8 byOriginalPitch;		// 41
	S8 chPitchCorrection;	// 42
	U16 wSampleLink;		// 44
	U16 sfSampleType;		// 46

};

////////////////////////////////////////////////////////////////////////////////

struct sfontinstbag
{
	U16 wInstGenNdx;
	U16 wInstModNdx;

};

////////////////////////////////////////////////////////////////////////////////

struct sfontprebag
{
	U16 wInstGenNdx;
	U16 wInstModNdx;

};

////////////////////////////////////////////////////////////////////////////////

struct SoundFontGenerator
{
	U16 muGeneratorID;
	S16 miGeneratorValue;

};

////////////////////////////////////////////////////////////////////////////////

struct SF2Sample
{
	SF2Sample( sfontsample * smp = nullptr );

	std::string name;


	int start;			// 24
	int end;
	int loopstart;
	int loopend;

	int samplerate;		// 40
	int originalpitch;	// 41
	int pitchcorrection;	// 42
	int samplelink;		// 44
	int sampletype;		// 46

	/////////////

};

////////////////////////////////////////////////////////////////////////////////

struct InstrumentZone
{
	int num_generators;
	int base_generator;

	int sampleID;
	int presetnum;
	int _loopModeOverride;
	int index;

	bool is_zone_used;
	int strip_order;
	int ipan;
	int mutexgroup;
	int keymin, keymax;
	int velmin, velmax;
	int samplerootoverride;
	int coarsetune; // semis
	int finetune; // cents
	int atten; // centibels
	float pan;
	float filterBaseCutoff;
	float filterBaseQ;

	float modLfoDelay;
	float modLfoFreq;
	float modLfoToPitch;
	float modLfoToCutoff;
	float modLfoToAmp;

	float vibLfoDelay;
	float vibLfoFreq;
	float vibLfoToPitch;

	float ampEnvDelay;
	float ampEnvAttack;
	float ampEnvHold;
	float ampEnvDecay;
	float ampEnvSustain;
	float ampEnvRelease;
	float ampEnvKeyTrackHold;
	float ampEnvKeyTrackDecay;

	float modEnvDelay;
	float modEnvAttack;
	float modEnvHold;
	float modEnvDecay;
	float modEnvSustain;
	float modEnvRelease;
	float modEnvKeyTrackHold;
	float modEnvKeyTrackDecay;

	std::map<ESF2Generators,S16> mGenerators;

	bool mbGlobalZone;
	int	miBaseModulator;

	//////////////////////////////////////////////////////////

	InstrumentZone();

	//////////////////////////////////////////////////////////

	bool IsGlobalZone( void ) const { return (mbGlobalZone); }
	void SetGlobalZone( void ) { mbGlobalZone=true; }

	int GetBaseModulator( void ) const { return miBaseModulator; }
	void SetBaseModulator( int idx ) { miBaseModulator=idx; }

	//////////////////////////////////////////////////////////

	void ApplyGenerator( ESF2Generators egen, S16 GenVal );

	//////////////////////////////////////////////////////////


};

////////////////////////////////////////////////////////////////////////////////

struct SF2Instrument
{
	SF2Instrument()
		: izone_base(0)
		, num_izones(0)
		, miIndex(0)
	{

	}

	size_t GetIndex( void ) const { return miIndex; }
	void SetIndex( size_t idx ) { miIndex = idx; }
	const std::string & GetName( void ) const { return mName; }
	void SetName( const std::string name ) { mName=name; }

	//////////////////////////////////////////////////////////////////////////////

	const InstrumentZone& GetIZoneFromIndex( size_t idx ) const
	{
		return mIZones[ idx ];
	}

	const InstrumentZone* GetIZoneFromVelKey( int ikey, int ivel ) const
	{
		const InstrumentZone* prval = 0;

		size_t inumiz = mIZones.size();

		for( size_t iz=0; iz<inumiz; iz++ )
		{
			const InstrumentZone & ptestz = mIZones[ iz ];

			if(		(ikey >= ptestz.keymin)
				&&	(ikey <= ptestz.keymax)
				&&	(ivel >= ptestz.velmin)
				&&	(ivel <= ptestz.velmax) )
			{
				prval = & ptestz;
			}
		}

		return prval;
	}

	size_t GetNumZones( void ) const { return mIZones.size(); }

	//////////////////////////////////////////////////////////////////////////////

	size_t							izone_base;
	size_t							num_izones;
	std::vector<InstrumentZone>	mIZones;

	std::string		mName;
	size_t			miIndex;

};

///////////////////////////////////////////////////////////////////////////////

class SF2ProgramZone
{
	public: //

	int num_generators;
	int base_generator;
	int *generator_ids;
	int *generator_values;

	int key_min;
	int key_max;
	int vel_min;
	int vel_max;

	int instrumentID;

	SF2ProgramZone()
		: num_generators( 0 )
		, base_generator( 0 )
		, generator_ids( 0 )
		, generator_values( 0 )
		, key_min( 0 )
		, key_max( 127 )
		, vel_min( 0 )
		, vel_max( 127 )
		, instrumentID( -1 )
	{
	}

};

///////////////////////////////////////////////////////////////////////////////

class SF2Program
{
	public: //

	std::string		mName;

	int preset;			// 22
	int bank;			// 24
	int pbag_base;		// 26
	int library;		// 30
	int genre;			// 34
	int morphology;		// 38

	int num_pbags;

	int mapped_preset;

	SF2Program();

	const std::string & GetName( void ) const { return mName; }
	void SetName( const std::string name ) { mName=name; }

	size_t GetNumZones( void ) const { return mPZones.size(); }
	const SF2ProgramZone & GetZone( size_t idx ) const { return mPZones[ idx ]; }
	void AddZone( const SF2ProgramZone & zone ) { mPZones.push_back( zone ); }

	std::vector<SF2ProgramZone>	mPZones;

};



////////////////////////////////////////////////////////////////////////////////

struct SoundFont
{
	SoundFont( const std::string & SoundFontName, const std::string& bankname = "sf2" );
	~SoundFont();

	void SetName( const std::string Name ) { mSoundFontName = Name; }

	void AddProgram( sfontpreset *preset );
	void AddSample( sfontsample *sample );
	void AddPresetGen( SoundFontGenerator *pgn );
	void AddInstrumentZone( sfontinstbag *ibg );
	void AddInstrument( sfontinst *inst );
	void AddInstrumentGen( SoundFontGenerator *igen );
	void AddProgramZone( sfontprebag *pbg );

	inline void Process( void )
	{
		ProcessInstruments();
		ProcessPresets();
	}

	//size_t GetSampleBlockSize( void ) const { return mSampleData.size(); }
	//S16 GetSampleData( int idx ) const { return mSampleData[idx]; }

	size_t GetNumInstruments( void ) const { return mPXMInstruments.size(); }
	size_t GetNumIZones( void ) const { return mPXMInstrumentZones.size(); }

	size_t GetNumPrograms( void ) const { return mPXMPrograms.size(); }
	size_t GetNumPZones( void ) const { return mPXMProgramZones.size(); }

	size_t GetNumSamples( void ) const { return mPXMSamples.size(); }

	const SF2Instrument * GetInstrument( size_t idx ) const
	{
		OrkAssert( idx < GetNumInstruments() );
		return mPXMInstruments[ idx ];
	}
	const InstrumentZone * GetInstrumentZone( size_t idx ) const
	{
		OrkAssert( idx < GetNumIZones() );
		return mPXMInstrumentZones[ idx ];
	}

	const SF2Program * GetProgram( size_t idx ) const
	{
		OrkAssert( idx < GetNumPrograms() );
		return mPXMPrograms[ idx ];
	}
	const SF2ProgramZone * GetProgramZone( size_t idx ) const
	{
		OrkAssert( idx < GetNumPZones() );
		return mPXMProgramZones[ idx ];
	}

	const SF2Sample * GetSample( size_t idx ) const
	{
		OrkAssert( idx < GetNumSamples() );
		return mPXMSamples[ idx ];
	}

	void genZpmDB();

	std::string							_bankName;

	int 								numinst;
	int 								numizones;
	int 								numigen;
	int 								numsamples;

	RIFFChunk *						root;
	RIFFChunk *						list_info;
	RIFFChunk *						list_sdta;
	RIFFChunk *						list_pdta;

	std::vector<RIFFChunk*>					mDynamicChunks;

	////////////////////////////////////////////////////
	// Orkid Data Structures

	std::vector<SF2Program *>			mPXMPrograms;
	std::vector<SF2Sample *>			mPXMSamples;
	std::vector<SF2Instrument *>		mPXMInstruments;
	std::vector<SF2ProgramZone *>		mPXMProgramZones;
	std::vector<SoundFontGenerator *>	mPXMPresetGen;
	std::vector<InstrumentZone *>	mPXMInstrumentZones;
	std::vector<SoundFontGenerator *>	mPXMInstrumentGen;

	//std::vector<S16>					mSampleData;

	VastObjectsDB* _zpmDB;

	////////////////////////////////////////////////////

	S16*								_chunkOfSampleData;
	int							        _sampleDataNumSamples;

	U32									sizofsmp;
	U32									numprebags;
	U32									numpresets;

	std::string							mSoundFontName;

	////////////////////////////////////////////////////////////

	U32 GetSBFK( RIFFChunk *pROOT );
	U32 GetINFOChunk( RIFFChunk *ParChunk, U32 offset );
	U32 GetINFOList( RIFFChunk* ParChunk, U32 offset );
	U32 GetSDTAChunk( RIFFChunk *ParChunk, U32 offset );
	U32 GetSDTAList( RIFFChunk* ParChunk, U32 offset ) ;
	U32 GetPDTAList( RIFFChunk* ParChunk, U32 offset );
	U32 GetPDTAChunk( RIFFChunk* ParChunk, U32 offset );

	virtual void ProcessInstruments( void );
	virtual void ProcessPresets( void );

};

////////////////////////////////////////////////////////////////////////////////

}
