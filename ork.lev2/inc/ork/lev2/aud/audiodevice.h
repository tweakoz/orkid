////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/cvector4.h>
#include <ork/math/cmatrix4.h>
#include <ork/asset/Asset.h>
#include <ork/util/endian.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/tempstring.h>
#include <ork/dataflow/dataflow.h>
#include <ork/kernel/orkpool.h>
#include <ork/math/multicurve.h>
#include <ork/math/TransformNode.h>
#include <ork/math/basicfilters.h>
#include <ork/kernel/any.h>

namespace ork { namespace lev2 {

typedef PoolString AudioBankKey;
typedef unsigned long int OrkAudPxvControlParamH;	// handle for changing misc audio params (like crossfade, filter params, effects, etc...)

struct WiiADPCMINFO
{
    // start context
    u16 coef[16];
    u16 gain;
    u16 pred_scale;
    u16 yn1;
    u16 yn2;

    // loop context
    u16 loop_pred_scale;
    u16 loop_yn1;
    u16 loop_yn2;

}; 

///////////////////////////////////////////////////////////////////////////////

class AudioDevice;
class AudioInstrumentZone;
class AudioSample;
class AudioProgram;
class AudioBank;
class AudioGraph;

typedef FixedString<24> SubMixString;

///////////////////////////////////////////////////////////////////////////////

class AudioZonePlayback;
static const int kmaxzonesperevent = 4;
struct AudioIntrumentPlayParam
{
	int					miNote;
	int					miVelocity;
	float				mPan;
	U32					mFlags;
	SubMixString		mMutexGroup;
	SubMixString		mSubMixGroup;
	bool				mEnable3D;
	bool				mPlayWhilePaused;
	AudioProgram*		mProgram;
	AudioZonePlayback*	mUserZonePlaybacks[kmaxzonesperevent];
	float				mfMaxDistance;
	const MultiCurve1D*		mAttenCurve;
	const ork::TransformNode*	mXf3d;

	static AudioIntrumentPlayParam DefaultParams;

	AudioIntrumentPlayParam();
};

///////////////////////////////////////////////////////////////////////////////

struct AudioInstrumentZoneContext
{
	float	mfLfo1Phase;
	float	mfLfo2Phase;
	float	mfCutoff;
	float	mfResonance;

	float	mfa1, mfa2;
	float	mfb0, mfb1, mfb2;

	float	mfPan;
	float	mfLinearAmplitude;
	AudioInstrumentZoneContext();
	void Reset();
	void SetLpfReson( float kfco, float krez );

};

///////////////////////////////////////////////////////////////////////////////

class AudioReverbProperties : public ork::Object
{
	RttiDeclareConcrete( AudioReverbProperties, ork::Object );

public:

	AudioReverbProperties();

	float GetDecayTime() const { return mfDecayTime; }
	float GetReflections() const { return mfReflections; }
	float GetReverbDelay() const { return mfReverbDelay; }
	float GetModulationDepth() const { return mfModulationDepth; }
	float GetEnvDiffusion() const { return mfEnvDiffusion; }
	float GetRoom() const { return mfRoom; }

private:

	float	mfDecayTime; // = 3.0f;			// 0.1 , 20.0 , 1.49 
	float	mfReflections; // = 0;			// -10000, 1000 , -2602  
	float	mfReverbDelay; // = 0.1;			// 0.0 , 0.1 , 0.011 
	float	mfModulationDepth; // = 1.0f;	// 0.0 , 1.0 , 0.0
	float	mfEnvDiffusion; // = 1.0f;		// 0.0 , 1.0 , 1.0 
	float	mfRoom; // = -500;				// -10000, 0 , -1000 

};

///////////////////////////////////////////////////////////////////////////////

class AudioInstrumentPlayback;

class AudioZonePlayback : public ork::Object
{
	RttiDeclareAbstract(AudioZonePlayback,ork::Object);

	friend class AudioDevice;

public:

	AudioZonePlayback();

	virtual void Update( float fdt ) {}

	const AudioInstrumentZoneContext&	GetContext() const { return mIzContext; }
	const AudioInstrumentPlayback*		GetInsPlayback() const { return mInsPlayback; }

