////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _AUD_AUDIOBANK_H
#define _AUD_AUDIOBANK_H

#include <ork/math/cvector4.h>
#include <ork/asset/Asset.h>
#include <ork/util/endian.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

class AudioSample
{
public:

	enum EDataFormat
	{
		EDF_UNCOMPRESSED = 0,	// Uncompressed Signed 16bit
		EDF_SONYADPCM,			// SONY ADPCM
		EDF_GCADPCM,			// GameCube/Wii ADPCM
		EDF_END,
	};

	//////////////////////////////////////////////////////////

	AudioSample()
		: meDataFormat( EDF_END )
		, mpData( 0 )
		, miNumSamples( 0 )
		, miSampleRate( 0 )
		, miRootKey( 0 )
		, mpSampleName( 0 )
		, mpPlatformHandle( 0 )
		, miLoopEnable( 0 )
		, miCompressedLength( 0 )
	{

	}

	//////////////////////////////////////////////////////////

	int GetSampleRate( void ) const { return miSampleRate; }
	int GetDataLength( void ) const;
	const void *GetDataPointer( void ) const { return mpData; }

	void  SetPlatformHandle( void * ph ) { mpPlatformHandle = ph; }
	const void* GetPlatformHandle( void  ) const { return mpPlatformHandle; }

	EDataFormat GetDataFormat( void ) const { return meDataFormat; }

	int GetNumChannels( void ) const { return 1; }
	int GetRootKey() const { return miRootKey; }
	int GetLoopStart( void ) const { return miLoopStart; }
	int GetLoopEnd( void ) const { return miLoopEnd; }

	bool IsLooped( void ) const { return miLoopEnable!=0; }

	const char *GetSampleName( void ) const { return mpSampleName; }

	void SetCompressedLength(int icl) { miCompressedLength=icl; }
	int GetCompressedLength() const { return miCompressedLength; }

	//////////////////////////////////////////////////////////

	void SetSampleName( const char *pname ) { mpSampleName=pname; }
	void SetSampleRate( int irate ) { miSampleRate=irate; }
	void SetDataLength( int ilen ) { miDataLength=ilen; }
	void SetNumSamples( int isamps ) { miNumSamples=isamps; }
	void SetRootKey( int ikey ) { miRootKey=ikey; }
	void SetFormat( EDataFormat efmt ) { meDataFormat = efmt; }
	void SetDataPointer( const void *pdata ) { mpData=pdata; }

	void SetPitchCorrection( int ipcor ) { miPitchCorrection=ipcor; }
	void SetLoopStart( int iloopstart ) { miLoopStart=iloopstart; }
	void SetLoopEnd( int iloopend ) { miLoopEnd=iloopend; }
	void SetLoopEnable( bool bloopena ) { miLoopEnable=int(bloopena); }

	//////////////////////////////////////////////////////////

	void EndianSwap()
	{
		swapbytes_dynamic( meDataFormat );
		swapbytes_dynamic( mpSampleName );
		swapbytes_dynamic( mpData );
		swapbytes_dynamic( mpPlatformHandle );
		swapbytes_dynamic( miDataLength );
		swapbytes_dynamic( miNumSamples );
		swapbytes_dynamic( miSampleRate );
		swapbytes_dynamic( miRootKey );
		swapbytes_dynamic( miCompressedLength );
		swapbytes_dynamic( miPitchCorrection );
		swapbytes_dynamic( miLoopStart );
		swapbytes_dynamic( miLoopEnd );
		swapbytes_dynamic( miLoopEnable );
	}

private:

	EDataFormat	meDataFormat;
	const char*	mpSampleName;
	const void*	mpData;
	void*		mpPlatformHandle;

	int32_t		miDataLength;
	int32_t		miNumSamples;
	int32_t		miSampleRate;
	int32_t		miRootKey;
	int32_t		miPitchCorrection;
	int32_t		miLoopStart;
	int32_t		miLoopEnd;
	int32_t		miLoopEnable;
	int32_t		miCompressedLength;
};

///////////////////////////////////////////////////////////////////////////////

class AudioInstrumentZone
{
public:

	AudioInstrumentZone();
	AudioInstrumentZone(const AudioInstrumentZone& oth );

	/////////////////////////////////////////////////////////////////////

	void SetSample( AudioSample *psamp ) { mpSample=psamp; }

