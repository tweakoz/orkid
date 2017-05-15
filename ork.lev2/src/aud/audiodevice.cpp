////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/audiobank.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/file/chunkfile.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/fixedlut.hpp>
#include <ork/kernel/tempstring.h>
#include <ork/asset/FileAssetLoader.h>
#include <ork/asset/FileAssetNamer.h>
#include <ork/math/audiomath.h>
#include <ork/reflect/RegisterProperty.h>

///////////////////////////////////////////////////////////////////////////////

#include "null/audiodevice_null.h"
#if defined(WII)
#include "wii/audiodevice_wii.h"
#define NativeDevice AudioDeviceWII
#elif defined(USE_FMOD)
#include "fmod/audiodevice_fmod.h"
#define NativeDevice AudioDeviceFMOD
#elif defined(_USE_XA2)
#include "xa2/audiodevice_xa2.h"
#define NativeDevice AudioDeviceXa2
#else
#define NativeDevice AudioDeviceNULL
#endif

bool gb_audio_filter = false;

using namespace ork::audiomath;

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::AudioReverbProperties, "lev2::AudioReverbProperties" );
INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::AudioZonePlayback, "lev2::audiozonepb" );
INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::AudioModularZonePlayback, "lev2::audiomodzonepb" );
INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::AudioSf2ZonePlayback, "lev2::audiosf2zonepb" );

template class ork::orklut<int,ork::lev2::AudioStream>;
template class ork::orklut<int,ork::PoolString>;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

AudioDevice* AudioDevice::gpDevice = 0;

AudioDevice *AudioDevice::GetDevice( void )
{
	if( 0 == gpDevice )
	{
		if( gb_audio_filter )
		{
			gpDevice = new AudioDeviceNULL;
		}
		else
		{
			gpDevice = new NativeDevice;
		}
		gpDevice->ReInitDevice();
	}

	return gpDevice;
}

///////////////////////////////////////////////////////////////////////////////