	AudioInstrumentPlayback*			GetInsPlayback() { return mInsPlayback; }
	AudioInstrumentZone*				GetZone() const { return mpPxvInsZone; }
	AudioInstrumentZoneContext&			GetContext() { return mIzContext; }
	AudioSample*						GetSample() const { return mpPxvSample; }
	void*								GetChannelPB() const { return mpChannelPB; }
	int									GetChannel() const { return miSoundChannel; }
	float								GetPBSampleRate() const { return mfPlaybackSampleRate; }

	void								SetBaseKey( int ikey ) { mibasekey=ikey; }
	void								SetChannel( int ich ) { miSoundChannel=ich; }
	void								SetZone( AudioInstrumentZone* pz ) { mpPxvInsZone=pz; }
	void								SetSample( AudioSample* ps ) { mpPxvSample=ps; }
	void								SetChannelPB( void* pch ) { mpChannelPB=pch; }
	void								SetPBSampleRate( float psr ) { mfPlaybackSampleRate=psr; }
	void								SetInsPlayback( AudioInstrumentPlayback* pb ) { mInsPlayback=pb; }

protected:

	int							miSoundChannel;
	AudioInstrumentPlayback*	mInsPlayback;
	AudioInstrumentZone*		mpPxvInsZone;
	AudioInstrumentZoneContext	mIzContext;
	AudioSample*				mpPxvSample;
	void*						mpChannelPB;
	float						mfPlaybackSampleRate;
	int							mibasekey;
};

///////////////////////////////////////////////////////////////////////////////

class AudioSf2ZonePlayback : public AudioZonePlayback
{
	RttiDeclareAbstract(AudioSf2ZonePlayback,AudioZonePlayback);
	void Update( float fdt ) final;
};

///////////////////////////////////////////////////////////////////////////////

class AudioModule : public ork::dataflow::dgmodule
{
	RttiDeclareAbstract( AudioModule, ork::dataflow::dgmodule );
	////////////////////////////////////////////////////////////
	void Compute( ork::dataflow::workunit* wu ) override {}
	void CombineWork( const ork::dataflow::cluster* c ) final {}
	////////////////////////////////////////////////////////////
protected:
	AudioModule() {}
public:
	virtual void Compute( float dt ) = 0; // virtual
	void Reset() {}
};

///////////////////////////////////////////////////////////////////////////////

class AudioGlobalModule : public AudioModule
{
	RttiDeclareConcrete( AudioGlobalModule, AudioModule );

private:

	DeclareFloatXfPlug( TimeScale );

	dataflow::inplugbase* GetInput(int idx) final { return &mPlugInpTimeScale; } 
	dataflow::outplugbase* GetOutput(int idx) final ;

	DeclareFloatOutPlug( Time );
	DeclareFloatOutPlug( TimeDiv10 );
	DeclareFloatOutPlug( TimeDiv100 );
	DeclareFloatOutPlug( Random );
	DeclareFloatOutPlug( Noise );
	DeclareFloatOutPlug( FastNoise );
	DeclareFloatOutPlug( SlowNoise );

	void Compute( float dt ) final; // virtual

	float	mfNoiseRat;
	float	mfSlowNoiseRat;
	float	mfFastNoiseRat;
	float	mfNoisePrv;
	float	mfNoiseNew;
	float	mfSlowNoisePrv;
	float	mfFastNoisePrv;
	float	mfNoiseBas;
	float	mfSlowNoiseBas;
	float	mfFastNoiseBas;
	float	mfNoiseTim;
	float	mfSlowNoiseTim;
	float	mfFastNoiseTim;

public:

	AudioGlobalModule(); 
};

///////////////////////////////////////////////////////////////////////////////

class AudioLfoModule : public AudioModule
{
	RttiDeclareConcrete( AudioLfoModule, AudioModule );

public:

	AudioLfoModule(); 

private:

	///////////////////////////////////////
	DeclareFloatXfPlug( LfoFrequency );
	DeclareFloatXfPlug( LfoBias );
	DeclareFloatXfPlug( LfoAmplitude );
	MultiCurve1D		mLfoWaveform;
	dataflow::inplugbase* GetInput(int idx) final;
	///////////////////////////////////////
	dataflow::outplugbase* GetOutput(int idx) final;
	DeclareFloatOutPlug( Output );
	///////////////////////////////////////

