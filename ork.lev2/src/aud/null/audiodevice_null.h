////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _HWPC_AUDIODEVICE_NULL_H
#define _HWPC_AUDIODEVICE_NULL_H

#include <ork/kernel/orkpool.h>

namespace ork { namespace lev2 {

struct NullStreamData
{
	orklut<Char8,float>	stream_markers;
	float mfstreamlen;

	NullStreamData() : mfstreamlen(5.0f) {}
};

struct NullPlayHandle
{
	float fstrtime;;
	NullStreamData*	mpstreamdata;


	NullPlayHandle() : fstrtime(0.0f), mpstreamdata(0) {}

	void Update( float fdt ) { fstrtime+=fdt; }
	void Init() { fstrtime=0.0f; }
};


///////////////////////////////////////////////////////////////////////////////

class AudioDeviceNULL : public AudioDevice
{
public:

	AudioDeviceNULL();

protected:

	static const int kmaxhwchannels = 8;

	typedef fixed_pool<AudioStreamPlayback,kmaxhwchannels> HandlePool;

	HandlePool mHandles;

	AudioStreamPlayback*				DoPlayStream( AudioStream* streamh ); // virtual
	void								DoStopStream( AudioStreamPlayback* pb ); // virtual
	bool								DoLoadStream( AudioStream* pstream, ConstString filename ); // virtual

	virtual void						DoReInitDevice( void );
	virtual void						DoPlaySound( AudioInstrumentPlayback *playbackhandle ); // virtual
	virtual void						DoStopSound( AudioInstrumentPlayback *playbackhandle ); // virtual
	virtual void						DoInitSample( AudioSample & sample ); // virtual
	virtual int							DoGetFreeVoice( void ) { return 0; } // virtual
	/*virtual*/	void					SetPauseState(bool bpause);

	void								SetStreamVolume( AudioStreamPlayback* streampb_handle, float fvolume ); // virtual
	void								SetStreamTime( AudioStreamPlayback* streampb_handle, float time );
	float								GetStreamTime( AudioStreamPlayback* streampb_handle );

	float								GetStreamPlaybackLength( AudioStreamPlayback* streampb_handle );

	void								FillInSyncPoints( AudioStream* streamhandle, orkmap<ork::PoolString, float> &points ); // virtual

	virtual void						Update( float fdt );

	//virtual void						StopStream( AudioStreamPlayback StreamHandle );
	
	virtual void						SetReverbProperties( const AudioReverbProperties& reverb_props ) {}

};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