	void SetKeyMin( int32_t ikeymin ) { mikeymin=ikeymin; }
	void SetKeyMax( int32_t ikeymax ) { mikeymax=ikeymax; }
	void SetVelMin( int32_t ivelmin ) { mivelmin=ivelmin; }
	void SetVelMax( int32_t ivelmax ) { mivelmin=ivelmax; }

	void SetPan( float fv ) { mfPan=fv; }
	void SetTuneSemis( int32_t fv ) { miTuneSemis=fv; }
	void SetTuneCents( int32_t fv ) { miTuneCents=fv; }
	void SetAttenCentibels( float atten ) { mfAttenCentibels = atten; }
	void SetSampleRootOverride( int32_t ior ) { miSampleRootOverride=ior; }
	void SetMutexGroup( int32_t img ) { miMutexGroup = img; }

	void SetFilterCutoff(float fv) { mfFilterCutoff=fv; }
	void SetFilterQ(float fv) { mfFilterQ=fv; }

	void SetAmpEnvDelay( float ftime ) { mfAmpEnvDelay=ftime; }
	void SetAmpEnvAttack( float ftime ) { mfAmpEnvAttack=ftime; }
	void SetAmpEnvHold( float ftime ) { mfAmpEnvHold=ftime; }
	void SetAmpEnvDecay( float ftime ) { mfAmpEnvDecay=ftime; }
	void SetAmpEnvSustain( float flevel ) { mfAmpEnvSustain=flevel; }
	void SetAmpEnvRelease( float ftime ) { mfAmpEnvRelease=ftime; }
	void SetAmpEnvKeyNumToHold( float ftimedel ) { mfAmpEnvKeyNumToHold=ftimedel; }
	void SetAmpEnvKeyNumToDecay( float ftimedel ) { mfAmpEnvKeyNumToDecay=ftimedel; }

	void SetModEnvDelay( float ftime ) { mfModEnvDelay=ftime; }
	void SetModEnvAttack( float ftime ) { mfModEnvAttack=ftime; }
	void SetModEnvHold( float ftime ) { mfModEnvHold=ftime; }
	void SetModEnvDecay( float ftime ) { mfModEnvDecay=ftime; }
	void SetModEnvSustain( float flevel ) { mfModEnvSustain=flevel; }
	void SetModEnvRelease( float ftime ) { mfModEnvRelease=ftime; }
	void SetModEnvKeyNumToHold( float ftimedel ) { mfModEnvKeyNumToHold=ftimedel; }
	void SetModEnvKeyNumToDecay( float ftimedel ) { mfModEnvKeyNumToDecay=ftimedel; }

	void SetModLfoDelay( float fv ) { mfModLfoDelay=fv; }
	void SetModLfoFrequency( float fv ) { mfModLfoFrequency=fv; }
	void SetModLfoToPitch( float fv ) { mfModLfoToPitch=fv; }
	void SetModLfoToCutoff( float fv ) { mfModLfoToCutoff=fv; }
	void SetModLfoToAmp( float fv ) { mfModLfoToAmp=fv; }
	
	void SetVibLfoDelay( float fv ) { mfVibLfoDelay=fv; }
	void SetVibLfoFrequency( float fv ) { mfVibLfoFrequency=fv; }
	void SetVibLfoToPitch( float fv ) { mfVibLfoToPitch=fv; }

	/////////////////////////////////////////////////////////////////////

	AudioSample* GetSample( void ) { return mpSample; }

	int32_t  GetKeyMin( void ) const { return mikeymin; }
	int32_t  GetKeyMax( void ) const { return mikeymax; }
	int32_t  GetVelMin( void ) const { return mivelmin; }
	int32_t  GetVelMax( void ) const { return mivelmax; }

	float GetPan() const { return mfPan; }
	int32_t GetTuneSemis( ) const { return miTuneSemis; }
	int32_t GetTuneCents( ) const { return miTuneCents; }
	float GetAttenCentibels( ) const { return mfAttenCentibels; }
	int32_t GetSampleRootOverride( ) const { return miSampleRootOverride; }
	int32_t GetMutexGroup() const { return miMutexGroup; }

	float GetFilterCutoff() const { return mfFilterCutoff; }
	float GetFilterQ() const { return mfFilterQ; }

