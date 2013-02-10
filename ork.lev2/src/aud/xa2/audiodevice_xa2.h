////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _HWPC_AUDIODEVICE_XA2_H
#define _HWPC_AUDIODEVICE_XA2_H

#include <XAudio2.h>
#include <X3daudio.h>
#include <Xaudio2fx.h>
#include <ork/file/file.h>
#include <ork/file/riff.h>

#if defined(_XBOX)
#define USE_XWMA
#endif

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

#if defined(USE_XWMA)

class XWMAStream
{
public:
	ork::RiffFile2*		mpFile;
    XMA2WAVEFORMATEX	mFormat;
	const char*			mpAudioData;
	const char*			mpDPDS;
	int					miAudioLength;
	int					miDPDSLength;
	XAUDIO2_BUFFER_WMA	mBufferWma;
	XAUDIO2_BUFFER		mAudioBuffer;

protected:

public:
	XWMAStream();
	~XWMAStream();

	bool ReadAudioData();

	bool Open( const ork::file::Path& pth );
    bool Close();

    const XMA2WAVEFORMATEX* GetFormat()
    {
        return &mFormat;
    };

	int GetAudioDataLen() const { return miAudioLength; }
	int GetDpDsDataLen() const { return miAudioLength; }
	const void* GetAudioData() const { return mpAudioData; }
	const void* GetDpDsData() const { return mpDPDS; }
};
#endif

class WAVStream
{
public:
	ork::RiffFile2*	mpFile;
	WAVEFORMATEX	mFormat;
	const char*		mpAudioData;
	const char*		mpDPDS;
	int				miAudioLength;
	int				miDPDSLength;
	XAUDIO2_BUFFER	mAudioBuffer;

protected:

public:
	WAVStream();
	~WAVStream();

	bool ReadAudioData();

	bool Open( const ork::file::Path& pth );
    bool Close();

    const WAVEFORMATEX* GetFormat()
    {
        return &mFormat;
    };

	int GetAudioDataLen() const { return miAudioLength; }
	int GetDpDsDataLen() const { return miAudioLength; }
	const void* GetAudioData() const { return mpAudioData; }
	const void* GetDpDsData() const { return mpDPDS; }
};
///////////////////////////////////////////////////////////////////////////////

struct AudDevPbXa2Base;
class AudioDeviceXa2;

///////////////////////////////////////////////////////////////////////////////

class AudDevXa2VoiceCallback : public IXAudio2VoiceCallback
{
public:
	AudDevXa2VoiceCallback();
	void BindVoice( AudDevPbXa2Base* pvoice );
	int GetNumStarts() const { return mNumStarts; }
	void UnBind();
	int GetSampleCounter() const { return int(miSampleIndex); }

private:
	
	void SyncSampleBase();

	AudDevPbXa2Base*						mVoice;
	int										mNumStarts;
	u64										miSampleIndexBase;
	u64										miSampleIndex;
	ork::recursive_mutex*					mMutex;

	STDMETHOD_(void, OnVoiceProcessingPassStart) (THIS_ UINT32 BytesRequired);
    STDMETHOD_(void, OnVoiceProcessingPassEnd) (THIS) {}
    STDMETHOD_(void, OnStreamEnd) (THIS);
    STDMETHOD_(void, OnBufferStart) (THIS_ void* pBufferContext);
    STDMETHOD_(void, OnBufferEnd) (THIS_ void* pBufferContext);
    STDMETHOD_(void, OnLoopEnd) (THIS_ void* pBufferContext) {}
    STDMETHOD_(void, OnVoiceError) (THIS_ void* pBufferContext, HRESULT Error);
};

///////////////////////////////////////////////////////////////////////////////

struct AudDevPbXa2Base
{
	AudDevXa2VoiceCallback		mCallback;
	IXAudio2SourceVoice*		mXa2Voice;
	AudioDeviceXa2*				mpDevice;
	AudioStreamPlayback::ESTATE	meState;

	AudioStreamPlayback::ESTATE GetState() const { return meState; }

	void UnBind()
	{
		mCallback.UnBind();
		DoUnBind();
	}
	void Bind( AudioDeviceXa2* pdev )
	{
		mpDevice = pdev;
		mCallback.BindVoice(this);
		DoBind(pdev);
	}
	virtual void DoStart() {}
	virtual void DoStop() {}
	virtual void DoBind(AudioDeviceXa2* pdev) {}
	virtual void DoUnBind() {}
	virtual void DoInit(AudioDeviceXa2* pdev) {}
	virtual void DoOnStreamStart() {}
	virtual void DoOnStreamEnd() {}
	virtual void DoSetState(AudioStreamPlayback::ESTATE estate ) {}
	void Init(AudioDeviceXa2* pdev)
	{
		mXa2Voice = 0;
		mpDevice = pdev;
		DoInit(pdev);
	}
	void Start()
	{
		SetState(AudioStreamPlayback::EST_STARTING);
		DoStart();
	}
	void Stop()
	{
		DoStop();
	}
	void OnStreamEnd()
	{
		SetState(AudioStreamPlayback::EST_ENDING);
		DoOnStreamEnd();
	}
	void OnStreamStart()
	{
		SetState(AudioStreamPlayback::EST_RUNNING);
		DoOnStreamStart();
	}
	void SetState( AudioStreamPlayback::ESTATE estate )
	{
		DoSetState( estate );
		meState = estate;
	}

