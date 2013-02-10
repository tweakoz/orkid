////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/ui.h>
#include <ork/file/file.h>

#if defined( ORK_CONFIG_DIRECT3D )
#include "dx.h"

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxTargetDX, "GfxTargetDX")

extern ork::ETocMode getocmod;

namespace ork { namespace lev2 {

//const CClass *GfxTargetDX::gpClass = 0;
/////////////////////////////////////////////////////////////////////////

CMatrix4 DxMatrixStackInterface::Frustum( float left, float right, float top, float bottom, float zn, float zf )
{
	CMatrix4 rval;
	rval.SetToIdentity();

	CReal width = right-left;
	CReal height = top-bottom;
	CReal depth = (zf-zn);

	/////////////////////////////////////////////

	rval.SetElemYX( 0,0, CReal(2.0f*zn)/-width );
	rval.SetElemYX( 1,1, CReal(2.0f*zn)/height );
	rval.SetElemYX( 2,2, CReal(zf)/depth );
	rval.SetElemYX( 3,3, CReal(0.0f) );

	rval.SetElemYX( 2,3, CReal(zn*zf)/CReal(zn-zf) );
	rval.SetElemYX( 3,2, CReal(1.0f) );

	//rval = Scale;
	/////////////////////////////////////////////
	//2*zn/w  0       0              0
	//0       2*zn/h  0              0
	//0       0       zf/(zf-zn)     1
	//0       0       zn*zf/(zn-zf)  0
	/////////////////////////////////////////////

	return rval;
}

CMatrix4 DxMatrixStackInterface::Ortho( float left, float right, float top, float bottom, float fnear, float ffar )
{
	CReal zero(0.0f);
	CReal one(1.0f);
	CReal two(2.0f);

	CReal invWidth = one / CReal(right - left);
	CReal invHeight = one / CReal(top - bottom);
	CReal invDepth = one / CReal(ffar - fnear);
	CReal fScaleX = two * invWidth;
	CReal fScaleY = two * invHeight;
	CReal fScaleZ = one * invDepth;
	CReal TransX = -CReal(right + left) * invWidth;
	CReal TransY = -CReal(top + bottom) * invHeight;
	CReal TransZ = -CReal(fnear)*invDepth; //T(0.0f);

	CMatrix4 rval;

	rval.SetElemYX( 0,0, fScaleX );
	rval.SetElemYX( 1,0, zero );
	rval.SetElemYX( 2,0, zero );
	rval.SetElemYX( 3,0, zero );

	rval.SetElemYX( 0,1, zero );
	rval.SetElemYX( 1,1, fScaleY );
	rval.SetElemYX( 2,1, zero );
	rval.SetElemYX( 3,1, zero );

	rval.SetElemYX( 0,2, zero );
	rval.SetElemYX( 1,2, zero );
	rval.SetElemYX( 2,2, fScaleZ );
	rval.SetElemYX( 3,2, zero );

	rval.SetElemYX( 0,3, TransX );
	rval.SetElemYX( 1,3, TransY );
	rval.SetElemYX( 2,3, TransZ );
	rval.SetElemYX( 3,3, one );


	return rval;
}

///////////////////////////////////////////////////////////////////////////////

static bool MungeDataPathForLowEndHighEnd( ork::file::Path& pth )
{
	//ork::file::Path::NameType folder = pth.GetFolder(ork::file::Path::EPATHTYPE_POSIX);

	const char* fxshader = strstr( pth.c_str(), "fxshader://" );

	if( fxshader )
	{
		ork::file::Path::NameType fname;
		fname.replace( pth.c_str(), "fxshader://", "fxshader://loend/" );
		pth = ork::file::Path( fname.c_str() );
		return true;
	}

	return false;
}
void Direct3dGfxTargetInit()
{
	//MConcreteClassReg(GfxTargetDX,GfxTarget);
	GfxEnv::SetTargetClass(GfxTargetDX::GetClassStatic());

	const ork::SFileDevContext& datactx = ork::CFileEnv::UrlBaseToContext( "data" );//, DataDirContext );

	static SFileDevContext FxShaderFileContext;
	file::Path::NameType fxshaderbase = datactx.GetFilesystemBaseAbs()+"/shaders/dx";
	file::Path fxshaderpath( fxshaderbase.c_str() );
	FxShaderFileContext.SetFilesystemBaseAbs( fxshaderpath.c_str() );
	FxShaderFileContext.SetPrependFilesystemBase( true );

#if defined(_XBOX)
	FillToc_fxshader( FxShaderFileContext.GetTOC() );
	FxShaderFileContext.SetTocMode( getocmod );
#else
	const GfxTargetCreationParams& CreationParams = GfxEnv::GetRef().GetCreationParams();
	if( CreationParams.miQuality<2 )
	{
		FxShaderFileContext.AddPathConverter( MungeDataPathForLowEndHighEnd );
	}
#endif

	CFileEnv::RegisterUrlBase( "fxshader://", FxShaderFileContext );


}

///////////////////////////////////////////////////////////////////////////////

void DxFrameBufferInterface::SetScissor( int iX, int iY, int iW, int iH )
{
	RECT scissor_rect;
	scissor_rect.left = iX;
	scissor_rect.top = iY;
	scissor_rect.right = iX + iW;
	scissor_rect.bottom = iY + iH;

	HRESULT hr = GetD3DDevice()->SetScissorRect( &scissor_rect );
	OrkAssert(SUCCEEDED(hr));
}

///////////////////////////////////////////////////////////////////////////////

void DxFrameBufferInterface::AttachViewport( CUIViewport *pVP )
{
	OrkAssert( pVP );

	int vpx = pVP->GetX();
	int vpy = pVP->GetY();
	int vpw = pVP->GetW();
	int vph = pVP->GetH();

	vpx = OrkSTXClampToRange( vpx, 0, 4096 );
	vpy = OrkSTXClampToRange( vpy, 0, 4096 );
	vpw = OrkSTXClampToRange( vpw, 8, 4096 );
	vph = OrkSTXClampToRange( vph, 8, 4096 );

	SetScissor( vpx, vpy, vpw, vph );
	SetViewport( vpx, vpy, vpw, vph );
}

} }

#endif

