////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/audiobank.h>
#include "audiodevice_wii.h"
#include <ork/file/file.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/kernel/mutex.h>
#include <ork/mem/wii_mem.h>
#include <revolution/sc.h>
#include <ork/application/application.h>

//#pragma comment( lib, "fmodex_vc.lib" )

static bool IsMonoMode() { return ORKetSoundMode()==SC_SOUND_MODE_MONO; }

//#define USE_THREAD

#define FMOD_VERBOSE 0
#if FMOD_VERBOSE
#	define DEBUG_FMOD_PRINT	orkprintf
#else
#	define DEBUG_FMOD_PRINT(...) ((void)0)
#endif

void SetDebugLevel(bool value)
{

	//if(value)
		//FMOD::Debug_SetLevel(  FMOD_DEBUG_LEVEL_WARNING  | FMOD_DEBUG_LEVEL_ERROR | FMOD_DEBUG_TYPE_FILE   );
	//else
		//FMOD::Debug_SetLevel(  FMOD_DEBUG_LEVEL_NONE  );

}



namespace ork { namespace lev2 {

//static dummy_critsect FmodCritSect( "fmod_critsect" );
//static dummy_critsect::standard_lock FmodCritLock(FmodCritSect);

static void* kmusicstreamtag = (void*) 0xb0b0;

struct WiiSample
{
	const FMOD::Sound*	mpFmodSound;
	//WiiAdpcm			mAdpcmHeader;
	const void*			mAdpcmData;

