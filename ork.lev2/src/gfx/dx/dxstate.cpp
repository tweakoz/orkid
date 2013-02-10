////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#if defined( ORK_CONFIG_DIRECT3D )
#include "dx.h"

namespace ork { namespace lev2 {

extern ECullTest GlobalCullTest;

//////////////////////////////////////////////////////////////////////////////

D3DGfxDevice DxRasterStateInterface::GetD3DDevice( void )
{
	return mTargetDX.GetD3DDevice();
}

//////////////////////////////////////////////////////////////////////////////

DxRasterStateInterface::DxRasterStateInterface(GfxTargetDX& target )
	: mTargetDX(target)
	, mLast_SrcBlend(D3DBLEND_ONE)
	, mLast_DstBlend(D3DBLEND_ONE)
{
}

//////////////////////////////////////////////////////////////////////////////

void DxRasterStateInterface::BindRasterState( SRasterState const &rNewState, bool bForce )
{
#if defined(_XBOX)
	bForce = true;
#else
	bForce = true;

	static int iinvyes = 0;
	static int iinvno = 0;

	if( this->mTargetDX.FXI()->GetLastFxMaterial() == 0 )
	{
		bForce = true;
		iinvyes++;
	}
	else
	{
		iinvno++;
	}
#endif

	SRasterState overridden;// = rNewState;
	GetOverrideMergedRasterState(rNewState, overridden);

	float fLinew = 1.0f;

	HRESULT hr;

	const SRasterState &rLast = mLastState;
	
	bool bAlphaTestChanged =
	(		overridden.GetAlphaTest()	!=	rLast.GetAlphaTest()		
		||	overridden.muAlphaRef != rLast.muAlphaRef	
	);
	bool bCullTestChanged =			(	overridden.GetCullTest()		!=	rLast.GetCullTest()			);
	bool bScissorTestChanged =		(	overridden.GetScissorTest()	!=	rLast.GetScissorTest()		);

	if( overridden.GetCullTest() != mLastState.GetCullTest() || bForce )
	{	switch( overridden.GetCullTest() )
		{	case ECULLTEST_OFF:
				GetD3DDevice()->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
				break;
			case ECULLTEST_PASS_FRONT:
				GetD3DDevice()->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );
				break;
			case ECULLTEST_PASS_BACK:
				GetD3DDevice()->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
				break;
		}
	}

	bool bZWRITEChenged = (	overridden.GetZWriteMask()	!=	rLast.GetZWriteMask() );

	//hr = GetD3DDevice()->SetRenderState( D3DRS_SPECULARENABLE , TRUE );
	if( bZWRITEChenged || bForce )
		 hr = GetD3DDevice()->SetRenderState( D3DRS_ZWRITEENABLE, overridden.GetZWriteMask() );

	//DWORD rgbawritemask = 0;
	//bool bAWRITEChenged = (	overridden.GetAWriteMask()	!=	rLast.GetAWriteMask() );
	//if( rgbawritemask |= overridden.GetAWriteMask() ? D3DCOLORWRITEENABLE_ALPHA : 0;
	//rgbawritemask |= overridden.GetRGBWriteMask() ? D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE : 0;
	//hr = GetD3DDevice()->SetRenderState( D3DRS_COLORWRITEENABLE, rgbawritemask );

	bool bSCISSORChanged = (overridden.GetScissorTest() != rLast.GetScissorTest());
	if(bSCISSORChanged || bForce)
		hr = GetD3DDevice()->SetRenderState(D3DRS_SCISSORTESTENABLE, overridden.GetScissorTest() == ESCISSORTEST_ON);

	/////////////////////////////////////////////////
	// Stencil

	EStencilMode emod;
	EStencilOp eoppass;
	EStencilOp eopfail;
	u8 uref;
	u8 umsk;

	EStencilMode lemod;
	EStencilOp leoppass;
	EStencilOp leopfail;
	u8 luref;
	u8 lumsk;
	
	overridden.GetStencilMode( emod, eoppass, eopfail, uref, umsk );
	rLast.GetStencilMode( lemod, leoppass, leopfail, luref, lumsk );
	
