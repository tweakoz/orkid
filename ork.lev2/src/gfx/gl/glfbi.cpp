////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/ui/viewport.h>

#include <ork/lev2/gfx/dbgfontman.h>

#define USE_OIIO

#if defined(USE_OIIO)

#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING
#endif


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
	GL_ERRORCHECK();
	mTargetGL.MakeCurrentContext();
	GL_ERRORCHECK();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	GL_ERRORCHECK();
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	GL_ERRORCHECK();
	//glDrawBuffer( GL_FRONT );
	//glDrawBuffer( GL_BACK );
	GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::DoBeginFrame( void )
{
	mTargetGL.MakeCurrentContext();
	//glFinish();

	GL_ERRORCHECK();


	if( mTargetGL.FBI()->GetRtGroup() )
	{
		glDepthRange( 0.0, 1.0f );
		float fx = 0.0f; //mTargetGL.FBI()->GetRtGroup()->GetX();
		float fy = 0.0f; //mTargetGL.FBI()->GetRtGroup()->GetY();
		float fw = mTargetGL.FBI()->GetRtGroup()->GetW();
		float fh = mTargetGL.FBI()->GetRtGroup()->GetH();
		//printf( "RTGroup begin x<%f> y<%f> w<%f> h<%f>\n", fx, fy, fw, fh );
		SRect extents( fx,fy,fw,fh );
		//SRect extents( mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH() );
		PushViewport(extents);
		PushScissor(extents);
		//printf( "BEGINFRAME<RtGroup>\n" );
	}
	////////////////////////////////
	else if( IsOffscreenTarget() )
	////////////////////////////////
	{
		glDepthRange( 0.0, 1.0f );
		SRect extents( mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH() );
		//printf( "OST begin x<%d> y<%d> w<%d> h<%d>\n", mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH() );
		PushViewport(extents);
		PushScissor(extents);
	}
	/////////////////////////////////////////////////
	else // window (On Screen Target)
	/////////////////////////////////////////////////
	{
		GL_ERRORCHECK();
		SetAsRenderTarget();
		GL_ERRORCHECK();
		
		glDepthRange( 0.0, 1.0f );
		SRect extents( mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH() );
		//printf( "WINtarg begin x<%d> y<%d> w<%d> h<%d>\n", mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH() );
		PushViewport(extents);
		PushScissor(extents);
		//printf( "BEGINFRAME<WIN> w<%d> h<%d>\n", mTarget.GetW(), mTarget.GetH() );
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
			glClearDepth( 1.0f );
			GL_ERRORCHECK();
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			GL_ERRORCHECK();
		}
	}
		
	/////////////////////////////////////////////////
	// Set Initial Rendering States
				GL_ERRORCHECK();

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

	//glFinish();
	
	////////////////////////////////
	auto rtg = mTargetGL.FBI()->GetRtGroup();
	
	if( rtg )
	{
		GlFboObject *FboObj = (GlFboObject *) rtg->GetInternalHandle();
		int inumtargets = rtg->GetNumTargets();

		for( int it=0; it<inumtargets; it++ )
		{
			auto b = rtg->GetMrt(it);

			if( b && b->mComputeMips )
			{
				auto tex_obj = FboObj->mTEX[it];

				printf( "GENMIPS texobj<%d>\n", tex_obj );

				//glBindTexture( GL_TEXTURE_2D, tex_obj );
				//glGenerateMipmap( GL_TEXTURE_2D );
			}

		}

		//printf( "ENDFRAME<RtGroup>\n" );
	}
	else if( IsOffscreenTarget() )
	{
		//printf( "ENDFRAME<OST>\n" );
		GfxBuffer* pbuf = GetThisBuffer();
		pbuf->GetTexture()->SetDirty(false);
		pbuf->SetDirty(false);
		//mTargetGL.EndContextFBO();
	}
	else
	{
		glFinish();
		mTargetGL.SwapGLContext(mTargetGL.GetCtxBase());
	}
	////////////////////////////////
	PopViewport();
	PopScissor();
	GL_ERRORCHECK();
	////////////////////////////////
	glBindTexture( GL_TEXTURE_2D, 0 );
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::InitializeContext( GfxBuffer* pBuf )
{
	///////////////////////////////////////////
	// create texture surface

	//D3DFORMAT efmt = D3DFMT_A8R8G8B8;
	int ibytesperpix = 0;

	bool Zonly = false;

	switch( pBuf->GetBufferFormat() )
	{
		case EBUFFMT_RGBA32:
			//efmt = D3DFMT_A8R8G8B8;
			ibytesperpix = 4;
			break;
		case EBUFFMT_RGBA64:
			//efmt = D3DFMT_A16B16G16R16F;
			ibytesperpix = 8;
			break;
		case EBUFFMT_RGBA128:
			//efmt = D3DFMT_A32B32G32R32F;
			ibytesperpix = 16;
			break;
		case EBUFFMT_Z16:
			//efmt = D3DFMT_R16F;
			ibytesperpix = 2;
			Zonly=true;
			break;
		case EBUFFMT_Z32:
			//efmt = D3DFMT_R32F;
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
	//ptexture->SetBytesPerPixel( ibytesperpix );
	ptexture->SetTexClass( ork::lev2::Texture::ETEXCLASS_RENDERTARGET );

	SetBufferTexture(ptexture);

	///////////////////////////////////////////
	// create material

	pBuf->SetTexture( ptexture );

	///////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::SetRtGroup( RtGroup* Base )
{	
	mTargetGL.MakeCurrentContext();
	
	if( 0 == Base )
	{
		if( mCurrentRtGroup )
		{
			GlFboObject *FboObj = (GlFboObject *) mCurrentRtGroup->GetInternalHandle();
			int inumtargets = mCurrentRtGroup->GetNumTargets();

			for( int it=0; it<inumtargets; it++ )
			{
				auto b = mCurrentRtGroup->GetMrt(it);

				if( FboObj && b && b->mComputeMips )
				{
					auto tex_obj = FboObj->mTEX[it];

					//printf( "GENMIPS texobj<%p>\n", (void*) tex_obj );

					//glBindTexture( GL_TEXTURE_2D, tex_obj );
					//glGenerateMipmap( GL_TEXTURE_2D );
				}

			}

		}


		//printf( "SetRtg::disable rt\n" );

		////////////////////////////////////////////////
		// disable mrt
		//  pop viewport/scissor that was pushed by SetRtGroup( nonzero )
		// on xbox, happens after resolve
		////////////////////////////////////////////////
		SetAsRenderTarget();
		mCurrentRtGroup = 0;
		return;
	}
	
	//////////////////////////////////////////////////
	// lazy create mrt's
	//////////////////////////////////////////////////

	int iw = Base->GetW();
	int ih = Base->GetH();

	iw = (iw<16) ? 16 : iw;
	ih = (ih<16) ? 16 : ih;

	GlFboObject *FboObj = (GlFboObject *) Base->GetInternalHandle();

	int inumtargets = Base->GetNumTargets();

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
	
	GL_ERRORCHECK();

	if( 0 == FboObj )
	{
		FboObj = new GlFboObject;

		Base->SetInternalHandle( FboObj );

		GL_ERRORCHECK();
		glGenRenderbuffers( 1, & FboObj->mDSBO );
		GL_ERRORCHECK();
		glGenFramebuffers ( 1, & FboObj->mFBOMaster );
		GL_ERRORCHECK();
		//printf( "GenFBO<%d>\n", int(FboObj->mFBOMaster) );

		//////////////////////////////////////////

		for( int it=0; it<inumtargets; it++ )
		{
			RtBuffer* pB = Base->GetMrt(it);
			pB->SetSizeDirty(true);
			//////////////////////////////////////////
			Texture* ptex = new Texture;
			GLTextureObject* ptexOBJ = new GLTextureObject;
			GL_ERRORCHECK();
			glGenTextures( 1, (GLuint *) & FboObj->mTEX[it] );
			GL_ERRORCHECK();
			ptexOBJ->mObject = FboObj->mTEX[it];
			//////////////////////////////////////////
			ptex->SetWidth( iw );
			ptex->SetHeight( ih );
			ptex->SetTexIH( (void*) ptexOBJ );
			ptex->SetTexClass( ork::lev2::Texture::ETEXCLASS_RENDERTARGET );
			mTargetGL.TXI()->ApplySamplingMode(ptex);
			pB->SetTexture(ptex);
			//////////////////////////////////////////
			ork::lev2::GfxMaterialUITextured* pmtl = new ork::lev2::GfxMaterialUITextured(&mTarget);
			pmtl->SetTexture( ETEXDEST_DIFFUSE, ptex );
			pB->SetMaterial( pmtl );
			//////////////////////////////////////////
		}
		Base->SetSizeDirty(true);
	}
	GL_ERRORCHECK();

	if( Base->IsSizeDirty() )
	{
		//////////////////////////////////////////
		// initialize depth renderbuffer
		GL_ERRORCHECK();
		glBindRenderbuffer(GL_RENDERBUFFER, FboObj->mDSBO );
		GL_ERRORCHECK();
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, iw, ih );
		GL_ERRORCHECK();
		
		//glRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER_EXT, 1, GL_DEPTH_COMPONENT24, iw, ih );
		
		//////
		// attach it to the FBO
		//////
		glBindFramebuffer(GL_FRAMEBUFFER, FboObj->mFBOMaster );
		GL_ERRORCHECK();
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, FboObj->mDSBO );
		GL_ERRORCHECK();

		//////
		for( int it=0; it<inumtargets; it++ )
		{
			RtBuffer* pB = Base->GetMrt(it);
			//D3DFORMAT efmt = D3DFMT_A8R8G8B8;
			GLuint glinternalformat = 0;
			GLuint glformat = GL_RGBA;
			GLenum gltype = 0;

			switch( pB->GetBufferFormat() )
			{
				case EBUFFMT_RGBA32: 
					glinternalformat = GL_RGBA8;
					gltype = GL_UNSIGNED_BYTE;
					break;
				case EBUFFMT_RGBA64: 
					glinternalformat = GL_RGBA16F;
					gltype = GL_HALF_FLOAT;
					break;
				default:
					OrkAssert(false);
					break;
				//case EBUFFMT_RGBA128: glinternalformat = GL_RGBA32; break;
			}
			OrkAssert(glinternalformat!=0);

			//////////////////////////////////////////
			// initialize texture
			//////////////////////////////////////////

			glBindTexture(GL_TEXTURE_2D, FboObj->mTEX[it] );
			GL_ERRORCHECK();
			glTexImage2D(GL_TEXTURE_2D, 0, glinternalformat, iw, ih, 0, glformat, gltype, NULL);
			GL_ERRORCHECK();

			//printf( "SetRtg::gentex<%d> w<%d> h<%d>\n", int(FboObj->mTEX[it]), iw,ih );

			//////////////////////////////////////////
			//////////////////////////////////////////
			// attach texture to framebuffercolor buffer

			glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+it, GL_TEXTURE_2D, FboObj->mTEX[it], 0);
			GL_ERRORCHECK();

			pB->SetSizeDirty(false);
		}
		Base->SetSizeDirty(false);
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		OrkAssert( status == GL_FRAMEBUFFER_COMPLETE );
	}
				GL_ERRORCHECK();

	//////////////////////////////////////////////////
	// enable mrts
	//////////////////////////////////////////////////

	GLenum buffers[] = {	GL_COLOR_ATTACHMENT0, 
							GL_COLOR_ATTACHMENT1,
							GL_COLOR_ATTACHMENT2,
							GL_COLOR_ATTACHMENT3 
					   };

	//printf( "SetRtg::BindFBO<%d> numattachments<%d>\n", int(FboObj->mFBOMaster), inumtargets );


	glBindFramebuffer(GL_FRAMEBUFFER, FboObj->mFBOMaster );
	glBindRenderbuffer(GL_RENDERBUFFER, FboObj->mDSBO );
	glDrawBuffers( inumtargets, buffers );
				GL_ERRORCHECK();

	//////////////////////////////////////////

	static const SRasterState defstate;
	mTarget.RSI()->BindRasterState( defstate, true );
	
	mCurrentRtGroup = Base;
	
	if( GetAutoClear() )
	{
		glClearColor( mcClearColor.GetX(), mcClearColor.GetY(), mcClearColor.GetZ(), mcClearColor.GetW() );
		//glClearColor( 1.0f,1.0f,0.0f,1.0f );
		GL_ERRORCHECK();
		glClearDepth( 1.0f );
		glDepthRange( 0.0, 1.0f );
		GL_ERRORCHECK();
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		GL_ERRORCHECK();
	}
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
					GL_ERRORCHECK();

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
				GL_ERRORCHECK();

	return OldRect;
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::SetScissor( int iX, int iY, int iW, int iH )
{
	iX = OrkSTXClampToRange( iX, 0, 8192 );
	iY = OrkSTXClampToRange( iY, 0, 8192 );
	iW = OrkSTXClampToRange( iW, 24, 8192 );
	iH = OrkSTXClampToRange( iH, 24, 8192 );

	//printf( "SetScissor<%d %d %d %d>\n", iX, iY, iW, iH );
	GL_ERRORCHECK();
	glScissor( iX, iY, iW, iH );
	GL_ERRORCHECK();
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
	iX = OrkSTXClampToRange( iX, 0, 8192 );
	iY = OrkSTXClampToRange( iY, 0, 8192 );
	iW = OrkSTXClampToRange( iW, 32, 8192 );
	iH = OrkSTXClampToRange( iH, 32, 8192 );

	miCurVPX = iX;
	miCurVPY = iY;
	miCurVPW = iW;
	miCurVPH = iH;
	//printf( "SetViewport<%d %d %d %d>\n", iX, iY, iW, iH );


	GL_ERRORCHECK();
	glViewport( iX, iY, iW, iH );
	GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::Clear( const CColor4 &color, float fdepth )
{
	if( IsPickState() )
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	else
		glClearColor( color.GetX(), color.GetY(), color.GetZ(), color.GetW() );

	//printf( "GlFrameBufferInterface::ClearViewport()\n" );
	GL_ERRORCHECK();
	glClearDepth( fdepth );
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
	mTargetDrawableSizeDirty = true;
	//mFbI.DeviceReset(ix,iy,iw,ih );
}

void GlFrameBufferInterface::ForceFlush( void )
{
	GL_ERRORCHECK();
	//glFlush();
	GL_ERRORCHECK();
}

void GlFrameBufferInterface::Capture( const RtGroup& rtg, int irt, const file::Path& pth )
{
	auto FboObj = (GlFboObject *) rtg.GetInternalHandle();

	if( 	(nullptr == FboObj)
		 || (irt>=rtg.GetNumTargets()) )
		return;

	auto tex_id = FboObj->mTEX[irt];

	int iw = rtg.GetW();
	int ih = rtg.GetH();
	RtBuffer* rtb = rtg.GetMrt(irt);

	printf( "pth<%s> BUFW<%d> BUF<%d>\n", pth.c_str(), iw, ih );

	uint8_t* pu8 = (uint8_t*) malloc(iw*ih*4);

	GL_ERRORCHECK();
	//glFlush();
	GL_ERRORCHECK();
	glBindTexture(GL_TEXTURE_2D, tex_id);
	GL_ERRORCHECK();
	//glReadPixels( 0, 0, iw, ih, GL_RGBA, GL_UNSIGNED_BYTE, (void*) pu8 ); 
#if defined(DARWIN)		
	glGetTexImage(	GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*) pu8 );
#else
	glGetTexImage(	GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, (void*) pu8 );
#endif

	GL_ERRORCHECK();
	glBindTexture(GL_TEXTURE_2D, 0);
	//glBindFramebuffer(GL_FRAMEBUFFER, 0 );
	GL_ERRORCHECK();

	for( int ipix=0; ipix<(iw*ih); ipix++ )
	{
		int ibyt = ipix*4;
		uint8_t c0 = pu8[ibyt+0];
		uint8_t c1 = pu8[ibyt+1];
		uint8_t c2 = pu8[ibyt+2];
		uint8_t c3 = pu8[ibyt+3];
		// c0 c1 c2

#if defined(DARWIN)		
		pu8[ibyt+0] = c0; // A
		pu8[ibyt+1] = c1;
		pu8[ibyt+2] = c2;
		pu8[ibyt+3] = c3;
#else
		pu8[ibyt+0] = c2; 
		pu8[ibyt+1] = c1;
		pu8[ibyt+2] = c0;
		pu8[ibyt+3] = c3; // A
#endif
	}

#if defined(USE_OIIO)
	auto out = ImageOutput::create (pth.c_str());
	if (! out)
		return;
	ImageSpec spec (iw, ih, 4, TypeDesc::UINT8);
	out->open (pth.c_str(), spec);
	out->write_image( TypeDesc::UINT8, pu8);
	out->close();

	free((void*)pu8);
#endif

}
/*void GlFrameBufferInterface::Capture( GfxBuffer& inpbuf, CaptureBuffer& buffer )
{

}*/
///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::GetPixel( const CVector4 &rAt, GetPixelContext& ctx )
{
    CColor4 Color( 0.0f,0.0f,0.0f,0.0f );
  
	int sx = int((rAt.GetX()) * CReal(mTarget.GetW()));
	int sy = int((1.0f-rAt.GetY()) * CReal(mTarget.GetH()));

	bool bInBounds = ( (sx<mTarget.GetW()) && (sy<mTarget.GetH()) && (sx>0) && (sy>0) );

	//printf( "InBounds<%d> sx<%d> sy<%d>\n", int(bInBounds), sx, sy );
	
	if( IsOffscreenTarget() && bInBounds )
	{
		if( ctx.mRtGroup )
		{
			GlFboObject *FboObj = (GlFboObject *) ctx.mRtGroup->GetInternalHandle();

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
					GL_ERRORCHECK();
					glGetIntegerv( GL_READ_BUFFER, & readbuffer );
					GL_ERRORCHECK();

					//printf( "readbuf<%d>\n", int(readbuffer) );

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
							GL_ERRORCHECK();
							glReadBuffer( GL_COLOR_ATTACHMENT0+MrtIndex );
							GL_ERRORCHECK();

							/*if( true )
							{
								int inumpix = miCurVPW*miCurVPH;

								glReadPixels( 0, 0, miCurVPW, miCurVPH, GL_RGBA, GL_FLOAT, (void*) rgba ); 
							}*/


							F32 rgba[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						
							glReadPixels( sx, sy, 1, 1, GL_RGBA, GL_FLOAT, (void*) rgba ); 
							GL_ERRORCHECK();
							//CVector4 rv = CVector4( rgba[0], rgba[1], rgba[2], rgba[3] );
							CVector4 rv = CVector4( rgba[0], rgba[1], rgba[2], rgba[3] );
							ctx.mPickColors[MrtIndex] = rv;

							//printf( "getpix MrtIndex<%d> rx<%d> ry<%d> %f %f %f %f\n", MrtIndex, sx, sy, rv.GetX(), rv.GetY(), rv.GetZ(), rv.GetW() );
						}
					}
					GL_ERRORCHECK();
					glBindFramebuffer(GL_FRAMEBUFFER, 0 );
					//glReadBuffer( readbuffer );
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

GlFboObject::GlFboObject()
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
