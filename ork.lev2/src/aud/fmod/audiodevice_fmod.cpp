////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/audiobank.h>
#include "audiodevice_fmod.h"
#include <ork/file/file.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/kernel/mutex.h>
#include <ork/application/application.h>

#pragma comment( lib, "fmodex_vc.lib" )

static bool IsStereoMode() { return true; }

#undef KILL_AUDIO
//#define KILL_AUDIO
//#define USE_THREAD

#define FMOD_VERBOSE 0
#if FMOD_VERBOSE
#	define DEBUG_FMOD_PRINT	orkprintf
#else
#	define DEBUG_FMOD_PRINT(...)
#endif

namespace ork { namespace lev2 {

static HANDLE UpdateThreadHandle = 0;
static DWORD UpdateThreadID = 0;
static critsect FmodCritSect( "fmod_critsect" );
static critsect::standard_lock FmodCritLock(FmodCritSect);

static FMOD_DSP_DESCRIPTION  fmod_sw_biquad_dspdesc;

///////////////////////////////////////////////////////////////////////////////

#if defined( USE_THREAD )
DWORD WINAPI fmodUpdateThread( LPVOID pArguments )
{
	AudioDeviceFMOD* pfmod = (AudioDeviceFMOD*) pArguments;

	while(1)
	{
		static float LastTimeStep = CSystem::GetRef().GetLoResTime();
	    float ThisTimeStep = CSystem::GetRef().GetLoResTime();
		float timestep = (ThisTimeStep-LastTimeStep);
		LastTimeStep = ThisTimeStep;

		FmodCritLock.Lock();
		static_cast<AudioDevice*>(pfmod)->Update(timestep);
		FmodCritLock.UnLock();
		OrkSleepMSec(33);
	}
	return 0;
}
#endif

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::SetReverbProperties( const AudioReverbProperties& reverb_props )
{
	FMOD_REVERB_PROPERTIES fmod_reverb_props = FMOD_PRESET_GENERIC ;

	fmod_reverb_props.DecayTime = reverb_props.GetDecayTime();				// 0.1 , 20.0 , 1.49
	fmod_reverb_props.Reflections = int(reverb_props.GetReflections());		// -10000, 1000 , -2602
	fmod_reverb_props.ReverbDelay = reverb_props.GetReverbDelay();			// 0.0 , 0.1 , 0.011
	fmod_reverb_props.ModulationDepth = reverb_props.GetModulationDepth();	// 0.0 , 1.0 , 0.0
	fmod_reverb_props.EnvDiffusion = reverb_props.GetEnvDiffusion();		// 0.0 , 1.0 , 1.0
	fmod_reverb_props.Room = int(reverb_props.GetRoom());					// -10000, 0 , -1000


	FmodCritLock.Lock();
	FMOD_RESULT result = mpFmodSystem->setReverbProperties( & fmod_reverb_props );
	OrkAssert( FMOD_OK==result );
	FmodCritLock.UnLock();

}

///////////////////////////////////////////////////////////////////////////////

struct fmod_sw_biquad
{
	float xm1, xm2;
	float ym1, ym2;
	const AudioInstrumentZoneContext* mpctx;

	fmod_sw_biquad()
		: xm1(0.0f), xm2(0.0f)
		, ym1(0.0f), ym2(0.0f)
		, mpctx( 0 )
	{
	}

	void set_ctx( const AudioInstrumentZoneContext* pctx ) { mpctx=pctx; }

