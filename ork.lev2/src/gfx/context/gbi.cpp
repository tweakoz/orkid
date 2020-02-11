////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/object/AutoConnector.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::render2dQuadEML(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2){
  // align source pixels to target pixels if sizes match
  float fx0 = QuadRect.x;
  float fy0 = QuadRect.y;
  float fx1 = QuadRect.x + QuadRect.z;
  float fy1 = QuadRect.y + QuadRect.w;

  float fua0 = UvRect.x;
  float fva0 = UvRect.y;
  float fua1 = UvRect.x + UvRect.z;
  float fva1 = UvRect.y + UvRect.w;

  float fub0 = UvRect2.x;
  float fvb0 = UvRect2.y;
  float fub1 = UvRect2.x + UvRect2.z;
  float fvb1 = UvRect2.y + UvRect2.w;

  DynamicVertexBuffer<SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();
  U32 uc                                = 0xffffffff;
  ork::lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(this, &vb, 6);
  vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, fua0, fva0, fub0, fvb0, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, fua1, fva1, fub1, fvb1, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy0, 0.0f, fua1, fva0, fub1, fvb0, uc));

  vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, fua0, fva0, fub0, fvb0, uc));
  vw.AddVertex(SVtxV12C4T16(fx0, fy1, 0.0f, fua0, fva1, fub0, fvb1, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, fua1, fva1, fub1, fvb1, uc));
  vw.UnLock(this);

  this->DrawPrimitiveEML(vw, EPRIM_TRIANGLES, 6);

}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::BeginFrame()
{
	miTrianglesRendered = 0;
	_doBeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::EndFrame()
{
	_doEndFrame();
	miTrianglesRendered = 0;
}


///////////////////////////////////////////////////////////////////////////////

GeometryBufferInterface::GeometryBufferInterface()
{
}

///////////////////////////////////////////////////////////////////////////////

GeometryBufferInterface::~GeometryBufferInterface()
{
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::FlushVB( VertexBufferBase& VBuf )
{
	if( VBuf.IsLocked() )
	{
		UnLockVB( VBuf );
	}
	if( VBuf.GetNumVertices() )
	{	DrawPrimitive( VBuf );
	}
	LockVB( VBuf );
	VBuf.Reset();
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::DrawPrimitive( const VtxWriterBase& VW, EPrimitiveType eType, int icount )
{
	if( 0 == icount )
	{
		icount = VW.miWriteCounter;
	}
	//printf( "GBI::DrawPrim(VW) ibase<%d> icount<%d>\n", VW.miWriteBase, icount );
	DrawPrimitive( *VW.mpVB, eType, VW.miWriteBase, icount );

}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::DrawPrimitiveEML( const VtxWriterBase& VW, EPrimitiveType eType, int icount )
{
	if( 0 == icount )
	{
		icount = VW.miWriteCounter;
	}
	//printf( "GBI::DrawPrim(VW) ibase<%d> icount<%d>\n", VW.miWriteBase, icount );
	DrawPrimitiveEML( *VW.mpVB, eType, VW.miWriteBase, icount );

}

///////////////////////////////////////////////////////////////////////////////

} }
