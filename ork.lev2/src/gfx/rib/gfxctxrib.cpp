////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#if 0
#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include "gfxctxrib.h"
#include <aqsis/ri/ri.h>

/////////////////////////////////////////////////////////////////////////
bool LoadIL(const ork::AssetPath& pth, ork::lev2::Texture *ptex);
/////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxTargetRib, "GfxTargetRib")
namespace ork { namespace lev2 {

/////////////////////////////////////////////////////////////////////////

void RibGfxTargetInit()
{
	GfxEnv::SetTargetClass(GfxTargetRib::GetClassStatic());
}

/////////////////////////////////////////////////////////////////////////

bool RibFxInterface::LoadFxShader( const AssetPath& pth, FxShader *pfxshader  )
{
	AssetPath assetname = pth;
	assetname.SetExtension( "ribxml" );
	FxShader* shader = new FxShader;
	pfxshader->SetInternalHandle( 0 );
	//bool bOK = LoadFxShader( shader );
	//OrkAssert(bOK);
	return true;
}

///////////////////////////////////////////////////////////////////////////////

CMatrix4 RibMatrixStackInterface::Ortho( float left, float right, float top, float bottom, float fnear, float ffar )
{
	CMatrix4 mat;
	mat.Ortho( left, right, top, bottom, fnear, ffar );
	return  mat;
}

///////////////////////////////////////////////////////////////////////////////

RibFrameBufferInterface::RibFrameBufferInterface(GfxTarget& target )
	: FrameBufferInterface( target )
{
}

RibFrameBufferInterface::~RibFrameBufferInterface()
{
}

///////////////////////////////////////////////////////////////////////////////

GfxTargetRib::~GfxTargetRib()
{
}

///////////////////////////////////////////////////////////////////////////////

GfxTargetRib::GfxTargetRib()
	: GfxTarget()
	, mFbI( *this )
	, mMtxI( *this )
{
	static bool binit = true;

	if( true == binit )
	{
		binit = false;
		FxShader::RegisterLoaders( "shaders\\rib\\", "ribxml" );
	}
    RiBegin("min.rib");
    RiDisplay ("min.tiff","file","rgb",RI_NULL);
    RiProjection ("perspective",RI_NULL);
    RiWorldBegin();
    RiTranslate(0,0,2);
    RiSphere(1,-1,1,360,RI_NULL);
    RiWorldEnd();
    RiEnd();

}

void GfxTargetRib::InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase )
{
}

void GfxTargetRib::InitializeContext( GfxBuffer *pBuf )
{
}

void GfxTargetRib::SetSize( int ix, int iy, int iw, int ih )
{
	miX=ix;
	miY=iy;
	miW=iw;
	miH=ih;
}

///////////////////////////////////////////////////////////////////////////////

void* RibGeometryBufferInterface::LockIB( IndexBufferBase& IdxBuf, int ibase, int icount )
{	if( 0 == IdxBuf.GetHandle() )
	{
		IdxBuf.SetHandle( (void*) std::malloc( IdxBuf.GetNumIndices() * IdxBuf.GetIndexSize()) );
	}
	char* pch = (char*) IdxBuf.GetHandle();
	return (void*) (pch+ibase);
}
void RibGeometryBufferInterface::UnLockIB( IndexBufferBase& IdxBuf)
{
}

const void* RibGeometryBufferInterface::LockIB( const IndexBufferBase& IdxBuf, int ibase, int icount )
{	if( 0 == IdxBuf.GetHandle() )
	{
		IdxBuf.SetHandle( (void*) std::malloc( IdxBuf.GetNumIndices() * IdxBuf.GetIndexSize()) );
	}
	const char* pch = (const char*) IdxBuf.GetHandle();
	return (const void*) (pch+ibase);
}
void RibGeometryBufferInterface::UnLockIB( const IndexBufferBase& IdxBuf)
{
}

void RibGeometryBufferInterface::ReleaseIB( IndexBufferBase& IdxBuf )
{
	std::free( IdxBuf.GetHandle() );
	IdxBuf.SetHandle(0);
}

void* RibGeometryBufferInterface::LockVB( VertexBufferBase& VBuf, int ibase, int icount )
{	OrkAssert( false == VBuf.IsLocked() );
	int iVBlen = VBuf.GetVtxSize()*VBuf.GetMax();
	if( 0 == VBuf.GetHandle() )
	{	void* pdata = std::malloc( iVBlen );
		//orkprintf( "DuGeometryBufferInterface::LockVB() malloc_vblen<%d>\n", iVBlen );
		VBuf.SetHandle( pdata );
	}
	VBuf.Lock();
	VBuf.Reset();
	return VBuf.GetHandle();
}

const void* RibGeometryBufferInterface::LockVB( const VertexBufferBase& VBuf, int ibase, int icount )
{	OrkAssert( false == VBuf.IsLocked() );
	int iVBlen = VBuf.GetVtxSize()*VBuf.GetMax();
	VBuf.Lock();
	const void* pdata = VBuf.GetHandle();
	OrkAssert(pdata!=0);
	return pdata;
}

void RibGeometryBufferInterface::UnLockVB( VertexBufferBase& VBuf )
{
	OrkAssert( VBuf.IsLocked() );
	VBuf.Unlock();
}
void RibGeometryBufferInterface::UnLockVB( const VertexBufferBase& VBuf )
{	OrkAssert( VBuf.IsLocked() );
	VBuf.Unlock();
}
void RibGeometryBufferInterface::ReleaseVB( VertexBufferBase& VBuf )
{
	std::free( (void *)  VBuf.GetHandle());
}

bool GfxTargetRib::SetDisplayMode(DisplayMode *mode)
{
	return false;
}

void RibGeometryBufferInterface::DrawIndexedPrimitive( const VertexBufferBase& VBuf,const IndexBufferBase& IdxBuf , EPrimitiveType eType, int ivbase, int ivcount)
{
}

void RibGeometryBufferInterface::DrawPrimitive( const VertexBufferBase& VBuf, EPrimitiveType eType, int ivbase, int ivcount)
{
}

void RibGeometryBufferInterface::DrawIndexedPrimitiveEML( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf , EPrimitiveType eType, int ivbase, int ivcount)
{
}
void RibGeometryBufferInterface::DrawPrimitiveEML( const VertexBufferBase& VBuf, EPrimitiveType eType, int ivbase, int ivcount)
{
}

bool RibTextureInterface::LoadTexture( const AssetPath& fname, Texture *ptex )
{
	///////////////////////////////////////////////
	AssetPath Filename = fname;
	bool bHasExt = Filename.HasExtension();
	if( false == bHasExt )
	{
		Filename.SetExtension( "dds" );
	}
	///////////////////////////////////////////////
	CFile TextureFile( Filename, ork::EFM_READ );
	if( false == TextureFile.IsOpen() )
	{
		return false;
	}
	return true;
}

} }
#endif