	AudDevPbXa2Base() : mCallback() {}
};

///////////////////////////////////////////////////////////////////////////////

struct AudDevPbXa2 : public AudDevPbXa2Base
{
	AudioInstrumentPlayback*	mInstPlayback;
	AudioZonePlayback*			mZonePlayback;
	int							miChannelIndex;
	X3DAUDIO_EMITTER			mXa2Emitter;
	float						mBaseVolume;

	AudDevPbXa2()
		: mInstPlayback(0)
		, mZonePlayback(0)
		, mBaseVolume(0.0f)
		, miChannelIndex(-1)
	{
		std::memset( & mXa2Emitter, 0, sizeof( mXa2Emitter ) );
	}
	void DoBind(AudioDeviceXa2* pdev) // virtual
	{
	}
	void DoUnBind() // virtual
	{
	}
	void DoInit(AudioDeviceXa2* pdev) // virtual
	{
	}
	void DoStop() // virtual
	{
		if( mZonePlayback ) mZonePlayback->SetChannelPB(0);
	}
};

///////////////////////////////////////////////////////////////////////////////

struct AudDevPbXwma : public AudDevPbXa2Base
{
	AudioStreamPlayback		mStreamPB;
	AudDevPbXwma()
		: mStreamPB()
	{
	}
	void DoBind(AudioDeviceXa2* pdev) // virtual
	{
		mStreamPB.mpPlatformHandle = (void*) this;
		OrkAssert( mStreamPB.IsAvailable() );
		mStreamPB.meState = AudioStreamPlayback::EST_ALLOCATED;
	}
	void DoUnBind() // virtual
	{
	}
	void DoInit(AudioDeviceXa2* pdev) // virtual
	{
		mStreamPB.Init();
	}
	void DoStart() // virtual
	{
	}
	void DoStop() // virtual
	{
	}
	void DoOnStreamEnd()
	{
		//mStreamPB.meState = AudioStreamPlayback::EST_ENDING;
	}
	void DoOnStreamStart()
	{
		//mStreamPB.meState = AudioStreamPlayback::EST_RUNNING;
	}
	void DoSetState(AudioStreamPlayback::ESTATE estate ) // virtual
	{
		mStreamPB.meState = estate;
	}
};	


///////////////////////////////////////////////////////////////////////////////

template <typename T, int isize>
struct Xa2PlaybackPool
{
	static const int ksize = isize;

	typedef ork::fixed_pool<T,ksize> VoicePoolType;

	VoicePoolType						mVoicePool;
	AudioDeviceXa2*						mpDevice;


	inline T& DirectAccess( int idx ) { return mVoicePool.direct_access(idx); }
	inline const T& DirectAccess( int idx ) const { return mVoicePool.direct_access(idx); }
	int GetNumUsed() const { return int(mVoicePool.used().size()); }
	inline T* GetUsedItem( int idx ) { return mVoicePool.used()[idx]; }
	void Clear() { mVoicePool.clear(); }
	const typename VoicePoolType::pointervect_type& GetUsedPool() const { return mVoicePool.used(); }

	T* Alloc()
	{
		T* pb = mVoicePool.allocate();
		pb->Bind( mpDevice );
		return 	pb;
	}
	void Free(T*pb)
	{
		typename VoicePoolType::pointervect_type::const_iterator it = std::find( mVoicePool.used().begin(), mVoicePool.used().end(), pb );
		if( it!=mVoicePool.used().end() ) 
		{
			mVoicePool.deallocate(pb);
		}
		pb->UnBind();
	}

	void Init()
	{
		for( int i=0; i<ksize; i++ )
		{
			mVoicePool.direct_access(i).Init(mpDevice);
		}

	}

	void StopVoice( T* pvoice )
	{
		if( pvoice->mXa2Voice )
		{
			mpDevice->Lock();
			{
				XAUDIO2_VOICE_STATE state;  
				pvoice->mXa2Voice->GetState(&state);  
				HRESULT hr = pvoice->mXa2Voice->Stop( 0, XAUDIO2_COMMIT_NOW );
				OrkAssert( SUCCEEDED(hr) );
				pvoice->mXa2Voice->FlushSourceBuffers();
			}
			mpDevice->UnLock();
			//giVoiceCount--;
		}
	}