	float GetAmpEnvDelay(  ) const { return mfAmpEnvDelay; }
	float GetAmpEnvAttack(  ) const { return mfAmpEnvAttack; }
	float GetAmpEnvHold(  ) const { return mfAmpEnvHold; }
	float GetAmpEnvDecay(  ) const { return mfAmpEnvDecay; }
	float GetAmpEnvSustain( float flevel ) { return mfAmpEnvSustain; }
	float GetAmpEnvRelease(  ) const { return mfAmpEnvRelease; }
	float GetAmpEnvKeyNumToHold( float ftimedel ) const { return mfAmpEnvKeyNumToHold; }
	float GetAmpEnvKeyNumToDecay( float ftimedel ) const { return mfAmpEnvKeyNumToDecay; }

	float GetModEnvDelay(  ) const { return mfModEnvDelay; }
	float GetModEnvAttack(  ) const { return mfModEnvAttack; }
	float GetModEnvHold(  ) const { return mfModEnvHold; }
	float GetModEnvDecay(  ) const { return mfModEnvDecay; }
	float GetModEnvSustain( float flevel ) { return mfModEnvSustain; }
	float GetModEnvRelease(  ) const { return mfModEnvRelease; }
	float GetModEnvKeyNumToHold( ) const { return mfModEnvKeyNumToHold; }
	float GetModEnvKeyNumToDecay( ) const { return mfModEnvKeyNumToDecay; }

	float GetModLfoDelay( ) const { return mfModLfoDelay; }
	float GetModLfoFrequency(  ) const { return mfModLfoFrequency; }
	float GetModLfoToPitch( ) const { return mfModLfoToPitch; }
	float GetModLfoToCutoff( ) const { return mfModLfoToCutoff; }
	float GetModLfoToAmp( ) const { return mfModLfoToAmp; }
	
	float GetVibLfoDelay( ) const { return mfVibLfoDelay; }
	float GetVibLfoFrequency(  ) const { return mfVibLfoFrequency; }
	float GetVibLfoToPitch( ) const { return mfVibLfoToPitch; }
	
	/////////////////////////////////////////////////////////////////////

	void EndianSwap();

	/////////////////////////////////////////////////////////////////////

private:

	int32_t		mikeymin;
	int32_t		mikeymax;
	int32_t		mivelmin;
	int32_t		mivelmax;
	float		mfPan;
	int32_t		miTuneSemis;
	int32_t		miTuneCents;
	int32_t		miSampleRootOverride;
	float		mfAttenCentibels;
	int32_t		miMutexGroup;
	float		mfFilterCutoff;
	float		mfFilterQ;

	float	mfAmpEnvDelay;
	float	mfAmpEnvAttack;
	float	mfAmpEnvHold;
	float	mfAmpEnvDecay;
	float	mfAmpEnvSustain;
	float	mfAmpEnvRelease;
	float	mfAmpEnvKeyNumToHold;
	float	mfAmpEnvKeyNumToDecay;

	float	mfModEnvDelay;
	float	mfModEnvAttack;
	float	mfModEnvHold;
	float	mfModEnvDecay;
	float	mfModEnvSustain;
	float	mfModEnvRelease;
	float	mfModEnvKeyNumToHold;
	float	mfModEnvKeyNumToDecay;

	float	mfModLfoDelay;
	float	mfModLfoFrequency;
	float	mfModLfoToPitch;
	float	mfModLfoToCutoff;
	float	mfModLfoToAmp;

	float	mfVibLfoDelay;
	float	mfVibLfoFrequency;
	float	mfVibLfoToPitch;

	AudioSample*		mpSample;
};

/////////////////////////////////////////////////////////////////////////////////

class AudioInstrument
{
public:

	AudioInstrument()
	{

	}

	AudioInstrumentZone& RefZone( int idx )	{ return mpInstrumentZones[ idx ]; }
	void SetNumZones( int32_t inumzones );

	void SetName( const char *pname ) { mpInstrumentName=pname; }

	int32_t GetNumZones( void ) const { return miNumZones; }

	void SetZones( AudioInstrumentZone* pzones, int icount ) { mpInstrumentZones=pzones; miNumZones=icount; }

private:

	const char *								mpInstrumentName;
	AudioInstrumentZone*						mpInstrumentZones;
	int32_t										miNumZones;

};

/////////////////////////////////////////////////////////////////////////////