	void Compute( float dt ) final;

};

///////////////////////////////////////////////////////////////////////////////

class AudioKRateFilterModule : public AudioModule
{
	RttiDeclareConcrete( AudioKRateFilterModule, AudioModule );

public:

	AudioKRateFilterModule(); 
private:
	float				mfWindow;
	avg_filter<60>		mFilter;	// replace with variable sample rate lowpass when available			
	///////////////////////////////////////
	DeclareFloatXfPlug( ControlInput );
	dataflow::inplugbase* GetInput(int idx) final;
	///////////////////////////////////////
	dataflow::outplugbase* GetOutput(int idx) final;
	DeclareFloatOutPlug( Output );
	///////////////////////////////////////

	void Compute( float dt ) final;

};

///////////////////////////////////////////////////////////////////////////////

enum EAUDOP2
{
	EAO2_ADD=0,
	EAO2_MUL,
	EAO2_AMINUSB,
	EAO2_BMINUSA,
};

class AudioOp2Module : public AudioModule
{
	RttiDeclareConcrete( AudioOp2Module, AudioModule );

public:

	AudioOp2Module(); 

private:

	DeclareFloatOutPlug( Output );
	DeclareFloatXfPlug( InputA );
	DeclareFloatXfPlug( InputB );

	EAUDOP2								meOp;

	dataflow::inplugbase* GetInput(int idx) final;
	dataflow::outplugbase* GetOutput(int idx) final;

	void Compute( float dt ) final; 

};

///////////////////////////////////////////////////////////////////////////////

class AudioExtConnectorModule : public AudioModule
{
	RttiDeclareConcrete( AudioExtConnectorModule, AudioModule );

	dataflow::dyn_external*	mpExternalConnector;
	ork::orklut<ork::PoolString, ork::dataflow::outplug<float>* >	mFloatPlugs;

	/////////////////////////////////////////////////////
	// data currently only flows in from externals
	dataflow::inplugbase* GetInput(int idx) final { return 0; }
	// data currently only flows in from externals
	/////////////////////////////////////////////////////

	int GetNumOutputs() const override;

	dataflow::outplugbase* GetOutput(int idx) final;
	void Compute( float dt ) final;


public:

	AudioExtConnectorModule(); 
	void BindConnector( dataflow::dyn_external* pconnector );
};

///////////////////////////////////////////////////////////////////////////////

class AudioModularZonePlayback;

class AudioHwSinkModule : public AudioModule
{
	RttiDeclareConcrete( AudioHwSinkModule, AudioModule );
	////////////////////////////////////////////////////////////
public:
	AudioHwSinkModule();
	void SetZonePB( AudioModularZonePlayback* pz ) { mZonePB=pz; }
private:
	void Compute( float dt ) final;
    AudioModularZonePlayback*	mZonePB;
	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////
	DeclareFloatXfPlug( Amplitude );	//	in absolute units (1.0=0db)
	DeclareFloatXfPlug( Pitch );		//	semitone offset 0.0f = original
	DeclareFloatXfPlug( Cutoff );		// 
	DeclareFloatXfPlug( Resonance );
	DeclareFloatXfPlug( Pan );
	dataflow::inplugbase* GetInput(int idx) final;
	/////////////////////////////////////////////////////
	// data currently only flows in to sinks
	dataflow::outplugbase* GetOutput(int idx) final { return 0; }
	// data currently only flows in to sinks
	/////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

class AudioGraph : public ork::dataflow::graph_inst
{
	RttiDeclareAbstract(AudioGraph,ork::dataflow::graph_inst);

public:

	AudioGraph();
	~AudioGraph();
	void operator=( const AudioGraph& oth );
	void compute();
	void Update( float fdt );
	void Reset();
	void PrepForStart();

	void BindZonePB( AudioModularZonePlayback* pb );

private:

	friend class AudioGraphPool;

	bool CanConnect( const ork::dataflow::inplugbase* pin, const ork::dataflow::outplugbase* pout ) const final;
	ork::dataflow::dgregisterblock					mdflowregisters;
	ork::dataflow::dgcontext						mdflowctx;

};

///////////////////////////////////////////////////////////////////////////////

class AudioGraphPool 
{
	const AudioGraph*			mTemplate;
	ork::pool<AudioGraph>*		mGraphPool;

public:

	AudioGraphPool();
	~AudioGraphPool();

