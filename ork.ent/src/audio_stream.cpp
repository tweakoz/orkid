////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/AudioComponent.h>
#include <ork/lev2/aud/audiobank.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>
#include <ork/gfx/camera.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/core_interface.h>
#include <pkg/ent/event/StartAudioEffectEvent.h>
#include <ork/reflect/enum_serializer.h>
#include <ork/application/application.h>

extern void SetDebugLevel(bool on);
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioStreamComponentData, "AudioStreamComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioStreamComponentInst, "AudioStreamComponentInst");
///////////////////////////////////////////////////////////////////////////////
template class ork::orklut<ork::PoolString,ork::lev2::AudioStream*>;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
bool AudioStreamComponentInst::DoStart(ork::ent::SceneInst *psi, const ork::CMatrix4 &world)
{
	return true;
}
///////////////////////////////////////////////////////////////////////////////
void AudioStreamComponentInst::DoStop(ork::ent::SceneInst *psi)
{
	for( ork::orklut<ork::PoolString,AudioStreamInstItem>::iterator it = mItems.begin(); it!=mItems.end(); it++ )
	{
		ork::lev2::AudioStreamPlayback* playback = it->second.mPlayback;
		ork::lev2::AudioDevice::GetDevice()->StopStream( playback );
	}
	mItems.clear();
}
///////////////////////////////////////////////////////////////////////////////
void AudioStreamComponentData::Describe()
{
	ork::reflect::RegisterMapProperty( "StreamMap", & AudioStreamComponentData::mStreamMap );
	ork::reflect::AnnotatePropertyForEditor<AudioStreamComponentData>("StreamMap", "editor.assettype", "lev2::audiostream");
	ork::reflect::AnnotatePropertyForEditor<AudioStreamComponentData>("StreamMap", "editor.assetclass", "lev2::audiostream");
}
///////////////////////////////////////////////////////////////////////////////
AudioStreamComponentData::AudioStreamComponentData()
{
}
///////////////////////////////////////////////////////////////////////////////
ork::ent::ComponentInst *AudioStreamComponentData::createComponent(ork::ent::Entity *pent) const
{
	return OrkNew AudioStreamComponentInst( *this, pent );
}
///////////////////////////////////////////////////////////////////////////////
void AudioStreamComponentInst::Describe()
{
	ork::reflect::RegisterFunctor("SetCrossfade", &AudioStreamComponentInst::SetCrossfade);
	ork::reflect::RegisterFunctor("SetVolume", &AudioStreamComponentInst::SetVolume);
	ork::reflect::RegisterFunctor("Play", &AudioStreamComponentInst::Play);
	ork::reflect::RegisterFunctor("Stop", &AudioStreamComponentInst::Stop);
	ork::reflect::RegisterFunctor("SetDebugLevel", &AudioStreamComponentInst::SetDebugLevel);

}

