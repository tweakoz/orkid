////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#if ! defined( _SOUNDFONT_H )
# define _SOUNDFONT_H
           
#define _USE_SOUNDFONT
#if defined(_USE_SOUNDFONT)

#include <orktool/filter/filter.h>
#include <ork/file/riff.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/audiobank.h>

namespace ork { namespace tool {

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

struct Ssfontpreset
{
	char achPresetName[20];	// 20
	S16 wPreset;			// 22
	S16 wBank;				// 24
	S16 wPresetBagNdx;		// 26
	S32 dwLibrary;			// 30
	S32 dwGenre;			// 34
	S32 dwMorphology;		// 38

	Ssfontpreset()
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

	std::string GetName( void ) const { return (std::string) & achPresetName[0]; }
};

////////////////////////////////////////////////////////////////////////////////

struct Ssfontinst
{
	char achInstName[20];
	U16 wInstBagNdx;

	Ssfontinst()
		: wInstBagNdx(0)
	{
		for( int i=0; i<21; i++ )
		{
			achInstName[i] = 0;
		}
	}

	std::string GetName( void ) const { return (std::string) & achInstName[0]; }


};

////////////////////////////////////////////////////////////////////////////////

struct Ssfontsample
{
	U8 achSampleName[20];	// 20
	S32 dwStart;			// 24
	S32 dwEnd;
	S32 dwStartloop;
	S32 dwEndloop;
	S32 dwSampleRate;		// 40
	U8 byOriginalPitch;		// 41
	U8 chPitchCorrection;	// 42
	U16 wSampleLink;		// 44
	U16 sfSampleType;		// 46

};

////////////////////////////////////////////////////////////////////////////////

struct Ssfontinstbag
{
	U16 wInstGenNdx;
	U16 wInstModNdx;

};

////////////////////////////////////////////////////////////////////////////////

struct Ssfontprebag
{
	U16 wInstGenNdx;
	U16 wInstModNdx;

};

////////////////////////////////////////////////////////////////////////////////

struct SSoundFontGenerator
{
	U16 muGeneratorID;
	S16 miGeneratorValue;

};

////////////////////////////////////////////////////////////////////////////////

class CSamplePlatformData
{
	private:

	std::string		mType;

	protected:

	CSamplePlatformData( const std::string &type )
		: mType( type )
	{

	}

	public:

	const std::string GetType( void ) const { return mType; }

};

////////////////////////////////////////////////////////////////////////////////

class CSoundFont;

class CSF2Sample
{
	private:

	int miloopstart;
	int miloopend;
	S16 *mpsampledata;
	int misamplelen;

	public: //
	
	std::string name;	
	S32 start;			// 24
	S32 end;
	S32 samplerate;		// 40
	U8 originalpitch;	// 41
	U8 pitchcorrection;	// 42
	U16 samplelink;		// 44
	U16 sampletype;		// 46
	bool loop;
	int mireaddata_offset;

	bool is_sample_used;
	U32 strip_order;

	CSamplePlatformData*	mpPlatformData;

	/////////////
	/////////////
	

	CSF2Sample( Ssfontsample * smp = 0 )
		: start( smp ? smp->dwStart : 0 )
		, end( smp ? smp->dwEnd : 0 )
		, miloopstart( smp ? smp->dwStartloop : 0 )
		, miloopend( smp ? smp->dwEndloop : 0 )
		, samplerate( smp ? smp->dwSampleRate : 0 )
		, originalpitch( smp ? smp->byOriginalPitch : 0 )
		, pitchcorrection( smp ? smp->chPitchCorrection : 0 )
		, samplelink( smp ? smp->wSampleLink : 0 )
		, sampletype( smp ? smp->sfSampleType : 0 )
		, misamplelen( 0 )
		, mpsampledata( 0 )
		, loop( FALSE )
		, is_sample_used( TRUE )
		, strip_order( 0 )
		, mpPlatformData( 0 )
		, mireaddata_offset( 0 )
	{
		char namebuf[21];
		for( U32 i=0; i<21; i++ )
			namebuf[i] = 0;

		for( U32 i=0; i<20; i++ )
			namebuf[i] = smp ? smp->achSampleName[i] : 0;
			
		name = (std::string) namebuf;

		if( pitchcorrection != 0 )
		{
			printf( "damn, pitchcorrection\n" );
		}
	}

	void ResampleForVag( F64 factor, bool interpFilt, bool largefilter, int & resample_len, S16 * & resample_data );
	U32  ResampleReadData( S16 *outptr, U32 bufsize, U32 xoff );

	void VagCheckResample( void );


	int GetNumSamples( void ) const { return misamplelen; }
	void SetNumSamples( int i ) { misamplelen=i; }

	const S16 *GetSampleData( void ) const { return mpsampledata; }

	int GetLoopStart( void ) const;
	int GetLoopEnd( void ) const;