	AudioGraph* Allocate();
	void Free(AudioGraph*);

	void BindTemplate( const AudioGraph& InTemplate, int icount );

};


///////////////////////////////////////////////////////////////////////////////

class AudioModularZonePlayback : public AudioZonePlayback
{
	RttiDeclareAbstract(AudioModularZonePlayback,AudioZonePlayback);
public:
	AudioGraph*	mAudioGraph;
	AudioModularZonePlayback() : mAudioGraph(0) {}
	void BindAudioGraph( AudioGraph* pgraph ) { mAudioGraph=pgraph; }
	void SetPitchOffset( float fpo ) { mfPitchOffset=fpo; }
	void SetAmplitude( float famp ) { mfAmplitude=famp; }
private:
	void Update( float fdt ) final;
    float mfPitchOffset;
	float mfAmplitude;
};

///////////////////////////////////////////////////////////////////////////////

class AudioInstrumentPlayback
{
	friend class AudioDevice;

public:

	AudioInstrumentPlayback() ;

	const AudioIntrumentPlayParam& GetPlaybackParam() const { return mParams; }

	const AudioZonePlayback* GetZonePlayback( int ich ) const;
	AudioZonePlayback* GetZonePlayback( int ich );
	void SetZonePlayback( int ich, AudioZonePlayback* );

	//////////////////////////////////////////////////
	// Basic Audio Interface

	void SetSubMix( const SubMixString& submix ) { mSubMix=submix; }
	void SetPitch( int ikey );										// 0 .. 127
	void SetFrequencyRatio( float fratio );							// 1.0f == base pitch
	void SetFrequencyOffset( float foffset );						// 1.0f == base pitch
	void SetVolume( int ivolume );									// 1.0f == base volume
	void SetStereoPan( int ipan );									// 0.0:left	1.0:right
	void SetParams( const AudioIntrumentPlayParam & params ) { mParams=params; }
	void SetFlags( U32 uflags ) { muFlags=uflags; }
	void SetDistanceAtten( float fv ) { mfDistanceAtten=fv; }

	//////////////////////////////////////////////////

	void SetMaxDist( float fv ) { mfMaxDistance=fv; }
	void SetAttenCurve( const MultiCurve1D* pcurve ) { mAttenCurve=pcurve; }

	//////////////////////////////////////////////////

	int	 GetStereoPan( void ) const { return mibasepan; }			// 0.0:left	1.0:right
	U32	 GetFlags( void ) { return muFlags; }

	//////////////////////////////////////////////////

	int  GetNumChannels( void ) const { return minumchannels; }
	int	GetNumActiveChannels( void ) const { return minumactivechannels; }
	void SetNumActiveChannels( int inch ) { minumactivechannels=inch; }

	void DecrNumActiveChannels( void )
	{
		if( minumactivechannels>0 )
		{
			minumactivechannels--;
		}
	}

	const SubMixString& GetSubMix() const { return mSubMix; }
	//////////////////////////////////////////////////

	void Stop( void );

	void SetLoopFlag( bool bena ) { mbLoopFlag=bena; }
	bool GetLoopFlag( void ) const { return mbLoopFlag; }

	//////////////////////////////////////////////////
	// Audio Effects Interface ( for data driven audio tweaking)

	OrkAudPxvControlParamH FindNamedParameter( PoolString paramname );

	void SetParameterInt( OrkAudPxvControlParamH param, int ival ) {}
	void SetParameterF32( OrkAudPxvControlParamH param, float fval ) {}

	//////////////////////////////////////////////////

	void DeviceUpdateChannel( float fdt, int ich );

	//////////////////////////////////////////////////

	void Update(float fDT);

	//////////////////////////////////////////////////

	void ReInit( void );

	void SetEmitterMatrix( const ork::TransformNode* mtx ) { mEmitterMatrix=mtx; }

	AudioGraph*							GetGraph() const { return mpAudioGraph; }
	void								SetGraph( AudioGraph* pgraph ) { mpAudioGraph=pgraph; }

	////////////////////////////////////////////////////
	// we supply the raw and cooked distance atten
	// in case a platgorm wants to use it's built in
	// attenuation, but we still provide the fallback
	// of builtin attenuation. 