	WiiSample()
		: mpFmodSound(0)
		, mAdpcmData(0)
	{
	}
};

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::SetReverbProperties( const AudioReverbProperties& reverb_props )
{
	FMOD_REVERB_PROPERTIES fmod_reverb_props = FMOD_PRESET_GENERIC ;

	fmod_reverb_props.DecayTime = reverb_props.GetDecayTime();				// 0.1 , 20.0 , 1.49
	fmod_reverb_props.ReverbDelay = reverb_props.GetReverbDelay();			// 0.0 , 0.1 , 0.011
	fmod_reverb_props.ModulationDepth = reverb_props.GetModulationDepth();	// 0.0 , 1.0 , 0.0
	fmod_reverb_props.EnvDiffusion = reverb_props.GetEnvDiffusion();		// 0.0 , 1.0 , 1.0

	fmod_reverb_props.Room = -10000; //int(reverb_props.GetRoom())*2;					// -10000, 0 , -1000
	fmod_reverb_props.Reflections = -10000; //int(reverb_props.GetReflections())*2;		// -10000, 1000 , -2602

	//FmodCritLock.Lock();
	//FMOD_RESULT result = mpFmodSystem->setReverbProperties( & fmod_reverb_props );
	//OrkAssert( FMOD_OK==result );
	//FmodCritLock.UnLock();

}

///////////////////////////////////////////////////////////////////////////////

float F_CALLBACK grolloffcb( FMOD_CHANNEL* channel, float  distance )
{
	AudioDevice* adev = AudioDevice::GetDevice();
	if( distance<adev->GetDistMin() ) distance=adev->GetDistMin();
	if( distance>adev->GetDistMax() ) distance=adev->GetDistMax();
	float atten = distance*adev->GetDistScale();
	atten = std::powf(atten,adev->GetDistAttenPower());
	if( atten<1.0f ) atten=1.0f;
	float fvol = 0.6f/atten;
	FMOD_VECTOR cpos, cvel;
	reinterpret_cast<FMOD::Channel*>(channel)->get3DAttributes( &cpos, &cvel );
//	orkprintf( "distance<%f> atten<%f> pos<%f %f %f>\n", distance, fvol, cpos.x, cpos.y, cpos.z );
	return fvol;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::DestroyStream( AudioStream* pstream )
{
	FMOD::Sound* phandle = (FMOD::Sound*) pstream->GetPlatformHandle();
	FMOD_RESULT result = phandle->release();
	OrkAssert( result == FMOD_OK );
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::DestroyBank( AudioBank* pbank )
{
	int inumsamples = pbank->GetNumSamples();

	for( int i=0; i<inumsamples; i++ )
	{
		AudioSample& sample = pbank->RefSample(i);
		WiiSample* pwiisample = (WiiSample*) sample.GetPlatformHandle();
		FMOD::Sound* phandle = (FMOD::Sound*) pwiisample->mpFmodSound;
		FMOD_RESULT result = phandle->release();
		OrkAssert( result == FMOD_OK );
	}

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::ShutdownNow()
{
	if( mpFmodSystem )
	{
		int inumchansplaying = -1;

		while( inumchansplaying != 0 )
		{
			FMOD_RESULT result = mpFmodSystem->getChannelsPlaying( & inumchansplaying );

			if( 0 != inumchansplaying )
			{
				FMOD::ChannelGroup * channelgroup;
				mpFmodSystem->getMasterChannelGroup( &channelgroup);
				if( channelgroup )
				{
					channelgroup->stop();
				}
			}
			mpFmodSystem->update();
		}
		FMOD::SoundGroup * soundgroup = NULL;
		mpFmodSystem->getMasterSoundGroup(&soundgroup);
		if(soundgroup)
		{
			int numsounds = 0;
			soundgroup->getNumSounds(&numsounds);
			for (int i=0;i<numsounds;i++)
			{
				FMOD::Sound *  sound=NULL;
				soundgroup->getSound(i,&sound);
				sound->release();

			}
		}
		mpFmodSystem->update();
		mpFmodSystem->release();
		mpFmodSystem = 0;
	}
}


AudioDeviceWII::AudioDeviceWII()
	: AudioDevice()
	, mpCurrentStream( 0 )
	, mpFmodSystem( 0 )
	, mpauseref(0)
	, mPauseChannelGroup( 0 )
{
	static const int kfmodpoolsize = 8<<20;
	static void* pfmodpool = ork::wii::MEM2Alloc( kfmodpoolsize );

	unsigned int memflags = (FMOD_MEMORY_NORMAL|FMOD_MEMORY_PERSISTENT);
	FMOD_RESULT result = FMOD::Memory_Initialize(
		pfmodpool, kfmodpoolsize,
		0,0,0 );

	if( 0 != mpFmodSystem )
	{
		mpFmodSystem->release();
	}

	result = FMOD::System_Create(&mpFmodSystem);		// Create the main system object.
	SetDebugLevel(false);
	if (result != FMOD_OK)
	{
		orkprintf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
		//exit(-1);
	}

	FMOD::Debug_SetLevel(  FMOD_DEBUG_LEVEL_WARNING  | FMOD_DEBUG_LEVEL_ERROR ); // | FMOD_DEBUG_TYPE_FILE   );

	////////////////////////////////////////////////////////

	result = mpFmodSystem->setOutput( FMOD_OUTPUTTYPE_WII );

	int inumdrivers = 0;

	mpFmodSystem->getNumDrivers( & inumdrivers );

	result = mpFmodSystem->setDriver( 0 ); // select primary driver
	result = mpFmodSystem->setSpeakerMode( FMOD_SPEAKERMODE_STEREO );
			//IsStereoMode() ? FMOD_SPEAKERMODE_STEREO : FMOD_SPEAKERMODE_MONO );

	mDriverName = "";

	////////////////////////////////////////////////////////

	//U32 initflags = FMOD_INIT_NORMAL|FMOD_INIT_3D_RIGHTHANDED|FMOD_INIT_SOFTWARE_DISABLE|FMOD_INIT_STREAM_FROM_UPDATE;
	U32 initflags = FMOD_INIT_NORMAL|FMOD_INIT_3D_RIGHTHANDED|FMOD_INIT_SOFTWARE_DISABLE;

	result = mpFmodSystem->init(kmaxhwchannels,initflags, 0);	// Initialize FMOD.
	if (result != FMOD_OK)
	{
		orkprintf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
		//exit(-1);
	}

	result = mpFmodSystem->setStreamBufferSize( 65536, FMOD_TIMEUNIT_RAWBYTES );

	result = mpFmodSystem->set3DSettings( 1.0f, 100.0f, 1.0f );

	result = mpFmodSystem->set3DRolloffCallback( grolloffcb );
	mpFmodSystem->set3DNumListeners(1);

	result = mpFmodSystem->createChannelGroup( "pausemenuchannel", & mPauseChannelGroup );

	OrkAssert( mPauseChannelGroup != 0 );

	////////////////////////////////////////////////////////



}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::PushContext()
{
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::PopContext()
{
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::DoReInitDevice( void )
{
	MCheckPointContext( "AudioDeviceWII::DoReInitDevice" );

	for( int ichan=0; ichan<kmaxhwchannels; ichan++ )
	{	FMOD::Channel *channel = 0;

		FMOD_RESULT result = mpFmodSystem->getChannel( ichan, & channel );

		if( FMOD_OK == result )
		{
			if( channel )
			{
				bool bisplaying = true;
				while( bisplaying )
				{
					result = channel->isPlaying( & bisplaying );

					if( FMOD_OK == result )
					{
						if( bisplaying )
						{
							channel->stop();
							DEBUG_FMOD_PRINT("DoReInitDevice:%x: \n",(void *) channel);
						}
					}
				}
			}
		}
	}

}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::Update( float fdt )
{
	MCheckPointContext( "AudioDeviceWII::Update" );

	FMOD_RESULT result;

	/////////////////////////////////////////////////
	// set listener
	/////////////////////////////////////////////////

	FMOD_VECTOR pos, fw, up;

	pos.x = mListenerPos.GetX();
	pos.y = mListenerPos.GetY();
	pos.z = mListenerPos.GetZ();

	fw.x = mListenerForward.GetX();
	fw.y = mListenerForward.GetY();
	fw.z = mListenerForward.GetZ();

	up.x = mListenerUp.GetX();
	up.y = mListenerUp.GetY();
	up.z = mListenerUp.GetZ();

	float fdot = mListenerUp.Dot(mListenerForward);


	result = mpFmodSystem->set3DListenerAttributes( 0, &pos, 0, &fw, &up );
	OrkAssertI( FMOD_OK == result, "fmod error" );

	/////////////////////////////////////////////////


	static int ilastnumplaying = 0;

	int inumchansplaying = 0;
	result = mpFmodSystem->getChannelsPlaying( & inumchansplaying );

	if( inumchansplaying!=ilastnumplaying )
	{
		//printf( "NumChannelsPlaying<%d>\n", inumchansplaying );
	}

	ilastnumplaying = inumchansplaying;

	for( int ichan=0; ichan<kmaxhwchannels; ichan++ )
	{	FMOD::Channel *channel = 0;
		FMOD_RESULT result = mpFmodSystem->getChannel( ichan, & channel );
		if( FMOD_OK == result )
		{	if( channel )
			{	bool bisplaying = false;
				result = channel->isPlaying( & bisplaying );
				if( FMOD_OK == result )
				{	if( bisplaying )
					{
						void *pud = 0;
						channel->getUserData( & pud );
						if( FMOD_OK == result )
						{
							if( pud == kmusicstreamtag )
							{
							}
							else if( pud )
							{
								AudDevPbFmod* padpbf = (AudDevPbFmod*) pud;
								AudioInstrumentPlayback *ppb = padpbf->mPlayback;
								OrkAssert(ppb!=0);

								int isubch = padpbf->miChannelIndex;

								const AudioInstrumentZoneContext& izctx = ppb->GetIzoneCtx(isubch);

								ppb->Update( fdt, isubch );
								float frate = ppb->GetPlaybackSampleRate( isubch );
								channel->setFrequency( frate );

								float fpan = IsMonoMode() ? 0.0f : izctx.mfPan;

								channel->setPan(fpan);

								float fsubmix = GetSubMix( ppb->GetSubMix() );
								if(izctx.mfLinearAmplitude*fsubmix < 1.0f)
									DEBUG_FMOD_PRINT("Update:%f\n",izctx.mfLinearAmplitude*fsubmix );

								channel->setVolume( izctx.mfLinearAmplitude*fsubmix );

							}
						}
					}
				}
			}
		}
	}

	mpFmodSystem->update();

	/////////////////////////////////////////////////

	result = mpFmodSystem->get3DListenerAttributes( 0, & pos, 0, 0, 0 );
	OrkAssertI( FMOD_OK == result, "fmod error" );

	//orkprintf( "listener<%f %f %f> fdot<%f>\n", pos.x, pos.y, pos.z, fdot );

	/////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
// upload sample data to FMOD

void AudioDeviceWII::DoInitSample( AudioSample & sample )
{
	//FmodCritLock.Lock();
	///////////////////////////////////////
	WiiSample* pwiisample = new WiiSample;
	const char* pfsbname = (const char*) sample.GetDataPointer();
	///////////////////////////////////////
	FMOD_CREATESOUNDEXINFO	FModInfo;
	memset( (void *) & FModInfo, 0, sizeof( FMOD_CREATESOUNDEXINFO ) );
	///////////////////////////////////////
	FModInfo.defaultfrequency = sample.GetSampleRate();
	FModInfo.numchannels = sample.GetNumChannels();
	FModInfo.format = FMOD_SOUND_FORMAT_GCADPCM; //FMOD_SOUND_FORMAT_PCM16;
	FModInfo.length = sample.GetCompressedLength(); //sample.GetDataLength();
	FModInfo.cbsize = sizeof( FMOD_CREATESOUNDEXINFO );

	///////////////////////////////////////
	U32 Mode = FMOD_OPENMEMORY|FMOD_HARDWARE|FMOD_3D; //|FMOD_CREATECOMPRESSEDSAMPLE|FMOD_HARDWARE;
	///////////////////////////////////////

	FMOD::Sound *pfmodhandle = 0;

	FMOD_RESULT result = mpFmodSystem->createSound( pfsbname, Mode, & FModInfo, & pfmodhandle );

	OrkAssertI( FMOD_OK == result, "fmod createsound error" );

	int inumsubsounds = 0;

	result = pfmodhandle->getNumSubSounds( & inumsubsounds );

	OrkAssert( 1 == inumsubsounds );

	FMOD::Sound* psubsound = 0;

	result = pfmodhandle->getSubSound( 0, & psubsound );

	OrkAssertI( FMOD_OK == result, "fmod createsound error" );

	pfmodhandle = psubsound;


	//result = pfmodhandle->setMode(FMOD_LOOP_OFF);
	pfmodhandle->setUserData( (void *) pwiisample );

	pwiisample->mpFmodSound = pfmodhandle;
	if( sample.IsLooped() )
	{
		//int iloopstart = sample.GetLoopStart();
		//int iloopend = sample.GetLoopEnd();
		//result = pfmodhandle->setLoopPoints( iloopstart, FMOD_TIMEUNIT_PCM, iloopend, FMOD_TIMEUNIT_PCM );
		result = pfmodhandle->setMode( FMOD_LOOP_NORMAL );
		OrkAssertI( FMOD_OK == result, "fmod setloop error" );
	}

	///////////////////////////////////////
	OrkAssertI( FMOD_OK == result, "fmod error" );

	///////////////////////////////////////

	sample.SetPlatformHandle( (void *) pwiisample );
	DEBUG_FMOD_PRINT("DoInitSample:%x\n",(void *) pwiisample );
	//FmodCritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::SetPauseState(bool bpause)
{
	if(bpause)
		mpauseref++;
	else
		mpauseref--;

	// we don't double pause
	// we don't blanket unpause

	if(1 == mpauseref)
	{
		FMOD::ChannelGroup * channelgroup;
		mpFmodSystem->getMasterChannelGroup( &channelgroup);
		channelgroup->setPaused(true);
	}
	else if(0 == mpauseref) {
		FMOD::ChannelGroup * channelgroup;
		mpFmodSystem->getMasterChannelGroup( &channelgroup);
		channelgroup->setPaused(false);
	}

}


void AudioDeviceWII::DoPlaySound( AudioInstrumentPlayback & PlaybackHandle )
{
	MCheckPointContext( "AudioDeviceWII::DoPlaySound" );
#ifdef KILL_AUDIO
	return;
#endif

	//FmodCritLock.Lock();

	const AudioIntrumentPlayParam& pbparam = PlaybackHandle.GetPlaybackParam();

	bool bplaywhilepaused = pbparam.mPlayWhilePaused;

	float fmastervolume = 1.0f;
	int inumchans = PlaybackHandle.GetNumChannels();

	int inumactive = 0;
	for( int ich=0; ich<inumchans; ich++ )
	{
		const AudioInstrumentZoneContext& izc = PlaybackHandle.GetIzoneCtx( ich );

		const AudioSample *psample = PlaybackHandle.GetSample( ich );
		if( 0 == psample ) continue;

		float frate = PlaybackHandle.GetPlaybackSampleRate(ich);

		const WiiSample* pwiisample = (const WiiSample*) psample->GetPlatformHandle();
		const FMOD::Sound *pfmodhandle = pwiisample->mpFmodSound;
		FMOD::Channel *channel;
		FMOD_RESULT result = mpFmodSystem->playSound( FMOD_CHANNEL_FREE, const_cast<FMOD::Sound *>( pfmodhandle ), true, &channel );
		OrkAssert( FMOD_OK==result );

		result = channel->setPriority( 4 );

		/////////////////////////////////////////////////////////////////
		// set initial volume
		/////////////////////////////////////////////////////////////////
		float fsubmix = GetSubMix( pbparam.mSubMixGroup );
		float fvol = fmastervolume*izc.mfLinearAmplitude*fsubmix;

		if( PlaybackHandle.GetPlaybackParam().mEnable3D )
		{

			fvol = 0.0f;
		}

		result = channel->setVolume(fvol);

		OrkAssert( FMOD_OK==result );
		/////////////////////////////////////////////////////////////////

		/////////////////////////////////////////////////////////////////
		// set pan, freq, pause
		/////////////////////////////////////////////////////////////////

		float fpan = izc.mfPan;

		result = channel->setPan( fpan );
		OrkAssert( FMOD_OK==result );
		result = channel->setFrequency( frate );
		OrkAssert( FMOD_OK==result );
		result = channel->setPaused(false);
		OrkAssert( FMOD_OK==result );

		/////////////////////////////////////////////////////////////////

		if( bplaywhilepaused )
		{
			result = channel->setChannelGroup( mPauseChannelGroup );
		}

		/////////////////////////////////////////////////////////////////

		int ifmodchanindex = -1;
		result = channel->getIndex( & ifmodchanindex );
		OrkAssert( FMOD_OK==result );

		AudDevPbFmod& rfmodpb = mFmodPlaybacks[ ifmodchanindex ];

		rfmodpb.mPlayback = & PlaybackHandle;
		rfmodpb.miChannelIndex = ich;
		rfmodpb.mpChannel = channel;

		channel->setUserData( (void *) & rfmodpb );

		//////////////////////////////////////////////

		FMOD_MODE mode;
		result = channel->getMode( & mode );
		OrkAssert( FMOD_OK==result );

		mode = (mode & ~FMOD_3D_HEADRELATIVE) | (FMOD_3D_WORLDRELATIVE) | FMOD_3D_LOGROLLOFF;
		channel->setMode(mode);
		OrkAssert( FMOD_OK==result );

		result = channel->set3DMinMaxDistance( 0.01f, 10000.0f );
		OrkAssert( FMOD_OK==result );

		bool blooped = psample->IsLooped();
		///////////////////////////////////////
		struct ChannelCallback
		{
			///////////////////////////////////////
			// End Callback
			///////////////////////////////////////
			static FMOD_RESULT F_CALLBACK EndCallback( FMOD_CHANNEL* channel, FMOD_CHANNEL_CALLBACKTYPE type, int command, unsigned int cmddata1, unsigned int cmddata2 )
			{
				FMOD::Channel *cppchannel = (FMOD::Channel *) channel;

				void *pudata = 0;

				FMOD_RESULT result = cppchannel->getUserData( & pudata );

				if( FMOD_OK == result )
				{
					if( pudata )
					{
						AudDevPbFmod* padpf = (AudDevPbFmod*) pudata;
						AudioInstrumentPlayback *phandle = padpf->mPlayback;

						phandle->DecrNumActiveChannels();

					}
				}
				return FMOD_OK;
			}
			///////////////////////////////////////
		};
		///////////////////////////////////////
		channel->setCallback( FMOD_CHANNEL_CALLBACKTYPE_END, ChannelCallback::EndCallback, ich );
		//channel->setCallback( FMOD_CHANNEL_CALLBACKTYPE_SYNCPOINT, ChannelCallback::SyncPointCallback, 0 );
		///////////////////////////////////////
		PlaybackHandle.SetPlatformPBHandle( ich, channel );
		DEBUG_FMOD_PRINT("DoPlaySound:channel:%d:%x\n",ich,(void *) channel);

		///////////////////////////////////////
		inumactive++;
	}
	///////////////////////////////////////
	PlaybackHandle.SetNumActiveChannels( inumactive );
	DEBUG_FMOD_PRINT("DoPlaySound:numactive:%d\n",inumactive);

	///////////////////////////////////////
	//FmodCritLock.UnLock();

}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::DoStopSound( AudioInstrumentPlayback & PlaybackHandle )
{
	MCheckPointContext( "AudioDeviceWII::DoStopSound" );
	//FmodCritLock.Lock();

	int inumchans = PlaybackHandle.GetNumChannels();
	for( int ich=0; ich<inumchans; ich++ )
	{	FMOD::Channel *channel = (FMOD::Channel *) PlaybackHandle.GetPlatformPBHandle( ich );
		if( channel )
		{	bool bplaying = false;
			FMOD_RESULT result = channel->isPlaying( & bplaying );
			if( FMOD_OK == result )
			{	if( bplaying )
				{	result = channel->stop();
					DEBUG_FMOD_PRINT("DoStopSound:%x: \n",(void *) channel);
				}
			}
		}
	}

	//FmodCritLock.UnLock();

}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::DoSetSoundSpatial( AudioInstrumentPlayback & PlaybackHandle, const ork::CVector3& pos )
{
	MCheckPointContext( "AudioDeviceWII::DoSetSoundSpatial" );
	//FmodCritLock.Lock();
	{
		bool bena3d = PlaybackHandle.GetPlaybackParam().mEnable3D;

		int inumchans = PlaybackHandle.GetNumChannels();
		for( int ich=0; ich<inumchans; ich++ )
		{	FMOD::Channel *channel = (FMOD::Channel *) PlaybackHandle.GetPlatformPBHandle( ich );
			if( channel )
			{	bool bplaying = false;
				FMOD_RESULT result = channel->isPlaying( & bplaying );
				if( FMOD_OK == result )
				{	if( bplaying )
					{
						FMOD_VECTOR fmv;

						if( bena3d )
						{
							if( IsMonoMode() )
							{
								float fdist = (mListenerPos-pos).Mag();

								ork::CVector3 npos = mListenerPos + (mListenerForward*fdist);
								fmv.x = npos.GetX();
								fmv.y = npos.GetY();
								fmv.z = npos.GetZ();
							}
							else
							{
								fmv.x = pos.GetX();
								fmv.y = pos.GetY();
								fmv.z = pos.GetZ();
							}
						}
						else
						{
							ork::CVector3 npos = mListenerPos + (mListenerForward*100.0f);
							fmv.x = npos.GetX();
							fmv.y = npos.GetY();
							fmv.z = npos.GetZ();
						}

						channel->set3DAttributes( & fmv, 0 );


					}
				}
			}
		}
	}
	//FmodCritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

bool AudioDeviceWII::DoLoadStream( AudioStream* streamhandle, ConstString fname )
{
	//FmodCritLock.Lock();

	FMOD_RESULT fmod_result;

	int currentalloced;
	int maxalloced;
	fmod_result = FMOD::Memory_GetStats( & currentalloced, & maxalloced );

	orkprintf( "FMOD memory currentalloced<%d> maxalloced<%d>\n", currentalloced, maxalloced );
	streamhandle->SetPlatformHandle( 0 );

	/////////////////////////////
	// get new stream
	AssetPath filename = fname.c_str();

	bool b_uncompressed = (strstr(filename.c_str(),"streams")!=0);

	bool b_looped = (strstr(filename.c_str(),"Music")!=0) | (strstr(filename.c_str(),"stings")!=0);
	     b_looped |= (strstr(filename.c_str(),"music")!=0) | (strstr(filename.c_str(),"Stings")!=0);

	orkprintf( "loadstream<%s> looped<%d>\n", filename.c_str(), int(b_looped) );

	filename.SetExtension( b_uncompressed ? "fsb" : "fsb" );

	U32 UnCompressedMode =  FMOD_2D | FMOD_HARDWARE ;
	U32 CompressedMode =  FMOD_CREATECOMPRESSEDSAMPLE | FMOD_2D |  FMOD_HARDWARE ;

	U32 Mode = b_uncompressed ? UnCompressedMode : CompressedMode;

	Mode |= b_looped ? (FMOD_LOOP_NORMAL | FMOD_CREATESTREAM) : 0;

	file::Path abspath = filename.ToAbsolute();
	FMOD::Sound *phandle = 0;

	fmod_result = FMOD_RESULT_FORCEINT;

	while( fmod_result != FMOD_OK )
	{
		fmod_result = mpFmodSystem->createStream( abspath.c_str(), Mode, 0, & phandle );

		if(fmod_result != FMOD_OK) //e Ouch disk error in fmod
		{
			//eSo try and read the file to generate the disk error in our system
			int icount = 0;
			CFile ifile( abspath.c_str(), ork::EFM_READ );
			ifile.Read( & icount, sizeof(icount) );
			ifile.Read( & icount, sizeof(icount) );
			ifile.Close();
		}
	}
	streamhandle->SetPlatformHandle( phandle );

	FMOD_SOUND_TYPE sndtype;
	FMOD_SOUND_FORMAT sndfmt;
	int inumchannels = 0;
	int inumbits = 0;

	fmod_result = phandle->getFormat( & sndtype, & sndfmt, & inumchannels, & inumbits );

	streamhandle->SetNumChannels( inumchannels );

	phandle->setUserData( b_looped ? kmusicstreamtag : 0 );

	/////////////////////////////

	if( false == b_uncompressed )
	{
		filename.SetExtension( "mkr" );

		CFile ifile( filename, ork::EFM_READ );

		int icount = 0;
		ifile.Read( & icount, sizeof(icount) );
		swapbytes( icount );

		for( int i=0; i<icount; i++ )
		{
			float ftime = 0.0f;
			int istrlen = 0;

			ifile.Read( & ftime, sizeof(ftime) );
			ifile.Read( & istrlen, sizeof(istrlen) );
			swapbytes( ftime );
			swapbytes( istrlen );

			char* pstring = new char[ istrlen+1 ];
			memset( pstring, 0, istrlen+1 );

			ifile.Read( pstring, istrlen );

			orkprintf( "StreamMarker<%d> time<%f> name<%s>\n", i, ftime, pstring );

			FMOD_SYNCPOINT *syncpoint = 0;

			unsigned int  offset = int(ftime*1000.0f);
			FMOD_TIMEUNIT offsettype = FMOD_TIMEUNIT_MS;
			const char *name = pstring;
			//FMOD_SYNCPOINT **  point
			fmod_result = phandle->addSyncPoint( offset, offsettype, name, & syncpoint );
			OrkAssert( fmod_result == FMOD_OK );
		}
		ifile.Close();
	}

	bool bOK = (streamhandle->GetPlatformHandle()!=0);

	//FmodCritLock.UnLock();
	OrkAssert(bOK);
	return bOK;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::FillInSyncPoints( AudioStream* streamhandle, orkmap<ork::PoolString, float> &points )
{
	//FmodCritLock.Lock();
	if(FMOD::Sound *pstream = (FMOD::Sound *)streamhandle->GetPlatformHandle())
	{
		int numsyncpoints;
		pstream->getNumSyncPoints(&numsyncpoints);
		for(int i = 0; i < numsyncpoints; i++)
		{
			FMOD_SYNCPOINT *sync = NULL;
			pstream->getSyncPoint(i, &sync);

			unsigned int offset = -1;
			char buffer[512];
			pstream->getSyncPointInfo(sync, buffer, sizeof(buffer), &offset, FMOD_TIMEUNIT_MS);

			orkprintf("%s (%d) : %d ms\n", buffer, strlen(buffer), offset);

			points.insert(std::make_pair(ork::AddPooledString(buffer), float(offset) * 0.001f));
		}

		unsigned int length = 0;
		pstream->getLength(&length, FMOD_TIMEUNIT_MS);

		orkprintf("END : %d ms\n", length);

		points.insert(std::make_pair(ork::AddPooledLiteral("END"), float(length) * 0.001f));
	}
	//FmodCritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

float AudioDeviceWII::GetStreamLength( AudioStream* streamhandle )
{
	MCheckPointContext( "AudioDeviceWII::GetStreamLength" );
	unsigned int length = 0;

	//FmodCritLock.Lock();
	if(FMOD::Sound *pstream = (FMOD::Sound *)streamhandle->GetPlatformHandle())
		pstream->getLength(&length, FMOD_TIMEUNIT_MS);
	//FmodCritLock.UnLock();

	return float(length) * 0.001f;
}

///////////////////////////////////////////////////////////////////////////////

AudioStreamPlayback AudioDeviceWII::DoPlayStream( AudioStream* streamhandle )
{
	MCheckPointContext( "AudioDeviceWII::DoPlayStream" );
	if( 0 == streamhandle ) return kInvalidStreamPlaybackHandle;

	//FmodCritLock.Lock();

	FMOD::Sound *pstream = (FMOD::Sound *) streamhandle->GetPlatformHandle();
	FMOD::Channel* pchannel = (FMOD::Channel*) kInvalidStreamPlaybackHandle;

	const float fvolume = 0.607f; //float(MusicVolume)/128.0f;

	if( pstream )
	{
		char namebuffer[128];
		pstream->getName(namebuffer,sizeof(namebuffer));

		//////////////////////////////////////////////////////////

		void* puserdata = 0;
		pstream->getUserData( & puserdata );
		bool bismusic = (puserdata==kmusicstreamtag);

		FMOD_RESULT result = FMOD_OK;

		//////////////////////////////////////////////////////////

		int inumsubsounds = 0;
		FMOD::Sound *psubsound = 0;

		result = pstream->getNumSubSounds( & inumsubsounds );
		result = pstream->getSubSound( 0, & psubsound );

		OrkAssert( psubsound != 0 );

		//////////////////////////////////////////////////////////

		orkprintf( "playstream<%s>\n", streamhandle->GetName().c_str() );

		result = mpFmodSystem->playSound( FMOD_CHANNEL_FREE, psubsound, true, &pchannel );
		if(result == FMOD_ERR_FILE_DISKEJECTED) //e  Ouch disk error in fmod
			return AudioStreamPlayback(kInvalidStreamPlaybackHandle);
		orkprintf( "playstream<%s> ch<%08x>\n", streamhandle->GetName().c_str(), pchannel );

		if(result != FMOD_OK) //e
		{
			DEBUG_FMOD_PRINT("DoPlayStream:playSound fmodresult %d, Error: \n",result);
			return AudioStreamPlayback(kInvalidStreamPlaybackHandle);
		}

		result = pchannel->setPriority( 8 );
		result = pchannel->setVolume(fvolume);
		result = pchannel->setPan( 0.0f );
		result = pchannel->setUserData( bismusic ? kmusicstreamtag : 0 );

		DEBUG_FMOD_PRINT("DoPlayStream:%s fmodresult %d, channel:%x volume %f: \n",namebuffer,result,(void *)pchannel,fvolume);
		///////////////////////////////////////////////////

		FMOD_REVERB_CHANNELPROPERTIES reverbprops;

		reverbprops.Direct = 0;
		reverbprops.DirectHF = 0;
		reverbprops.Room = -10000;
		reverbprops.RoomHF = -10000;
		reverbprops.Obstruction = -10000;
		reverbprops.Occlusion = -10000;
		reverbprops.Exclusion = -10000;
		reverbprops.OutsideVolumeHF = -10000;

		reverbprops.Flags = FMOD_REVERB_CHANNELFLAGS_DEFAULT;

		result = pchannel->setReverbProperties( & reverbprops );

		if(result != FMOD_OK) //e
		{
			DEBUG_FMOD_PRINT("DoPlayStream:setReverbProperties fmodresult %d, Error: \n",result);
			return AudioStreamPlayback(kInvalidStreamPlaybackHandle);
		}
		///////////////////////////////////////////////////

		if( IsMonoMode() )
		{
			const float fgrpa = fvolume;
			const float fgrpb = fvolume;
			const float fgrpc = 0.0f;
			const float kfrms = 0.607f;

			float flevelsL[6] = { fgrpa*kfrms, fgrpa*kfrms, fgrpb*kfrms, fgrpb*kfrms, fgrpc*kfrms, fgrpc*kfrms };
			float flevelsR[6] = { fgrpa*kfrms, fgrpa*kfrms, fgrpb*kfrms, fgrpb*kfrms, fgrpc*kfrms, fgrpc*kfrms };
			result = pchannel->setSpeakerLevels( FMOD_SPEAKER_FRONT_LEFT, flevelsL, 6 );
			OrkAssert( result == FMOD_OK );
			result = pchannel->setSpeakerLevels( FMOD_SPEAKER_FRONT_RIGHT, flevelsR, 6 );
			DEBUG_FMOD_PRINT("DoPlayStream:%f %f\n",flevelsL,flevelsR);
			OrkAssert( result == FMOD_OK );
		}

		///////////////////////////////////////////////////

		result = pchannel->setPaused(false);

		///////////////////////////////////////////////////
	}
	else
	{
		DEBUG_FMOD_PRINT("DoPlayStream: NullStream \n");
	}


	//FmodCritLock.UnLock();
	DEBUG_FMOD_PRINT("DoPlayStream:%x: %x\n",(void *)pchannel,(void *) pstream);
	return AudioStreamPlayback(pchannel);
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::SetStreamSubMix( AudioStreamPlayback pb, float fgrpa, float fgrpb, float fgrpc )
{
	MCheckPointContext( "AudioDeviceWII::SetStreamSubMix" );
	//FmodCritLock.Lock();
	FMOD::Channel* pchannel = (FMOD::Channel*) pb;
	if( pchannel )
	{
		FMOD::Sound* sound = 0;
		FMOD_RESULT result = pchannel->getCurrentSound( & sound );
		if( sound )
		{
			//////////////////////////////////////////////////////////

			int inumsubsounds = 0;
			FMOD::Sound *psubsound = 0;

			result = sound->getNumSubSounds( & inumsubsounds );
			result = sound->getSubSound( 0, & psubsound );

			OrkAssert( psubsound != 0 );

			//////////////////////////////////////////////////////////

			OrkAssert( result == FMOD_OK );
			FMOD_SOUND_TYPE sndtype;
			FMOD_SOUND_FORMAT sndfmt;
			int inumchannels = 0;
			int inumbits = 0;
			result = psubsound->getFormat( & sndtype, & sndfmt, & inumchannels, & inumbits );
			OrkAssert( result == FMOD_OK );
			OrkAssert( inumchannels==6 );

			float flevelsL[6] = { fgrpa, 0.0f, fgrpb, 0.0f, fgrpc, 0.0f };
			float flevelsR[6] = { 0.0f, fgrpa, 0.0f, fgrpb, 0.0f, fgrpc };

			if( IsMonoMode() )
			{
				const float kfrms = 0.70f;
				flevelsL[0] = fgrpa*kfrms;
				flevelsL[1] = fgrpa*kfrms;
				flevelsL[2] = fgrpb*kfrms;
				flevelsL[3] = fgrpb*kfrms;
				flevelsL[4] = fgrpc*kfrms;
				flevelsL[5] = fgrpc*kfrms;

				flevelsR[0] = fgrpa*kfrms;
				flevelsR[1] = fgrpa*kfrms;
				flevelsR[2] = fgrpb*kfrms;
				flevelsR[3] = fgrpb*kfrms;
				flevelsR[4] = fgrpc*kfrms;
				flevelsR[5] = fgrpc*kfrms;
			}

			DEBUG_FMOD_PRINT("setSpeakerLevels:%f %f %f    %f %f %f:\n",flevelsL[0],flevelsL[2],flevelsL[4],flevelsL[1],flevelsL[3],flevelsL[5]);
			result = pchannel->setSpeakerLevels( FMOD_SPEAKER_FRONT_LEFT, flevelsL, 6 );
			OrkAssert( result == FMOD_OK );
			result = pchannel->setSpeakerLevels( FMOD_SPEAKER_FRONT_RIGHT, flevelsR, 6 );
			OrkAssert( result == FMOD_OK );
		}

	}
	//FmodCritLock.UnLock();
}
///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::DoStopStream( AudioStreamPlayback pb )
{
	FMOD::Channel* pchannel = (FMOD::Channel*) pb;
	orkprintf( "stopstream channel<%08x>\n", pchannel );

	MCheckPointContext( "AudioDeviceWII::DoStopStream" );
	DEBUG_FMOD_PRINT("DoStopStream:%x:\n",(void *) pb);
	//FmodCritLock.Lock();

	if( pchannel )
	{
		bool bisplaying = false;
		FMOD_RESULT result = pchannel->isPlaying(&bisplaying);
		if( FMOD_OK == result )
		{
			if( bisplaying )
			{
				pchannel->stop();
			}
		}
	}

	//FmodCritLock.UnLock();

}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::SetStreamVolume( AudioStreamPlayback streampbh, float fvol )
{
	MCheckPointContext( "AudioDeviceWII::SetStreamVolume" );

	//FmodCritLock.Lock();
	if( kInvalidStreamPlaybackHandle != streampbh )
	{
		float fmastervolume = 1.0f;
		FMOD::Channel *pchannel = (FMOD::Channel *) streampbh;

		DEBUG_FMOD_PRINT("SetStreamVolume: %x %f \n",(void *) pchannel,fvol);
		pchannel->setVolume( fvol*fmastervolume );
	}
	//FmodCritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceWII::SetStreamTime( AudioStreamPlayback streampbh, float time )
{
	MCheckPointContext( "AudioDeviceWII::SetStreamTime" );
	if(kInvalidStreamPlaybackHandle != streampbh)
	{
		//FmodCritLock.Lock();
		FMOD::Channel *pchannel = (FMOD::Channel *) streampbh;
		unsigned int position = (unsigned int)(time * 1000.0f);
		DEBUG_FMOD_PRINT("SetStreamTime:%d \n",position);
		pchannel->setPosition(position, FMOD_TIMEUNIT_MS);
		//FmodCritLock.UnLock();
	}
}

///////////////////////////////////////////////////////////////////////////////

float AudioDeviceWII::GetStreamTime( AudioStreamPlayback streampbh )
{
	MCheckPointContext( "AudioDeviceWII::GetStreamTime" );
	if(kInvalidStreamPlaybackHandle != streampbh)
	{
		FMOD::Channel *pchannel = (FMOD::Channel *) streampbh;

		bool bplaying = false;

		FMOD_RESULT result = pchannel->isPlaying( & bplaying );

		if( FMOD_OK == result )
		{
			if( bplaying )
			{
				unsigned int position = 0;
				result = pchannel->getPosition(&position, FMOD_TIMEUNIT_MS);

				if( FMOD_OK == result )
				{
					float debugvolume;
					//pchannel->getVolume(&debugvolume);
					DEBUG_FMOD_PRINT("GetStreamTime:%x %f \n",pchannel,debugvolume);
					return float(position) * 0.001f;
				}
			}
		}
	}
	return -1.0f;
}

float AudioDeviceWII::GetStreamPlaybackLength( AudioStreamPlayback streampb_handle )
{
	MCheckPointContext( "AudioDeviceWII::GetStreamPlaybackLength" );
	if(kInvalidStreamPlaybackHandle != streampb_handle)
	{
		//FmodCritLock.Lock();
		FMOD::Channel *pchannel = (FMOD::Channel *)streampb_handle;
		FMOD::Sound *psound = NULL;
		pchannel->getCurrentSound(&psound);
		if(psound)
		{
			unsigned int length;
			psound->getLength(&length, FMOD_TIMEUNIT_MS);
			return float(length) * 0.001f;
		}
		//FmodCritLock.UnLock();
	}
	return 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

}}