	void SetLoopStart( int i ) { miloopstart=i; }
	void SetLoopEnd( int i ) { miloopend=i; }

	void SetSampleData( S16 *psmp ) { mpsampledata=psmp; }

	void SetSample( int idx, S16 smp );
	S16  GetSample( int idx ) const ;

};

////////////////////////////////////////////////////////////////////////////////

class CSF2InstrumentZone : public ork::lev2::AudioInstrumentZone
{
	public: //

	int num_generators;
	int base_generator;
	
	int sampleID;
	int presetnum;
	int loop;
	int index;

	bool is_zone_used;
	int strip_order;
	int ipan;

	orkmap<ESF2Generators,S16> mGenerators;

	//////////////////////////////////////////////////////////

	CSF2InstrumentZone()
		: miBaseModulator( 0 )
		, mbGlobalZone( false )
		, num_generators( 0 )
		, base_generator( 0 )
		, sampleID( 0 )
		, presetnum( 0xffffffff )
		, loop( 0 )
		, index( 0xffffffff )
		, is_zone_used( TRUE )
		, strip_order( 0 )
		, ipan( 0 )
	{	
	}

	//////////////////////////////////////////////////////////

	bool IsGlobalZone( void ) const { return (mbGlobalZone); }
	void SetGlobalZone( void ) { mbGlobalZone=true; }

	int GetBaseModulator( void ) const { return miBaseModulator; }
	void SetBaseModulator( int idx ) { miBaseModulator=idx; }

	//////////////////////////////////////////////////////////

	void ApplyGenerator( ESF2Generators egen, S16 GenVal );

	//////////////////////////////////////////////////////////

private:

	bool							mbGlobalZone;
	int								miBaseModulator;

};

////////////////////////////////////////////////////////////////////////////////

class CSF2Instrument
{
	public: //
	
	CSF2Instrument()
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

	const CSF2InstrumentZone& GetIZoneFromIndex( size_t idx ) const
	{
		return mIZones[ idx ];
	}

	const CSF2InstrumentZone* GetIZoneFromVelKey( int ikey, int ivel ) const
	{
		const CSF2InstrumentZone* prval = 0;
		
		size_t inumiz = mIZones.size();

		for( size_t iz=0; iz<inumiz; iz++ )
		{
			const CSF2InstrumentZone & ptestz = mIZones[ iz ];

			if(		(ikey >= ptestz.GetKeyMin())
				&&	(ikey <= ptestz.GetKeyMax())
				&&	(ivel >= ptestz.GetVelMin())
				&&	(ivel <= ptestz.GetVelMax()) )
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
	orkvector<CSF2InstrumentZone>	mIZones;

private:

	std::string		mName;
	size_t			miIndex;

};

///////////////////////////////////////////////////////////////////////////////

class CSF2ProgramZone
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

	CSF2ProgramZone()
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

class CSF2Program
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

	CSF2Program();

	const std::string & GetName( void ) const { return mName; }
	void SetName( const std::string name ) { mName=name; }

	size_t GetNumZones( void ) const { return mPZones.size(); }
	const CSF2ProgramZone & GetZone( size_t idx ) const { return mPZones[ idx ]; }
	void AddZone( const CSF2ProgramZone & zone ) { mPZones.push_back( zone ); }

	orkvector<CSF2ProgramZone>	mPZones;

};



////////////////////////////////////////////////////////////////////////////////

class CSoundFontConversionEngine
{
	public:

	CSoundFontConversionEngine( const std::string & SoundFontName );
	virtual ~CSoundFontConversionEngine();

	void SetSoundFontName( const std::string Name ) { mSoundFontName = Name; }

	virtual void AddProgram( Ssfontpreset *preset );
	virtual void AddSample( Ssfontsample *sample );
	virtual void AddPresetGen( SSoundFontGenerator *pgn );
	virtual void AddInstrumentZone( Ssfontinstbag *ibg );
	virtual void AddInstrument( Ssfontinst *inst );
	virtual void AddInstrumentGen( SSoundFontGenerator *igen );
	virtual void AddProgramZone( Ssfontprebag *pbg );

	inline void Process( void )
	{
		ProcessInstruments();
		ProcessPresets();
	}

	size_t GetSampleBlockSize( void ) const { return mSampleData.size(); }
	S16 GetSampleData( int idx ) const { return mSampleData[idx]; }

	size_t GetNumInstruments( void ) const { return mPXMInstruments.size(); }
	size_t GetNumIZones( void ) const { return mPXMInstrumentZones.size(); }

	size_t GetNumPrograms( void ) const { return mPXMPrograms.size(); }
	size_t GetNumPZones( void ) const { return mPXMProgramZones.size(); }

	size_t GetNumSamples( void ) const { return mPXMSamples.size(); }