	float GetDistanceAtten() const { return mfDistanceAtten; }
	float GetMaxDist() const { return mfMaxDistance; }
	const MultiCurve1D* GetAttenCurve() const { return mAttenCurve; }
	const ork::TransformNode* GetEmitterMatrix() const { return mEmitterMatrix; }

	const AudioProgram* GetProgram() const { return mpProgram; }

	////////////////////////////////////////////////////

	int GetSerialNumber() const { return miSerialNumber; }
	void SetSerialNumber(int is) { miSerialNumber=is; }

	void SetUserData0( anyp v0 ) { mUserData0=v0; }
	const anyp& GetUserData0() const { return mUserData0; }

protected:

	AudioZonePlayback*			mZonePlaybacks[kmaxzonesperevent];

	AudioGraph*					mpAudioGraph;
	AudioProgram*				mpProgram;
	AudioIntrumentPlayParam		mParams;
	float						mfDistanceAtten;
	const MultiCurve1D*			mAttenCurve;
	int							minumchannels;
	int							minumactivechannels;
	U32							muFlags;
	SubMixString				mSubMix;
	int							mibasepan;
	//fvec3					mEmitterPos;
	float						mfMaxDistance;
	const ork::TransformNode*	mEmitterMatrix;
	int							miSerialNumber;
	bool						mbLoopFlag;
	anyp						mUserData0;

	static int					giSerialCounter;

};

///////////////////////////////////////////////////////////////////////////////

class AudioStream : public asset::Asset 
{
	RttiDeclareConcrete( AudioStream, asset::Asset );

	void*	mpPlatformData;
	int		miNumChannels;

public:

	AudioStream() : mpPlatformData(0), miNumChannels(0) {}

	void* GetPlatformHandle() const { return mpPlatformData; }
	void SetPlatformHandle( void* pdata ) { mpPlatformData=pdata; }
	int GetNumChannels() const { return miNumChannels; }
	void SetNumChannels( int inumchan ) { miNumChannels=inumchan; }

};

class AudioStreamPlayback
{
public:
	enum ESTATE
	{
		EST_INACTIVE = 0,
		EST_ALLOCATED,
		EST_STARTING,
		EST_RUNNING,
		EST_ENDING,
	};

	AudioStreamPlayback()  {}
	bool					IsAvailable() const { return (meState==EST_INACTIVE); }
	void					Init() { meState=EST_INACTIVE; mpPlatformHandle=0; }
	ESTATE	meState;
	void*	mpPlatformHandle;
};

///////////////////////////////////////////////////////////////////////////////

class AudioDevice
{
public:
	static const int				kMaxPlayBackHandles = 48;

	typedef fixed_pool<AudioInstrumentPlayback,kMaxPlayBackHandles> PlaybackPool;
	typedef fixed_pool<AudioSf2ZonePlayback,kMaxPlayBackHandles> Sf2PlaybackPool;
	typedef fixed_pool<AudioModularZonePlayback,kMaxPlayBackHandles> ModPlaybackPool;
		
	virtual void ShutdownNow() {}

	const ork::fvec3& GetListenerPos1() const { return mListenerPos1; }
	const ork::fvec3& GetListenerForward1() const { return mListenerForward1; }
	const ork::fvec3& GetListenerPos2() const { return mListenerPos2; }
	const ork::fvec3& GetListenerForward2() const { return mListenerForward2; }

	void SetSubMix( const SubMixString& submixname, float fvolume );

	void SetDistMin(float fv) { mfMinDist = fv; }
	void SetDistMax(float fv) { mfMaxDist = fv; }
	void SetDistScale(float fv) { mfDistScale = fv; }
	void SetDistAttenPower(float fv) { mfDistAttenPower = fv; }

	float GetDistMin() const { return mfMinDist; }
	float GetDistMax() const { return mfMaxDist; }
	float GetDistScale() const { return mfDistScale; }
	float GetDistAttenPower() const { return mfDistAttenPower; }

	static const int			FLAGS_ONESHOT = 1;

	static AudioDevice *		GetDevice( void );

	void 						GameLevelInit(void);
	void						ReInitDevice( void );

	//////////////////

	virtual bool IsUserPlaylistEnabled() const { return false; }

