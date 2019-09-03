////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include "audiodevice_null.h"
#include <ork/file/file.h>
#include <ork/util/endian.h>
#include <ork/kernel/orklut.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/Array.h>
#include <ork/kernel/Array.hpp>
#include <ork/application/application.h>

namespace ork {

template class orklut<Char8,float>;

namespace lev2 {


///////////////////////////////////////////////////////////////////////////////

static const float flength = 1.0f;

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceNULL::DoReInitDevice( void )
{

}

///////////////////////////////////////////////////////////////////////////////

AudioDeviceNULL::AudioDeviceNULL()
	: AudioDevice()
	, mHandles()
{
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceNULL::Update( float fdt )
{
	const HandlePool::pointervect_type& used = mHandles.used();

	fixedvector<AudioStreamPlayback*,8> killed;

	for( HandlePool::pointervect_type::const_iterator it=used.begin(); it!=used.end(); it++ )
	{
		AudioStreamPlayback* pb = (*it);
		NullPlayHandle* phandle = (NullPlayHandle*) pb->mpPlatformHandle;
		phandle->Update(fdt);

		NullStreamData* psd = phandle->mpstreamdata;

		if( psd )
		{
			if( phandle->fstrtime > psd->mfstreamlen )
			{
				killed.push_back( pb );
			}
		}
	}

	for( fixedvector<AudioStreamPlayback*,8>::const_iterator it=killed.begin(); it!=killed.end(); it++ )
	{
		AudioStreamPlayback* handle = (*it);
		mHandles.deallocate(handle);
	}
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceNULL::DoInitSample( AudioSample & sample )
{
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceNULL::SetPauseState(bool bpause)
{
	return;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceNULL::DoPlaySound( AudioInstrumentPlayback * PlaybackHandle )
{
	return;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceNULL::DoStopSound( AudioInstrumentPlayback * PlaybackHandle )
{
}

///////////////////////////////////////////////////////////////////////////////

bool AudioDeviceNULL::DoLoadStream( AudioStream* streamhandle, ConstString fname )
{
	AssetPath filename = fname.c_str();

	bool b_uncompressed = (strstr(filename.c_str(),"streams")!=0);

	bool b_looped = (strstr(filename.c_str(),"Music")!=0) | (strstr(filename.c_str(),"stings")!=0);
	     b_looped |= (strstr(filename.c_str(),"music")!=0) | (strstr(filename.c_str(),"Stings")!=0);

	NullStreamData* psdata = new NullStreamData;

	float fmaxtime = 0.0f;

	streamhandle->SetPlatformHandle( psdata );

	if( false == b_uncompressed )
	{
		ork::EndianContext ec;
		ec.mendian = EENDIAN_LITTLE;

		filename.SetExtension( "mkr" );

		File ifile( filename, ork::EFM_READ );

		int icount = 0;
		ifile.Read( & icount, sizeof(icount) );
		swapbytes_dynamic( icount );

		psdata->stream_markers.reserve(icount);

		for( int i=0; i<icount; i++ )
		{
			float ftime = 0.0f;
			int istrlen = 0;

			ifile.Read( & ftime, sizeof(ftime) );
			ifile.Read( & istrlen, sizeof(istrlen) );
			swapbytes_dynamic( ftime );
			swapbytes_dynamic( istrlen );

			char* pstring = new char[ istrlen+1 ];
			memset( pstring, 0, istrlen+1 );

			ifile.Read( pstring, istrlen );

			orkprintf( "StreamMarker<%d> time<%f> name<%s>\n", i, ftime, pstring );



			//FMOD_SYNCPOINT *syncpoint = 0;

			unsigned int  offset = int(ftime*1000.0f);
			//FMOD_TIMEUNIT offsettype = FMOD_TIMEUNIT_MS;
			const char *name = pstring;
			//FMOD_SYNCPOINT **  point
			//fmod_result = phandle->addSyncPoint( offset, offsettype, name, & syncpoint );

			if( ftime > fmaxtime )
			{
				fmaxtime =ftime;
			}
			psdata->stream_markers.insert( std::pair<Char8,float>( name,ftime ) );
			//OrkAssert( fmod_result == FMOD_OK );
		}
		ifile.Close();
	}
	psdata->mfstreamlen = fmaxtime+3.0f;
	psdata->stream_markers.insert( std::pair<Char8,float>( "END",psdata->mfstreamlen ) );

	return true;
}

///////////////////////////////////////////////////////////////////////////////

AudioStreamPlayback* AudioDeviceNULL::DoPlayStream( AudioStream* streamhandle )
{
	AudioStreamPlayback* pb = mHandles.allocate();
	NullPlayHandle* phandle = (NullPlayHandle*) pb->mpPlatformHandle;

	phandle->Init();

	phandle->mpstreamdata = (NullStreamData*) streamhandle->GetPlatformHandle();

	return pb;
}

///////////////////////////////////////////////////////////////////////////////

float AudioDeviceNULL::GetStreamTime( AudioStreamPlayback* streampb_handle )
{
	NullPlayHandle* phandle = (NullPlayHandle*) streampb_handle;
	return phandle->fstrtime;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceNULL::SetStreamTime( AudioStreamPlayback* streampb_handle, float ftime )
{
	NullPlayHandle* phandle = (NullPlayHandle*) streampb_handle;
	if( phandle )
	{
		phandle->fstrtime = ftime;
	}
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceNULL::DoStopStream( AudioStreamPlayback* pb )
{
	NullPlayHandle* phandle = (NullPlayHandle*) pb->mpPlatformHandle;

	const HandlePool::pointervect_type& used = mHandles.used();
	fixedvector<AudioStreamPlayback*,8> killed;

	for( HandlePool::pointervect_type::const_iterator it=used.begin(); it!=used.end(); it++ )
	{
		AudioStreamPlayback* ptest = (*it);

		if( ptest==pb )
		{
			killed.push_back(ptest);
		}
	}

	for( fixedvector<AudioStreamPlayback*,8>::const_iterator it=killed.begin(); it!=killed.end(); it++ )
	{
		AudioStreamPlayback* pb = (*it);
		NullPlayHandle* phandle = (NullPlayHandle*) pb->mpPlatformHandle;

		phandle->mpstreamdata = (NullStreamData*) 0;
		mHandles.deallocate(pb);
	}

}

float AudioDeviceNULL::GetStreamPlaybackLength( AudioStreamPlayback* streampb_handle )
{
	MCheckPointContext( "AudioDeviceWII::GetStreamPlaybackLength" );
	if(streampb_handle)
	{
		NullPlayHandle* pnph = (NullPlayHandle*)streampb_handle;
		NullStreamData* psd = pnph->mpstreamdata;

		if( psd  )
		{
			return psd->mfstreamlen;
		}
	}
	return 5.0f;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceNULL::SetStreamVolume( AudioStreamPlayback* streampbh, float fvol )
{
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceNULL::FillInSyncPoints( AudioStream* streamhandle, orkmap<ork::PoolString, float> &points )
{
	NullStreamData* psd = (NullStreamData*) streamhandle->GetPlatformHandle();

	if( psd )
	{
		for( orklut<Char8,float>::const_iterator it = psd->stream_markers.begin(); it!=psd->stream_markers.end(); it++ )
		{
			float fv = it->second;
			Char8 ch8 = it->first;
			points.insert( std::make_pair(ork::AddPooledString(ch8.c_str()), fv) );
		}
	}
}
///////////////////////////////////////////////////////////////////////////////

}}
