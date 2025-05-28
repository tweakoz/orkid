////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/config.h>
#if defined(ENABLE_CORE_AUDIO)

#include "CoreAudioBuffer.h"
#include "CoreAudioBuffer.hpp"
#include <libkern/OSAtomic.h>
#include <stdio.h>
#include <math.h>
//#include <OpenGL/gl.h>
//#include <OpenGL/glu.h>
#include <ork/kernel/fixedstring.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::ca {
///////////////////////////////////////////////////////////////////////////////
Fragment::Fragment()
	: mNumFrames(0)
	, mFrameAccum(0.0)
	, mTrack(nullptr)
{
	for( int i=0; i<kMAXFRAMESIZE; i++ )
		mSampleData[i] = 0.0f;
}
///////////////////////////////////////////////////////////////////////////////
float Fragment::ComputePower() const
{
	float fmin = 10000000.0f;
	float fmax = -10000000.0f;
	
	for( int ifr=0; ifr<mNumFrames; ifr++ )
	{
		float fv = mSampleData[ifr];		
		if( fv > fmax ) fmax = fv;
		if( fv < fmin ) fmin = fv;
	}
	float fabsmin = fabs(fmin);
	float fabsmax = fabs(fmax);
	float fPWR = (fabsmax>fabsmin) ? fabsmax : fabsmin;
	return fPWR;
}
///////////////////////////////////////////////////////////////////////////////
void Fragment::Clear()
{
	for(int i=0; i<kMAXFRAMESIZE; i++)
		mSampleData[i]=0.0f;
}
///////////////////////////////////////////////////////////////////////////////
void Fragment::Sum( const Fragment& from, float level )
{
	if( mNumFrames!=from.mNumFrames)
	{
		printf( "mNumFrames<%d> frm.mNumFrames<%d>\n", mNumFrames, from.mNumFrames );
	}
	assert(mNumFrames == from.mNumFrames);
	
	const auto& inpdata = from.mSampleData;

	for( int i=0; i<from.mNumFrames; i++ )
	{
		mSampleData[i] += inpdata[i]*level;
	}
}
///////////////////////////////////////////////////////////////////////////////
StereoFragment::StereoFragment() 
	: mNumUsed(0)
	, mNumFrames(0)
{
	mMixLeft.mChannel=0;
	mMixRight.mChannel=1;
}
///////////////////////////////////////////////////////////////////////////////
void StereoFragment::Init(int numfr)
{
	Clear();

	mMixLeft.mNumFrames = numfr;
	mMixRight.mNumFrames = numfr;
	mNumUsed = 0;
	mNumFrames = numfr;
}
void StereoFragment::Clear()
{
	mMixLeft.Clear();
	mMixRight.Clear();
}
///////////////////////////////////////////////////////////////////////////////
template struct ObjectPool<Fragment>;
template struct ObjectPool<StereoFragment>;
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ca {
///////////////////////////////////////////////////////////////////////////////
#endif // #if defined(ENABLE_CORE_AUDIO)