	bool						LoadSoundBank( AudioBank* pstream, ConstString filename );
	AudioInstrumentPlayback *	PlaySound(	AudioProgram* prog, 
											const AudioIntrumentPlayParam & PlaybackParams
											= AudioIntrumentPlayParam::DefaultParams );
	AudioInstrumentPlayback *	PlaySound(	AudioProgram* prog, 
											const AudioIntrumentPlayParam & PlaybackParams,
											AudioGraph* controlgraph );

	void						StopSound( AudioInstrumentPlayback * phandle );
	void						StopAllVoices( void );
	void						MuteAllVoices( void ) { mbAllMuted=true; }
	void						UnMuteAllVoices( void ) { mbAllMuted=false; }

	//////////////////


	bool						LoadStream( AudioStream* pstream, ConstString filename );
	virtual void				FillInSyncPoints( AudioStream* streamhandle, orkmap<ork::PoolString, float> &points ) {}
	virtual float				GetStreamLength( AudioStream* streamhandle ) { return 0.0f; }
	AudioStreamPlayback*		PlayStream( AudioStream* pstream );
	void						StopStream( AudioStreamPlayback* pb );
	virtual void				SetStreamVolume( AudioStreamPlayback* streampb_handle, float fvolume ) = 0;
	virtual void				SetStreamTime( AudioStreamPlayback* streampb_handle, float time ) = 0;
	virtual float				GetStreamTime( AudioStreamPlayback* streampb_handle ) = 0;
	virtual float				GetStreamPlaybackLength( AudioStreamPlayback* streampb_handle ) { return 0.0f; }
	virtual void				SetStreamSubMix( AudioStreamPlayback*, float fgrpa, float fgrpb, float fgrpc ) {}

	virtual void				DestroyStream( AudioStream* pstream ) {}
	virtual void				DestroyBank( AudioBank* pbank ) {}
	virtual void				SetPauseState(bool bpause)=0;

	//////////////////

	void						TransportPause( void );
	void						TransportPlay( void );
	virtual void				Update( float fdt ) = 0;

	virtual void				SetReverbProperties( const AudioReverbProperties& reverb_props ) = 0;

	//////////////////

	void						SetListener1( const ork::fvec3& pos, const ork::fvec3& up, const ork::fvec3& forward );
	void						SetListener2( const ork::fvec3& pos, const ork::fvec3& up, const ork::fvec3& forward );

	//////////////////

	virtual void					PushContext() {}
	virtual void					PopContext() {}

	LockedResource<ModPlaybackPool>&ModZonePBPool() { return mModPlaybacks; }

	virtual ~AudioDevice() {}

protected:

	AudioDevice();

	LockedResource<Sf2PlaybackPool>	mSf2Playbacks;
	LockedResource<ModPlaybackPool>	mModPlaybacks;
	LockedResource<PlaybackPool>	mPlaybackHandles;

	ork::fvec3					mListenerPos1;
	ork::fvec3					mListenerUp1;
	ork::fvec3					mListenerForward1;
	ork::fvec3					mListenerPos2;
	ork::fvec3					mListenerUp2;
	ork::fvec3					mListenerForward2;

	float							mfDistScale;
	float							mfMinDist;
	float							mfMaxDist;
	float							mfDistAttenPower;
	int								miSerialNumber;
	bool							mbAllMuted;

	void							FreeZonePlayback( AudioZonePlayback* pzone );

	typedef ork::fixedlut<SubMixString,float,32> SubMixLut;

	SubMixLut mSubMixer;

	float GetSubMix( const SubMixString& submixname ) const;

private:

	AudioInstrumentPlayback*		GetFreePlayback( void );

	void							InitBank( AudioBank & bank );

	virtual void					DoStopStream( AudioStreamPlayback* pb ) = 0;
	virtual AudioStreamPlayback*	DoPlayStream( AudioStream* streamh ) = 0;
	virtual bool					DoLoadStream( AudioStream* pstream, ConstString filename ) = 0;

	virtual void					DoPlaySound( AudioInstrumentPlayback *playbackhandle ) = 0;
	virtual void					DoStopSound( AudioInstrumentPlayback *playbackhandle ) = 0;
	virtual void					DoInitSample( AudioSample & sample ) = 0;

	virtual void					DoReInitDevice( void ) = 0;
	virtual void					DoStopAllVoices( void ) {}

	static AudioDevice*		gpDevice;

	//////////////////

};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
