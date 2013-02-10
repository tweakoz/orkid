////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _HWWII_AUDIODEVICE_FMOD_H
#define _HWWII_AUDIODEVICE_FMOD_H

#include <fmod/fmod.hpp>
#include <fmod/fmod_errors.h>
#include <fmod/fmod_event.hpp>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

struct AudDevPbFmod
{
	AudioInstrumentPlayback*	mPlayback;
	FMOD::Channel*				mpChannel;
	//FMOD::DSP*					mpFilter;
	int							miChannelIndex;
};

///////////////////////////////////////////////////////////////////////////////

class AudioDeviceWII : public AudioDevice
{
public:

	AudioDeviceWII();

protected:

	static const int kmaxhwchannels = 32;

	std::string						mDriverName;
	FMOD::System*					mpFmodSystem;
	FMOD::Sound*					mpCurrentStream;
	int 							mpauseref;
	FMOD::ChannelGroup*				mPauseChannelGroup;

	AudDevPbFmod					mFmodPlaybacks[ kmaxhwchannels ];
	//FMOD::DSP*						mFilterHandles[ kmaxhwchannels ];

	/*virtual*/ AudioStreamPlayback	DoPlayStream( AudioStream* streamh );
	/*virtual*/ void				DoStopStream( AudioStreamPlayback pb );
	/*virtual*/ bool				DoLoadStream( AudioStream* pstream, ConstString filename );

	/*virtual*/ void				FillInSyncPoints( AudioStream* streamhandle, orkmap<ork::PoolString, float> &points );
	/*virtual*/ float				GetStreamLength( AudioStream* streamhandle );

	/*virtual*/ void				DoReInitDevice( void );
	/*virtual*/ void				DoPlaySound( AudioInstrumentPlayback &playbackhandle );
	/*virtual*/ void				DoStopSound( AudioInstrumentPlayback &playbackhandle );
	/*virtual*/ void				DoSetSoundSpatial( AudioInstrumentPlayback & PlaybackHandle, const CVector3& pos );
	/*virtual*/ void				DoInitSample( AudioSample & sample );
	/*virtual*/ int					DoGetFreeVoice( void ) { return 0; }
	/*virtual*/	void				SetPauseState(bool bpause);

	/*virtual*/ void				SetStreamVolume( AudioStreamPlayback streampb_handle, float fvolume );
	/*virtual*/ void				SetStreamTime( AudioStreamPlayback streampb_handle, float time );
	/*virtual*/ float				GetStreamTime( AudioStreamPlayback streampb_handle );
	/*virtual*/ float				GetStreamPlaybackLength( AudioStreamPlayback streampb_handle );

	/*virtual*/ void				Update( float fdt );

	/*virtual*/ void				SetStreamSubMix( AudioStreamPlayback pb, float fgrpa, float fgrpb, float fgrpc );

	/*virtual*/ void				SetReverbProperties( const AudioReverbProperties& reverb_props );

	virtual void					PushContext();
	virtual void					PopContext();

	virtual void					DestroyStream( AudioStream* pstream );
	virtual void					DestroyBank( AudioBank* pbank );

	virtual void ShutdownNow();

};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