	if( uref!=luref || bForce )
	{	hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILREF, uref );
		hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILMASK, 0xffffffff );
	}

	if( eoppass != leoppass || bForce)
	switch( eoppass )
	{	case ESTENCILOP_ZERO:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_ZERO );
			break;
		case ESTENCILOP_KEEP:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
			break;
		case ESTENCILOP_REPLACE:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE );
			break;
		case ESTENCILOP_INCR:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCR );
			break;
		case ESTENCILOP_INCRSAT:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT );
			break;
		case ESTENCILOP_DECR:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_DECR );
			break;
		case ESTENCILOP_DECRSAT:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_DECRSAT );
			break;
		case ESTENCILOP_INVERT:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INVERT );
			break;
		default:
			break;
	}

	if( eopfail != leopfail || bForce)
	switch( eopfail )
	{	case ESTENCILOP_ZERO:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_ZERO );
			break;
		case ESTENCILOP_KEEP:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			break;
		case ESTENCILOP_REPLACE:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_REPLACE );
			break;
		case ESTENCILOP_INCR:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_INCR );
			break;
		case ESTENCILOP_INCRSAT:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_INCRSAT );
			break;
		case ESTENCILOP_DECR:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_DECR );
			break;
		case ESTENCILOP_DECRSAT:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_DECRSAT );
			break;
		case ESTENCILOP_INVERT:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_INVERT );
			break;
		default:
			break;
	}

	if( emod != lemod || bForce)
	switch( emod )
	{
		case ESTENCILTEST_NEVER:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_NEVER );
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILENABLE, true );
			break;
		case ESTENCILTEST_ALWAYS:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILENABLE, true );
			break;
		case ESTENCILTEST_EQUALS:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILENABLE, true );
			break;
		case ESTENCILTEST_LEQUALS:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILENABLE, true );
			break;
		case ESTENCILTEST_GEQUALS:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_GREATEREQUAL );
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILENABLE, true );
			break;
		case ESTENCILTEST_OFF:
		default:
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
			hr = GetD3DDevice()->SetRenderState( D3DRS_STENCILENABLE, false );
			break;
	}

	/////////////////////////////////////////////////
	//	Win32 GL Alpha
	
	bool batena = false;
	D3DCMPFUNC func = D3DCMP_GREATER;
	unsigned int aref = (overridden.muAlphaRef << 4);
	switch( overridden.muAlphaTest )
	{	case EALPHATEST_OFF:
			batena = false;
			break;
		case EALPHATEST_GREATER:
		{	
			batena = true;
			break;
		}
		case EALPHATEST_LESS:
			batena = true;
			break;
	}
	if( batena != mLast_AlphaTestEnable || bForce )
	{
		hr = GetD3DDevice()->SetRenderState( D3DRS_ALPHATESTENABLE, batena );
	}
	if( func != mLast_AFUNC || bForce )
	{
		hr = GetD3DDevice()->SetRenderState( D3DRS_ALPHAFUNC, func );
	}
	if( aref != mLast_ARef || bForce )
	{
		hr = GetD3DDevice()->SetRenderState( D3DRS_ALPHAREF, aref );
	}
	mLast_AlphaTestEnable = batena;
	mLast_AFUNC = func;
	mLast_ARef = aref;
		
	/////////////////////////////////////////////////
	//	Depth

	D3DZBUFFERTYPE ZENABLE = mLast_ZENABLE;
	D3DCMPFUNC ZFUNC = mLast_ZFUNC;
	switch( overridden.GetDepthTest() )
	{	//Depth-buffering state as one member of the D3DZBUFFERTYPE enumerated type. Set this state to D3DZB_TRUE to enable z-buffering, D3DZB_USEW to enable w-buffering, or D3DZB_FALSE to disable depth buffering.
		//The default value for this render state is D3DZB_TRUE if a depth stencil was created along with the swap chain by setting the EnableAutoDepthStencil member of the D3DPRESENT_PARAMETERS structure to TRUE, and D3DZB_FALSE otherwise.
		case EDEPTHTEST_OFF:
			ZENABLE = D3DZB_FALSE;
			ZFUNC = D3DCMP_ALWAYS;
			break;
		case EDEPTHTEST_LESS:
			ZFUNC = D3DCMP_LESS;
			ZENABLE = D3DZB_TRUE;
			break;
		case EDEPTHTEST_LEQUALS:
			ZFUNC = D3DCMP_LESSEQUAL;
			ZENABLE = D3DZB_TRUE;
			break;
		case EDEPTHTEST_GREATER:
			ZFUNC = D3DCMP_GREATER;
			ZENABLE = D3DZB_TRUE;
			break;
		case EDEPTHTEST_GEQUALS:
			ZFUNC = D3DCMP_GREATEREQUAL;
			ZENABLE = D3DZB_TRUE;
			break;
		case EDEPTHTEST_EQUALS:
			ZFUNC = D3DCMP_EQUAL;
			ZENABLE = D3DZB_TRUE;
			break;
		case EDEPTHTEST_ALWAYS:
			ZFUNC = D3DCMP_ALWAYS;
			ZENABLE = D3DZB_TRUE;
			break;
		default:
			OrkAssert( false );
			break;
	}
	if( ZENABLE != mLast_ZENABLE || bForce )
	{
		hr = GetD3DDevice()->SetRenderState( D3DRS_ZENABLE, ZENABLE );
		mLast_ZENABLE = ZENABLE;
	}
	if( ZFUNC != mLast_ZFUNC || bForce )
	{
		hr = GetD3DDevice()->SetRenderState( D3DRS_ZFUNC, ZFUNC );
		mLast_ZFUNC = ZFUNC;
	}

	int ioffset = int(overridden.GetPolyOffset());
	float foffset = float(ioffset)*-0.001f;
	hr = GetD3DDevice()->SetRenderState( D3DRS_DEPTHBIAS, FtoDW(foffset) );

	/////////////////////////////////////////////////

	if( bScissorTestChanged || bForce )
	{
	}
	
	/////////////////////////////////////////////////

	bool			BlendEnable = true;
	D3DBLEND		SrcBlend = mLast_SrcBlend;
	D3DBLEND		DstBlend = mLast_DstBlend;
	switch( overridden.GetBlending() )
	{	case EBLENDING_OFF:					BlendEnable = false;				break;
		case EBLENDING_DSTALPHA:			SrcBlend = D3DBLEND_DESTALPHA;
											DstBlend = D3DBLEND_INVDESTALPHA;	break;
		case EBLENDING_PREMA:				SrcBlend = D3DBLEND_ONE;
											DstBlend = D3DBLEND_INVSRCALPHA;	break;
		case EBLENDING_ALPHA:				SrcBlend = D3DBLEND_SRCALPHA;
											DstBlend = D3DBLEND_INVSRCALPHA;	break;
		case EBLENDING_ADDITIVE:			SrcBlend = D3DBLEND_ONE;
											DstBlend = D3DBLEND_ONE;			break;
		case EBLENDING_SUBTRACTIVE:			SrcBlend = D3DBLEND_ZERO;
											DstBlend = D3DBLEND_INVSRCCOLOR;	break;
		case EBLENDING_ALPHA_ADDITIVE:		SrcBlend = D3DBLEND_SRCALPHA;
											DstBlend = D3DBLEND_ONE;			break;
		case EBLENDING_ALPHA_SUBTRACTIVE:	SrcBlend = D3DBLEND_ZERO;
											DstBlend = D3DBLEND_INVSRCALPHA;	break;
		case EBLENDING_MODULATE:			SrcBlend = D3DBLEND_ZERO;
											DstBlend = D3DBLEND_SRCCOLOR;		break;
		default :
			OrkAssert( false );
			break;
	}
	if( BlendEnable != mLast_BlendEnable || bForce )
	{
		hr = GetD3DDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, BlendEnable);
	}
	if( SrcBlend != mLast_SrcBlend || bForce )
	{
		hr = GetD3DDevice()->SetRenderState(D3DRS_SRCBLEND, SrcBlend);
	}
	if( DstBlend != mLast_DstBlend || bForce )
	{
		hr = GetD3DDevice()->SetRenderState(D3DRS_DESTBLEND, DstBlend);
	}
	mLast_BlendEnable = BlendEnable;
	mLast_SrcBlend = SrcBlend;
	mLast_DstBlend = DstBlend;
	
	/////////////////////////////////////////////////

	if( (overridden.GetShadeModel() != rLast.GetShadeModel()) || bForce)
	{
		D3DGfxDevice Device = GetD3DDevice();

#if ! defined(_XBOX)
		switch( overridden.GetShadeModel() )
		{
			case ESHADEMODEL_FLAT:
				hr = Device->SetRenderState( D3DRS_SHADEMODE , D3DSHADE_FLAT );
				break;
			case ESHADEMODEL_SMOOTH:
				hr = Device->SetRenderState( D3DRS_SHADEMODE , D3DSHADE_GOURAUD );
				break;
			default:
				break;
		}
#endif
	}
	/////////////////////////////////////////////////
	mLastState = overridden;
}

} }

#endif