	void UpdatePool()
	{
		const VoicePoolType::pointervect_type& used = GetUsedPool();

		size_t inumused = used.size();

		ork::fixedvector<T*,ksize> ToFree;
		for( size_t i=0; i<inumused; i++ )
		{
			T* pvoice = used[i];

			XAUDIO2_VOICE_STATE state;  
			pvoice->mXa2Voice->GetState(&state);  

			if( state.BuffersQueued == 0 )
			{
				ToFree.push_back(pvoice);
			}
			else if( pvoice->GetState() == AudioStreamPlayback::EST_ENDING )
			{
				ToFree.push_back(pvoice);
			}
		}

		size_t inumtofree = ToFree.size();
		for( size_t i=0; i<inumtofree; i++ )
		{
			T* pvoice = ToFree[i];
			Free(pvoice);
			pvoice->Stop();
			pvoice->SetState( AudioStreamPlayback::EST_INACTIVE );

		}
	}

	Xa2PlaybackPool( AudioDeviceXa2* pdevice ) : mpDevice(pdevice) {}
};

///////////////////////////////////////////////////////////////////////////////

class AudioDeviceXa2 : public AudioDevice
{
public:

	AudioDeviceXa2();
	~AudioDeviceXa2();

	void Lock();
	void UnLock();

private:


	static const int kmaxhwchannels = 32;
	static const int kmaxhwstreamchannels = 4;
	typedef Xa2PlaybackPool<AudDevPbXa2,kmaxhwchannels>		Xa2VoicePool;
	typedef Xa2PlaybackPool<AudDevPbXwma,kmaxhwstreamchannels> Xa2xwmaVoicePool;

	std::string						mDriverName;
	int 							mpauseref;
	Xa2VoicePool					mXa2VoicePool;
	Xa2xwmaVoicePool				mXwmaVoicePool;
	bool							mbUserMusicMode;
	bool							mbLastUserMusicMode;

	UINT32							mOperationSetCounter;
	X3DAUDIO_LISTENER				mXa2Listener1;
	X3DAUDIO_LISTENER				mXa2Listener2;
	X3DAUDIO_DSP_SETTINGS			mXa2DspSettings1;
	X3DAUDIO_DSP_SETTINGS			mXa2DspSettings2;
	X3DAUDIO_DSP_SETTINGS			mXa2DspSettingsM;
	XAUDIO2_DEVICE_DETAILS			mXa2DeviceDetails;

	void							BeginUserMusic();
	void							EndUserMusic();

	bool IsUserPlaylistEnabled() const { return mbUserMusicMode; } // virtual

	/*virtual*/ AudioStreamPlayback*DoPlayStream( AudioStream* streamh );
	/*virtual*/ void				DoStopStream( AudioStreamPlayback* pb );
	/*virtual*/ bool				DoLoadStream( AudioStream* pstream, ConstString filename );

	/*virtual*/ void				FillInSyncPoints( AudioStream* streamhandle, orkmap<ork::PoolString, float> &points );
	/*virtual*/ float				GetStreamLength( AudioStream* streamhandle );

	/*virtual*/ void				DoReInitDevice( void );
	/*virtual*/ void				DoPlaySound( AudioInstrumentPlayback *playbackhandle );
	/*virtual*/ void				DoStopSound( AudioInstrumentPlayback *playbackhandle );
	/*virtual*/ void				DoInitSample( AudioSample & sample );
	/*virtual*/ int					DoGetFreeVoice( void ) { return 0; }
	/*virtual*/	void				SetPauseState(bool bpause);

	/*virtual*/ void				SetStreamVolume( AudioStreamPlayback* streampb_handle, float fvolume );
	/*virtual*/ void				SetStreamTime( AudioStreamPlayback* streampb_handle, float time );
	/*virtual*/ float				GetStreamTime( AudioStreamPlayback* streampb_handle );
	/*virtual*/ float				GetStreamPlaybackLength( AudioStreamPlayback* streampb_handle );
	/*virtual*/ void				SetStreamSubMix( AudioStreamPlayback*, float fgrpa, float fgrpb, float fgrpc );

	/*virtual*/ void				Update( float fdt );

	/*virtual*/ void				StopStream( AudioStreamPlayback* StreamHandle );

	/*virtual*/ void				SetReverbProperties( const AudioReverbProperties& reverb_props );

	/*virtual*/ void				DoStopAllVoices( void );

	virtual void					PushContext();
	virtual void					PopContext();

	virtual void					DestroyStream( AudioStream* pstream );
	virtual void					DestroyBank( AudioBank* pbank );

	void							SetVoice3dParams(AudDevPbXa2* pb,bool bforce);

	void UpdateVoicePools(bool bforce);

	void InitEffects();

};

///////////////////////////////////////////////////////////////////////////////

}}
#endif // HEADER GUARD
