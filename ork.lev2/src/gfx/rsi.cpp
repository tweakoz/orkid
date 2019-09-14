////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rendercontext.h>
namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

void GfxTarget::PushModColor( const fvec4 &mColor )
{
	OrkAssert( miModColorStackIndex<(kiModColorStackMax-1) );
	maModColorStack[ ++miModColorStackIndex ] = mColor;
	RefModColor() = mColor;
}

///////////////////////////////////////////////////////////////////////////////

fvec4& GfxTarget::PopModColor( void )
{
	OrkAssert( miModColorStackIndex>0 );
	RefModColor() = maModColorStack[ --miModColorStackIndex ];
	return RefModColor();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

RasterStateInterface::RasterStateInterface()
{
}

///////////////////////////////////////////////////////////////////////////////
/*
void RasterStateInterface::ClearOverrides()
{
	mOverrideZWriteMask = false;
	mOverrideAWriteMask = false;
	mOverrideRGBWriteMask = false;
	mOverrideAlphaTest = false;
	mOverrideAlphaRef = false;
	mOverrideBlending = false;
	mOverrideDepthTest = false;
	mOverrideScissorTest = false;
	mOverrideShadeModel = false;
	mOverrideCullTest = false;
	mOverrideStencilMode = false;
	mOverrideStencilRef = false;
	mOverrideStencilMask = false;
	mOverrideStencilOpPass = false;
	mOverrideStencilOpFail = false;
	mOverridePolyOffset = false;
	mOverrideSortID = false;
	mOverrideTransparent = false;
	mOverridePointSize = false;
}

///////////////////////////////////////////////////////////////////////////////

void RasterStateInterface::GetOverrideMergedRasterState(const SRasterState &in, SRasterState &out)
{
	std::memcpy(&out, &in, sizeof(SRasterState));

	if(mOverrideZWriteMask)
		out.muZWriteMask = mOverrideState.muZWriteMask;
	if(mOverrideAWriteMask)
		out.muAWriteMask = mOverrideState.muAWriteMask;
	if(mOverrideRGBWriteMask)
		out.muRGBWriteMask = mOverrideState.muRGBWriteMask;
	if(mOverrideAlphaTest)
		out.muAlphaTest = mOverrideState.muAlphaTest;
	if(mOverrideAlphaRef)
		out.muAlphaRef = mOverrideState.muAlphaRef;
	if(mOverrideBlending)
		out.muBlending = mOverrideState.muBlending;
	if(mOverrideDepthTest)
		out.muDepthTest = mOverrideState.muDepthTest;
	if(mOverrideScissorTest)
		out.muScissorTest = mOverrideState.muScissorTest;
	if(mOverrideShadeModel)
		out.muShadeModel = mOverrideState.muShadeModel;
	if(mOverrideCullTest)
		out.muCullTest = mOverrideState.muCullTest;
	if(mOverrideStencilMode)
		out.muStencilMode = mOverrideState.muStencilMode;
	if(mOverrideStencilRef)
		out.muStencilRef = mOverrideState.muStencilRef;
	if(mOverrideStencilMask)
		out.muStencilMask = mOverrideState.muStencilMask;
	if(mOverrideStencilOpPass)
		out.muStencilOpPass = mOverrideState.muStencilOpPass;
	if(mOverrideStencilOpFail)
		out.muStencilOpFail = mOverrideState.muStencilOpFail;
	if(mOverridePolyOffset)
		out.muPolyOffset = mOverrideState.muPolyOffset;
	if(mOverrideSortID)
		out.muSortID = mOverrideState.muSortID;
	if(mOverrideTransparent)
		out.muTransparent = mOverrideState.muTransparent;
	if(mOverridePointSize)
		out.mPointSize = mOverrideState.mPointSize;
}
*/
///////////////////////////////////////////////////////////////////////////////

} }
