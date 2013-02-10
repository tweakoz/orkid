////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#if defined( ORK_CONFIG_OPENGL )
#include <ork/orkmath.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include "gl.h"
#include <stdlib.h>
#include <ork/lev2/ui/ui.h>
//#include <ork/lev2/gfx/modeler/modeler_base.h>

////////////////////////////////////////////////////////////////////////////////

//#define USE_GENERIC_VATTRIB

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

static const bool USEVBO = true;
#define USEIBO 0

namespace ork { namespace lev2 {

////////////////////////////////////////////////////////////////////////////////
//	GL Vertex Buffer Implementation
///////////////////////////////////////////////////////////////////////////////

static bool gUSEAPPLEFLUSHBUFFERRANGE = false;

struct GLVtxBufHandle
{
	u32	mVBO;
	long mBufSize;
	int	miLockBase;
	int	miLockCount;
	
	GLVtxBufHandle() : mVBO(0), mBufSize(0), miLockBase(0), miLockCount(0) {}
	
	void CreateVbo(VertexBufferBase & VBuf)
	{
		VertexBufferBase * pnonconst = const_cast<VertexBufferBase *>( & VBuf );
		// Create A VBO and copy data into it
		mVBO = 0;
		glGenBuffers( 1, (GLuint*) & mVBO );
		//hPB = GLuint(ubh);
		//pnonconst->SetPBHandle( (void*)hPB );
		glBindBuffer( GL_ARRAY_BUFFER, mVBO );
		GL_ERRORCHECK();
		int iVBlen = VBuf.GetVtxSize()*VBuf.GetMax();

		//orkprintf( "CreateVBO len<%d>\n", iVBlen );
		
		bool bSTATIC = VBuf.IsStatic();
		
		static void* gzerobuf = calloc( 32<<20, 1 );
		glBufferData( GL_ARRAY_BUFFER, iVBlen, bSTATIC ? gzerobuf : 0, bSTATIC ? GL_STATIC_DRAW : GL_STREAM_DRAW );
		
		//////////////////////////////////////////////
		// we always update dynamic VBOs sequentially
		//  we also dont want to pay the cost of copying any data
		//  so we will use a VBO map range extension
		//  either GL_ARB_map_buffer_range or GL_APPLE_flush_buffer_range
		 
		if( gUSEAPPLEFLUSHBUFFERRANGE && (false == bSTATIC) )
		{
			//glBufferParameteriAPPLE(GL_ARRAY_BUFFER_ARB, GL_BUFFER_FLUSHING_UNMAP_APPLE, GL_FALSE);
			//glBufferParameteriAPPLE(GL_ARRAY_BUFFER_ARB, GL_BUFFER_SERIALIZED_MODIFY_APPLE, GL_FALSE);
		}

		//////////////////////////////////////////////

		GL_ERRORCHECK();

		GLint ibufsize = 0;
		glGetBufferParameteriv( GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &ibufsize );
		mBufSize = ibufsize;

		OrkAssert(mBufSize>0);
	}
};

///////////////////////////////////////////////////////////////////////////////

GlGeometryBufferInterface::GlGeometryBufferInterface( GfxTargetGL& target )
	: mTargetGL(target)
{
}

///////////////////////////////////////////////////////////////////////////////

void* GlGeometryBufferInterface::LockVB( VertexBufferBase & VBuf, int ibase, int icount )
{
	OrkAssert( false == VBuf.IsLocked() );

	void* rVal = 0;
	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());

	//////////////////////////////////////////////////////////
	// create the vbo ?
	//////////////////////////////////////////////////////////
	if( 0 == hBuf )
	{
		hBuf = new GLVtxBufHandle;
		VBuf.SetHandle( reinterpret_cast<void*> (hBuf) );

		hBuf->CreateVbo( VBuf );
	}

	int iMax = VBuf.GetMax();

	int ibasebytes = ibase*VBuf.GetVtxSize();
	int isizebytes = icount*VBuf.GetVtxSize();

	//////////////////////////////////////////////////////////
	// bind the vbo
	//////////////////////////////////////////////////////////

	glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );

	if( VBuf.IsStatic() )
	{
		if( isizebytes )
		{
			rVal = glMapBufferOES( GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES );
			OrkAssert( rVal );
		}
	}
	else
	{
		//printf( "LOCKVB VB<%p> ibase<%d> icount<%d>\n", & VBuf, ibase, icount );
		//////////////////////////////////////////////
		// we always update dynamic VBOs sequentially (no overrwrite)
		//  we also dont want to pay the cost of copying any data
		//  so we will use a VBO map range extension
		//  either GL_ARB_map_buffer_range or GL_APPLE_flush_buffer_range
		//////////////////////////////////////////////
		OrkAssert(isizebytes);
		//DWORD dlock = D3DLOCK_NOOVERWRITE;
		//if( 0 == ibase && 0 == icount ) dlock = D3dLockDiscard;
		if( isizebytes )
		{
			//hr = hBuf->Lock( ibasebytes, isizebytes, (void**)&rVal, dlock );
			//OrkAssert( SUCCEEDED( hr ) );
			rVal = glMapBufferOES( GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES );
			OrkAssert( rVal );
			hBuf->miLockBase = ibase;
			hBuf->miLockCount = icount;
						
		}
	}

	///////////////////////////////////
	// offset
	///////////////////////////////////

	char* pbase = (char*) rVal;
	pbase = pbase + ibasebytes;
	rVal = (void*) pbase;

	//////////////////////////////////////////////////////////
	// boilerplate stuff all devices do
	//////////////////////////////////////////////////////////

	VBuf.Lock();
	//VBuf.SetVertexPointer( rVal );

	//////////////////////////////////////////////////////////

	return rVal;
}

///////////////////////////////////////////////////////////////////////////////

const void* GlGeometryBufferInterface::LockVB( const VertexBufferBase & VBuf, int ibase, int icount )
{
	OrkAssert( false == VBuf.IsLocked() );

	void* rVal = 0;
	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());

	OrkAssert( hBuf != 0 );
	
	int iMax = VBuf.GetMax();

	if( icount == 0 )
	{
		icount = VBuf.GetMax();
	}
	int ibasebytes = ibase*VBuf.GetVtxSize();
	int isizebytes = icount*VBuf.GetVtxSize();

	OrkAssert(isizebytes);

	//printf( "ibasebytes<%d> isizebytes<%d> icount<%d> \n", ibasebytes, isizebytes, icount );

	//////////////////////////////////////////////////////////
	// bind the vbo
	//////////////////////////////////////////////////////////

	glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );

	//////////////////////////////////////////////////////////

	if( isizebytes )
	{
		//rVal = glMapBufferOES( GL_ARRAY_BUFFER, GL_READ_ONLY_OES );
		OrkAssert( rVal );
		OrkAssert( rVal!=(void*)0xffffffff );
		hBuf->miLockBase = ibase;
		hBuf->miLockCount = icount;
					
	}

	///////////////////////////////////
	// offset
	///////////////////////////////////

	char* pbase = (char*) rVal;
	pbase = pbase + ibasebytes;
	rVal = (void*) pbase;


	VBuf.Lock();

	return rVal;
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::UnLockVB( VertexBufferBase& VBuf )
{
	OrkAssert( VBuf.IsLocked() );
	
	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());
		
	if( VBuf.IsStatic() )
	{
		glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );
		glUnmapBufferOES( GL_ARRAY_BUFFER );
		glBindBuffer( GL_ARRAY_BUFFER, 0 );
		
		VBuf.Unlock();
	}
	else
	{
		//void FlushMappedBufferRangeAPPLE(enum target, intptr offset,
		//                                       sizeiptr size);
		
		int basebytes = VBuf.GetVtxSize()*hBuf->miLockBase;
		int countbytes = VBuf.GetVtxSize()*hBuf->miLockCount;
		
		//printf( "UNLOCK VB<%p> base<%d> count<%d>\n", & VBuf, basebytes, countbytes );
		
		glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );
		//glFlushMappedBufferRangeAPPLE(GL_ARRAY_BUFFER, (GLintptr)(basebytes), countbytes);
		glUnmapBufferOES( GL_ARRAY_BUFFER );
		glBindBuffer( GL_ARRAY_BUFFER, 0 );

		VBuf.Unlock();
	}
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::UnLockVB( const VertexBufferBase& VBuf )
{
	OrkAssert( VBuf.IsLocked() );
	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());
	glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );
	glUnmapBufferOES( GL_ARRAY_BUFFER );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	VBuf.Unlock();
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::ReleaseVB( VertexBufferBase& VBuf )
{
	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());
	
	if( hBuf )
	{
		glDeleteBuffers( 1, (GLuint*) & hBuf->mVBO );
	}
}