class AudioProgramZone
{
public:

	AudioProgramZone()
		: mikeymin( 0 )
		, mikeymax( 127 )
		, mivelmin( 0 )
		, mivelmax( 127 )
		, mpInstrument( 0 )
	{

	}

	void SetInstrument( AudioInstrument *pinst ) { mpInstrument=pinst; }

	void SetKeyMin( int32_t ikeymin ) { mikeymin=ikeymin; }
	void SetKeyMax( int32_t ikeymax ) { mikeymax=ikeymax; }
	void SetVelMin( int32_t ivelmin ) { mivelmin=ivelmin; }
	void SetVelMax( int32_t ivelmax ) { mivelmax=ivelmax; }

	int32_t  GetKeyMin( void ) const { return mikeymin; }
	int32_t  GetKeyMax( void ) const { return mikeymax; }
	int32_t  GetVelMin( void ) const { return mivelmin; }
	int32_t  GetVelMax( void ) const { return mivelmax; }

	AudioInstrument * GetInstrument( void ) const { return mpInstrument; }

	void EndianSwap()
	{
		swapbytes_dynamic( mikeymin );
		swapbytes_dynamic( mikeymax );
		swapbytes_dynamic( mivelmin );
		swapbytes_dynamic( mivelmax );
		swapbytes_dynamic( mpInstrument );
	}

private:

	int32_t				mikeymin;
	int32_t				mikeymax;
	int32_t				mivelmin;
	int32_t				mivelmax;
	AudioInstrument*	mpInstrument;
};

///////////////////////////////////////////////////////////////////////////////

class AudioProgram
{
public:

	AudioProgram()
	{

	}

	AudioProgramZone& RefZone( int idx )	{ return mpProgramZones[ idx ]; }

	void SetNumZones( int32_t inumzones );
	void SetName( PoolString pname ) { mProgramName=pname; }

	PoolString GetName( void ) const { return mProgramName; }
	int32_t GetNumZones( void ) const { return miNumZones; }

	void SetZones( AudioProgramZone* pzones, int32_t icount ) { mpProgramZones=pzones; miNumZones=icount; }

private:

	PoolString									mProgramName;
	AudioProgramZone*							mpProgramZones;
	int32_t										miNumZones;

};

///////////////////////////////////////////////////////////////////////////////

class AudioBank : public asset::Asset 
{
	RttiDeclareConcrete( AudioBank, asset::Asset );

public:

	int32_t								miNumSamples;
	int32_t								miNumInstruments;
	int32_t								miNumPrograms;

	bool								mbEnabled;
	AudioSample*						mpSamples;
	AudioInstrument*					mpInstruments;

	orklut<PoolString,AudioProgram*>	mPrograms;

	//////////////////////////////////////////////////////////

	void SetNumSamples( int32_t inumsamples );
	void SetNumInstruments( int32_t inuminst );
	void SetNumPrograms( int32_t inumprogs );

	int32_t GetNumSamples( void ) const { return miNumSamples; }

	void AddProgram( const PoolString& name, AudioProgram*prog ) { mPrograms.AddSorted(name,prog); }

	//////////////////////////////////////////////////////////
	AudioSample & RefSample( int idx )
	{
		OrkAssertI( idx < miNumSamples, "invalid sample index" );
		return mpSamples[ idx ];
	}
	//////////////////////////////////////////////////////////
	AudioInstrument & RefInstrument( int idx )
	{
		OrkAssertI( idx < miNumInstruments, "invalid instrument index" );
		return mpInstruments[ idx ];
	}
	//////////////////////////////////////////////////////////
	AudioProgram* RefProgram( const PoolString& name )
	{
		orklut<PoolString,AudioProgram*>::const_iterator it = mPrograms.find(name);
		return (it==mPrograms.end()) ? 0 : it->second;
	}

	//////////////////////////////////////////////////////////
	AudioBank()
		: mpSamples( 0 )
		, mpInstruments( 0 )
		, miNumSamples( 0 )
		, miNumInstruments( 0 )
		, miNumPrograms( 0 )
		, mbEnabled( false )
	{
		ReInit();
	}
	//////////////////////////////////////////////////////////
	void Enable( void ) { mbEnabled=true; }
	void Disable( void ) { mbEnabled=false; }
	void ReInit( void )	{}

};

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////

#endif