	float compute( float input )
	{	//y(n) = b_0x(n) + b_1x(n-1) + b_2x(n-2) - a_1y(n-1) - a_2y(n-2)
		float a1 = mpctx->mfa1;
		float a2 = mpctx->mfa2;
		float b0 = mpctx->mfb0;
		float b1 = mpctx->mfb1;
		float b2 = mpctx->mfb2;

		float output = (b0*input)+(b1*xm1)+(b2*xm2)-(a1*ym1)-(a2*ym2);
		xm2 = xm1;
		xm1 = input;
		ym2 = ym1;
		ym1 = output;
		return output;
	}
};

///////////////////////////////////////////////////////////////////////////////

FMOD_RESULT F_CALLBACK fmod_sw_biquad_callback(FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int outchannels)
{
    unsigned int count;
    int count2;
    char name[256];
    FMOD::DSP *thisdsp = (FMOD::DSP *)dsp_state->instance;
	thisdsp->getInfo(name, 0, 0, 0, 0);
	void* puserdata = 0;
    thisdsp->getUserData( & puserdata );
	fmod_sw_biquad* pbiquad = (fmod_sw_biquad*) puserdata;
	bool bena = (pbiquad->mpctx->mfCutoff!=0.0f);
	for (count = 0; count < length; count++)
    {   for (count2 = 0; count2 < outchannels; count2++)
        {
			float input = inbuffer[(count * inchannels) + count2];
			float output = bena ? pbiquad->compute( input ) : input;
			outbuffer[(count * outchannels) + count2] = output;
        }
    }
    return FMOD_OK;
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
	float fvol = 1.0f/atten;
	FMOD_VECTOR cpos, cvel;
	reinterpret_cast<FMOD::Channel*>(channel)->get3DAttributes( &cpos, &cvel );
//	orkprintf( "distance<%f> atten<%f> pos<%f %f %f>\n", distance, fvol, cpos.x, cpos.y, cpos.z );
	return fvol;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::DestroyStream( AudioStream* pstream )
{
	MCheckPointContext( "AudioDeviceFMOD::DestroyStream" );
	FMOD::Sound* phandle = (FMOD::Sound*) pstream->GetPlatformHandle();
	FMOD_RESULT result = phandle->release();
	OrkAssert( result == FMOD_OK );
	pstream->SetPlatformHandle(0);
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::DestroyBank( AudioBank* pbank )
{
	MCheckPointContext( "AudioDeviceFMOD::DestroyBank" );
	int inumsamples = pbank->GetNumSamples();

	for( int i=0; i<inumsamples; i++ )
	{
		AudioSample& sample = pbank->RefSample(i);
		FMOD::Sound* phandle = (FMOD::Sound*) sample.GetPlatformHandle();
		FMOD_RESULT result = phandle->release();
		OrkAssert( result == FMOD_OK );
	}

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AudioDeviceFMOD::AudioDeviceFMOD()
	: AudioDevice()
	, mpCurrentStream( 0 )
	, mpFmodSystem( 0 )
	, mpauseref(0)
{
	FMOD_RESULT result;

	static const int kfmodpoolsize = 64<<20;
	static void* pfmodpool = malloc( kfmodpoolsize );

	unsigned int memflags = (FMOD_MEMORY_NORMAL|FMOD_MEMORY_PERSISTENT);
	result = FMOD::Memory_Initialize(
		pfmodpool,kfmodpoolsize,
		0,0,0 );

	PushContext();

	if( 0 != mpFmodSystem )
	{
		mpFmodSystem->release();
	}

	result = FMOD::System_Create(&mpFmodSystem);		// Create the main system object.
	if (result != FMOD_OK)
	{
		orkprintf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
		exit(-1);
	}

	////////////////////////////////////////////////////////

	//char drivernamebuffer[128];
	result = mpFmodSystem->setOutput( FMOD_OUTPUTTYPE_DSOUND );

	int inumdrivers = 0;

	mpFmodSystem->getNumDrivers( & inumdrivers );

	result = mpFmodSystem->setDriver( 0 ); // select primary driver
	//result = mpFmodSystem->getDriverName( 0, drivernamebuffer, 128 );
	result = mpFmodSystem->setSpeakerMode( FMOD_SPEAKERMODE_STEREO );

	mDriverName = ""; //std::string( drivernamebuffer );


	////////////////////////////////////////////////////////

	result = mpFmodSystem->init(kmaxhwchannels, FMOD_INIT_NORMAL|FMOD_INIT_3D_RIGHTHANDED, 0);	// Initialize FMOD.
	if (result != FMOD_OK)
	{
		orkprintf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
		exit(-1);
	}

	result = mpFmodSystem->set3DSettings( 1.0f, 100.0f, 1.0f );

	result = mpFmodSystem->set3DRolloffCallback( grolloffcb );
	mpFmodSystem->set3DNumListeners(1);

	////////////////////////////////////////////////////////

	memset(&fmod_sw_biquad_dspdesc, 0, sizeof(FMOD_DSP_DESCRIPTION));
	strcpy(fmod_sw_biquad_dspdesc.name, "wii biquad filter emulator");
	fmod_sw_biquad_dspdesc.channels     = 1;                   // 0 = whatever comes in, else specify.
	fmod_sw_biquad_dspdesc.read         = fmod_sw_biquad_callback;
	fmod_sw_biquad_dspdesc.userdata     = (void *) 0;

	for( int i=0; i<kmaxhwchannels; i++ )
	{
		result = mpFmodSystem->createDSP(&fmod_sw_biquad_dspdesc, &mFilterHandles[i]);
		OrkAssert( FMOD_OK==result );

		fmod_sw_biquad* pbiquadinstance = new fmod_sw_biquad;
		mFilterHandles[i]->setUserData( (void*) pbiquadinstance );
		mFilterHandles[i]->setActive( true );
		mFilterHandles[i]->setBypass( false );
	}

	////////////////////////////////////////////////////////

#if defined( USE_THREAD )
	UpdateThreadHandle = CreateThread( NULL, 0, & fmodUpdateThread, (void*)this, 0, & UpdateThreadID );
#endif

	/*for( int i=1; i<100; i++ )
	{
		mpFmodSystem->update();
		OrkSleepMSec(1);
	}*/
}

//////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::PushContext()
{
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::PopContext()
{
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::DoReInitDevice( void )
{
	//FmodCritLock.Lock();
	int pchannelidx = NULL;
	//FMOD_RESULT result = mpFmodSystem->getChannelsPlaying( &pchannelidx );
	//while(pchannelidx)
	for( int ichan=0; ichan<kmaxhwchannels; ichan++ )
	{	FMOD::Channel *channel = 0;

		FMOD_RESULT result = mpFmodSystem->getChannel( ichan, & channel );
		if( FMOD_OK == result )
		{	if( channel )
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
	//FmodCritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::Update( float fdt )
{
#if 0
MCheckPointContext( "AudioDeviceFMOD::Update" );
	FMOD_RESULT result;
	//FmodCritLock.Lock();
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

	int inumchansplaying = 0;
	result = mpFmodSystem->getChannelsPlaying( & inumchansplaying );

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
						{	//OrkAssert(pud!=0);

							if( pud )
							{
								AudDevPbFmod* padpbf = (AudDevPbFmod*) pud;
								AudioInstrumentPlayback *ppb = padpbf->mPlayback;

								//float fdistamp = ppb->GetDistanceAmp();

								OrkAssert(ppb!=0);

								int isubch = padpbf->miChannelIndex;

								const AudioInstrumentZoneContext& izctx = ppb->GetZonePlayback(isubch)->GetContext();

								ppb->DeviceUpdateChannel( fdt, isubch );
								float frate = ppb->GetZonePlayback(isubch)->GetPBSampleRate();

								//orkprintf( "ch<%p> izctx<%p> isubcg<%d> frq<%f>\n", channel, & izctx, isubch, frate );

								channel->setFrequency( frate );

								float fpan = izctx.mfPan;

								//channel->setPan(fpan);

								float fsubmix = GetSubMix( ppb->GetSubMix().c_str() );


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
	//FmodCritLock.UnLock();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// upload sample data to FMOD

void AudioDeviceFMOD::DoInitSample( AudioSample & sample )
{
	FmodCritLock.Lock();
	///////////////////////////////////////
	FMOD_CREATESOUNDEXINFO	FModInfo;
	memset( (void *) & FModInfo, 0, sizeof( FMOD_CREATESOUNDEXINFO ) );
	///////////////////////////////////////
	FModInfo.defaultfrequency = sample.GetSampleRate();
	FModInfo.numchannels = sample.GetNumChannels();
	FModInfo.format = FMOD_SOUND_FORMAT_PCM16;
	FModInfo.length = sample.GetDataLength();
	FModInfo.cbsize = sizeof( FMOD_CREATESOUNDEXINFO );
	///////////////////////////////////////
	U32 Mode = FMOD_OPENRAW|FMOD_OPENMEMORY|FMOD_SOFTWARE|FMOD_3D;
	///////////////////////////////////////
	FMOD::Sound *pfmodhandle = 0;
	const char* pdata = (const char *) sample.GetDataPointer();
	FMOD_RESULT result = mpFmodSystem->createSound( pdata, Mode, & FModInfo, & pfmodhandle );
    //result = pfmodhandle->setMode(FMOD_LOOP_OFF);
	pfmodhandle->setUserData( (void *) sample.GetDataPointer() );
	//mSounds->push_back( pfmodhandle );
	if( sample.IsLooped() )
	{
		int iloopstart = sample.GetLoopStart();
		int iloopend = sample.GetLoopEnd();
		pfmodhandle->setLoopPoints( iloopstart, FMOD_TIMEUNIT_PCM, iloopend, FMOD_TIMEUNIT_PCM );
		pfmodhandle->setMode( FMOD_LOOP_NORMAL );
	}
	///////////////////////////////////////
	OrkAssertI( FMOD_OK == result, "fmod error" );
	///////////////////////////////////////
	sample.SetPlatformHandle( (void *) pfmodhandle );
	DEBUG_FMOD_PRINT("DoInitSample:%x\n",(void *) pfmodhandle);
	FmodCritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::DoPlaySound( AudioInstrumentPlayback * PlaybackHandle )
{
	MCheckPointContext( "AudioDeviceFMOD::DoPlaySound" );
#ifdef KILL_AUDIO
	return;
#endif

	FmodCritLock.Lock();

	const AudioIntrumentPlayParam& pbparam = PlaybackHandle->GetPlaybackParam();

	float fmastervolume = 1.0f; //float(EffectsVolume)/128.0f;
	//float fpan = 0.0f; //(2.0f*(float( PlaybackHandle.GetStereoPan() ) / 127.0f))-1.0f;
	int inumchans = PlaybackHandle->GetNumChannels();

	int inumactive = 0;

	float fmaxdist = pbparam.mfMaxDistance;
	const MultiCurve1D* atten_curve = pbparam.mAttenCurve;

	for( int ich=0; ich<inumchans; ich++ )
	{
		AudioZonePlayback* ZonePB = PlaybackHandle->GetZonePlayback(ich);
		const AudioInstrumentZoneContext& izc = ZonePB->GetContext();

		const AudioSample *psample = ZonePB->GetSample();
		if( 0 == psample ) continue;

		float frate = ZonePB->GetPBSampleRate();

		const FMOD::Sound *pfmodhandle = (const FMOD::Sound *) psample->GetPlatformHandle();
		FMOD::Channel *channel;
		FMOD_RESULT result = mpFmodSystem->playSound( FMOD_CHANNEL_FREE, const_cast<FMOD::Sound *>( pfmodhandle ), true, &channel );
		OrkAssert( FMOD_OK==result );

		/////////////////////////////////////////////////////////////////
		// set initial volume
		/////////////////////////////////////////////////////////////////
		float fsubmix = GetSubMix( pbparam.mSubMixGroup.c_str() );
		float fvol = fmastervolume*izc.mfLinearAmplitude*fsubmix;

		if( PlaybackHandle->GetPlaybackParam().mEnable3D )
		{
			float fvol = 0.0f; //fmastervolume*izc.mfLinearAmplitude*fsubmix;
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
		int ifmodchanindex = -1;
		result = channel->getIndex( & ifmodchanindex );
		OrkAssert( FMOD_OK==result );
		AudDevPbFmod& rfmodpb = mFmodPlaybacks[ ifmodchanindex ];
		rfmodpb.mPlayback = PlaybackHandle;
		rfmodpb.miChannelIndex = ich;
		rfmodpb.mpChannel = channel;
		rfmodpb.mpFilter = mFilterHandles[ifmodchanindex];
		void* pbiquadd = 0;
		result = rfmodpb.mpFilter->getUserData( & pbiquadd );
		fmod_sw_biquad* pbiquad = (fmod_sw_biquad*) pbiquadd;
		pbiquad->set_ctx( & izc );
		channel->setUserData( (void *) & rfmodpb );
		result = channel->addDSP( rfmodpb.mpFilter, 0 );
		//rfmodpb.mpFilter->setBypass(false);
		OrkAssert( FMOD_OK==result );
		//////////////////////////////////////////////
		FMOD_MODE mode;
		result = channel->getMode( & mode );
		OrkAssert( FMOD_OK==result );



		if( fmaxdist != 0.0f ) //atten_curve )
		{
			result = channel->set3DMinMaxDistance( 1.0f, fmaxdist );
			OrkAssert( FMOD_OK==result );
			//mAttenShape
			//miNumAttenPoints
		}
		else
		{
		//	result = channel->set3DMinMaxDistance( 0.01f, 10000.0f );
		//	OrkAssert( FMOD_OK==result );
			//static FMOD_VECTOR knorolloff[2] = 
			//{
			//	{ 0.0f,		1.0f, 0.0f },
			//	{ 1000.0f,  1.0f, 0.0f }
			//};
			//channel->set3DCustomRolloff( knorolloff, 2 );
		}

		//mode = (mode & ~FMOD_3D_WORLDRELATIVE) | (FMOD_3D_HEADRELATIVE) | FMOD_3D_LOGROLLOFF | FMOD_3D_HEADRELATIVE | FMOD_LOOP_OFF;
		mode = (mode & ~FMOD_3D_HEADRELATIVE) | (FMOD_3D_WORLDRELATIVE) | FMOD_3D_LINEARROLLOFF; //FMOD_3D_CUSTOMROLLOFF;
		channel->setMode(mode);
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

						//padpf->mpFilter->setBypass(true);

						phandle->DecrNumActiveChannels();

					}
				}
				return FMOD_OK;
			}
			///////////////////////////////////////
			// SyncPoint Callback ( for wave markers )
			///////////////////////////////////////
			static FMOD_RESULT F_CALLBACK SyncPointCallback( FMOD_CHANNEL* channel, FMOD_CHANNEL_CALLBACKTYPE type, int command, unsigned int cmddata1, unsigned int cmddata2 )
			{
				FMOD::Channel *cppchannel = (FMOD::Channel *)channel;
				return FMOD_OK;
			}
			///////////////////////////////////////
		};
		///////////////////////////////////////
		channel->setCallback( FMOD_CHANNEL_CALLBACKTYPE_END, ChannelCallback::EndCallback, ich );
		channel->setCallback( FMOD_CHANNEL_CALLBACKTYPE_SYNCPOINT, ChannelCallback::SyncPointCallback, 0 );
		///////////////////////////////////////
		ZonePB->SetChannelPB( channel );
		DEBUG_FMOD_PRINT("DoPlaySound:channel:%d:%x\n",ich,(void *) channel);

		///////////////////////////////////////
		inumactive++;
	}
	///////////////////////////////////////
	PlaybackHandle->SetNumActiveChannels( inumactive );
	DEBUG_FMOD_PRINT("DoPlaySound:numactive:%d\n",inumactive);

	///////////////////////////////////////
	FmodCritLock.UnLock();

}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::DoStopSound( AudioInstrumentPlayback * PlaybackHandle )
{
	MCheckPointContext( "AudioDeviceFMOD::DoStopSound" );

	FmodCritLock.Lock();
	int inumchans = PlaybackHandle->GetNumChannels();
	for( int ich=0; ich<inumchans; ich++ )
	{	AudioZonePlayback* zone = PlaybackHandle->GetZonePlayback(ich);
		FMOD::Channel *channel = (FMOD::Channel *) zone->GetChannelPB();
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
	FmodCritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

/*void AudioDeviceFMOD::DoSetSoundSpatial( AudioInstrumentPlayback * PlaybackHandle, const ork::CVector3& pos )
{
	MCheckPointContext( "AudioDeviceFMOD::DoSetSoundSpatial" );

	if( 0 == PlaybackHandle ) return;

	FmodCritLock.Lock();
	{	
		bool bena3d = PlaybackHandle->GetPlaybackParam().mEnable3D;
		int inumchans = PlaybackHandle->GetNumChannels();
		for( int ich=0; ich<inumchans; ich++ )
		{	AudioZonePlayback* zone = PlaybackHandle->GetZonePlayback(ich);
			FMOD::Channel *channel = (FMOD::Channel *) zone->GetChannelPB();
			if( channel )
			{	bool bplaying = false;
				FMOD_RESULT result = channel->isPlaying( & bplaying );
				if( FMOD_OK == result )
				{	if( bplaying )
					{	FMOD_VECTOR fmv;
						ork::CVector3 npos = bena3d	 ? pos : mListenerPos + (mListenerForward*100.0f);
						fmv.x = pos.GetX();
						fmv.y = pos.GetY();
						fmv.z = pos.GetZ();
						channel->set3DAttributes( & fmv, 0 );
					}
				}
			}
		}
	}
	FmodCritLock.UnLock();
}*/

void AudioDeviceFMOD::SetPauseState(bool bpause)
{
	MCheckPointContext( "AudioDeviceFMOD::SetPauseState" );

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
		//channelgroup->setVolume(0.0f);
		channelgroup->setPaused(true);
	}
	else if(0 == mpauseref) {
		FMOD::ChannelGroup * channelgroup;
		mpFmodSystem->getMasterChannelGroup( &channelgroup);
		//channelgroup->setVolume(1.0f);
		channelgroup->setPaused(false);
	}


}


///////////////////////////////////////////////////////////////////////////////

bool AudioDeviceFMOD::DoLoadStream( AudioStream* streamhandle, ConstString fname )
{
	FmodCritLock.Lock();
	FMOD_RESULT fmod_result;
	streamhandle->SetPlatformHandle( 0 );
	/////////////////////////////
	// get new stream
	AssetPath filename = fname.c_str();
	U32 Mode = FMOD_SOFTWARE|FMOD_CREATESTREAM|FMOD_2D  ;
	file::Path abspath = filename.ToAbsolute();
	FMOD::Sound *phandle = 0;
	fmod_result = mpFmodSystem->createStream( abspath.c_str(), Mode, 0, & phandle );
	streamhandle->SetPlatformHandle( phandle );

	FMOD_SOUND_TYPE sndtype;
	FMOD_SOUND_FORMAT sndfmt;
	int inumchannels = 0;
	int inumbits = 0;

	fmod_result = phandle->getFormat( & sndtype, & sndfmt, & inumchannels, & inumbits );

	streamhandle->SetNumChannels( inumchannels );

	//mSounds->push_back( phandle );
	/////////////////////////////
	FmodCritLock.UnLock();
	return (streamhandle->GetPlatformHandle()!=0);
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::SetStreamSubMix( AudioStreamPlayback* pb, float fgrpa, float fgrpb, float fgrpc )
{
	MCheckPointContext( "AudioDeviceFMOD::SetStreamSubMix" );
	FmodCritLock.Lock();
	FMOD::Channel* pchannel = (FMOD::Channel*) pb;
	if( pchannel )
	{
		FMOD::Sound* sound = 0;
		FMOD_RESULT result = pchannel->getCurrentSound( & sound );
		if( sound )
		{
			OrkAssert( result == FMOD_OK );
			FMOD_SOUND_TYPE sndtype;
			FMOD_SOUND_FORMAT sndfmt;
			int inumchannels = 0;
			int inumbits = 0;
			result = sound->getFormat( & sndtype, & sndfmt, & inumchannels, & inumbits );
			OrkAssert( result == FMOD_OK );
			OrkAssert( inumchannels==6 );

			float flevelsL[6] = { fgrpa, 0.0f, fgrpb, 0.0f, fgrpc, 0.0f };
			float flevelsR[6] = { 0.0f, fgrpa, 0.0f, fgrpb, 0.0f, fgrpc };

			result = pchannel->setSpeakerLevels( FMOD_SPEAKER_FRONT_LEFT, flevelsL, 6 );
			OrkAssert( result == FMOD_OK );
			result = pchannel->setSpeakerLevels( FMOD_SPEAKER_FRONT_RIGHT, flevelsR, 6 );
			OrkAssert( result == FMOD_OK );
		}

	}
	FmodCritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::FillInSyncPoints( AudioStream* streamhandle, orkmap<ork::PoolString, float> &points )
{	FmodCritLock.Lock();
	if(FMOD::Sound *pstream = (FMOD::Sound *)streamhandle->GetPlatformHandle())
	{	int numsyncpoints;
		pstream->getNumSyncPoints(&numsyncpoints);
		for(int i = 0; i < numsyncpoints; i++)
		{	FMOD_SYNCPOINT *sync = NULL;
			pstream->getSyncPoint(i, &sync);
			unsigned int offset = 0xffffffff;
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
	FmodCritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

float AudioDeviceFMOD::GetStreamLength( AudioStream* streamhandle )
{	FmodCritLock.Lock();
	unsigned int length = 0;
	if(FMOD::Sound *pstream = (FMOD::Sound *)streamhandle->GetPlatformHandle())
		pstream->getLength(&length, FMOD_TIMEUNIT_MS);
	FmodCritLock.UnLock();
	return float(length) * 0.001f;
}

///////////////////////////////////////////////////////////////////////////////

AudioStreamPlayback* AudioDeviceFMOD::DoPlayStream( AudioStream* streamhandle )
{
	MCheckPointContext( "AudioDeviceFMOD::DoPlayStream" );

	if( 0 == streamhandle ) return 0;
	FmodCritLock.Lock();
	FMOD::Sound *pstream = (FMOD::Sound *) streamhandle->GetPlatformHandle();
	FMOD::Channel* pchannel = 0;
#ifdef KILL_AUDIO
	float fvolume = 0.0f;
#else
	//float fvolume = pdev->mCurrentStreamContext.mfVolume;
	float fvolume = 1.0f; //float(MusicVolume)/128.0f;
#endif
	if( pstream )
	{	FMOD_RESULT result;

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

		result = mpFmodSystem->playSound( FMOD_CHANNEL_FREE, pstream, true, &pchannel );
		result = pchannel->setVolume(fvolume);
		result = pchannel->setPan( 0.0f );
		result = pchannel->setPaused(false);
		result = pchannel->setUserData( NULL);
		result = pchannel->setReverbProperties( & reverbprops );

	}
	FmodCritLock.UnLock();
	DEBUG_FMOD_PRINT("DoPlayStream:%x: %x\n",(void *)pchannel,(void *) pstream);
	return 0; //AudioStreamPlayback(pchannel);
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::DoStopStream( AudioStreamPlayback* pb )
{
	MCheckPointContext( "AudioDeviceFMOD::DoStopStream" );
	DEBUG_FMOD_PRINT("DoStopStream:%x:\n",(void *) pb);
	FmodCritLock.Lock();
	FMOD::Channel* pchannel = (FMOD::Channel*) pb;
	if( pchannel )
	{	pchannel->stop();
	}
	FmodCritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::SetStreamVolume( AudioStreamPlayback* streampbh, float fvol )
{
	MCheckPointContext( "AudioDeviceFMOD::SetStreamVolume" );
	FmodCritLock.Lock();
	if( streampbh )
	{
		float fmastervolume = 1.0f;
		FMOD::Channel *pchannel = (FMOD::Channel *) streampbh;
		pchannel->setVolume( fvol*fmastervolume );
	}
	FmodCritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::SetStreamTime( AudioStreamPlayback* streampbh, float time )
{
	MCheckPointContext( "AudioDeviceFMOD::SetStreamTime" );
	if(streampbh)
	{	FmodCritLock.Lock();
		FMOD::Channel *pchannel = (FMOD::Channel *) streampbh;
		unsigned int position = (unsigned int)(time * 1000.0f);
		pchannel->setPosition(position, FMOD_TIMEUNIT_MS);
		FmodCritLock.UnLock();
	}
}

///////////////////////////////////////////////////////////////////////////////

float AudioDeviceFMOD::GetStreamTime( AudioStreamPlayback* streampbh )
{
	MCheckPointContext( "AudioDeviceWII::GetStreamTime" );
	if(streampbh)
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
					return float(position) * 0.001f;
				}
			}
		}
	}
	return -1.0f;
}

float AudioDeviceFMOD::GetStreamPlaybackLength( AudioStreamPlayback* streampb_handle )
{
	MCheckPointContext( "AudioDeviceFMOD::GetStreamPlaybackLength" );
	if(streampb_handle)
	{	FmodCritLock.Lock();
		FMOD::Channel *pchannel = (FMOD::Channel *)streampb_handle;
		FMOD::Sound *psound = NULL;
		pchannel->getCurrentSound(&psound);
		if(psound)
		{	unsigned int length;
			psound->getLength(&length, FMOD_TIMEUNIT_MS);
			return float(length) * 0.001f;
		}
		FmodCritLock.UnLock();
	}
	return 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceFMOD::StopStream( AudioStreamPlayback* StreamHandle )
{
	MCheckPointContext( "AudioDeviceFMOD::StopStream" );
	DEBUG_FMOD_PRINT("StopStream:%x: \n",(void *) StreamHandle);
	if( 0 == StreamHandle ) return; // dani is an asshole
	FMOD::Channel *pchannel = (FMOD::Channel *) StreamHandle;
	FmodCritLock.Lock();
	FMOD_RESULT result = pchannel->stop();
	FmodCritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

}}