///////////////////////////////////////////////////////////////////////////////

bool GlGeometryBufferInterface::BindVertexStreamSource( const VertexBufferBase& VBuf )
{
	bool rval = false;
	////////////////////////////////////////////////////////////////////
	// setup VBO or DL
	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());
	OrkAssert( hBuf );
	glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );
	//printf( "VBO<%d>\n", int(hBuf->mVBO) );
	GL_ERRORCHECK();
	//////////////////////////////////////////////
	EVtxStreamFormat eStrFmt = VBuf.GetStreamFormat();
	int iStride = VBuf.GetVtxSize();
	//printf( "estrmfmt<%d> istride<%d>\n", int(eStrFmt), iStride );
	//////////////////////////////////////////////
	switch( eStrFmt )
	{
		case lev2::EVTXSTREAMFMT_V12N12B12T16:
		{
/*
			// VNB
			glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );
			glEnableClientState( GL_VERTEX_ARRAY );
			glNormalPointer( GL_FLOAT, iStride, (void*) 12 );	
			glEnableClientState( GL_NORMAL_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glTexCoordPointer( 3, GL_FLOAT,	iStride, (void*) 24 );	// T8
			// UV01
			glClientActiveTextureARB(GL_TEXTURE0);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 36 );	// T8
			glClientActiveTextureARB(GL_TEXTURE1);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 48 );	// T8

			glDisableClientState( GL_COLOR_ARRAY );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			rval = true;
*/
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12B12T8I4W4:
		{
/*
			glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );
			glEnableClientState( GL_VERTEX_ARRAY );
			glNormalPointer( GL_FLOAT, iStride, (void*) 12 );	
			glEnableClientState( GL_NORMAL_ARRAY );
			//glBinormalPointerARB( GL_FLOAT, iStride, (void*) 24 );	
			glClientActiveTextureARB(GL_TEXTURE1);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glTexCoordPointer( 3, GL_FLOAT,	iStride, (void*) 24 );	// T8
			//glEnableClientState( GL_BINORMAL_ARRAY_ARB );

			glClientActiveTextureARB(GL_TEXTURE0);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 36 );	// T8

			glDisableClientState( GL_COLOR_ARRAY );
			//glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 40 );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;
*/
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12T16C4:
		{
/*
			glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );
			glEnableClientState( GL_VERTEX_ARRAY );
			glNormalPointer( GL_FLOAT, iStride, (void*) 12 );	
			glEnableClientState( GL_NORMAL_ARRAY );

			glClientActiveTextureARB(GL_TEXTURE0);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 24 );	// T8
			glClientActiveTextureARB(GL_TEXTURE1);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 32 );	// T8

			glEnableClientState( GL_COLOR_ARRAY );
			glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 40 );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;
*/
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12T8I4W4:
		{
/*
			glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );
			glEnableClientState( GL_VERTEX_ARRAY );
			glNormalPointer( GL_FLOAT, iStride, (void*) 12 );	
			glEnableClientState( GL_NORMAL_ARRAY );

			glClientActiveTextureARB(GL_TEXTURE0);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 24 );	// T8
			glClientActiveTextureARB(GL_TEXTURE1);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );

			glDisableClientState( GL_COLOR_ARRAY );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;
*/
			break;
		}
		case EVTXSTREAMFMT_V12N12B12T8C4:
		{
/*
			glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );
			glNormalPointer( GL_FLOAT, iStride, (void*) 12 );	
			glClientActiveTextureARB(GL_TEXTURE1); // binormals
			glTexCoordPointer( 3, GL_FLOAT,	iStride, (void*) 24 );	// T8
			glClientActiveTextureARB(GL_TEXTURE0); // texture UV
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 36 );	// T8

			glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 44 );
			GL_ERRORCHECK();

			glClientActiveTextureARB(GL_TEXTURE0);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE1);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_COLOR_ARRAY );
			glEnableClientState( GL_NORMAL_ARRAY );

			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;
*/
			break;
		}
		case EVTXSTREAMFMT_V12C4N6I2T8:
		{
/*
			glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );	// V12
			glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 12 );	// C4
			glNormalPointer( GL_SHORT, iStride, (void*) 16 );	// N6
			GL_ERRORCHECK();

			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_COLOR_ARRAY );
			glEnableClientState( GL_NORMAL_ARRAY );

			glClientActiveTextureARB(GL_TEXTURE0);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE1);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;
*/
			break;
		}
		case EVTXSTREAMFMT_V12I4N12T8:
		{
/*
			glClientActiveTextureARB(GL_TEXTURE0);
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 28 );	// T8

			glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );	// V12
			glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 12 );	// C4
			glNormalPointer( GL_FLOAT, iStride, (void*) 16 );	// N6
			GL_ERRORCHECK();

			glClientActiveTextureARB(GL_TEXTURE0);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_COLOR_ARRAY );
			glEnableClientState( GL_NORMAL_ARRAY );

			glClientActiveTextureARB(GL_TEXTURE1);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;
*/
			break;
		}
		case EVTXSTREAMFMT_V12C4T16:
		{	
/*
			glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );
			glEnableClientState( GL_VERTEX_ARRAY );
			glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 12 );
			glEnableClientState( GL_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE0);
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 16 );
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE1);
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 24 );
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			////////////////////////////////////////////
			glDisableClientState( GL_NORMAL_ARRAY );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;
*/
			break;
		}
		case EVTXSTREAMFMT_V4C4:
		{
/*
			glVertexPointer( 2, GL_SHORT, iStride, (void*) 0 );
	GL_ERRORCHECK();
			glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 4);
	GL_ERRORCHECK();

			glEnableClientState( GL_VERTEX_ARRAY );
	GL_ERRORCHECK();
			glEnableClientState( GL_COLOR_ARRAY );
	GL_ERRORCHECK();

			glClientActiveTextureARB(GL_TEXTURE0);
	GL_ERRORCHECK();
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	GL_ERRORCHECK();
			glClientActiveTextureARB(GL_TEXTURE1);
	GL_ERRORCHECK();
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	GL_ERRORCHECK();
			glDisableClientState( GL_NORMAL_ARRAY );
	GL_ERRORCHECK();
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
	GL_ERRORCHECK();
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;
*/

			break;
		}
		case EVTXSTREAMFMT_V4T4:
		{
/*
			glVertexPointer( 2, GL_SHORT, iStride, (void*) 0 );
	GL_ERRORCHECK();
			glClientActiveTextureARB(GL_TEXTURE0);
	GL_ERRORCHECK();
			glTexCoordPointer( 2, GL_SHORT,	iStride, (void*) 4 );
	GL_ERRORCHECK();

			glClientActiveTextureARB(GL_TEXTURE0);
	GL_ERRORCHECK();
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	GL_ERRORCHECK();
			glEnableClientState( GL_VERTEX_ARRAY );
	GL_ERRORCHECK();

			glClientActiveTextureARB(GL_TEXTURE1);
	GL_ERRORCHECK();
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	GL_ERRORCHECK();
			glDisableClientState( GL_NORMAL_ARRAY );
	GL_ERRORCHECK();
			glDisableClientState( GL_COLOR_ARRAY );
	GL_ERRORCHECK();
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
	GL_ERRORCHECK();
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;
*/


			break;
		}
		case EVTXSTREAMFMT_V4T4C4:
		{
/*
			glVertexPointer( 2, GL_SHORT, iStride, (void*) 0 );
	GL_ERRORCHECK();
			glClientActiveTextureARB(GL_TEXTURE0);
	GL_ERRORCHECK();
			glTexCoordPointer( 2, GL_SHORT,	iStride, (void*) 4 );
	GL_ERRORCHECK();
			glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 8 );
	GL_ERRORCHECK();

			glClientActiveTextureARB(GL_TEXTURE0);
	GL_ERRORCHECK();
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	GL_ERRORCHECK();
			glEnableClientState( GL_VERTEX_ARRAY );
	GL_ERRORCHECK();
			glEnableClientState( GL_COLOR_ARRAY );
	GL_ERRORCHECK();
			
			glClientActiveTextureARB(GL_TEXTURE1);
	GL_ERRORCHECK();
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	GL_ERRORCHECK();
			glDisableClientState( GL_NORMAL_ARRAY );
	GL_ERRORCHECK();
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
	GL_ERRORCHECK();
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;
*/

			break;
		}

		default:
		{	OrkAssert(false);
			break;
		}
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

#define KVANRM_TRUE		true
#define KVANRM_FALSE	false

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::DrawPrimitive( const VertexBufferBase& VBuf, EPrimitiveType eTyp, int ivbase, int ivcount)
{
	int imax = VBuf.GetMax();

	if( imax )
	{
		int inumpasses = mTargetGL.GetCurMaterial()->BeginBlock(&mTargetGL);

		for( int ipass=0; ipass<inumpasses; ipass++ )
		{
			bool bDRAW = mTargetGL.GetCurMaterial()->BeginPass( &mTargetGL,ipass );

			if( bDRAW )
			{
				static bool lbwire = false;

				if( EPRIM_NONE == eTyp )
				{	
					eTyp = VBuf.GetPrimType();
				}

				DrawPrimitiveEML( VBuf, eTyp, ivbase, ivcount );

				mTargetGL.GetCurMaterial()->EndPass(&mTargetGL);
			}

		}
		
		mTargetGL.GetCurMaterial()->EndBlock(&mTargetGL);

	}

	GL_ERRORCHECK();

}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::DrawIndexedPrimitive( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf , EPrimitiveType eType, int ivbase, int ivcount)
{
	int imax = VBuf.GetMax();

	if( imax )
	{
		int inumpasses = mTargetGL.GetCurMaterial()->BeginBlock(&mTargetGL);

		for( int ipass=0; ipass<inumpasses; ipass++ )
		{
			bool bDRAW = mTargetGL.GetCurMaterial()->BeginPass( &mTargetGL,ipass );

			if( bDRAW )
			{
				static bool lbwire = false;

				if( EPRIM_NONE == eType )
				{	
					eType = VBuf.GetPrimType();
				}

				DrawIndexedPrimitiveEML( VBuf, IdxBuf, eType, ivbase, ivcount );

				mTargetGL.GetCurMaterial()->EndPass(&mTargetGL);
			}

		}
		
		mTargetGL.GetCurMaterial()->EndBlock(&mTargetGL);

	}

	GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::DrawPrimitiveEML( const VertexBufferBase& VBuf, EPrimitiveType eType, int ivbase, int ivcount)
{
	////////////////////////////////////////////////////////////////////

	bool bOK = BindVertexStreamSource( VBuf );
	if( false == bOK ) return;
//	return; //
	////////////////////////////////////////////////////////////////////

	int inum = (ivcount==0) ? VBuf.GetNumVertices() : ivcount;

	if( eType == EPRIM_NONE )
	{
		eType = VBuf.GetPrimType();
	}
	if( inum )
	{
		switch( eType )
		{
			case EPRIM_LINES:
			{	//orkprintf( "drawarrays: <ivbase %d> <inum %d> lines\n", ivbase, inum );
				GL_ERRORCHECK();
				glDrawArrays( GL_LINES, ivbase, inum );
				GL_ERRORCHECK();
				break;
			}
			case EPRIM_QUADS:
				//orkprintf( "drawarrays: %d quads\n", inum );
				GL_ERRORCHECK();
				//glDrawArrays( GL_QUADS, ivbase, inum );
				GL_ERRORCHECK();
				miTrianglesRendered += (inum/2);
				break;
			case EPRIM_TRIANGLES:
				//orkprintf( "drawarrays: <ivbase %d> <inum %d> tris\n", ivbase, inum );
				GL_ERRORCHECK();
				glDrawArrays( GL_TRIANGLES, ivbase, inum );
				GL_ERRORCHECK();
				miTrianglesRendered += (inum/3);
				break;
			case EPRIM_TRIANGLESTRIP:
				GL_ERRORCHECK();
				glDrawArrays( GL_TRIANGLE_STRIP, ivbase, inum );
				GL_ERRORCHECK();
				miTrianglesRendered += (inum-2);
				break;
/*			case EPRIM_POINTS:
				GL_ERRORCHECK();
				glDrawArrays( GL_POINTS, 0, iNum );
				GL_ERRORCHECK();
				break;
			case EPRIM_POINTSPRITES:
				GL_ERRORCHECK();
				glPointSize( mTargetGL.GetCurMaterial()->mfParticleSize );
											
				glEnable( GL_POINT_SPRITE_ARB );
				glDrawArrays( GL_POINTS, 0, iNum );
				glDisable( GL_POINT_SPRITE_ARB );

				GL_ERRORCHECK();
				break;
			default:
				glDrawArrays( GL_POINTS, 0, iNum );
				//OrkAssert( false );
				break;*/
			default:
				break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//epass thru

void GlGeometryBufferInterface::DrawIndexedPrimitiveEML( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType, int ivbase, int ivcount)
{
	////////////////////////////////////////////////////////////////////

	BindVertexStreamSource( VBuf );

	const U16* pindices = (const U16*) IdxBuf.GetHandle();
	
	OrkAssert( pindices!=0 );
	
	////////////////////////////////////////////////////////////////////

	int iNum = IdxBuf.GetNumIndices();

	if( iNum )
	{
		switch( eType )
		{
			case EPRIM_LINES:
			{	//orkprintf( "drawarrays: %d lines\n", iNum );
				GL_ERRORCHECK();
				glDrawElements( GL_LINES, iNum, GL_UNSIGNED_SHORT, pindices );
				GL_ERRORCHECK();
				break;
			}
			case EPRIM_QUADS:
				GL_ERRORCHECK();
				//glDrawElements( GL_QUADS, iNum, GL_UNSIGNED_SHORT, pindices );
				GL_ERRORCHECK();
				miTrianglesRendered += (iNum/2);
				break;
			case EPRIM_TRIANGLES:
				GL_ERRORCHECK();
				glDrawElements( GL_TRIANGLES, iNum, GL_UNSIGNED_SHORT, pindices );
				GL_ERRORCHECK();
				miTrianglesRendered += (iNum/3);
				break;
			case EPRIM_TRIANGLESTRIP:
				GL_ERRORCHECK();
				glDrawElements( GL_TRIANGLE_STRIP, iNum, GL_UNSIGNED_SHORT, pindices );
				GL_ERRORCHECK();
				miTrianglesRendered += (iNum-2);
				break;
			case EPRIM_POINTS:
				GL_ERRORCHECK();
				glDrawElements( GL_POINTS, iNum, GL_UNSIGNED_SHORT, pindices );
				GL_ERRORCHECK();
				break;
			case EPRIM_POINTSPRITES:
				GL_ERRORCHECK();
				//glPointSize( mTargetGL.GetCurMaterial()->mfParticleSize );
	GL_ERRORCHECK();
											
				//glEnable( GL_POINT_SPRITE_ARB );
	GL_ERRORCHECK();
				glDrawElements( GL_POINTS, iNum, GL_UNSIGNED_SHORT, pindices );
	GL_ERRORCHECK();
				//glDisable( GL_POINT_SPRITE_ARB );
	GL_ERRORCHECK();

				GL_ERRORCHECK();
				break;
			default:
				//glDrawArrays( GL_POINTS, 0, iNum );
				OrkAssert( false );
				break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void* GlGeometryBufferInterface::LockIB( IndexBufferBase& IdxBuf, int ibase, int icount )
{
	if( 0 == IdxBuf.GetHandle() )
	{
		IdxBuf.SetHandle( (void*) malloc( IdxBuf.GetNumIndices() * IdxBuf.GetIndexSize()) );
	}
	return (void*) IdxBuf.GetHandle();
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::UnLockIB( IndexBufferBase& IdxBuf)
{
}

///////////////////////////////////////////////////////////////////////////////

const void* GlGeometryBufferInterface::LockIB( const IndexBufferBase& IdxBuf, int ibase, int icount )
{
	return (const void*) IdxBuf.GetHandle();
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::UnLockIB( const IndexBufferBase& IdxBuf)
{
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::ReleaseIB( IndexBufferBase& IdxBuf )
{
	free((void *) IdxBuf.GetHandle());
	IdxBuf.SetHandle(0);
}

///////////////////////////////////////////////////////////////////////////////

} } //namespace ork::lev2
#endif
