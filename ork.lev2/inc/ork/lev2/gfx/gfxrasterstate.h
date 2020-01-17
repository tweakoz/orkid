////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

class Texture;

struct	SRasterState
{
	// Render States

	unsigned	muZWriteMask		:1;		// 1
	unsigned	muAWriteMask		:1;		// 2
	unsigned	muRGBWriteMask		:1;		// 3

	unsigned	muAlphaTest			:2;		// 5
	unsigned	muAlphaRef			:4;		// 9

	unsigned	muBlending			:3;		// 12
	unsigned	muDepthTest			:3;		// 15
	unsigned	muScissorTest		:1;		// 16
	unsigned	muShadeModel		:1;		// 17
	unsigned	muCullTest			:2;		// 19
	unsigned	muStencilMode		:4;		// 23
	unsigned	muStencilRef		:8;		// 27
	unsigned	muStencilMask		:3;		// 30
	unsigned	muStencilOpPass		:3;
	unsigned	muStencilOpFail		:3;

	// Sort (Highest Priority)

	unsigned	muSortID			:11;	// 63		512 Hard Sort Bins (Use Object Depth and Pass Num for This)
	unsigned	muTransparent		:1;		// 64		All Transparent Objects Should have Highest mu_SortID'S ( so they get drawn last )

	float			mPointSize;

	/////////////////////////////
	// <> Ops For Sorting

	inline U64 AsU64( int iIndex = 0 ) const { return U64(((U64 *)this)[iIndex]); }
	inline U32 AsU32LO( void ) const { return U32(*((U32 *)this)); }
	inline U32 AsU32HI( void ) const { return U32(*((U32 *)this+1)); }

	/////////////////////////////
	// Accessors

	void				SetRGBAWriteMask( bool rgb, bool a ) { muRGBWriteMask = (unsigned) rgb; muAWriteMask = (unsigned) a; }
	void				SetZWriteMask( bool bv ) { muZWriteMask = (unsigned) bv; }

	bool				GetZWriteMask( void ) const { return (bool) muZWriteMask; }
	bool				GetAWriteMask( void ) const { return (bool) muAWriteMask; }
	bool				GetRGBWriteMask( void ) const { return (bool) muRGBWriteMask; }

	/////////////////////////////

	void				SetScissorTest( EScissorTest eVal ) { muScissorTest = eVal; }
	void				SetAlphaTest( EAlphaTest eVal, f32 falpharef=0.0f ) { muAlphaTest = eVal; muAlphaRef=(unsigned)(falpharef*16.0f); }
	void				SetBlending( EBlending eVal ) { muBlending = eVal; }
	void				SetDepthTest( EDepthTest eVal ) { muDepthTest = eVal; }
	void				SetShadeModel( EShadeModel eVal ) { muShadeModel = eVal; }
	void				SetCullTest( ECullTest eVal ) { muCullTest = eVal; }
	void				SetStencilMode( EStencilMode eVal, EStencilOp ePassOp, EStencilOp eFailOp, u8 uRef, u8 uMsk )
								{ 
									muStencilOpPass=(unsigned)ePassOp; 
									muStencilOpFail=(unsigned)eFailOp; 
									muStencilMode=(unsigned)eVal; 
									muStencilRef=uRef; 
									muStencilMask=uMsk;
								}

	EScissorTest		GetScissorTest( void ) const { return EScissorTest(muScissorTest); }
	EAlphaTest			GetAlphaTest( void ) const { return EAlphaTest(muAlphaTest); }
	EBlending			GetBlending( void ) const { return EBlending(muBlending); }
	EDepthTest			GetDepthTest( void ) const { return EDepthTest(muDepthTest); }
	EShadeModel			GetShadeModel( void ) const { return EShadeModel(muShadeModel); }
	ECullTest			GetCullTest( void ) const { return ECullTest(muCullTest); }
	void				GetStencilMode( EStencilMode& eVal, EStencilOp &ePassOp, EStencilOp& eFailOp, u8& uRef, u8& uMsk ) const
								{ 
									ePassOp=(EStencilOp)muStencilOpPass;
									eFailOp=(EStencilOp)muStencilOpFail;
									eVal=(EStencilMode)muStencilMode;
									uRef=muStencilRef;
									uMsk=muStencilMask;
								}

	/////////////////////////////

	void				SetSortID( unsigned int uVal ) { muSortID = uVal; }
	void				SetTransparent( bool bVal ) { muTransparent = bVal; }

	unsigned int		GetSortID( void ) const { return (unsigned int)(muSortID); }
	bool				GetTransparent( void ) const { return bool(muTransparent); }

	void				SetPointSize( float i ) { mPointSize=i; }
	float				GetPointSize() const  { return mPointSize; }

	/////////////////////////////////////////////////////////////////////////

	SRasterState();

};

} }