AudioIntrumentPlayParam::AudioIntrumentPlayParam() 
	: miNote(-1)
	, miVelocity(-1)
	, mPan(0.0f)
	, mFlags(0)
	, mMutexGroup( )
	, mEnable3D( false )
	, mPlayWhilePaused( false )
	, mfMaxDistance(0.0f)
	, mAttenCurve(0)
{
	for( int i=0; i<kmaxzonesperevent; i++ )
	{
		mUserZonePlaybacks[i] = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////

AudioDevice::AudioDevice()
	: mfMinDist(1.0f) // cm
	, mfMaxDist(10000.0f) // cm = 100m
	, mfDistScale( 0.01f )
	, mfDistAttenPower(1.4f)
	, mbAllMuted(false)
{
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevice::StopAllVoices( void )
{
	//typedef fixed_pool<AudioInstrumentPlayback,kMaxPlayBackHandles> PlaybackPool;
	//typedef fixed_pool<AudioSf2ZonePlayback,kMaxPlayBackHandles> Sf2PlaybackPool;
	//typedef fixed_pool<AudioModularZonePlayback,kMaxPlayBackHandles> ModPlaybackPool;
	//LockedResource<Sf2PlaybackPool>	mSf2Playbacks;
	//LockedResource<ModPlaybackPool>	mModPlaybacks;
	//LockedResource<PlaybackPool>	mPlaybackHandles;
	//	virtual void					DoStopSound( AudioInstrumentPlayback *playbackhandle ) = 0;

	////////////////////////////////////////////////////////
	// copy vector of used playbacks
	////////////////////////////////////////////////////////

	orkvector<AudioInstrumentPlayback*> pbs;
	PlaybackPool& pool = mPlaybackHandles.LockForWrite();
	{
		const PlaybackPool::pointervect_type& used = pool.used();
		
		size_t inumused = used.size();
		for( size_t i=0; i<inumused; i++ )
		{
			AudioInstrumentPlayback* pvoice = used[i];
			pbs.push_back(pvoice);
		}
	}
	mPlaybackHandles.UnLock();

	////////////////////////////////////////////////////////
	// stop each voice
	////////////////////////////////////////////////////////
	size_t inumused = pbs.size();
	for( size_t i=0; i<inumused; i++ )
	{
		AudioInstrumentPlayback* pvoice = pbs[i];
		StopSound(pvoice);
	}

	////////////////////////////////////////////////////////
	//
	////////////////////////////////////////////////////////
	//pool.clear();
	DoStopAllVoices();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevice::SetSubMix( const SubMixString& submixname, float fvolume )
{
	SubMixLut::iterator it = mSubMixer.find(submixname);

	if( it == mSubMixer.end() )
	{
		mSubMixer.AddSorted( submixname,fvolume );
	}
	else
	{
		it->second = fvolume;
	}
}

///////////////////////////////////////////////////////////////////////////////

float AudioDevice::GetSubMix( const SubMixString& submixname ) const
{
	float fmix = 0.707f;

	SubMixLut::const_iterator it = mSubMixer.find(submixname);

	if( it == mSubMixer.end() )
	{
		it = mSubMixer.find("none");
	}
	
	if ( it != mSubMixer.end() )
	{
		fmix = it->second;
	}
	
	return fmix;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevice::ReInitDevice( void )
{
	StopAllVoices();
 
	PlaybackPool& pool = mPlaybackHandles.LockForWrite();
	const PlaybackPool::pointervect_type& used = pool.used();

	int inumch = int(used.size());
	for( int ich=0; ich<inumch; ich++ )
	{
		used[ ich ]->ReInit();
	}

	mPlaybackHandles.UnLock();


	DoReInitDevice();
}

///////////////////////////////////////////////////////////////////////////////

void AudioZonePlayback::Describe(){}

AudioZonePlayback::AudioZonePlayback()
	: miSoundChannel(-1)
	, mpPxvInsZone(0)
	, mpPxvSample(0)
	, mpChannelPB(0)
	, mfPlaybackSampleRate(0.0f)
	, mibasekey(0)
{
}

///////////////////////////////////////////////////////////////////////////////

int AudioInstrumentPlayback::giSerialCounter = 0;

void AudioInstrumentPlayback::ReInit()
{
	mEmitterMatrix = 0;

	mpAudioGraph = 0;
	minumactivechannels = 0;
	minumchannels = 0;

	miSerialNumber = giSerialCounter++;

	mfMaxDistance = 0.0f;
	mAttenCurve = 0;
	mfDistanceAtten = 1.0f;

	//meVoiceState = ESTATE_INACTIVE;
	//mbActive = false;

	for( int ich=0; ich<kmaxzonesperevent; ich++ )
	{
		AudioZonePlayback* zone = GetZonePlayback( ich );
		if( zone ) 
		{
			mZonePlaybacks[ich] = 0;
			//zone->SetChannel(-1);
			//zone->SetZone(0);
			//zone->SetSample(0);
			//zone->GetContext().Reset();
		}
	}

	mpProgram = 0;
//	mibasevel = 0;
}


AudioZonePlayback* AudioInstrumentPlayback::GetZonePlayback( int ich )
{
	OrkAssert( ich<kmaxzonesperevent );
	return mZonePlaybacks[ ich ];
}
const AudioZonePlayback* AudioInstrumentPlayback::GetZonePlayback( int ich ) const
{
	OrkAssert( ich<kmaxzonesperevent );
	return mZonePlaybacks[ ich ];
}
void AudioInstrumentPlayback::SetZonePlayback( int ich, AudioZonePlayback* pb )
{
	OrkAssert( ich<kmaxzonesperevent );
	pb->SetInsPlayback( this );
	mZonePlaybacks[ ich ] = pb;
}

///////////////////////////////////////////////////////////////////////////////

float tri_osc( float fphase_radians )
{
	float fmod_rad = std::fmod( fphase_radians, PI2 ) / PI; // 0 .. 2.0f

	if( fmod_rad > 1.5f )
	{
		float seg_ph = fmod_rad-1.5f;
		return (seg_ph*2.0f)-1.0f;
	}
	else if( fmod_rad > 1.0f )
	{
		float seg_ph = fmod_rad-1.0f;
		return -(seg_ph*2.0f);
	}
	else if( fmod_rad > 0.5f )
	{
		float seg_ph = fmod_rad-0.5f;
		return 1.0f-(seg_ph*2.0f);
	}
	else // 0.0 .. 0.5f
	{
		float seg_ph = fmod_rad;
		return seg_ph*2.0f;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Fixed function sound playback
///////////////////////////////////////////////////////////////////////////////

void AudioSf2ZonePlayback::Describe(){}

void AudioSf2ZonePlayback::Update( float fdt )
{
	AudioInstrumentZoneContext& izc = GetContext();
	AudioSample* psamp = GetSample();
	AudioInstrumentZone* pizone = GetZone();
	//const AudioIntrumentPlayParam& pbparam = GetPlaybackParam();
	
	if( 0 == pizone ) return;

	///////////////////////////////////////////////////
	// update time variant parameters
	///////////////////////////////////////////////////

	float vibhz = pizone->GetVibLfoFrequency(); // 1hz == PI2 radians per sec
	float modhz = pizone->GetModLfoFrequency(); // 1hz == PI2 radians per sec

	izc.mfLfo1Phase += (fdt*vibhz*PI2);
	izc.mfLfo2Phase += (fdt*modhz*PI2);

	float fsinVIB = tri_osc( izc.mfLfo1Phase );
	float fsinMOD = tri_osc( izc.mfLfo2Phase );

	float vib2pitch_cents = pizone->GetVibLfoToPitch();
	float mod2pitch_cents = pizone->GetModLfoToPitch();

	///////////////////////////////////////////////////
	// update pitch
	///////////////////////////////////////////////////

	float fbaserate = float(psamp->GetSampleRate());
	int isamprootkey = psamp->GetRootKey();
	int irootkeyoverride = pizone->GetSampleRootOverride();
	int irootkey = irootkeyoverride;
	int itunesemis = pizone->GetTuneSemis();
	int itunecents = pizone->GetTuneCents() + int(fsinVIB*vib2pitch_cents) + int(fsinMOD*mod2pitch_cents);
	int isemidelta = (mibasekey-irootkey)+itunesemis; //-itunesemis; //+(mibasekey-irootkey);
	float fcents = float(isemidelta*100.0f)+float(itunecents);
	float fratio = cents_to_linear_freq_ratio(fcents);
	float fnewrate = fbaserate*fratio;
	SetPBSampleRate( fnewrate );

	///////////////////////////////////////////////////
	// update filter
	///////////////////////////////////////////////////

	float fcbase = pizone->GetFilterCutoff();
	float fcmod_cents = pizone->GetModLfoToCutoff()*fsinMOD;
	float fc = fcbase * cents_to_linear_freq_ratio(fcmod_cents);
	float fq = (pizone->GetFilterQ()/960.0f);

	float fsq = ork::powf( fq, 2.0f );
	float fdiq = fq-fsq;

	fq = fq+fdiq;

	if( fq < 0.0f ) fq = 0.0f;
	if( fq > 0.99f ) fq = 0.99f;

	izc.SetLpfReson( fc, fq );

	///////////////////////////////////////////////////
	// update pan / amp
	///////////////////////////////////////////////////

	int ivel = GetInsPlayback()->GetPlaybackParam().miVelocity;
	if( ivel == -1 ) ivel = 127;

	const float kvelsc = (96.0f/127.0f);
	float atten_velocity = float(127-ivel)*kvelsc;

	float atten_centibel	= pizone->GetAttenCentibels();
	float atten_linear		= decibel_to_linear_amp_ratio( -atten_centibel*0.1f );
	float atten_modlin		= decibel_to_linear_amp_ratio( fsinMOD * pizone->GetModLfoToAmp()*0.1f );
	float atten_linear_vel	= decibel_to_linear_amp_ratio( -atten_velocity );
	izc.mfLinearAmplitude = atten_linear*atten_modlin*atten_linear_vel;

	izc.mfPan = pizone->GetPan()*0.005f; // limit pan spread to 50% for better spatialization while still allowing for stereo offset

}

void AudioInstrumentPlayback::DeviceUpdateChannel( float fdt, int ich )
{
	GetZonePlayback(ich)->Update(fdt);
}
///////////////////////////////////////////////////////////////////////////////

AudioInstrumentPlayback* AudioDevice::GetFreePlayback( void )
{
	AudioInstrumentPlayback* rval = 0;

	PlaybackPool& pool = mPlaybackHandles.LockForWrite();
	rval = pool.allocate();
	mPlaybackHandles.UnLock();
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

AudioReverbProperties::AudioReverbProperties()
	: mfDecayTime( 1.49f )	// = 3.0f;			// 0.1 , 20.0 , 1.49 
	, mfReflections( -2602.0f )	// = 0;			// // -10000, 1000 , -2602  
	, mfReverbDelay( 0.011f )		// = 0.1;			// 0.0 , 0.1 , 0.011 
	, mfModulationDepth( 0.0f )	// = 1.0f;	// 0.0 , 1.0 , 0.0
	, mfEnvDiffusion( 1.0f )		// = 1.0f;		// 0.0 , 1.0 , 1.0 
	, mfRoom( -1000.0f )			// = -500;				// -10000, 0 , -1000 
{

}

///////////////////////////////////////////////////////////////////////////////

void AudioReverbProperties::Describe()
{
	ork::reflect::RegisterProperty( "DecayTime", & AudioReverbProperties::mfDecayTime );
	ork::reflect::RegisterProperty( "Reflections", & AudioReverbProperties::mfReflections );
	ork::reflect::RegisterProperty( "ReverbDelay", & AudioReverbProperties::mfReverbDelay );
	ork::reflect::RegisterProperty( "ModulationDepth", & AudioReverbProperties::mfModulationDepth );
	ork::reflect::RegisterProperty( "EnvDiffusion", & AudioReverbProperties::mfEnvDiffusion );
	ork::reflect::RegisterProperty( "Room", & AudioReverbProperties::mfRoom );

	ork::reflect::AnnotatePropertyForEditor< AudioReverbProperties >( "DecayTime", "editor.range.min", "0.1" );
	ork::reflect::AnnotatePropertyForEditor< AudioReverbProperties >( "DecayTime", "editor.range.max", "20.0" );

	ork::reflect::AnnotatePropertyForEditor< AudioReverbProperties >( "Reflections", "editor.range.min", "-10000.0" );
	ork::reflect::AnnotatePropertyForEditor< AudioReverbProperties >( "Reflections", "editor.range.max", "1000.0" );

	ork::reflect::AnnotatePropertyForEditor< AudioReverbProperties >( "ReverbDelay", "editor.range.min", "0.0" );
	ork::reflect::AnnotatePropertyForEditor< AudioReverbProperties >( "ReverbDelay", "editor.range.max", "0.1" );

	ork::reflect::AnnotatePropertyForEditor< AudioReverbProperties >( "ModulationDepth", "editor.range.min", "0.0" );
	ork::reflect::AnnotatePropertyForEditor< AudioReverbProperties >( "ModulationDepth", "editor.range.max", "1.0" );

	ork::reflect::AnnotatePropertyForEditor< AudioReverbProperties >( "EnvDiffusion", "editor.range.min", "0.0" );
	ork::reflect::AnnotatePropertyForEditor< AudioReverbProperties >( "EnvDiffusion", "editor.range.max", "1.0" );

	ork::reflect::AnnotatePropertyForEditor< AudioReverbProperties >( "Room", "editor.range.min", "-10000.0" );
	ork::reflect::AnnotatePropertyForEditor< AudioReverbProperties >( "Room", "editor.range.max", "0.0" );

}

///////////////////////////////////////////////////////////////////////////////

AudioInstrumentZoneContext::AudioInstrumentZoneContext()
	: mfLinearAmplitude(1.0f)
{
	Reset();
}

///////////////////////////////////////////////////////////////////////////////

void AudioInstrumentZoneContext::Reset()
{
	mfLfo1Phase = 0.0f;
	mfLfo2Phase = 0.0f;
	SetLpfReson( 2000.0f, 0.9f );
}

///////////////////////////////////////////////////////////////////////////////

void AudioInstrumentZoneContext::SetLpfReson( float kfco, float krez )
{
	mfCutoff = kfco;
	mfResonance = krez;

	const float sr = 44100.0f;
	const float isr = 1.0f / sr;
	const float pi2isr = 2.0f*3.14159265f*isr;

	float kfcon = kfco*pi2isr;
	float kfcon2 = (2.0f*kfcon);

	float cos_kfcon = std::cos( kfcon );
	float cos_kfcon2 = std::cos( kfcon2 );
	float sin_kfcon = std::sin( kfcon );
	float sin_kfcon2 = std::sin( kfcon2 );
	float krezsq = (krez*krez);
	float krez_cos_kfcon = krez*cos_kfcon;

	float kalpha = 1.0f-(2.0f*krez_cos_kfcon*cos_kfcon)+(krezsq*cos_kfcon2);
	float kbeta = (krezsq*sin_kfcon2)-(2.0f*krez_cos_kfcon*sin_kfcon);
	float kgama = 1.0f+cos_kfcon;
	float kag = kalpha*kgama;
	float kbs = kbeta*sin_kfcon;
	float km1 = kag+kbs;
	float km2 = kag-kbs;

	float kden = std::sqrt(km1*km1+km2*km2);
	
	mfa1 = -2.0f*krez*cos_kfcon;
	mfa2 = krez*krez;
	mfb2 = 0.0f;
	mfb0 = 1.5f*(kalpha*kalpha+kbeta*kbeta)/kden;
	mfb1 = mfb0;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevice::SetListener1( const ork::CVector3& pos, const ork::CVector3& up, const ork::CVector3& forward )
{
	mListenerPos1 = pos;
	mListenerUp1 = up;
	mListenerForward1 = forward;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevice::SetListener2( const ork::CVector3& pos, const ork::CVector3& up, const ork::CVector3& forward )
{
	mListenerPos2 = pos;
	mListenerUp2 = up;
	mListenerForward2 = forward;
}

///////////////////////////////////////////////////////////////////////////////

}}
