////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/orkpool.h>

namespace ork::lev2 {

struct PaStreamData
{
	orklut<Char8,float>	stream_markers;
	float mfstreamlen;

	PaStreamData() : mfstreamlen(5.0f) {}
};

struct PaPlayHandle
{
	float fstrtime;;
	PaStreamData*	mpstreamdata;


	PaPlayHandle() : fstrtime(0.0f), mpstreamdata(0) {}

	void Update( float fdt ) { fstrtime+=fdt; }
	void Init() { fstrtime=0.0f; }
};


///////////////////////////////////////////////////////////////////////////////

class AudioDevicePa : public AudioDevice
{
public:

	AudioDevicePa();

protected:

	static const int kmaxhwchannels = 8;

	typedef fixed_pool<AudioStreamPlayback,kmaxhwchannels> HandlePool;

	HandlePool mHandles;

    void					            SetPauseState(bool bpause);
    float								GetStreamPlaybackLength( AudioStreamPlayback* streampb_handle );
    int							        DoGetFreeVoice( void ) { return 0; }
    void						        StopStream( AudioStreamPlayback StreamHandle );

	AudioStreamPlayback*				DoPlayStream( AudioStream* streamh ) final;
	void								DoStopStream( AudioStreamPlayback* pb ) final;
	bool								DoLoadStream( AudioStream* pstream, ConstString filename ) final;

	void						        DoReInitDevice( void ) final;
	void						        DoPlaySound( AudioInstrumentPlayback *playbackhandle ) final;
	void						        DoStopSound( AudioInstrumentPlayback *playbackhandle ) final;
	void						        DoInitSample( AudioSample & sample ) final;

	void								SetStreamVolume( AudioStreamPlayback* streampb_handle, float fvolume ) final;
	void								SetStreamTime( AudioStreamPlayback* streampb_handle, float time );
	float								GetStreamTime( AudioStreamPlayback* streampb_handle );

	void								FillInSyncPoints( AudioStream* streamhandle, orkmap<ork::PoolString, float> &points ) final;

	void						        Update( float fdt ) final;
	void						        SetReverbProperties( const AudioReverbProperties& reverb_props ) final {}


};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2 {
