////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#if defined( ORK_CONFIG_OPENGL )
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/dxt.h>
#include <ork/lev2/ui/ui.h>
#include <ork/file/file.h>
//#include <OpenGLES/glu.h>

namespace ork { namespace lev2 {
extern std::vector<ork::Objc::Object*> gWindowContexts;
ork::Objc::Object* MainCtx() { return ork::lev2::gWindowContexts[1]; }

///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::DestroyTexture( Texture* ptex )
{
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::TexManInit( void )
{

}

///////////////////////////////////////////////////////////////////////////////

PboSet::PboSet( int isize )
	: miCurIndex( 0 )
{
	static int kPBOBASE = 0x1234beef;
	for( int i=0; i<knumpbos; i++ )
	{
		mPBOS[i] = kPBOBASE++;
		//glBindBuffer( GL_PIXEL_UNPACK_BUFFER_OES, mPBOS[i] );
		//glBufferData( GL_PIXEL_UNPACK_BUFFER_ARB, isize, NULL, GL_STREAM_DRAW );
		//glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );
	}
}
PboSet::~PboSet()
{
}
GLuint PboSet::Get()
{
	GLuint rval = mPBOS[miCurIndex];
	miCurIndex = (miCurIndex+1)%knumpbos;
	return rval;
}
GLuint GlTextureInterface::GetPBO( int isize )
{
	PboSet* pbs = 0;
	std::map<int,PboSet*>::iterator it=mPBOSets.find(isize);
	if( it==mPBOSets.end() )
	{
		pbs = new PboSet( isize );
		mPBOSets[isize] = pbs;
	}
	else
	{
		pbs = it->second;
	}
	return pbs->Get();
}

///////////////////////////////////////////////////////////////////////////////

struct TexSetter
{
	static const GLuint PBOOBJBASE = 0x12340000;

	static void Set2D(GlTextureInterface* txi, GLuint numC, GLuint fmt, GLuint typ, GLuint tgt, int BPP, int inummips, int& iw, int& ih, CFile& file ) //, int& irdptr, const u8* dataBASE )
	{	
		size_t ifilelen = 0;
		EFileErrCode eFileErr = file.GetLength( ifilelen );
	
		//const u8* pimgdata = & dataBASE[irdptr];
		int isize = iw*ih*BPP;
		for( int imip=0; imip<inummips; imip++ )
		{			
			/////////////////////////////////////////////////
			// allocate space for image
 			// see http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Board=3&Number=159972
			// and http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=240547
			// basically you can call glTexImage2D once with the s3tc format as the internalformat
			//  and a null data ptr to let the driver 'allocate' space for the texture
			//  then use the glCompressedTexSubImage2D to overwrite the data in the pre allocated space
			//  this decouples allocation from writing, allowing you to overwrite more efficiently
			/////////////////////////////////////////////////
					
			glTexImage2D(	tgt,
							imip,
							numC,
							iw, ih,
							0,
							fmt,
							typ,
							0 );
							
			/////////////////////////////
			// imgdata->PBO
			/////////////////////////////

			printf( "UPDATE IMAGE UNC imip<%d> iw<%d> ih<%d> isiz<%d>\n", imip, iw, ih, isize );

			const GLuint PBOOBJ = txi->GetPBO( isize );
			
			//glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, PBOOBJ );
			//void* pgfxmem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY );
			//file.Read( pgfxmem, isize );
			//glUnmapBufferOES(GL_PIXEL_UNPACK_BUFFER_ARB);

			////////////////////////
			// PBO->texture
			////////////////////////

			glTexSubImage2D(	tgt,
								imip,
								0, 0,
								iw, ih,
								fmt,
								typ,
								0 );

			////////////////////////
			// unbind the PBO
			////////////////////////

			//glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );

			////////////////////////

			//irdptr+=isize;
			//pimgdata = & dataBASE[irdptr];

			iw>>=1;
			ih>>=1;
			isize = iw*ih*BPP;
		}
	}
	static void Set3D(GlTextureInterface* txi, GLuint numC, GLuint fmt, GLuint typ, GLuint tgt, int BPP, int inummips, int& iw, int& ih, int& id, CFile& file ) //, int& irdptr, const u8* dataBASE )
	{	for( int imip=0; imip<inummips; imip++ )
		{
			/////////////////////////////////////////////////
			// allocate space for image
 			// see http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Board=3&Number=159972
			// and http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=240547
			// basically you can call glTexImage2D once with the s3tc format as the internalformat
			//  and a null data ptr to let the driver 'allocate' space for the texture
			//  then use the glCompressedTexSubImage2D to overwrite the data in the pre allocated space
			//  this decouples allocation from writing, allowing you to overwrite more efficiently
			/////////////////////////////////////////////////

			//glTexImage3D(	tgt,
			//				imip,
			//				numC,
			//				iw, ih, id,
			//				0,
			//				fmt,
			//				typ,
			//				0 );

			/////////////////////////////
			// imgdata->PBO
			/////////////////////////////

			int isize = id*iw*ih*BPP;

			printf( "UPDATE IMAGE 3dUNC imip<%d> iw<%d> ih<%d> id<%d> isiz<%d>\n", imip, iw, ih, id, isize );

			const GLuint PBOOBJ = txi->GetPBO( isize );
			
			//glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, PBOOBJ );
			//void* pgfxmem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY );
			//file.Read( pgfxmem, isize );
			//glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);

			////////////////////////
			// PBO->texture
			////////////////////////
			
			//glTexSubImage3D(	tgt,
			//					imip,
			//					0,0,0,
			//					iw, ih, id,
			//					fmt,
			//					typ,
			//					0 );

			////////////////////////
			// unbind the PBO
			////////////////////////

			//glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );

			/////////////////////////////

			iw>>=1;
			ih>>=1;
			id>>=1;
			//irdptr+=isize;
		}
	}
	static void Set2DC(GlTextureInterface* txi, GLuint fmt, GLuint tgt, int BPP, int inummips, int& iw, int& ih, CFile& file ) //, int& irdptr, const u8* dataBASE )
	{	for( int imip=0; imip<inummips; imip++ )
		{	int iBwidth = (iw+3)/4;
			int iBheight = (ih+3)/4;
			int isize = (iBwidth*iBheight) * BPP;
			//const u8* pimgdata = & dataBASE[irdptr];
			//irdptr+=isize;
			
			
			/////////////////////////////////////////////////
			// allocate space for image
 			// see http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Board=3&Number=159972
			// and http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=240547
			// basically you can call glTexImage2D once with the s3tc format as the internalformat
			//  and a null data ptr to let the driver 'allocate' space for the texture
			//  then use the glCompressedTexSubImage2D to overwrite the data in the pre allocated space
			//  this decouples allocation from writing, allowing you to overwrite more efficiently
			/////////////////////////////////////////////////
			
			/*bool hasalpha = (fmt==GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)||(fmt==GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
			GLenum extfmt = hasalpha ? GL_RGBA : GL_RGB;

			printf( "ALLOCATE IMAGE S3TC\n" );

			glTexImage2D( tgt, imip, fmt, iw, ih, 0, extfmt, GL_UNSIGNED_BYTE, NULL );
			
			/////////////////////////////
			// imgdata->PBO
			/////////////////////////////

			printf( "UPDATE IMAGE  S3TC\n" );

			const GLuint PBOOBJ = txi->GetPBO( isize );
			
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, PBOOBJ );
			void* pgfxmem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY );
			file.Read( pgfxmem, isize );
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);

			////////////////////////
			// PBO->texture
			////////////////////////
			
			glCompressedTexSubImage2D(	tgt,
										imip,
										0,0,
										iw, ih,
										fmt,
//										0,
										isize,
										0 );

			////////////////////////
			// unbind the PBO
			////////////////////////

			glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );

			////////////////////////
            */

			iw>>=1;
			ih>>=1;
		}
	}
	static void Set3DC(GlTextureInterface* txi, GLuint fmt, GLuint tgt, int BPP, int inummips, int& iw, int& ih, int& id, CFile& file ) //, int& irdptr, const u8* dataBASE )
	{	for( int imip=0; imip<inummips; imip++ )
		{	
			int iBwidth = (iw+3)/4;
			int iBheight = (ih+3)/4;
			int isize = id*(iBwidth*iBheight) * BPP;
			//const u8* pimgdata = & dataBASE[irdptr];
			printf( "READ3DT iw<%d> ih<%d> id<%d> isize<%d>\n", iw, ih, id, isize );
			//irdptr+=isize;

			if( isize )
			{
				//glCompressedTexImage3D(	tgt,
				//						imip,
				//						fmt,
				//						iw, ih, id,
				//						0,
				//						isize,
				//						pimgdata );
				//GL_ERRORCHECK();
			}
			iw>>=1;
			ih>>=1;
			id>>=1;
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::LoadTexture( const AssetPath& infname, Texture *ptex )
{
	///////////////////////////////////////////////
	AssetPath DdsFilename = infname;
	AssetPath VdsFilename = infname;
	DdsFilename.SetExtension( "dds" );
	VdsFilename.SetExtension( "vds" );
	bool bDDSPRESENT = CFileEnv::GetRef().DoesFileExist( DdsFilename );
	bool bVDSPRESENT = CFileEnv::GetRef().DoesFileExist( VdsFilename );

	if( bVDSPRESENT )
		return LoadVDSTexture( VdsFilename, ptex );
	else if( bDDSPRESENT )
		return LoadDDSTexture( DdsFilename, ptex );
	else
		return false;
}

///////////////////////////////////////////////////////////////////////////////
// VDS texture animation

// use a cache-bank of pbo's and texobj's
//  lru or stochastic replacement
//  load slices of a UNC/S3TC DDS non mipped volume texture as frames.
//   this enables random access since s3tc is block compressed.
//  DDS has shared header and seeking to a specific frame is simple.

// 1. psys requests frame (from TexAnim/TexAnimInst)
// 2. vds texanimation class checks the image cache.
//      it can be:
//      A. present in texobj, nothing needs to be done.
//      B. on disk only, allocate a PBO from the cache manager,
//           read texture from disk into the PBO, update the cache info
//           allocate a texture object from cache manager
//            if texobj cache empty, use lru or stochastic cache spill
//           upload from PBO to allocated texture using texsubimage call with PBO bound
//           return the PBO to the cache manager
//		C. perhaps check texanim frame index velocity, if it is relatively constant,
//          then batching multiple cache fills may be advantageous (and may lower effective latency)
///////////////////////////////////////////////////////////////////////////////

void PushIdentity()
{
	//glMatrixMode(GL_PROJECTION);
	//glPushMatrix();
	//glLoadIdentity();
	//glMatrixMode(GL_MODELVIEW);
	//glPushMatrix();
	//glLoadIdentity();
}
void PopIdentity()
{
	//glMatrixMode(GL_MODELVIEW);
	//glPopMatrix();
	//glMatrixMode(GL_PROJECTION);
	//glPopMatrix();
	//glMatrixMode(GL_MODELVIEW);
}

/*void QuartzComposerTextureAnimation::UpdateQtzFBO(GLTextureObject&glto,float ftime)
{
	QCRenderer* prenderer = (QCRenderer*) mQCRenderer.mInstance;
	NSOpenGLContext* poglctx = MainCtx()->mInstance;

	GLenum buffers[] =	{	GL_COLOR_ATTACHMENT0_EXT, 
						};

	NSOpenGLContext* prevCTX = [NSOpenGLContext currentContext];
	[poglctx makeCurrentContext];
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, glto.mFbo );
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, glto.mDbo );
	glDrawBuffers( 1, buffers );
	{
		PushIdentity();
		glPushAttrib( GL_ALL_ATTRIB_BITS );
		glViewport( 0,0, 512, 512 );
		glScissor( 0,0, 512, 512 );
		{
			glClearColor( 1.0f,0.0f,0.0f,1.0f);
			glClearDepth( 1.0f );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			glColor3f( 1.0f,1.0f, 0.0f );
			glDisable(GL_LIGHTING);
			glDisable(GL_ALPHA_TEST);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_BLEND);
			//DrawCross( 0.5f );
			BOOL bOK = [prenderer renderAtTime:double(ftime) arguments:nil];
			OrkAssert( bOK );
			//[prenderer description];
			[poglctx flushBuffer];
		}
		PopIdentity();
		glPopAttrib();

	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
	glDrawBuffer( GL_BACK );
	[prevCTX makeCurrentContext];

}*/

///////////////////////////////////////////////////////////////////////////////

/*
QuartzComposerTextureAnimation::QuartzComposerTextureAnimation(Objc::Object qcr)
	: mQCRenderer(qcr)
{
}
QuartzComposerTextureAnimation::~QuartzComposerTextureAnimation()
{
}
void QuartzComposerTextureAnimation::UpdateTexture( TextureInterface* txi, Texture* ptex, TextureAnimationInst* ptexanim ) // virtual
{
	GLTextureObject* pTEXOBJ = (GLTextureObject*) ptex->GetTexIH();
	float ftime = ptexanim->GetCurrentTime();
	if( pTEXOBJ && pTEXOBJ->mFbo )
        UpdateQtzFBO( *pTEXOBJ, ftime );
} 
float QuartzComposerTextureAnimation::GetLengthOfTime( void ) const // virtual 
{
	QCRenderer* prenderer = (QCRenderer*) mQCRenderer.mInstance;
	QCComposition* pcomposition = [prenderer composition];
	return 10.0f;
}

///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::LoadQTZTexture( const AssetPath& infname, Texture *ptex )
{
	GLTextureObject* pTEXOBJ = new GLTextureObject;
	ptex->SetTexIH( (void*) pTEXOBJ );
	QuartzComposerTextureAnimation* qcta = 0;

	int iW = 512;
	int iH = 512;
	
	file::Path abspath = infname.ToAbsolute();
	bool bQTZPRESENT = CFileEnv::GetRef().DoesFileExist( abspath );
	QCRenderer* pqcren = 0;
	if( bQTZPRESENT )
	{
		printf( "found qtz file<%s> \n", abspath.c_str() );
		NSString* PathToComp = [NSString stringWithUTF8String:abspath.c_str()];
		////////////////////////////////
		QCComposition* pcomp = [QCComposition compositionWithFile:PathToComp];
		////////////////////////////////
		NSSize size;
		size.width = float(iW);
		size.height = float(iH);
		CGColorSpaceRef csref = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
		pqcren = [	[QCRenderer alloc]
					initWithOpenGLContext:MainCtx()->mInstance
					pixelFormat:lev2::gNSPixelFormat->mInstance
					file:PathToComp];

		OrkAssert( pqcren!=nil );
					
		qcta = new QuartzComposerTextureAnimation( pqcren );
		
		ptex->SetTexAnim( qcta );
			
		//pTEXOBJ->mQCRenderer = pqcren;
	}

	//////////////////////////////////////////
	// gen RTT texture
	//////////////////////////////////////////

	glGenTextures(1, &pTEXOBJ->mObject);
	glBindTexture(GL_TEXTURE_2D, pTEXOBJ->mObject);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, iW, iH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	printf( "TEX<%d>\n", int(pTEXOBJ->mObject) );

	//////////////////////////////////////////
	// gen FBO and DBO
	//////////////////////////////////////////

	glGenFramebuffersEXT( 1, & pTEXOBJ->mFbo );
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, pTEXOBJ->mFbo);
	glGenRenderbuffersEXT(1, & pTEXOBJ->mDbo);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, pTEXOBJ->mDbo);

	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, iW, iH );
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, pTEXOBJ->mDbo);

	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, pTEXOBJ->mObject, 0);

	printf( "FBO<%d>\n", int(pTEXOBJ->mFbo) );
	printf( "DBO<%d>\n", int(pTEXOBJ->mDbo) );

	//////////////////////////////////////////
	// check for FBO completeness
	//////////////////////////////////////////

	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);

	OrkAssert( status == GL_FRAMEBUFFER_COMPLETE_EXT );
	
	if( qcta )
	{
		TextureAnimationInst tai( qcta );
		qcta->UpdateTexture( this, ptex, &tai );
	}
	return true;
}*/

void GlTextureInterface::UpdateAnimatedTexture( Texture *ptex, TextureAnimationInst* tai )
{
    //printf( "GlTextureInterface::UpdateAnimatedTexture( ptex<%p> tai<%p> )\n", ptex, tai );
	GLTextureObject* pTEXOBJ = (GLTextureObject*) ptex->GetTexIH();
	if( pTEXOBJ && ptex->GetTexAnim() )
	{
		ptex->GetTexAnim()->UpdateTexture( this, ptex, tai );
	}
}

///////////////////////////////////////////////////////////////////////////////
    
VdsTextureAnimation::VdsTextureAnimation( const AssetPath& pth )
{
    mPath = pth.c_str();
    printf( "Loading VDS<%s>\n", pth.c_str() );
	AssetPath Filename = pth;
	///////////////////////////////////////////////
	mpFile = new CFile( Filename, EFM_READ );
    mpFile->mbEnableBuffering = false;
	if( false == mpFile->IsOpen() )
	{
		return;
	}
	///////////////////////////////////////////////
	size_t ifilelen = 0;
	EFileErrCode eFileErr = mpFile->GetLength( ifilelen );
	U8* pdata = (U8*) malloc(ifilelen);
    miFileLength = int(ifilelen);
	OrkAssertI( pdata!=0, "out of memory ?" );
	eFileErr = mpFile->Read( pdata, sizeof( dxt::DDS_HEADER ) );
	mpDDSHEADER = (dxt::DDS_HEADER*) pdata;
    miFrameBaseOffset = sizeof( dxt::DDS_HEADER );
	////////////////////////////////////////////////////////////////////
	miW = mpDDSHEADER->dwWidth;
	miH = mpDDSHEADER->dwHeight;
	miNumFrames = (mpDDSHEADER->dwDepth>1) ? mpDDSHEADER->dwDepth : 1;
	////////////////////////////////////////////////////////////////////		
	int NumMips = (mpDDSHEADER->dwFlags & dxt::DDSD_MIPMAPCOUNT) ? mpDDSHEADER->dwMipMapCount : 1;
	int iwidth = mpDDSHEADER->dwWidth;
	int iheight = mpDDSHEADER->dwHeight;
	int idepth = mpDDSHEADER->dwDepth;
	//int ireadptr = sizeof( dxt::DDS_HEADER );
	////////////////////////////////////////////////////////////////////
	//printf( "  tex<%s> ptex<%p>\n", pth.c_str(), ptex );
	printf( "  tex<%s> width<%d>\n", pth.c_str(), iwidth );
	printf( "  tex<%s> height<%d>\n", pth.c_str(), iheight );
	printf( "  tex<%s> depth<%d>\n", pth.c_str(), idepth );
	bool bVOLUMETEX = (idepth>1);
    
    int iBwidth = (iwidth+3)/4;
    int iBheight = (iheight+3)/4;
    miFrameBaseSize = 0;
    
	if( dxt::IsBGRA8( mpDDSHEADER->ddspf ) )
	{
		const dxt::DdsLoadInfo & li = dxt::loadInfoBGRA8;
		miFrameBaseSize = iwidth*iheight*4;
    }
    else if( dxt::IsBGR8( mpDDSHEADER->ddspf ) )
    {
        const dxt::DdsLoadInfo & li = dxt::loadInfoBGR8;
		miFrameBaseSize = iwidth*iheight*3;
    }
    else if( dxt::IsDXT1( mpDDSHEADER->ddspf ) )
    {
		const dxt::DdsLoadInfo & li = dxt::loadInfoDXT1;
		miFrameBaseSize = (iBwidth*iBheight) * li.blockBytes;
    }
    else if( dxt::IsDXT3( mpDDSHEADER->ddspf ) )
    {
		const dxt::DdsLoadInfo & li = dxt::loadInfoDXT3;
		miFrameBaseSize = (iBwidth*iBheight) * li.blockBytes;
    }
    else if( dxt::IsDXT5( mpDDSHEADER->ddspf ) )
    {
		const dxt::DdsLoadInfo & li = dxt::loadInfoDXT5;
		miFrameBaseSize = (iBwidth*iBheight) * li.blockBytes;
    }
	////////////////////////////////////////////////////////////////////
    for( int i=0; i<kframecachesize; i++ )
    {
        void* pbuffer = malloc( miFrameBaseSize );
        mFrameBuffers[i] = pbuffer;
    }
    ////////////////////////////////////////////////////////////////////
}
VdsTextureAnimation::~VdsTextureAnimation()
{
    delete mpFile;
    for( int i=0; i<kframecachesize; i++ )
    {
        free( mFrameBuffers[i] );
        mFrameBuffers[i] = 0;
    }
}
void* VdsTextureAnimation::ReadFromFrameCache( int iframe, int isize )
{
    int icacheentry = 0;
    std::map<int,int>::iterator it=mFrameCache.find(iframe);
    if( it!=mFrameCache.end() ) // cache hit
    {
        icacheentry = it->second;
       // printf( "cachehit iframe<%d> entry<%d>\n", iframe, icacheentry );
    }
    else // cache miss
    {
        int inumincache = int(mFrameCache.size());
        icacheentry = inumincache;
        if( inumincache>=kframecachesize ) // cache full
        {
            std::map<int,int>::iterator item = mFrameCache.begin();
            std::advance( item, rand()%inumincache );
            int iframeevicted = item->first;
            int ientryevicted = item->second;
            mFrameCache.erase(item);
            icacheentry = ientryevicted;
        }
        void* pcachedest = mFrameBuffers[ icacheentry ];
        mpFile->Read( pcachedest, miFrameBaseSize );
        mFrameCache[iframe] = icacheentry;
        printf( "cachemiss vds<%s> iframe<%d> entry<%d>\n", mPath.c_str(), iframe, icacheentry );
    }
    void* pcachedest = mFrameBuffers[ icacheentry ];
    return pcachedest;
}

void VdsTextureAnimation::UpdateTexture( TextureInterface* txi, lev2::Texture* ptex, TextureAnimationInst* pinst )
{
	GLTextureObject* pTEXOBJ = (GLTextureObject*) ptex->GetTexIH();
    GlTextureInterface* pgltxi = (GlTextureInterface*) txi;
    float ftime = pinst->GetCurrentTime();
    float fps = 30.0f;
    
    int iframe = int(ftime*fps)%miNumFrames;
    int iseekpos = miFrameBaseOffset+(iframe*miFrameBaseSize);
    mpFile->SeekFromStart( iseekpos );
    
    //printf( "VdsTextureAnimation::UpdateTexture(ptex<%p> time<%f> seekpos<%d> readsiz<%d> fillen<%d> iframe<%d>)\n", ptex, pinst->GetCurrentTime(),iseekpos,miFrameBaseSize, miFileLength, iframe );

    void* pdata = ReadFromFrameCache( iframe, miFrameBaseSize );
    
    if( dxt::IsBGRA8( mpDDSHEADER->ddspf ) )
	{
        /////////////////////////////////////////////////
        // allocate space for image
        // see http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Board=3&Number=159972
        // and http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=240547
        // basically you can call glTexImage2D once with the s3tc format as the internalformat
        //  and a null data ptr to let the driver 'allocate' space for the texture
        //  then use the glCompressedTexSubImage2D to overwrite the data in the pre allocated space
        //  this decouples allocation from writing, allowing you to overwrite more efficiently
        /////////////////////////////////////////////////
        
        glBindTexture( GL_TEXTURE_2D, pTEXOBJ->mObject );
        
        /////////////////////////////
        // imgdata->PBO
        /////////////////////////////
        
       // printf( "UPDATE IMAGE UNC iw<%d> ih<%d> to<%d>\n", miW, miH, int(pTEXOBJ->mObject) );
        
        const GLuint PBOOBJ = pgltxi->GetPBO( miFrameBaseSize );
        
        //glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, PBOOBJ );
        //void* pgfxmem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY );
       // mpFile->Read( pgfxmem, miFrameBaseSize );
        //memcpy( pgfxmem, pdata, miFrameBaseSize );
        //glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
        
        ////////////////////////
        // PBO->texture
        ////////////////////////
        
        glTexSubImage2D(	GL_TEXTURE_2D,
                            0,
                            0, 0,
                            miW, miH,
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            0 );
        
        ////////////////////////
        // unbind the PBO
        ////////////////////////
        
        //glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );
    }
    if( dxt::IsDXT5( mpDDSHEADER->ddspf ) )
	{
        /////////////////////////////////////////////////
        // allocate space for image
        // see http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Board=3&Number=159972
        // and http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=240547
        // basically you can call glTexImage2D once with the s3tc format as the internalformat
        //  and a null data ptr to let the driver 'allocate' space for the texture
        //  then use the glCompressedTexSubImage2D to overwrite the data in the pre allocated space
        //  this decouples allocation from writing, allowing you to overwrite more efficiently
        /////////////////////////////////////////////////
        
        glBindTexture( GL_TEXTURE_2D, pTEXOBJ->mObject );
                
        /////////////////////////////
        // imgdata->PBO
        /////////////////////////////
        
        // printf( "UPDATE IMAGE UNC iw<%d> ih<%d> to<%d>\n", miW, miH, int(pTEXOBJ->mObject) );
        
        const GLuint PBOOBJ = pgltxi->GetPBO( miFrameBaseSize );
        
        //glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, PBOOBJ );
        //void* pgfxmem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY );
//        mpFile->Read( pgfxmem, miFrameBaseSize );
        //memcpy( pgfxmem, pdata, miFrameBaseSize );
        //glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
        
        ////////////////////////
        // PBO->texture
        ////////////////////////
        
        //glCompressedTexSubImage2D( GL_TEXTURE_2D, 0, 0,0, miW, miH, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, miFrameBaseSize, 0 );
        
        ////////////////////////
        // unbind the PBO
        ////////////////////////
        
        //glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );
    }    
}
float VdsTextureAnimation::GetLengthOfTime() const
{
    return 10.0f;
}
bool GlTextureInterface::LoadVDSTexture( const AssetPath& infname, Texture *ptex )
{
	GLTextureObject* pTEXOBJ = new GLTextureObject;
	ptex->SetTexIH( (void*) pTEXOBJ );
	glGenTextures( 1, & pTEXOBJ->mObject );

    VdsTextureAnimation* vta = new VdsTextureAnimation( infname );
    ptex->SetTexAnim( vta );

	ptex->SetWidth( vta->miW );
	ptex->SetHeight( vta->miH );
	ptex->SetDepth( 1 );
    
	glBindTexture( GL_TEXTURE_2D, pTEXOBJ->mObject );
    
	//glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);
	//glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    if( dxt::IsBGRA8( vta->mpDDSHEADER->ddspf ) )
    {   // allocate uncompressed
        //glTexImage2D( GL_TEXTURE_2D, 0, 4, vta->miW, vta->miH, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
    }
    else if( dxt::IsDXT5( vta->mpDDSHEADER->ddspf ) )
    {   // allocate compressed
        //glTexImage2D( GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, vta->miW, vta->miH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
    }
    
    return true;
}
    
///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::LoadDDSTexture( const AssetPath& infname, Texture *ptex )
{
	AssetPath Filename = infname;
	///////////////////////////////////////////////
	CFile TextureFile( Filename, EFM_READ );
	if( false == TextureFile.IsOpen() )
	{
		return false;
	}
	///////////////////////////////////////////////
	size_t ifilelen = 0;
	EFileErrCode eFileErr = TextureFile.GetLength( ifilelen );
	U8* pdata = (U8*) malloc(ifilelen);
	OrkAssertI( pdata!=0, "out of memory ?" );
	eFileErr = TextureFile.Read( pdata, sizeof( dxt::DDS_HEADER ) );
	dxt::DDS_HEADER* ddsh = (dxt::DDS_HEADER*) pdata;
	////////////////////////////////////////////////////////////////////
	ptex->SetWidth( ddsh->dwWidth );
	ptex->SetHeight( ddsh->dwHeight );
	ptex->SetDepth( (ddsh->dwDepth>1) ? ddsh->dwDepth : 1 );
	////////////////////////////////////////////////////////////////////		
	int NumMips = (ddsh->dwFlags & dxt::DDSD_MIPMAPCOUNT) ? ddsh->dwMipMapCount : 1;
	int iwidth = ddsh->dwWidth;
	int iheight = ddsh->dwHeight;
	int idepth = ddsh->dwDepth;
	//int ireadptr = sizeof( dxt::DDS_HEADER );
	////////////////////////////////////////////////////////////////////
	//printf( "  tex<%s> ptex<%p>\n", infname.c_str(), ptex );
	//printf( "  tex<%s> width<%d>\n", infname.c_str(), iwidth );
	//printf( "  tex<%s> height<%d>\n", infname.c_str(), iheight );
	//printf( "  tex<%s> depth<%d>\n", infname.c_str(), idepth );
	//printf( "  tex<%s> nummips<%d>\n", infname.c_str(), NumMips );
	//printf( "  tex<%s> flgs<%p>\n", infname.c_str(), int(ddsh->dwFlags) );
	//printf( "  tex<%s> 4cc<%p>\n", infname.c_str(), int(ddsh->ddspf.dwFourCC) );
	//printf( "  tex<%s> bitcnt<%d>\n", infname.c_str(), int(ddsh->ddspf.dwRGBBitCount) );
	//printf( "  tex<%s> rmask<0x%p>\n", infname.c_str(), int(ddsh->ddspf.dwRBitMask) );
	//printf( "  tex<%s> gmask<0x%p>\n", infname.c_str(), int(ddsh->ddspf.dwGBitMask) );
	//printf( "  tex<%s> bmask<0x%p>\n", infname.c_str(), int(ddsh->ddspf.dwBBitMask) );

	//data/platform_lev2/textures/voltex_pn0.dds
	////////////////////////////////////////////////////////////////////
	int iBwidth = (iwidth+3)/4;
	int iBheight = (iheight+3)/4;
	//const u8* pimgdata = & pdata[ireadptr];
	///////////////////////////////////////////////
	GLTextureObject* pTEXOBJ = new GLTextureObject;
	GL_ERRORCHECK();
	
	bool bVOLUMETEX = (idepth>1);
	
	GLuint TARGET = GL_TEXTURE_2D;
	if( bVOLUMETEX )
	{
		//TARGET = GL_TEXTURE_3D;
        OrkAssert(false);
	}
	
	glGenTextures( 1, & pTEXOBJ->mObject );
	glBindTexture( TARGET, pTEXOBJ->mObject );
	GL_ERRORCHECK();
	ptex->SetTexIH( (void*) pTEXOBJ );
	//printf( "  tex<%s> ORKTEXOBJECT<%p>\n", infname.c_str(), pTEXOBJ );
	//printf( "  tex<%s> GLTEXOBJECT<%d>\n", infname.c_str(), int(pTEXOBJ->mObject) );
	////////////////////////////////////////////////////////////////////
	// 
	////////////////////////////////////////////////////////////////////
	
	//glTexParameteri(TARGET,GL_TEXTURE_BASE_LEVEL,0);
	//glTexParameteri(TARGET,GL_TEXTURE_MAX_LEVEL,NumMips-1);
	
	if( dxt::IsLUM( ddsh->ddspf ) )
	{
		//printf( "  tex<%s> LUM\n", infname.c_str() );
		if( bVOLUMETEX )
			TexSetter::Set3D(	this, 4, GL_LUMINANCE, GL_UNSIGNED_BYTE, TARGET, 1, 
								NumMips, iwidth, iheight, idepth, TextureFile ); //ireadptr, pdata );
		else
			TexSetter::Set2D(	this, 4, GL_LUMINANCE, GL_UNSIGNED_BYTE, TARGET, 1, 
								NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
	}
	else if( dxt::IsBGR5A1( ddsh->ddspf ) )
	{
		const dxt::DdsLoadInfo & li = dxt::loadInfoBGR5A1;
		//printf( "  tex<%s> BGR5A1\n", infname.c_str() );
		//printf( "  tex<%s> size<%d>\n", infname.c_str(), 2 );
		if( bVOLUMETEX )
			TexSetter::Set3D(	this, 4, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, TARGET, 2, 
								NumMips, iwidth, iheight, idepth, TextureFile ); // ireadptr, pdata );
		else
			TexSetter::Set2D(	this, 4, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, TARGET, 2, 
								NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
	}
	else if( dxt::IsBGRA8( ddsh->ddspf ) )
	{
		const dxt::DdsLoadInfo & li = dxt::loadInfoBGRA8;
		int size = idepth*iwidth*iheight*4;
		//printf( "  tex<%s> BGRA8\n", infname.c_str() );
		//printf( "  tex<%s> size<%d>\n", infname.c_str(), size );
		//if( bVOLUMETEX )
		//	TexSetter::Set3D(	this, 4, GL_RGBA, GL_UNSIGNED_BYTE, TARGET, 4, 
		//						NumMips, iwidth, iheight, idepth, TextureFile ); // ireadptr, pdata );
		//else
			TexSetter::Set2D(	this, 4, GL_RGBA, GL_UNSIGNED_BYTE, TARGET, 4, 
								NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
		GL_ERRORCHECK();
	}
	else if( dxt::IsBGR8( ddsh->ddspf ) )
	{
		//printf( "  tex<%s> BGR8\n", infname.c_str() );
		//if( bVOLUMETEX )
		//	TexSetter::Set3D(	this, 3, GL_BGR, GL_UNSIGNED_BYTE, TARGET, 3, 
		//						NumMips, iwidth, iheight, idepth, TextureFile ); // ireadptr, pdata );
		//else
		//	TexSetter::Set2D(	this, 3, GL_BGR_OES, GL_UNSIGNED_BYTE, TARGET, 3, 
		//						NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
		GL_ERRORCHECK();
	}
	//////////////////////////////////////////////////////////
	// DXT5: texturing fast path (8 bits per pixel true color)
	//////////////////////////////////////////////////////////
	else if( dxt::IsDXT5( ddsh->ddspf ) )
	{
		const dxt::DdsLoadInfo & li = dxt::loadInfoDXT5;
		int size = (iBwidth*iBheight) * li.blockBytes;
		//printf( "  tex<%s> DXT5\n", infname.c_str() );
		//printf( "  tex<%s> size<%d>\n", infname.c_str(), size );		
		//if( bVOLUMETEX )
		//	TexSetter::Set3DC(	this, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, TARGET, li.blockBytes, 
		//						NumMips, iwidth, iheight, idepth, TextureFile ); // ireadptr, pdata );
		//else
		//	TexSetter::Set2DC(	this, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, TARGET, li.blockBytes, 
		//						NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
		//GL_ERRORCHECK();
		//////////////////////////////////////
	}
	//////////////////////////////////////////////////////////
	// DXT3: texturing fast path (8 bits per pixel true color)
	//////////////////////////////////////////////////////////
	else if( dxt::IsDXT3( ddsh->ddspf ) )
	{
		const dxt::DdsLoadInfo & li = dxt::loadInfoDXT3;
		int size = (iBwidth*iBheight) * li.blockBytes;
		//printf( "  tex<%s> DXT3\n", infname.c_str() );
		//printf( "  tex<%s> size<%d>\n", infname.c_str(), size );
		
		//if( bVOLUMETEX )
		//	TexSetter::Set3DC(	this, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, TARGET, li.blockBytes, 
		//						NumMips, iwidth, iheight, idepth, TextureFile ); // ireadptr, pdata );
		//else
		//	TexSetter::Set2DC(	this, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, TARGET, li.blockBytes, 
		//						NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
	}
	//////////////////////////////////////////////////////////
	// DXT1: texturing fast path (4 bits per pixel true color)
	//////////////////////////////////////////////////////////
	else if( dxt::IsDXT1( ddsh->ddspf ) )
	{
		const dxt::DdsLoadInfo & li = dxt::loadInfoDXT1;
		int size = (iBwidth*iBheight) * li.blockBytes;
		//printf( "  tex<%s> DXT1\n", infname.c_str() );
		//printf( "  tex<%s> size<%d>\n", infname.c_str(), size );		
		//if( bVOLUMETEX )
		//	TexSetter::Set3DC(	this, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, TARGET, li.blockBytes, 
		//						NumMips, iwidth, iheight, idepth, TextureFile ); // ireadptr, pdata );
		//else
		//	TexSetter::Set2DC(	this, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, TARGET, li.blockBytes, 
		//						NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
	}
	//////////////////////////////////////////////////////////
	// ???
	//////////////////////////////////////////////////////////
	else
	{
		OrkAssert(false);
	}

	ptex->SetDirty( false );
	glBindTexture( TARGET, 0 );

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::SaveTexture( const ork::AssetPath& fname, Texture *ptex )
{
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::VRamDeport( Texture *pTex)
{
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::VRamUpload( Texture *pTex )
{
}

///////////////////////////////////////////////////////////////////////////////

} } //namespace ork::lev2
#endif
