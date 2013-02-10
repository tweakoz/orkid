////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _HWPC_AUDIODEVICE_FMOD_H
#define _HWPC_AUDIODEVICE_FMOD_H

#include <fmod/fmod.hpp>
#include <fmod/fmod_errors.h>
#include <fmod/fmod_event.hpp>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

struct AudDevPbFmod
{
	AudioInstrumentPlayback*	mPlayback;
	FMOD::Channel*				mpChannel;
	FMOD::DSP*					mpFilter;
	int							miChannelIndex;

	static const int kmaxattenpoints = 16;

	FMOD_VECTOR					mAttenShape[kmaxattenpoints];
	int							miNumAttenPoints;
};

///////////////////////////////////////////////////////////////////////////////

class AudioDeviceFMOD : public AudioDevice
{
public:

	AudioDeviceFMOD();

protected:

	static const int kmaxhwchannels = 16;

	std::string						mDriverName;
	FMOD::System*					mpFmodSystem;
	FMOD::Sound*					mpCurrentStream;
	int 							mpauseref;

	AudDevPbFmod					mFmodPlaybacks[ kmaxhwchannels ];
	FMOD::DSP*						mFilterHandles[ kmaxhwchannels ];

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

	virtual void					PushContext();
	virtual void					PopContext();

	virtual void					DestroyStream( AudioStream* pstream );
	virtual void					DestroyBank( AudioBank* pbank );


};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
