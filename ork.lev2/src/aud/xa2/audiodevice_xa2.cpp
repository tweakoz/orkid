////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/audiobank.h>
#include "audiodevice_xa2.h"
#include <ork/file/file.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/kernel/mutex.h>
#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/array.hpp>

#if defined(WIN32)
#if defined(_XBOX)
#include <xmp.h>
#endif
#pragma comment( lib, "X3daudio.lib" )

static bool IsStereoMode() { return true; }

#undef KILL_AUDIO
#define USE_XMP

#define Xa2Printf orkprintf

void OrkCoInitialize()
{
	static bool binit = true;

//	if( binit )
	{
#if ! defined(_XBOX)
		HRESULT hr = CoInitialize(NULL);
		OrkAssert( SUCCEEDED(hr) );
#endif
		binit = false;
	}
}

namespace ork { namespace lev2 {

static HANDLE UpdateThreadHandle = 0;
static DWORD UpdateThreadID = 0;
static ork::recursive_mutex Xa2CritLock( "Xa2_critsect" );

static IXAudio2*	gxa = 0;
static IXAudio2MasteringVoice*		gMasterVoice = NULL;
static IXAudio2SubmixVoice*			gGlobalUiVoice = NULL;
static IXAudio2SubmixVoice*			gGlobalDryVoice = NULL;
static float*						gDryMixMatrix = 0;
static X3DAUDIO_HANDLE gxa3d;

volatile static int giVoiceCount = 0;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static IXAudio2SubmixVoice*			gGlobalReverbVoice = NULL;
static XAUDIO2_EFFECT_DESCRIPTOR	gGlobalReverbDescriptor;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if defined( USE_THREAD )
DWORD WINAPI Xa2UpdateThread( LPVOID pArguments )
{
	AudioDeviceXa2* pXa2 = (AudioDeviceXa2*) pArguments;

	while(1)
	{
		static float LastTimeStep = CSystem::GetRef().GetLoResTime();
	    float ThisTimeStep = CSystem::GetRef().GetLoResTime();
		float timestep = (ThisTimeStep-LastTimeStep);
		LastTimeStep = ThisTimeStep;

		Xa2CritLock.Lock();
		static_cast<AudioDevice*>(pXa2)->Update(timestep);
		Xa2CritLock.UnLock();
		ork::msleep(33);
	}
	return 0;
}
#endif

#if defined(_XBOX) && defined(USE_XMP)
static HANDLE m_hNotificationListener;
static XMP_SONGINFO m_songInfo;                // Info about the current song.
static XMP_STATE m_XMPState;                // The current status of the music player
#endif


///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::SetReverbProperties( const AudioReverbProperties& reverb_props )
{
	Xa2CritLock.Lock();
	Xa2CritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static const XAUDIO2_VOICE_SENDS* GetSfxOutput()
{	static XAUDIO2_VOICE_SENDS gXa2Sends;
	static XAUDIO2_SEND_DESCRIPTOR gOutputVoices[3] =
	{	{ 0, gGlobalDryVoice },
		{ 0, gGlobalReverbVoice  },
		{ 0, gGlobalUiVoice  },
	};
	gXa2Sends.SendCount = 3;
	gXa2Sends.pSends = gOutputVoices;
	return & gXa2Sends;
}

static const XAUDIO2_VOICE_SENDS* GetMusicOutput()
{	static XAUDIO2_VOICE_SENDS gXa2Sends;
	static XAUDIO2_SEND_DESCRIPTOR gOutputVoices[1] =
	{	{ 0, gMasterVoice  },
	};
	gXa2Sends.SendCount = 1;
	gXa2Sends.pSends = gOutputVoices;
	return & gXa2Sends;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::DestroyStream( AudioStream* pstream )
{
	MCheckPointContext( "AudioDeviceXa2::DestroyStream" );
#if defined(USE_XWMA)
	XWMAStream* xmas = (XWMAStream*) pstream->GetPlatformHandle();
#else
	WAVStream* xmas = (WAVStream*) pstream->GetPlatformHandle();
#endif
	if( xmas )
	{
		delete xmas;
	}
	pstream->SetPlatformHandle(0);
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::DestroyBank( AudioBank* pbank )
{	MCheckPointContext( "AudioDeviceXa2::DestroyBank" );
	int inumsamples = pbank->GetNumSamples();
	for( int i=0; i<inumsamples; i++ )
	{	AudioSample& sample = pbank->RefSample(i);
		XAUDIO2_BUFFER* pBUFFER = (XAUDIO2_BUFFER*)	sample.GetPlatformHandle();
		if( pBUFFER )
		{	delete pBUFFER;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AudioDeviceXa2::~AudioDeviceXa2()
{
	if( gxa )
	{
		gxa->Release();
	}
}

AudioDeviceXa2::AudioDeviceXa2()
	: AudioDevice()
	, mpauseref(0)
	, mOperationSetCounter(0)
	, mXa2VoicePool(this)
	, mXwmaVoicePool(this)
	, mbUserMusicMode(false)
	, mbLastUserMusicMode(false)
{

	OrkCoInitialize();

	std::memset( & mXa2Listener1, 0, sizeof( mXa2Listener1 ) );
	std::memset( & mXa2Listener2, 0, sizeof( mXa2Listener2 ) );
	std::memset( & mXa2DspSettings1, 0, sizeof( mXa2DspSettings1 ) );
	std::memset( & mXa2DspSettings2, 0, sizeof( mXa2DspSettings2 ) );
	std::memset( & mXa2DspSettingsM, 0, sizeof( mXa2DspSettingsM ) );

#if 0 // defined(_DEBUG)
	HRESULT hr = XAudio2Create( & gxa, XAUDIO2_DEBUG_ENGINE , XAUDIO2_DEFAULT_PROCESSOR ) ; // 0
#else
	HRESULT hr = XAudio2Create( & gxa, 0 , XAUDIO2_DEFAULT_PROCESSOR ) ; // 0
#endif

	switch( hr )
	{
		case XAUDIO2_E_INVALID_CALL:
			orkprintf( "Xaudio2Create Failed<XAUDIO2_E_INVALID_CALL>\n" );
			break;
		case XAUDIO2_E_XMA_DECODER_ERROR:
			orkprintf( "Xaudio2Create Failed<XAUDIO2_E_XMA_DECODER_ERROR>\n" );
			break;
		case XAUDIO2_E_XAPO_CREATION_FAILED:
			orkprintf( "Xaudio2Create Failed<XAUDIO2_E_XAPO_CREATION_FAILED>\n" );
			break;
		case XAUDIO2_E_DEVICE_INVALIDATED:
			orkprintf( "Xaudio2Create Failed<XAUDIO2_E_DEVICE_INVALIDATED>\n" );
			break;
		case 0:
			break;
		default:
			orkprintf( "Xaudio2Create Failed<UNKNOWN %08x>\n", hr );
			break;
	}
	OrkAssert( SUCCEEDED(hr) );

	UINT32 deviceCount;
	gxa->GetDeviceCount(&deviceCount);
	XAUDIO2_DEVICE_DETAILS deviceDetails;
	int preferredDevice = 0;
	for (unsigned int i = 0; i < deviceCount; i++)
	{
	    gxa->GetDeviceDetails(i,&deviceDetails);
	    if (deviceDetails.OutputFormat.Format.nChannels > 2)
	    {
	        preferredDevice = i;
	    }
	}

	hr = gxa->GetDeviceDetails( preferredDevice, & mXa2DeviceDetails );
	OrkAssert( SUCCEEDED(hr) );

	hr = gxa->CreateMasteringVoice(	& gMasterVoice,
									XAUDIO2_DEFAULT_CHANNELS,
									XAUDIO2_DEFAULT_SAMPLERATE,
									0, preferredDevice, NULL );
	OrkAssert( SUCCEEDED(hr) );

	PushContext();


	////////////////////////////////////////////////////////

	mXa2DspSettings1.pMatrixCoefficients = new FLOAT32[mXa2DeviceDetails.OutputFormat.Format.nChannels];
	mXa2DspSettings1.SrcChannelCount = 1;
	mXa2DspSettings1.DstChannelCount = mXa2DeviceDetails.OutputFormat.Format.nChannels;
	mXa2DspSettings2.pMatrixCoefficients = new FLOAT32[mXa2DeviceDetails.OutputFormat.Format.nChannels];
	mXa2DspSettings2.SrcChannelCount = 1;
	mXa2DspSettings2.DstChannelCount = mXa2DeviceDetails.OutputFormat.Format.nChannels;
	mXa2DspSettingsM.pMatrixCoefficients = new FLOAT32[mXa2DeviceDetails.OutputFormat.Format.nChannels];
	mXa2DspSettingsM.SrcChannelCount = 1;
	mXa2DspSettingsM.DstChannelCount = mXa2DeviceDetails.OutputFormat.Format.nChannels;

	X3DAudioInitialize( mXa2DeviceDetails.OutputFormat.dwChannelMask, X3DAUDIO_SPEED_OF_SOUND, gxa3d );

	////////////////////////////////////////////////////////

	char nambuf[1024];
	WideCharToMultiByte(CP_ACP, 0, mXa2DeviceDetails.DisplayName, -1, nambuf, 1024, NULL, NULL);

	mDriverName = std::string( nambuf );

	////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////

	InitEffects();

	////////////////////////////////////////////////////////
	// Create SFX SubMix
	////////////////////////////////////////////////////////

	hr = gxa->CreateSubmixVoice(
		& gGlobalUiVoice,
		1,
		mXa2DeviceDetails.OutputFormat.Format.nSamplesPerSec,
		0,
		1,
		NULL,
		0
	);
	OrkAssert( SUCCEEDED(hr) );

	hr = gGlobalUiVoice->SetVolume( 1.0f, XAUDIO2_COMMIT_NOW );
	OrkAssert( SUCCEEDED(hr) );

	hr = gGlobalUiVoice->SetOutputVoices( GetMusicOutput() );
	OrkAssert( SUCCEEDED(hr) );

	float submixV[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

	hr = gGlobalUiVoice->SetOutputMatrix( gMasterVoice, 1, mXa2DspSettingsM.DstChannelCount, submixV, 0 );
	OrkAssert( SUCCEEDED(hr) );

	//

	hr = gxa->CreateSubmixVoice(
		& gGlobalDryVoice,
		mXa2DeviceDetails.OutputFormat.Format.nChannels,
		mXa2DeviceDetails.OutputFormat.Format.nSamplesPerSec,
		0,
		1,
		NULL,
		0
	);
	OrkAssert( SUCCEEDED(hr) );

	hr = gGlobalDryVoice->SetVolume( 1.0f, XAUDIO2_COMMIT_NOW );
	OrkAssert( SUCCEEDED(hr) );

	hr = gGlobalDryVoice->SetOutputVoices( GetMusicOutput() );
	OrkAssert( SUCCEEDED(hr) );

	int inumf = mXa2DspSettingsM.DstChannelCount*mXa2DspSettingsM.DstChannelCount;
	gDryMixMatrix = new float[ inumf ];

	for( int i=0; i<inumf; i++ )
	{
		int x = i/mXa2DspSettingsM.DstChannelCount;
		int y = i%mXa2DspSettingsM.DstChannelCount;
		gDryMixMatrix[i] = (x==y) ? 1.0f : 0.0f;
	}

	hr = gGlobalDryVoice->SetOutputMatrix( gMasterVoice, mXa2DspSettingsM.DstChannelCount, mXa2DspSettingsM.DstChannelCount, gDryMixMatrix, 0 );
	OrkAssert( SUCCEEDED(hr) );

	////////////////////////////////////////////////////////
	// Create SFX Voices
	////////////////////////////////////////////////////////

	WAVEFORMATEX fmtex;
	fmtex.wFormatTag = WAVE_FORMAT_PCM;
	fmtex.nChannels = 1;
	fmtex.nSamplesPerSec = 44100;
	fmtex.nAvgBytesPerSec = 88200;
	fmtex.nBlockAlign = 2;
	fmtex.wBitsPerSample  = 16;
	fmtex.cbSize = 0;

	mXa2VoicePool.Init();

	for( int i=0; i<kmaxhwchannels; i++ )
	{
		AudDevPbXa2& pb = mXa2VoicePool.DirectAccess( i );

		AudDevXa2VoiceCallback* callback = & pb.mCallback;


		hr = gxa->CreateSourceVoice(	& pb.mXa2Voice,
										& fmtex,
										XAUDIO2_VOICE_USEFILTER,
										XAUDIO2_MAX_FREQ_RATIO,
										callback,
										0, 0 );

		OrkAssert( SUCCEEDED(hr) );

		hr = pb.mXa2Voice->SetOutputVoices( GetSfxOutput() );
		OrkAssert( SUCCEEDED(hr) );

	}
	int inumused = mXa2VoicePool.GetNumUsed();
	//orkprintf("Init used %d\n",inumused);
	////////////////////////////////////////////////////////
	// Create Music Voices
	////////////////////////////////////////////////////////

#if defined(USE_XWMA)
	XMA2WAVEFORMATEX stereo_fmtex;
	stereo_fmtex.wfx.wFormatTag = 353;
	stereo_fmtex.wfx.wBitsPerSample = 16;
	stereo_fmtex.wfx.nSamplesPerSec = 44100;
	stereo_fmtex.wfx.nChannels = 2;
	stereo_fmtex.wfx.nBlockAlign = 2230;
	stereo_fmtex.wfx.cbSize = 0;
	stereo_fmtex.wfx.nAvgBytesPerSec = 6000;
#else
	WAVEFORMATEX stereo_fmtex;
	stereo_fmtex.wFormatTag = WAVE_FORMAT_PCM;
	stereo_fmtex.nChannels = 2;
	stereo_fmtex.nSamplesPerSec = 44100;
	stereo_fmtex.nAvgBytesPerSec = (44100*4);
	stereo_fmtex.nBlockAlign = 4;
	stereo_fmtex.wBitsPerSample  = 16;
	stereo_fmtex.cbSize = 0;
#endif
	mXwmaVoicePool.Init();

	for( int i=0; i<kmaxhwstreamchannels; i++ )
	{
		AudDevPbXwma& xwmapb = mXwmaVoicePool.DirectAccess( i );
		AudioStreamPlayback& pb = xwmapb.mStreamPB;

		AudDevXa2VoiceCallback* callback = & xwmapb.mCallback;


		hr = gxa->CreateSourceVoice(	& xwmapb.mXa2Voice,
										(WAVEFORMATEX*) & stereo_fmtex,
										XAUDIO2_VOICE_NOPITCH,
										XAUDIO2_MAX_FREQ_RATIO,
										callback,
										0, 0 );

		OrkAssert( SUCCEEDED(hr) );

		hr = xwmapb.mXa2Voice->SetOutputVoices( GetMusicOutput() );
		OrkAssert( SUCCEEDED(hr) );
		hr = xwmapb.mXa2Voice->SetVolume(1.0f,XAUDIO2_COMMIT_NOW);
		OrkAssert( SUCCEEDED(hr) );

	}

	////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////

#if defined( USE_THREAD )
	UpdateThreadHandle = CreateThread( NULL, 0, & Xa2UpdateThread, (void*)this, 0, & UpdateThreadID );
#endif


#if defined(_DEBUG)
	static XAUDIO2_DEBUG_CONFIGURATION dbgcfg;
	dbgcfg.TraceMask = XAUDIO2_LOG_ERRORS;
	dbgcfg.BreakMask = XAUDIO2_LOG_ERRORS;
	dbgcfg.LogFileline = true;
	dbgcfg.LogFunctionName = true;
	dbgcfg.LogThreadID = true;
	dbgcfg.LogTiming = true;
	gxa->SetDebugConfiguration( & dbgcfg );
#endif

#if defined(_XBOX) && defined(USE_XMP)
	// Create the notification listener to listen for XMP notifications
	m_hNotificationListener = XNotifyCreateListener( XNOTIFY_XMP );
	if( !m_hNotificationListener )
	{	OrkAssertI( false, "Error calling XNotifyCreateListener\n" );
	}
#endif

}

//////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::PushContext()
{
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::PopContext()
{
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::DoReInitDevice( void )
{
	Xa2CritLock.Lock();
	Xa2CritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::SetVoice3dParams( AudDevPbXa2* pb,bool bforce )
{
	HRESULT hr;
	XAUDIO2_VOICE_STATE voice_state;
	IXAudio2SourceVoice* xa2voice = pb->mXa2Voice;
	xa2voice->GetState( & voice_state );
	const AudioSample* psample_playing = (AudioSample*) voice_state.pCurrentBufferContext;

	////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////
	if( false == (psample_playing || bforce) ) return;
	////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////

	UINT64 inumplayed = voice_state.SamplesPlayed;
	UINT64 inumqueued = voice_state.BuffersQueued;

	AudioZonePlayback*					zonePB = pb->mZonePlayback;

	bool bPBMATCH = (zonePB->GetChannelPB() != (void*) pb);

	//if( false == bPBMATCH ) return;

	AudioInstrumentPlayback*			instPB = zonePB->GetInsPlayback();
	const AudioSample*					pchksample = zonePB->GetSample();
	const AudioInstrumentZoneContext&	izc = zonePB->GetContext();

	const AudioIntrumentPlayParam& pbparam = instPB->GetPlaybackParam();

	bool bena3d = pbparam.mEnable3D;

	if( bena3d && bforce )
	{
		instPB->SetEmitterMatrix( pbparam.mXf3d );
	}
	/////////////////////////////////////////////////////////////////
	// set initial volume
	/////////////////////////////////////////////////////////////////

	const float fmastervolume = 1.0f; //float(EffectsVolume)/128.0f;
	const float flinamp = izc.mfLinearAmplitude;
	float fvel = float(pbparam.miVelocity)/127.0f;
	if( -1 == pbparam.miVelocity ) fvel = 1.0f;
	const float fdisamp = instPB->GetDistanceAtten();
	const ork::lev2::SubMixString& submixname = instPB->GetSubMix();
	const float fsubmix = GetSubMix( submixname.c_str() );
	const float fcomposite_amp = fmastervolume*flinamp*fsubmix*fvel;
	float fpan = izc.mfPan;
	bool bUIVOICE = (0==strcmp(submixname.c_str(),""));

	//////////////////////////////////////////////
	// Set X3D emitter

	D3DVECTOR& xa2_dir_front = pb->mXa2Emitter.OrientFront;
	D3DVECTOR& xa2_dir_top = pb->mXa2Emitter.OrientTop;
	D3DVECTOR& xa2_dir_pos = pb->mXa2Emitter.Position;
	D3DVECTOR& xa2_dir_vel = pb->mXa2Emitter.Velocity;

	const ork::TransformNode3D* pxf = instPB->GetEmitterMatrix();
	if( 0 == pxf )
	{
		static TransformNode3D xfo;
		xfo.GetTransform()->SetPosition( mListenerPos1 + (mListenerForward1*10.0f) );
		pxf = & xfo;
	}
	CMatrix4 mtx;
	pxf->GetMatrix(mtx);
	CVector3 emi_X = mtx.GetXNormal();
	CVector3 emi_Y = mtx.GetYNormal();
	CVector3 emi_Z = mtx.GetZNormal();
	CVector3 emi_P = mtx.GetTranslation();

	emi_P =	bena3d 	?	emi_P
					:	mListenerPos1 + (mListenerForward1*10.0f);

	xa2_dir_front.x = emi_Z.GetX();
	xa2_dir_front.y = emi_Z.GetY();
	xa2_dir_front.z = emi_Z.GetZ();

	xa2_dir_top.x = emi_Y.GetX();
	xa2_dir_top.y = emi_Y.GetY();
	xa2_dir_top.z = emi_Y.GetZ();

	xa2_dir_pos.x = emi_P.GetX();
	xa2_dir_pos.y = emi_P.GetY();
	xa2_dir_pos.z = emi_P.GetZ();

	xa2_dir_vel.x = 0.0f;
	xa2_dir_vel.y = 0.0f;
	xa2_dir_vel.z = 0.0f;

	pb->mXa2Emitter.ChannelCount = 1;
	pb->mXa2Emitter.CurveDistanceScaler = 1.7f; ///20.0f;
	//pused->mXa2Emitter.pVolumeCurve = & xdc;

	//////////////////////////////////////////////

	X3DAudioCalculate(	gxa3d,
						& mXa2Listener1,
						& pb->mXa2Emitter,
						X3DAUDIO_CALCULATE_MATRIX|X3DAUDIO_CALCULATE_REVERB ,
						& mXa2DspSettings1 );

	X3DAudioCalculate(	gxa3d,
						& mXa2Listener2,
						& pb->mXa2Emitter,
						X3DAUDIO_CALCULATE_MATRIX|X3DAUDIO_CALCULATE_REVERB ,
						& mXa2DspSettings2 );


	//////////////////////////////////////////////////
	// MERGE listeners
	//////////////////////////////////////////////////

	if( bena3d )
	{
		const X3DAUDIO_VECTOR& v1 = mXa2Listener1.Position;
		const X3DAUDIO_VECTOR& v2 = mXa2Listener2.Position;
		ork::CVector3 xal1p( v1.x, v1.y, v1.z );
		ork::CVector3 xal2p( v2.x, v2.y, v2.z );

		float fd1 = (xal1p-emi_P).Mag();
		float fd2 = (xal2p-emi_P).Mag();

		mXa2DspSettingsM = (fd1<fd2) ? mXa2DspSettings1 : mXa2DspSettings2;
	}
	else
	{
		mXa2DspSettingsM = mXa2DspSettings1;
	}

	for( int i=0; i<int(mXa2DspSettingsM.DstChannelCount); i++ )
	{
		float fs = mXa2DspSettingsM.pMatrixCoefficients[i];
		mXa2DspSettingsM.pMatrixCoefficients[i] = std::powf(fs,.60f);
	}
	//////////////////////////////////////////////////

	//////////////////////////////////////////////////
	// compute send levels
	//////////////////////////////////////////////////

	CVector3 vOUTPUT; // dry rev ui

	if( bUIVOICE )
	{
		vOUTPUT = CVector3(0.0f,0.0f,1.0f);
	}
	else
	{
		vOUTPUT = CVector3(1.0f,0.2f,0.0);
	}

	//////////////////////////////////////////////////
	// update send levels
	//////////////////////////////////////////////////

	//////////////////////////
	//int inumf = mXa2DspSettings.DstChannelCount*mXa2DspSettings.DstChannelCount;
	//bool bdump = ( 0==strcmp(psample_playing->GetSampleName(),"Stinger_5") );

	for( int i=0; i<int(mXa2DspSettingsM.DstChannelCount); i++ )
	{
		float fs = vOUTPUT.GetArray()[0]*fcomposite_amp;
		gDryMixMatrix[i] = bena3d ? mXa2DspSettingsM.pMatrixCoefficients[i]*fs : fs;
		///////////////////////////////////
		// clamp
		///////////////////////////////////
		if( gDryMixMatrix[i]>0.75f ) gDryMixMatrix[i]=0.75f;
	}


	//const ork::lev2::AudioProgram* progr = instPB->GetProgram();
	//if( psample_playing && 0 == strcmp( "SpecialAttack_Sponge", progr->GetName().c_str() ) )
	//{
	//	const char* psampname = psample_playing->GetSampleName();
		//orkprintf( "UPD3d:: samp<%s> atten<%f,%f> pos<%f,%f,%f>\n", psampname, gDryMixMatrix[0], gDryMixMatrix[1], emi_P.GetX(), emi_P.GetY(), emi_P.GetZ() );
		//const ork::CMatrix4& mtx = mXform->GetMatrix();
		//const ork::CVector3 pos = mtx.GetTranslation();
		//if( instPB
	//}
	//////////////////////////
	// 3D Mix
	//////////////////////////
	hr = xa2voice->SetOutputMatrix( gGlobalDryVoice,
			1,
			mXa2DspSettingsM.DstChannelCount,
			gDryMixMatrix ,
			XAUDIO2_COMMIT_NOW
			);

	//////////////////////////
	// Reverb Mix
	//////////////////////////
	float freverb;
	freverb = ((gDryMixMatrix[0]+gDryMixMatrix[1])*0.5f)*vOUTPUT[1];
	hr = xa2voice->SetOutputMatrix( gGlobalReverbVoice,
			1,
			1,
			& freverb,
			XAUDIO2_COMMIT_NOW
			); //mXa2DspSettings.ReverbLevel);
	//////////////////////////
	// UI Mix
	//////////////////////////
	float fuimix;
	fuimix = vOUTPUT.GetArray()[2]*fcomposite_amp;
	hr = xa2voice->SetOutputMatrix( gGlobalUiVoice,
			1,
			1,
			& fuimix,
			XAUDIO2_COMMIT_NOW
			); //mXa2DspSettings.ReverbLevel);

	//////////////////////////////////////////////
	if( (zonePB->GetChannelPB() != (void*) pb) )
	{
		orkprintf(" zonePB->GetChannelPB():%x sampleaddr:%x sample name:%s zonePB: %x\n",zonePB->GetChannelPB(), pb, pchksample->GetSampleName(), zonePB);
	}

	if( zonePB->GetChannelPB() == (void*) pb )
	{

		//OrkAssert( zonePB->GetChannelPB() == (void*) pb );
		//OrkAssert( psample_playing == pchksample );

		float fzonerate = zonePB->GetPBSampleRate();
		float fsamprate = float(pchksample->GetSampleRate());
		float fratio = (fzonerate/fsamprate);


		hr = xa2voice->SetFrequencyRatio( fratio ); //, OperationID );
		if(SUCCEEDED(hr) == 0)
		{
			orkprintf("fzonerate:%f  fsamprate:%f  fratio:%f voice:%x sampleaddr:%x sample name:%s\n",fzonerate,fsamprate, fratio,xa2voice, pchksample, pchksample->GetSampleName());
		}
		OrkAssert( SUCCEEDED(hr) );
	}
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::Update( float fdt )
{
	const bool bMUTED = mbAllMuted;

	///////////////////////////////////////////////////////////////

#if defined(_XBOX) && defined(USE_XMP)
	BOOL busermusic;
	HRESULT hr = XMPTitleHasPlaybackControl( & busermusic );
	mbUserMusicMode = (false==busermusic);

	if( mbUserMusicMode && (false==mbLastUserMusicMode) )
	{
		BeginUserMusic();
	}
	if( (false==mbUserMusicMode) && mbLastUserMusicMode )
	{
		EndUserMusic();
	}

	mbLastUserMusicMode = mbUserMusicMode;
#endif

	///////////////////////////////////////////////////////////////

	Xa2CritLock.Lock();
	///////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////
	UINT32 OperationID = XAUDIO2_COMMIT_NOW; //UINT32(InterlockedIncrement(LPLONG(&mOperationSetCounter)));
	///////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////
	UpdateVoicePools(false);
	///////////////////////////////////////////////////////////

	if( bMUTED )
	{
		gGlobalUiVoice->SetVolume(1.0f,OperationID);
		gGlobalDryVoice->SetVolume(0.0f,OperationID);
		gGlobalReverbVoice->SetVolume(0.0f,OperationID);
	}
	else
	{
		gGlobalUiVoice->SetVolume(1.0f,OperationID);
		gGlobalDryVoice->SetVolume(1.0f,OperationID);
		gGlobalReverbVoice->SetVolume(1.0f,OperationID);
	}

	///////////////////////////////////////////////////////////

	int inumused = mXa2VoicePool.GetNumUsed();

	int inumupd = 0;

	//////////////////////////////////////////////
	{	// Set X3D listener 1
		D3DVECTOR& xa2_dir_front = mXa2Listener1.OrientFront;
		D3DVECTOR& xa2_dir_top = mXa2Listener1.OrientTop;
		D3DVECTOR& xa2_dir_pos = mXa2Listener1.Position;
		D3DVECTOR& xa2_dir_vel = mXa2Listener1.Velocity;

		xa2_dir_front.x = -mListenerForward1.GetX();
		xa2_dir_front.y = -mListenerForward1.GetY();
		xa2_dir_front.z = -mListenerForward1.GetZ();

		xa2_dir_top.x = mListenerUp1.GetX();
		xa2_dir_top.y = mListenerUp1.GetY();
		xa2_dir_top.z = mListenerUp1.GetZ();

		xa2_dir_pos.x = mListenerPos1.GetX();
		xa2_dir_pos.y = mListenerPos1.GetY();
		xa2_dir_pos.z = mListenerPos1.GetZ();

		xa2_dir_vel.x = 0.0f;
		xa2_dir_vel.y = 0.0f;
		xa2_dir_vel.z = 0.0f;
	}
	//////////////////////////////////////////////
	{	// Set X3D listener 1
		D3DVECTOR& xa2_dir_front = mXa2Listener2.OrientFront;
		D3DVECTOR& xa2_dir_top = mXa2Listener2.OrientTop;
		D3DVECTOR& xa2_dir_pos = mXa2Listener2.Position;
		D3DVECTOR& xa2_dir_vel = mXa2Listener2.Velocity;

		xa2_dir_front.x = -mListenerForward2.GetX();
		xa2_dir_front.y = -mListenerForward2.GetY();
		xa2_dir_front.z = -mListenerForward2.GetZ();

		xa2_dir_top.x = mListenerUp2.GetX();
		xa2_dir_top.y = mListenerUp2.GetY();
		xa2_dir_top.z = mListenerUp2.GetZ();

		xa2_dir_pos.x = mListenerPos2.GetX();
		xa2_dir_pos.y = mListenerPos2.GetY();
		xa2_dir_pos.z = mListenerPos2.GetZ();

		xa2_dir_vel.x = 0.0f;
		xa2_dir_vel.y = 0.0f;
		xa2_dir_vel.z = 0.0f;
	}
	//////////////////////////////////////////////
	//orkprintf("update used %d\n",inumused);
	for( int iu=0; iu<inumused; iu++ )
	{
		AudDevPbXa2* pused = mXa2VoicePool.GetUsedItem(iu);
		SetVoice3dParams( pused, false );
		inumupd++;
	}

	inumused = mXwmaVoicePool.GetNumUsed();
	float fmusicvol = GetSubMix("music");
	for( int iu=0; iu<inumused; iu++ )
	{
		AudDevPbXwma* pused = mXwmaVoicePool.GetUsedItem(iu);
		HRESULT hr = pused->mXa2Voice->SetVolume(fmusicvol,XAUDIO2_COMMIT_NOW);
		//SetVoice3dParams( pused, false );
		//inumupd++;
	}

#if defined(_XBOX)
	if( mbUserMusicMode )
	{
		hr = XMPSetVolume( fmusicvol, 0 );
	}
#endif

	Xa2CritLock.UnLock();
	miSerialNumber++;
}

///////////////////////////////////////////////////////////////////////////////
// upload sample data to Xa2

void AudioDeviceXa2::DoInitSample( AudioSample & sample )
{
	Xa2CritLock.Lock();
	///////////////////////////////////////
	XAUDIO2_BUFFER* pBUFFER = new XAUDIO2_BUFFER;
	sample.SetPlatformHandle( (void*) pBUFFER );
	pBUFFER->Flags = XAUDIO2_END_OF_STREAM;
	pBUFFER->AudioBytes = sample.GetDataLength();
	pBUFFER->pAudioData = (BYTE*) sample.GetDataPointer();
	pBUFFER->PlayBegin = 0;
	pBUFFER->PlayLength = sample.GetDataLength()/2;
	pBUFFER->pContext = (void*) & sample;
	///////////////////////////////////////
	if( sample.IsLooped() )
	{	int iloopstart = sample.GetLoopStart();
		int iloopend = sample.GetLoopEnd();
		pBUFFER->LoopBegin  = iloopstart;
		pBUFFER->LoopLength  = iloopend-iloopstart;
		pBUFFER->LoopCount = XAUDIO2_LOOP_INFINITE;
	}
	else
	{	pBUFFER->LoopBegin  = XAUDIO2_NO_LOOP_REGION;
		pBUFFER->LoopLength  = 0;
		pBUFFER->LoopCount = 0;
	}
	///////////////////////////////////////
	Xa2CritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::DoPlaySound( AudioInstrumentPlayback * PlaybackHandle )
{
	MCheckPointContext( "AudioDeviceXa2::DoPlaySound" );
#ifdef KILL_AUDIO
	return;
#endif

	Xa2CritLock.Lock();

	const AudioIntrumentPlayParam& pbparam = PlaybackHandle->GetPlaybackParam();

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

		AudDevPbXa2*xa2pb = mXa2VoicePool.Alloc();
		IXAudio2SourceVoice* xa2voice = xa2pb->mXa2Voice;
		xa2pb->mZonePlayback = ZonePB;
		
		Xa2Printf("DoPlaySound:channel:%d:Sample<%s>  xa2pb:0x%x currentPB:0x%x  zonepb:%x\n",ich, psample->GetSampleName(), xa2pb,ZonePB->GetChannelPB(), ZonePB);
		
		ZonePB->SetChannelPB( xa2pb );


		float frate = ZonePB->GetPBSampleRate();

		const XAUDIO2_BUFFER* pBUFFER = (XAUDIO2_BUFFER*) psample->GetPlatformHandle();

		HRESULT hr = xa2voice->SubmitSourceBuffer( pBUFFER );
		OrkAssert( SUCCEEDED(hr) );

		if( false == psample->IsLooped() )
		{
			xa2voice->Discontinuity();
		}

		SetVoice3dParams( xa2pb, true );

		/////////////////////////////////////////////////////////////////
		// set pan, freq, pause
		/////////////////////////////////////////////////////////////////


		hr = xa2voice->SetFrequencyRatio( 0.0f );

		OrkAssert( SUCCEEDED(hr) );


		bool blooped = psample->IsLooped();

		///////////////////////////////////////
		//Xa2Printf("DoPlaySound:channel:%d:Sample<%s>  xa2pb:0x%x addr of zonepb:%x\n",ich, psample->GetSampleName(), xa2pb, zonePB);
		if( 0 == strcmp( psample->GetSampleName(), "Pickup_02" ) )
		{
			printf( "yo\n" );
		}
		hr = xa2voice->Start( 0, XAUDIO2_COMMIT_NOW );
		xa2pb->Start();
		giVoiceCount++;
		OrkAssert( SUCCEEDED(hr) );
		///////////////////////////////////////
		inumactive++;
	}
	///////////////////////////////////////
	PlaybackHandle->SetNumActiveChannels( inumactive );
//	Xa2Printf("DoPlaySound:numactive:%d\n",inumactive, );

	///////////////////////////////////////
	Xa2CritLock.UnLock();

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::DoStopSound( AudioInstrumentPlayback * PlaybackHandle )
{
	MCheckPointContext( "AudioDeviceXa2::DoStopSound" );
	int inumchans = PlaybackHandle->GetNumChannels();
	for( int ich=0; ich<inumchans; ich++ )
	{	AudioZonePlayback* zone = PlaybackHandle->GetZonePlayback(ich);
		AudDevPbXa2* xa2pb = (AudDevPbXa2*) zone->GetChannelPB();
		if( xa2pb )
		{
			mXa2VoicePool.StopVoice(xa2pb);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AudDevXa2VoiceCallback::AudDevXa2VoiceCallback()
	: mVoice(0)
	, mNumStarts(0)
{
	mMutex = new ork::recursive_mutex("AudDevXa2VoiceCallback");
}
void AudDevXa2VoiceCallback::UnBind()
{
	mMutex->Lock();
	{
		mVoice=0;
		mNumStarts=0;
	}
	mMutex->UnLock();
}

void AudDevXa2VoiceCallback::SyncSampleBase()
{
	if( mVoice )
	{
		XAUDIO2_VOICE_STATE state;
		mVoice->mXa2Voice->GetState( & state );                                     
		miSampleIndexBase = int(state.SamplesPlayed);
		miSampleIndex = 0;
	}
	else
	{
		miSampleIndexBase = 0;
		miSampleIndex = 0;
	}
}
void AudDevXa2VoiceCallback::BindVoice( AudDevPbXa2Base* pvoice )
{
	mMutex->Lock();
	{
		mVoice=pvoice;
		mNumStarts=0;
		SyncSampleBase();
	}
	mMutex->UnLock();
}

static int gicounter = 0;

void AudDevXa2VoiceCallback::OnBufferStart(void* pBufferContext)
{
	mMutex->Lock();
	{
		SyncSampleBase();
		miSampleIndex = 0;
		mNumStarts++;
		OrkAssert( mVoice->GetState()!=AudioStreamPlayback::EST_RUNNING );
		mVoice->OnStreamStart();
		gicounter++;
	}
	mMutex->UnLock();
}

void AudDevXa2VoiceCallback::OnBufferEnd(void* pBufferContext)
{
	mMutex->Lock();
	{
		OrkAssert(mVoice);
		if( mVoice->GetState()==AudioStreamPlayback::EST_RUNNING )
		{
			mVoice->OnStreamEnd();
			SyncSampleBase();
		}
		gicounter--;
	}
	mMutex->UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudDevXa2VoiceCallback::OnStreamEnd()
{
	mMutex->Lock();
	{
		if( 0 == mVoice )
		{
			mMutex->UnLock();
			return;
		}
		if( mVoice->GetState()==AudioStreamPlayback::EST_RUNNING )
		{
			mVoice->OnStreamEnd();
			SyncSampleBase();
		}
	}
	mMutex->UnLock();
}

void AudDevXa2VoiceCallback::OnVoiceProcessingPassStart( UINT32 BytesRequired )
{
	mMutex->Lock();
	{
		if( mVoice )
		{
			XAUDIO2_VOICE_STATE state;
			mVoice->mXa2Voice->GetState( & state );
			miSampleIndex = state.SamplesPlayed-miSampleIndexBase;
		}
		else
		{
			miSampleIndex = 0;
		}
	}
	mMutex->UnLock();
}

void AudDevXa2VoiceCallback::OnVoiceError( void* pBufferContext, HRESULT Error)
{
	mMutex->Lock();
	{
		OrkAssert(false);
	}
	mMutex->UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::DoStopAllVoices( void )
{
	miSerialNumber += 100;

	//const Xa2VoicePool::pointervect_type& used = mXa2VoicePool.used();
	int inumtries = 0;
	while( mXa2VoicePool.GetNumUsed() )
	{
		Xa2VoicePool::VoicePoolType::pointervect_type myused = mXa2VoicePool.GetUsedPool();
		size_t inumused = myused.size();
		for( size_t i=0; i<inumused; i++ )
		{	AudDevPbXa2* pvoice = myused[i];
			mXa2VoicePool.StopVoice(pvoice);
		}
		Sleep(10);
		UpdateVoicePools(true);
		inumtries++;
	}
	PlaybackPool& pbpool = mPlaybackHandles.LockForWrite();
	{
		size_t icnt = pbpool.size();
		for( size_t i=0; i<icnt; i++ )
		{
			AudioInstrumentPlayback& pb = pbpool.direct_access(i);
			pb.SetSerialNumber( miSerialNumber++ );
		}
	}
	mPlaybackHandles.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::UpdateVoicePools(bool bforce)
{
	//////////////////////////////////////////////////

	mXa2VoicePool.UpdatePool();
	mXwmaVoicePool.UpdatePool();

	//////////////////////////////////////////////////

	PlaybackPool& pool = mPlaybackHandles.LockForWrite();
	{	const PlaybackPool::pointervect_type& used = pool.used();
		size_t inumused = used.size();
		orkvector<AudioInstrumentPlayback*> DonePBS;
		for( size_t i=0; i<inumused; i++ )
		{	AudioInstrumentPlayback* insPB = used[i];
			int inumz = insPB->GetNumChannels();
			int inumrunning = 0;
			bool bvoicedone = true;
			for( int j=0; j<inumz; j++ )
			{	AudioZonePlayback* pzb = insPB->GetZonePlayback(j);
				void* chb = pzb->GetChannelPB();
				if( 0 != chb ) bvoicedone=false;
			}
			if( bvoicedone )
			{
				DonePBS.push_back(insPB);
			}
		}
		///////////////////////////////
		// Free Done PBS
		///////////////////////////////
		size_t inumdone = DonePBS.size();
		for( size_t i=0; i<inumdone; i++ )
		{
			AudioInstrumentPlayback* insPB = DonePBS[i];
			int inumz = insPB->GetNumChannels();
			for( int j=0; j<inumz; j++ )
			{	AudioZonePlayback* pzb = insPB->GetZonePlayback(j);
				FreeZonePlayback(pzb);
			}
			pool.deallocate(insPB);

		}
	}
	mPlaybackHandles.UnLock();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::Lock()
{
	Xa2CritLock.Lock();
}
void AudioDeviceXa2::UnLock()
{
	Xa2CritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::SetPauseState(bool bpause)
{
	MCheckPointContext( "AudioDeviceXa2::SetPauseState" );

	if(bpause)
		mpauseref++;
	else
		mpauseref=0;
	// we don't double pause
	// we don't blanket unpause
	if(0 == mpauseref)
	{
		UnMuteAllVoices();
	}
	else
	{
		MuteAllVoices();
	}
}


///////////////////////////////////////////////////////////////////////////////

bool AudioDeviceXa2::DoLoadStream( AudioStream* streamhandle, ConstString fname )
{
	Xa2CritLock.Lock();
	/////////////////////////////
	// get new stream
	AssetPath filename = fname.c_str();
#if defined(USE_XWMA)
	filename.SetExtension("xwma");
	XWMAStream* xmas = new XWMAStream;
#else
	filename.SetExtension("wav");
	WAVStream* xmas = new WAVStream;
#endif
	file::Path abspath = filename.ToAbsolute();

	xmas->Open( filename );

	streamhandle->SetPlatformHandle(xmas);
	/////////////////////////////
	Xa2CritLock.UnLock();
	return (streamhandle->GetPlatformHandle()!=0);
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::SetStreamSubMix( AudioStreamPlayback* pb, float fgrpa, float fgrpb, float fgrpc )
{
	MCheckPointContext( "AudioDeviceXa2::SetStreamSubMix" );
	Xa2CritLock.Lock();
	Xa2CritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::FillInSyncPoints( AudioStream* streamhandle, orkmap<ork::PoolString, float> &points )
{	Xa2CritLock.Lock();
	Xa2CritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

float AudioDeviceXa2::GetStreamLength( AudioStream* streamhandle )
{	Xa2CritLock.Lock();
	Xa2CritLock.UnLock();
	return 1.0f; //float(length) * 0.001f;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::BeginUserMusic()
{
#if defined(_XBOX) && defined(USE_XMP)

	int inumused = mXwmaVoicePool.GetNumUsed();
	float fmusicvol = GetSubMix("music");
	for( int iu=0; iu<inumused; iu++ )
	{
		AudDevPbXwma* pused = mXwmaVoicePool.GetUsedItem(iu);
		HRESULT hr = pused->mXa2Voice->Stop(); //fmusicvol,XAUDIO2_COMMIT_NOW);
		//SetVoice3dParams( pused, false );
		//inumupd++;
	}


	////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////
	// Initialize XMP state variable
	DWORD dwStatus = XMPGetStatus( &m_XMPState );
	assert( dwStatus == ERROR_SUCCESS );

	HANDLE userh;
	DWORD CitemBuffer[2];
	HRESULT hr = XMPCreateUserPlaylistEnumerator(
         0, // DWORD dwFlags,
	     1, // DWORD cItems,
		 CitemBuffer, //DWORD *pcbBuffer,
			& userh
		);

	XMP_USERPLAYLISTINFO* upis = new XMP_USERPLAYLISTINFO[CitemBuffer[0]];

	DWORD numret = 0;
	hr = XEnumerate(
         userh,
         upis+0,
         (sizeof(XMP_USERPLAYLISTINFO)*1),
         & numret,
         0 );

	hr = XMPPlayUserPlaylist( upis, 0 );

	// Set the playback behavior to be in order and repeat the entire playlist
	//XMPSetPlaybackBehavior( XMP_PLAYBACKMODE_INORDER,
	//						XMP_REPEATMODE_PLAYLIST,
	//						0,
	//						NULL );

	// Play the playlist
	//XMPPlayTitlePlaylist( m_hXMPPlaylist, NULL, NULL );

	////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////
#endif
}

void AudioDeviceXa2::EndUserMusic()
{
#if defined(_XBOX) && defined(USE_XMP)
	////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////
	// Stop the XMP music
	////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////
	XMPStop( NULL );

	// Wait for the music to stop
	DWORD dwStatus = XMPGetStatus( &m_XMPState );
	assert( dwStatus == ERROR_SUCCESS );

	while( m_XMPState != XMP_STATE_IDLE )
	{
		Sleep( 1 );
		dwStatus = XMPGetStatus( &m_XMPState );
		assert( dwStatus == ERROR_SUCCESS );
	}

#endif

}

///////////////////////////////////////////////////////////////////////////////

AudioStreamPlayback* AudioDeviceXa2::DoPlayStream( AudioStream* streamhandle )
{
	if( mbUserMusicMode ) return 0;

	MCheckPointContext( "AudioDeviceXa2::DoPlayStream" );

	if( 0 == streamhandle ) return 0;
	Xa2CritLock.Lock();
#ifdef KILL_AUDIO
	float fvolume = 0.0f;
#else
	float fvolume = 1.0f; //float(MusicVolume)/128.0f;

#if defined(USE_XWMA)
	XWMAStream* xmas = (XWMAStream*) streamhandle->GetPlatformHandle();
#else
	WAVStream* xmas = (WAVStream*) streamhandle->GetPlatformHandle();
#endif

	AudioStreamPlayback* pb = 0;

	if( xmas )
	{
		AudDevPbXwma* xa2pb = mXwmaVoicePool.Alloc();
		pb = & xa2pb->mStreamPB;
		if( xa2pb )
		{
			XAUDIO2_VOICE_STATE state;
			xa2pb->mXa2Voice->GetState(&state);

			float fmusicvol = GetSubMix("music");

			HRESULT hr = xa2pb->mXa2Voice->FlushSourceBuffers();
			OrkAssert( SUCCEEDED(hr) );
#if defined(USE_XWMA)
			hr = xa2pb->mXa2Voice->SubmitSourceBuffer( & xmas->mAudioBuffer, & xmas->mBufferWma );
#else
			hr = xa2pb->mXa2Voice->SubmitSourceBuffer( & xmas->mAudioBuffer );
#endif
			OrkAssert( SUCCEEDED(hr) );
			hr = xa2pb->mXa2Voice->SetVolume(fmusicvol,XAUDIO2_COMMIT_NOW);
			OrkAssert( SUCCEEDED(hr) );
			//hr = xa2pb->mXa2Voice->SetFrequencyRatio(1.0f,XAUDIO2_COMMIT_NOW);
			//OrkAssert( SUCCEEDED(hr) );

			xa2pb->mXa2Voice->Discontinuity();


			hr = xa2pb->mXa2Voice->Start( 0, XAUDIO2_COMMIT_NOW );
			giVoiceCount++;
			OrkAssert( SUCCEEDED(hr) );
			xa2pb->Start();

			xa2pb->mXa2Voice->GetState(&state);

			pb->meState = AudioStreamPlayback::EST_STARTING;


			gxa->CommitChanges( XAUDIO2_COMMIT_ALL );
		}
	}
	else
	{
		orkprintf( "Out Of Stream Voices\n" );
	}

#endif

	Xa2CritLock.UnLock();
	return pb;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::DoStopStream( AudioStreamPlayback* pb )
{
	OrkAssert(pb!=0);
	MCheckPointContext( "AudioDeviceXa2::DoStopStream" );
	Xa2Printf("DoStopStream:%x:\n",(void *) pb);
	Xa2CritLock.Lock();

	AudDevPbXwma* xa2pb = (AudDevPbXwma*) pb->mpPlatformHandle;

	mXwmaVoicePool.StopVoice(xa2pb);

	Xa2CritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::SetStreamVolume( AudioStreamPlayback* streampbh, float fvol )
{
	OrkAssert(streampbh!=0);
	MCheckPointContext( "AudioDeviceXa2::SetStreamVolume" );
	Xa2CritLock.Lock();
	if( streampbh )
	{
		float fmastervolume = 1.0f;
	}
	Xa2CritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::SetStreamTime( AudioStreamPlayback* streampbh, float time )
{
	OrkAssert(streampbh!=0);
	MCheckPointContext( "AudioDeviceXa2::SetStreamTime" );
	if(streampbh)
	{	Xa2CritLock.Lock();
		unsigned int position = (unsigned int)(time * 1000.0f);
		Xa2CritLock.UnLock();
	}
}

///////////////////////////////////////////////////////////////////////////////

float AudioDeviceXa2::GetStreamTime( AudioStreamPlayback* streampbh )
{
	OrkAssert(streampbh!=0);
	MCheckPointContext( "AudioDeviceWII::GetStreamTime" );
	if(streampbh && (streampbh->meState == AudioStreamPlayback::EST_RUNNING) )
	{
		AudDevPbXwma* xa2pb = (AudDevPbXwma*) streampbh->mpPlatformHandle;
		if( xa2pb  )
		{
			XAUDIO2_VOICE_STATE state;
			xa2pb->mXa2Voice->GetState(&state);

			WAVStream* pstream = (WAVStream*) state.pCurrentBufferContext;

 	        bool isRunning = (state.BuffersQueued > 0) != 0;
			if( isRunning )
			{
				AudDevXa2VoiceCallback& callb = xa2pb->mCallback;

				float ftime = float(callb.GetSampleCounter())/48000.0f;
				return ftime;
			}
		}
	}
	return -1.0f;
}

float AudioDeviceXa2::GetStreamPlaybackLength( AudioStreamPlayback* streampb_handle )
{
	OrkAssert(streampb_handle!=0);
	MCheckPointContext( "AudioDeviceXa2::GetStreamPlaybackLength" );
	if(streampb_handle)
	{	Xa2CritLock.Lock();
		Xa2CritLock.UnLock();
	}
	return 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::StopStream( AudioStreamPlayback* StreamHandle )
{
	OrkAssert(StreamHandle!=0);
	MCheckPointContext( "AudioDeviceXa2::StopStream" );
	Xa2Printf("StopStream:%x: \n",(void *) StreamHandle);
	if( 0 == StreamHandle ) return; // dani is an asshole
	Xa2CritLock.Lock();
	Xa2CritLock.UnLock();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioDeviceXa2::InitEffects()
{
	////////////////////////////////////////////////////////

	static IUnknown * gpXAPO;
	HRESULT hr = XAudio2CreateReverb(&gpXAPO);

	gGlobalReverbDescriptor.pEffect = gpXAPO;
	gGlobalReverbDescriptor.InitialState = TRUE;
	gGlobalReverbDescriptor.OutputChannels = 1;

	static XAUDIO2_EFFECT_CHAIN gchain;
	gchain.EffectCount = 1;
	gchain.pEffectDescriptors = &gGlobalReverbDescriptor;

	hr = gxa->CreateSubmixVoice(
		& gGlobalReverbVoice,
		1,
		mXa2DeviceDetails.OutputFormat.Format.nSamplesPerSec,
		0,
		1,
		NULL,
		& gchain
	);
	OrkAssert( SUCCEEDED(hr) );

	hr = gGlobalReverbVoice->SetVolume( 1.0f, XAUDIO2_COMMIT_NOW );
	OrkAssert( SUCCEEDED(hr) );

	///////////////////////////////////////////////////////////////

	static XAUDIO2FX_REVERB_PARAMETERS gReverbParams;
	std::memset( & gReverbParams, 0, sizeof(gReverbParams) );

	gReverbParams.WetDryMix = 100.0f;
	gReverbParams.ReflectionsDelay = 300;
	gReverbParams.ReverbDelay = 85;
	gReverbParams.RearDelay = 5;
	gReverbParams.PositionLeft = 0;
	gReverbParams.PositionRight = 0;
	gReverbParams.PositionMatrixLeft = 0;
	gReverbParams.PositionMatrixRight = 0;
	gReverbParams.EarlyDiffusion = 15;
	gReverbParams.LateDiffusion = 15;
	gReverbParams.LowEQGain = 8;
	gReverbParams.LowEQCutoff = 9;
	gReverbParams.HighEQGain = 8;
	gReverbParams.HighEQCutoff = 14;
	gReverbParams.RoomFilterFreq = 2000.0f;
	gReverbParams.RoomFilterMain = 0.0f;
	gReverbParams.RoomFilterHF = 0.0f;
	gReverbParams.ReflectionsGain = 0.0f;
	gReverbParams.ReverbGain = 0.0f;
	gReverbParams.DecayTime = 3.0f;
	gReverbParams.Density = 100.0f;
	gReverbParams.RoomSize = 50.0f;

	hr = gGlobalReverbVoice->SetEffectParameters(
			0,
			(const void*) & gReverbParams,
			sizeof(gReverbParams),
			XAUDIO2_COMMIT_NOW
			);
	OrkAssert( SUCCEEDED(hr) );

}

}}
#endif // WIN32