void AudioStreamComponentInst::SetDebugLevel(bool on )
{
#if defined(WII)
	::SetDebugLevel(on);
#endif
}
///////////////////////////////////////////////////////////////////////////////
void AudioStreamComponentInst::Play( ork::PoolString streamname )
{
	const AudioStreamComponentData::StreamMap& smap = mData.GetStreamMap();
	AudioStreamComponentData::StreamMap::const_iterator it = smap.find( streamname );
	if( it != smap.end() )
	{
		if( mItems.find( streamname ) == mItems.end() )
		{
			ork::lev2::AudioStream* shandle = it->second;
			if( shandle )
			{
				ork::lev2::AudioStreamPlayback* stream_playback = ork::lev2::AudioDevice::GetDevice()->PlayStream( shandle );

				AudioStreamInstItem item;
				item.mPlayback = stream_playback;
				item.mStream = shandle;

				mItems.AddSorted( streamname, item );
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
void AudioStreamComponentInst::Stop( ork::PoolString streamname )
{
	ork::orklut<ork::PoolString,AudioStreamInstItem>::iterator it = mItems.find( streamname );
	if( it != mItems.end() )
	{	ork::lev2::AudioStreamPlayback* playback = it->second.mPlayback;
		ork::lev2::AudioDevice::GetDevice()->StopStream( playback );
		mItems.erase( it );
	}
}
///////////////////////////////////////////////////////////////////////////////
AudioStreamComponentInst::AudioStreamComponentInst( const AudioStreamComponentData& data, ork::ent::Entity* pent )
	: ComponentInst( & data, pent )
	, mData( data )
{

}

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

void AudioStreamComponentInst::DoUpdate(ork::ent::SceneInst *inst)
{
	for( ork::orklut<ork::PoolString, AudioStreamInstItem>::iterator it=mItems.begin(); it!=mItems.end(); it++ )
	{
		/////////////////////////////////////////////////////////////////////////////
		AudioStreamInstItem& item = it->second;
		item.Update(inst->GetDeltaTime());
		/////////////////////////////////////////////////////////////////////////////

		if( item.mStream )
		{
			float fvol = item.mParamVolume.mfCurrent;

			switch( item.mStream->GetNumChannels() )
			{
				case 6:
					switch( item.meCrossFadeType )
					{
						case EXFTYP_6CH_LINEAR_BC: // 6 channel, a is constant, linear xfade between b and c
						{
							if( item.mStream->GetNumChannels() == 6 )
							{
								float ferp = item.mParamCrossfade.mfCurrent;

								const float kfstreamscale = 0.607f*fvol;

								/////////////////////////////////////////////////
								// countour ferp value to bias it near the endpoints
								/////////////////////////////////////////////////
								const float fcountour_slope = 6.0f;
								const float fcountour_begin = 0.2f;
								const float fcountour_end = 0.8f;
								float ferpin = 0.0f;

								if( ferp < fcountour_begin ) ferpin = 0.0f;
								else if( ferp > fcountour_end ) ferpin = 1.0f;
								else ferpin = (ferp-fcountour_begin)/(fcountour_end-fcountour_begin);

								float ferp1 = ::powf( ferp, 1.0f/fcountour_slope );
								float ferp2 = ::powf( ferp, fcountour_slope );
								ferp = (ferp1*ferpin)+(ferp2*(1.0f-ferpin));
								/////////////////////////////////////////////////
								// countour ferp value to bias it near
								/////////////////////////////////////////////////

								/////////////////////////////////////////////////
								// generate 2 level values using pseudo RMS modified interp
								float fleva = ::powf(ferp,0.9f); // pseudo RMS
								float flevb = ::powf(1.0f-ferp,0.9f); // pseudo RMS
								/////////////////////////////////////////////////
								//printf( "streamlerp <%s> xfade<%f> ferp<%f>, ferp1<%f>, ferp2<%f>, leva<%f> levb<%f>\n", it->first.c_str(), item.mfCurrentCrossFade, ferp, ferp1, ferp2, fleva, flevb );
								ork::lev2::AudioDevice::GetDevice()->SetStreamSubMix( item.mPlayback, 1.0f*kfstreamscale, fleva*kfstreamscale, flevb*kfstreamscale );
								break;
							}
						}
					}
					break;
				default:
					ork::lev2::AudioDevice::GetDevice()->SetStreamVolume( item.mPlayback, fvol );
					break;
			}

		}
		/////////////////////////////////////////////////////////////////////////////
	}

}

///////////////////////////////////////////////////////////////////////////////

void AudioStreamComponentInst::SetVolume( ork::PoolString streamname, float fxfade, float fxtransitiontime )
{
	ork::orklut<ork::PoolString,AudioStreamInstItem>::iterator it = mItems.find( streamname );
	if( it != mItems.end() )
	{
		it->second.VolumeTransitionTo( fxfade, fxtransitiontime );
	}
}

///////////////////////////////////////////////////////////////////////////////

void AudioStreamComponentInst::SetCrossfade( ork::PoolString streamname, float fxfade, float fxtransitiontime )
{
	ork::orklut<ork::PoolString,AudioStreamInstItem>::iterator it = mItems.find( streamname );
	if( it != mItems.end() )
	{
		it->second.CrossfadeTransitionTo( fxfade, fxtransitiontime );
	}
}


///////////////////////////////////////////////////////////////////////////////

AudioStreamLerpableParam::AudioStreamLerpableParam()
	: mfCurrent( 0.0f )
	, mfPrevious( 0.0f )
	, mfTarget( 0.0f )
	, mfTransitionTime( 0.0f )
	, mbQuickGate( false )
{
}

AudioStreamInstItem::AudioStreamInstItem()
	: mPlayback( 0 )
	, mStream( 0 )
	, meCrossFadeType( EXFTYP_6CH_LINEAR_BC )
{

	mParamCrossfade.mbQuickGate = true;
	mParamVolume.mfCurrent = 1.0f;
	mParamVolume.mfTarget = 1.0f;
}


void AudioStreamLerpableParam::TransitionTo( float flevel, float fseconds )
{
	mfTransitionTime = fseconds;
	mfTarget = flevel;
	mfPrevious = mfCurrent;
}

///////////////////////////////////////////////////////////////////////////////

void AudioStreamInstItem::CrossfadeTransitionTo( float flevel, float fseconds )
{
	mParamCrossfade.TransitionTo( flevel, fseconds );
}

///////////////////////////////////////////////////////////////////////////////

void AudioStreamInstItem::VolumeTransitionTo( float flevel, float fseconds )
{
	mParamVolume.TransitionTo( flevel, fseconds );
}

///////////////////////////////////////////////////////////////////////////////

void AudioStreamLerpableParam::Update( float fdeltatime )
{
	float fdelta = (mfTarget-mfCurrent);
	int isdelta = (fdelta==0.0f) ? 0 : (fdelta<0.0f) ? -1 : 1;
	if( std::fabs(fdelta) > 0.0f )
	{	if( mfTransitionTime == 0.0f )
		{	mfCurrent = mfTarget;
		}
		else
		{	float frate = (mfTarget-mfPrevious) / mfTransitionTime;
			mfCurrent += (frate*fdeltatime);
			float fnewdelta = (mfTarget-mfCurrent);
			int insdelta = (fnewdelta==0.0f) ? 0 : (fnewdelta<0.0f) ? -1 : 1;
			if( isdelta != insdelta ) // did we overshoot ?
			{	mfCurrent = mfTarget;
				mfPrevious = mfCurrent;
			}
		}
	}

}

///////////////////////////////////////////////////////////////////////////////

void AudioStreamInstItem::Update( float fdeltatime )
{
	mParamCrossfade.Update( fdeltatime );
	mParamVolume.Update( fdeltatime );
}


float getsubrange( float fin, float flow, float fhi )
{
	if( fin > fhi ) return 0.0f;
	if( fin < flow ) return 0.0f;

	float fmid = (flow+fhi)*0.5f;
	float frng = fhi-flow;
	float fit = (fin-flow);

	float fret = 0.0f;

	float funitized = fit/frng;

	if( funitized < 0.5f ) // climbing
	{
		fret = funitized*2.0f;
	}
	else // funitized>0.5f
	{
		funitized -= 0.5f;
		fret = 1.0f - funitized*2.0f;
	}

	return fret;
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
