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