	const CSF2Instrument * GetInstrument( size_t idx ) const
	{
		OrkAssert( idx < GetNumInstruments() );
		return mPXMInstruments[ idx ];
	}
	const CSF2InstrumentZone * GetInstrumentZone( size_t idx ) const
	{
		OrkAssert( idx < GetNumIZones() );
		return mPXMInstrumentZones[ idx ];
	}

	const CSF2Program * GetProgram( size_t idx ) const
	{
		OrkAssert( idx < GetNumPrograms() );
		return mPXMPrograms[ idx ];
	}
	const CSF2ProgramZone * GetProgramZone( size_t idx ) const
	{
		OrkAssert( idx < GetNumPZones() );
		return mPXMProgramZones[ idx ];
	}

	const CSF2Sample * GetSample( size_t idx ) const
	{
		OrkAssert( idx < GetNumSamples() );
		return mPXMSamples[ idx ];
	}

	

protected:

	int 								numinst;
	int 								numizones;
	int 								numigen;
	int 								numsamples;
	const int							misampleblockdatalen;

	CRIFFChunk *						root;
	CRIFFChunk *						list_info;
	CRIFFChunk *						list_sdta;
	CRIFFChunk *						list_pdta;

	orkvector<CRIFFChunk*>					mDynamicChunks;

	////////////////////////////////////////////////////
	// Orkid Data Structures

	orkvector<CSF2Program *>			mPXMPrograms;
	orkvector<CSF2Sample *>			mPXMSamples;
	orkvector<CSF2Instrument *>		mPXMInstruments;
	orkvector<CSF2ProgramZone *>		mPXMProgramZones;
	orkvector<SSoundFontGenerator *>	mPXMPresetGen;
	orkvector<CSF2InstrumentZone *>	mPXMInstrumentZones;
	orkvector<SSoundFontGenerator *>	mPXMInstrumentGen;

	orkvector<S16>					mSampleData;

	////////////////////////////////////////////////////

	S16 *								sampledata;

	U32									sizofsmp;
	U32									numprebags;
	U32									numpresets;

	std::string							mSoundFontName;

	////////////////////////////////////////////////////////////

	U32 GetSBFK( CRIFFChunk *pROOT );
	U32 GetINFOChunk( CRIFFChunk *ParChunk, U32 offset );
	U32 GetINFOList( CRIFFChunk* ParChunk, U32 offset );
	U32 GetSDTAChunk( CRIFFChunk *ParChunk, U32 offset );
	U32 GetSDTAList( CRIFFChunk* ParChunk, U32 offset ) ;
	U32 GetPDTAList( CRIFFChunk* ParChunk, U32 offset );
	U32 GetPDTAChunk( CRIFFChunk* ParChunk, U32 offset );

	virtual void ProcessInstruments( void );
	virtual void ProcessPresets( void );

};

////////////////////////////////////////////////////////////////////////////////

class CSoundFont //: public CObject
{

	public:

	//static CClass *gpClass;

	//static void ClassInit( CClass *pclass );

	CSoundFont( CSoundFontConversionEngine *pengine );
	CSoundFont( /*CClass *pclass*/ );
	
	//////////////////////////////////////

	const std::string & GetName( void ) const { return mSoundFontName; }

	const CSF2Sample * GetSample( int idx ) const;

	S16* GetSampleData( int idx ) const
	{
		OrkAssert( idx < miSampleBlockLength );
		return & mpSampleData[ idx ];
	}

	//////////////////////////////////////

	void SetName( const std::string &name ) { mSoundFontName=name; }

	//////////////////////////////////////

	const CSF2Instrument * GetInstrument( int idx ) const;

	//////////////////////////////////////

	static void AddSearchPath( const std::string & srchpath );
	static void LoadSearchPaths( void );

	static std::string FindSoundFont( const std::string & named );

	//////////////////////////////////////

	//static CChoiceManager				mSF2IntrumentChoices;

private:

	std::string							mSoundFontName;
	orkvector<CSF2Instrument>			mSF2Instruments;
	orkvector<CSF2InstrumentZone>		mSF2InstrumentZones;
	orkvector<CSF2Sample>				mSF2Samples;
	s16*								mpSampleData;
	int									miSampleBlockLength;

	static std::set<std::string>		mSearchPaths;
};

////////////////////////////////////////////////////////////////////////////////

class SF2XABFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(SF2XABFilter,CAssetFilterBase);
public: //
	SF2XABFilter(  );
	virtual bool ConvertAsset( const tokenlist& toklist );
};

////////////////////////////////////////////////////////////////////////////////

class SF2GABFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(SF2GABFilter,CAssetFilterBase);
public: //
	SF2GABFilter(  );
	virtual bool ConvertAsset( const tokenlist& toklist );
};

////////////////////////////////////////////////////////////////////////////////

} }

#endif // _USE_SOUNDFONT
#endif
