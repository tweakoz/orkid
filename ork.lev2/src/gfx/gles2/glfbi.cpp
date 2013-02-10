////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#if 1 // defined( ORK_CONFIG_OPENGL )
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/ui/ui.h>

#include <ork/lev2/gfx/dbgfontman.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

GlFrameBufferInterface::GlFrameBufferInterface( GfxTargetGL& target )
	: FrameBufferInterface( target )
	, mTargetGL( target )
{
}

GlFrameBufferInterface::~GlFrameBufferInterface()
{
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::SetAsRenderTarget( void ) 
{
	ork::lev2::GlIosPlatformObject* plato = (ork::lev2::GlIosPlatformObject*) mTargetGL.GetPlatformHandle();
	ork::lev2::IosMainFbo& main_fbo = plato->mMainFbo;
	mTargetGL.MakeCurrentContext();
    glBindFramebuffer(GL_FRAMEBUFFER, main_fbo.mFBO);
    glViewport(0,0,main_fbo.miWidth,main_fbo.miHeight);
    glScissor(0,0,main_fbo.miWidth,main_fbo.miHeight);
	//glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	//glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
	GL_ERRORCHECK();
	//glDrawBuffer( GL_BACK );
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::DoBeginFrame( void )
{
	//glFinish();

	ork::lev2::GlIosPlatformObject* plato = (ork::lev2::GlIosPlatformObject*) mTargetGL.GetPlatformHandle();
	ork::lev2::IosMainFbo& main_fbo = plato->mMainFbo;

	if( mTargetGL.FBI()->GetRtGroup() )
	{
		//printf( "BEGINFRAME<RtGroup>\n" );
	}
	////////////////////////////////
	else if( IsOffscreenTarget() )
	////////////////////////////////
	{
	}
	/////////////////////////////////////////////////
	else // window (On Screen Target)
	/////////////////////////////////////////////////
	{
	//	printf( "BEGINFRAME<WIN>\n" );
		SetAsRenderTarget();

		/////////////////////////////////////////////////

		if( GetAutoClear() )
		{
			CVector4 rCol = GetClearColor();
			//U32 ClearColorU = mTarget.CColor4ToU32(GetClearColor());
			if( IsPickState() )
				glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			else
				glClearColor( rCol.GetX(), rCol.GetY(), rCol.GetZ(), 1.0f );

			//printf( "GlFrameBufferInterface::ClearViewport()\n" );
			GL_ERRORCHECK();
			glClearDepthf( 1.0f );
			GL_ERRORCHECK();
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			GL_ERRORCHECK();
		}
        /////////////
        // clear
        /////////////
	}
	
	glDepthRangef( 0.0, 1.0f );

	SRect extents( mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH() );
	PushViewport(extents);
	PushScissor(extents);
	
	/////////////////////////////////////////////////
	// Set Initial Rendering States

	const SRasterState defstate;
	mTarget.RSI()->BindRasterState( defstate, true );

}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::DoEndFrame( void )
{
	GL_ERRORCHECK();

	///////////////////////////////////////////
	// release all resources for this frame
	///////////////////////////////////////////

	glBindTexture( GL_TEXTURE_2D, 0 );
	
	////////////////////////////////
	if( mTargetGL.FBI()->GetRtGroup() )
	{
		//printf( "ENDFRAME<RtGroup>\n" );
	}
	else if( IsOffscreenTarget() )
	{
		//printf( "ENDFRAME<OST>\n" );
		GfxBuffer* pbuf = GetThisBuffer();
		pbuf->mpTexture->SetDirty(false);
		pbuf->SetDirty(false);
	}
	else
	{
	//	printf( "ENDFRAME<WIN>\n" );
		mTargetGL.SwapGLContext(mTargetGL.GetCtxBase());
	}
	////////////////////////////////
	PopViewport();
	PopScissor();
	////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::InitializeContext( GfxBuffer* pBuf )
{
	///////////////////////////////////////////
	// create texture surface

	int ibytesperpix = 0;

	bool Zonly = false;

	switch( pBuf->meFormat )
	{
		case EBUFFMT_RGBA32:
			ibytesperpix = 4;
			break;
		case EBUFFMT_RGBA64:
			ibytesperpix = 8;
			break;
		case EBUFFMT_RGBA128:
			ibytesperpix = 16;
			break;
		case EBUFFMT_Z16:
			ibytesperpix = 2;
			Zonly=true;
			break;
		case EBUFFMT_Z32:
			ibytesperpix = 2;
			Zonly=true;
			break;
		default:
			OrkAssert(false);
			break;
	}

	///////////////////////////////////////////
	// create orknum texture and link it

	Texture* ptexture = new Texture();
	ptexture->SetWidth( mTarget.GetW() );
	ptexture->SetHeight( mTarget.GetH() );
	ptexture->SetTexClass( ork::lev2::Texture::ETEXCLASS_RENDERTARGET );

	SetBufferTexture(ptexture);

	///////////////////////////////////////////
	// create material

	pBuf->mpTexture = ptexture;

	///////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::SetRtGroup( RtGroup* Base )
{	
	mTargetGL.MakeCurrentContext();
	
	if( 0 == Base )
	{
		////////////////////////////////////////////////
		// disable mrt
		//  pop viewport/scissor that was pushed by SetRtGroup( nonzero )
		// on xbox, happens after resolve
		////////////////////////////////////////////////
		PopScissor();
		PopViewport();
		SetAsRenderTarget();
		mCurrentRtGroup = 0;
		return;
	}
	
	//////////////////////////////////////////////////
	// lazy create mrt's
	//////////////////////////////////////////////////

	int iw = Base->GetW();
	int ih = Base->GetH();

	FBOObject *FboObj = (FBOObject *) Base->GetInternalHandle();

	int inumtargets = Base->GetNumTargets();

    OrkAssert(inumtargets<2); // ES does not support MRT's yet... ;<
    
	//D3DMULTISAMPLE_TYPE sampletype;
	switch (Base->GetSamples()) {
		case 2:
			//sampletype = D3DMULTISAMPLE_2_SAMPLES;
			break;
		case 4:
			//sampletype = D3DMULTISAMPLE_4_SAMPLES;
			break;
		default:
			//sampletype = D3DMULTISAMPLE_NONE;
			break;
	}

	if( 0 == FboObj )
	{
		FboObj = new FBOObject;

		Base->SetInternalHandle( FboObj );

		glGenRenderbuffers( 1, & FboObj->mDSBO );
		glGenFramebuffers( 1, & FboObj->mFBOMaster );

		printf( "GenFBO<%d>\n", int(FboObj->mFBOMaster) );

		//////////////////////////////////////////
		// initialize depth renderbuffer

		// attach renderbufferto framebufferdepth buffer
		glBindFramebuffer(GL_FRAMEBUFFER, FboObj->mFBOMaster );
		GL_ERRORCHECK();
		glBindRenderbuffer(GL_RENDERBUFFER, FboObj->mDSBO );
		
		//glRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER_EXT, 1, GL_DEPTH_COMPONENT24, iw, ih );
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, iw, ih );
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, FboObj->mDSBO );
		GL_ERRORCHECK();

		//////////////////////////////////////////

		for( int it=0; it<inumtargets; it++ )
		{
			//D3DFORMAT efmt = D3DFMT_A8R8G8B8;
			GLuint glinternalformat = 0;
			GLuint glformat = GL_RGBA;
			GLenum gltype = 0;

			switch( Base->GetMrt(it)->GetBufferFormat() )
			{
				case EBUFFMT_RGBA32: 
					glinternalformat = GL_RGBA8;
					gltype = GL_UNSIGNED_BYTE;
					break;
				case EBUFFMT_RGBA64: 
                    assert(false);
					break;
				default:
					OrkAssert(false);
					break;
				//case EBUFFMT_RGBA128: glinternalformat = GL_RGBA32; break;
			}
			OrkAssert(glinternalformat!=0);

			//////////////////////////////////////////
			// initialize texture

			glGenTextures( 1, (GLuint *) & FboObj->mTEX[it] );
			GL_ERRORCHECK();

			glBindTexture(GL_TEXTURE_2D, FboObj->mTEX[it] );
			GL_ERRORCHECK();
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			GL_ERRORCHECK();
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			GL_ERRORCHECK();
			glTexImage2D(GL_TEXTURE_2D, 0, glinternalformat, iw, ih, 0, glformat, gltype, NULL);
			GL_ERRORCHECK();

			//////////////////////////////////////////
			//////////////////////////////////////////
			// attach texture to framebuffercolor buffer

			glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+it, GL_TEXTURE_2D, FboObj->mTEX[it], 0);
			GL_ERRORCHECK();

			//////////////////////////////////////////
			GLTextureObject* ptexOBJ = new GLTextureObject;
			ptexOBJ->mObject = FboObj->mTEX[it];
			//////////////////////////////////////////
			Base->GetMrt(it)->mpTexture = new Texture;
			Base->GetMrt(it)->mpTexture->SetWidth( iw );
			Base->GetMrt(it)->mpTexture->SetHeight( ih );
			Base->GetMrt(it)->mpTexture->SetTexIH( (void*) ptexOBJ );
			Base->GetMrt(it)->mpTexture->SetTexClass( ork::lev2::Texture::ETEXCLASS_RENDERTARGET );
			//////////////////////////////////////////
			Base->GetMrt(it)->mpMaterial = new ork::lev2::GfxMaterialUITextured( & mTarget ); 
			Base->GetMrt(it)->mpMaterial->SetTexture( ETEXDEST_DIFFUSE, Base->GetMrt(it)->mpTexture );
			//////////////////////////////////////////
		}
	}

	//////////////////////////////////////////////////
	// enable mrts
	//////////////////////////////////////////////////

	GLenum buffers[] = {	GL_COLOR_ATTACHMENT0, 
							//GL_COLOR_ATTACHMENT1,
							//GL_COLOR_ATTACHMENT2,
							//GL_COLOR_ATTACHMENT3 
					   };

	//printf( "BindFBO<%d> numattachments<%d>\n", int(FboObj->mFBOMaster), inumtargets );

	glBindFramebuffer(GL_FRAMEBUFFER, FboObj->mFBOMaster );
	glBindRenderbuffer(GL_RENDERBUFFER, FboObj->mDSBO );
	//glDrawBuffers( inumtargets, buffers );

	//////////////////////////////////////////

	PushViewport( SRect(0,0, Base->GetW(), Base->GetH()) );
	PushScissor( SRect(0,0, Base->GetW(), Base->GetH()) );

	static const SRasterState defstate;
	mTarget.RSI()->BindRasterState( defstate, true );

	U32 ucolor = mcClearColor.GetRGBAU32();
	
	mCurrentRtGroup = Base;
	
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	GL_ERRORCHECK();
	glClearDepthf( 1.0f );
	glDepthRangef( 0.0, 1.0f );
	GL_ERRORCHECK();
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::PushScissor( const SRect &rScissorRect )
{
	SRect OldRect;
	OldRect.miX = miCurScissorX;
	OldRect.miY = miCurScissorY;
	OldRect.miX2 = miCurScissorX+miCurScissorW;
	OldRect.miY2 = miCurScissorY+miCurScissorH;
	OldRect.miW = miCurScissorW;
	OldRect.miH = miCurScissorH;
	
	maScissorStack[miScissorStackIndex] = OldRect;

	SetScissor( rScissorRect.miX, rScissorRect.miY, rScissorRect.miW, rScissorRect.miH );
	
	miScissorStackIndex++;
}

///////////////////////////////////////////////////////////////////////////////

SRect &GlFrameBufferInterface::PopScissor( void )
{
	miScissorStackIndex--;

	SRect &OldRect = maScissorStack[miScissorStackIndex];
	int W = OldRect.miX2 - OldRect.miX;
	int H = OldRect.miY2 - OldRect.miY;

	SetScissor( OldRect.miX, OldRect.miY, W, H );

	return OldRect;
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::SetScissor( int iX, int iY, int iW, int iH )
{
	iX = OrkSTXClampToRange( iX, 0, 2048 );
	iY = OrkSTXClampToRange( iY, 0, 2048 );
	iW = OrkSTXClampToRange( iW, 24, 2048 );
	iH = OrkSTXClampToRange( iH, 24, 2048 );

	//printf( "SetScissor<%d %d %d %d>\n", iX, iY, iW, iH );
	glScissor( iX, iY, iW, iH );
	glEnable( GL_SCISSOR_TEST );

	GL_ERRORCHECK();

	miCurScissorX = iX;
	miCurScissorY = iY;
	miCurScissorW = iW;
	miCurScissorH = iH;

}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::SetViewport( int iX, int iY, int iW, int iH )
{
	iX = OrkSTXClampToRange( iX, 0, 4096 );
	iY = OrkSTXClampToRange( iY, 0, 4096 );
	iW = OrkSTXClampToRange( iW, 8, 4096 );
	iH = OrkSTXClampToRange( iH, 8, 4096 );

	miCurVPX = iX;
	miCurVPY = iY;
	miCurVPW = iW;
	miCurVPH = iH;
	//printf( "SetViewport<%d %d %d %d>\n", iX, iY, iW, iH );


	glViewport( iX, iY, iW, iH );
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::ClearViewport( CUIViewport *pVP )
{
	const CColor3 &rCol = pVP->GetClearColorRef();

	if( IsPickState() )
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	else
		glClearColor( rCol.GetX(), rCol.GetY(), rCol.GetZ(), 1.0f );

	//printf( "GlFrameBufferInterface::ClearViewport()\n" );
	GL_ERRORCHECK();
	glClearDepthf( 1.0f );
	GL_ERRORCHECK();
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GfxTargetGL::SetSize( int ix, int iy, int iw, int ih )
{
	miX=ix;
	miY=iy;
	miW=iw;
	miH=ih;
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::AttachViewport( CUIViewport *pVP )
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

	//printf( "AttachViewport<%d %d %d %d>\n", vpx, vpy, vpw, vph );

	SetScissor( vpx, vpy, vpw, vph );
	SetViewport( vpx, vpy, vpw, vph );
}

void GlFrameBufferInterface::ForceFlush( void )
{
	glFlush();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::GetPixel( const CVector4 &rAt, GetPixelContext& ctx )
{
    CColor4 Color( 0.0f,0.0f,0.0f,0.0f );
  
	int sx = int((rAt.GetX()) * CReal(mTarget.GetW()));
	int sy = int((rAt.GetY()) * CReal(mTarget.GetH()));

	bool bInBounds = ( (sx<mTarget.GetW()) && (sy<mTarget.GetH()) && (sx>0) && (sy>0) );

	//printf( "InBounds<%d>\n", int(bInBounds) );
	
	if( IsOffscreenTarget() && bInBounds )
	{
		if( ctx.mRtGroup )
		{
			FBOObject *FboObj = (FBOObject *) ctx.mRtGroup->GetInternalHandle();

			if( FboObj )
			{
				GL_ERRORCHECK();
				glBindFramebuffer(GL_FRAMEBUFFER, FboObj->mFBOMaster );
				GL_ERRORCHECK();

				//printf( "GetPix BindFBO<%d>\n", FboObj->mFBOMaster );
				
				if( FboObj->mFBOMaster )
				{
					int MrtMask = ctx.miMrtMask;

					GLint readbuffer = 0;
					glGetIntegerv( GL_READ_BUFFER, & readbuffer );
					
					for( int MrtIndex=0; MrtIndex<4; MrtIndex++ )
					{
						int MrtTest = 1<<MrtIndex;
						
						ctx.mPickColors[MrtIndex] = CColor4(0.0f,0.0f,0.0f,0.0f);

						if( MrtTest&MrtMask )
						{

							//printf( "MrtIndex<%d>\n", MrtIndex );

							OrkAssert( MrtIndex<ctx.mRtGroup->GetNumTargets() );

							GL_ERRORCHECK();
							glDepthMask( GL_TRUE );
							GL_ERRORCHECK();
							int Rx = sx;
							int Ry = miCurVPH-sy;
							GL_ERRORCHECK();
							//glReadBuffer( GL_COLOR_ATTACHMENT0+MrtIndex );
							GL_ERRORCHECK();
							F32 rgba[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						
							glReadPixels( Rx, Ry, 1, 1, GL_RGBA, GL_FLOAT, (void*) rgba ); 
							GL_ERRORCHECK();
							//CVector4 rv = CVector4( rgba[0], rgba[1], rgba[2], rgba[3] );
							CVector4 rv = CVector4( rgba[0], rgba[1], rgba[2], rgba[3] );
							ctx.mPickColors[MrtIndex] = rv;

							//printf( "MrtIndex<%d> rx<%d> ry<%d> %f %f %f %f\n", MrtIndex, Rx, Ry, rv.GetX(), rv.GetY(), rv.GetZ(), rv.GetW() );
						}
					}
					GL_ERRORCHECK();
					glBindFramebuffer(GL_FRAMEBUFFER, 0 );
					glReadBuffer( readbuffer );
					GL_ERRORCHECK();
				}
				else
				{
					printf( "!!!ERR - GetPix BindFBO<%d>\n", FboObj->mFBOMaster );
				}
			}
		}
		else
		{
		}
	}
	else if( bInBounds )
	{
		ctx.mPickColors[0] = Color;
	}
}

///////////////////////////////////////////////////////////////////////////////

FBOObject::FBOObject()
{
	for( int i=0; i<kmaxrt; i++ )
	{
		mTEX[i] = 0;
	}
	mDSBO = 0;
	mFBOMaster=0;
}

///////////////////////////////////////////////////////////////////////////////

} } //namespace ork::lev2

#endif
