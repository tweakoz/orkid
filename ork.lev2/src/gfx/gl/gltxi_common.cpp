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
#include <ork/lev2/gfx/dxt.h>
#include <ork/lev2/ui/ui.h>
#include <ork/file/file.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/debug.h>

GLuint gLastBoundNonZeroTex = 0;

namespace ork { namespace lev2 {

static const uint16_t kRGB_DXT1 = 0x83F0;
static const uint16_t kRGBA_DXT1 = 0x83F1;
static const uint16_t kRGBA_DXT3 = 0x83F2;
static const uint16_t kRGBA_DXT5 = 0x83F3;

///////////////////////////////////////////////////////////////////////////////

GlTextureInterface::GlTextureInterface( GfxTargetGL& tgt )
	: mTargetGL(tgt)
{

}

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
	GL_ERRORCHECK();
	glGenBuffers(knumpbos, mPBOS);
	GL_ERRORCHECK();
	
	for( int i=0; i<knumpbos; i++ )
	{
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, mPBOS[i] );
	GL_ERRORCHECK();
		glBufferData( GL_PIXEL_UNPACK_BUFFER, isize, NULL, GL_DYNAMIC_DRAW );
	GL_ERRORCHECK();
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
	GL_ERRORCHECK();
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

	struct texcfg
	{
		GLuint mInternalFormat;
		GLuint mFormat;
		int    mBPP;
		int    mNumC;
	};

	static texcfg GetInternalFormat(GLuint fmt,GLuint typ)
	{
		texcfg rval;
		rval.mInternalFormat = 0;
		rval.mFormat = fmt;
		rval.mBPP = 4;
		rval.mNumC = 4;
		printf( "fmt<%04x> typ<%04x>\n", fmt, typ );
		switch( fmt )
		{
			case GL_RGB:
				rval.mInternalFormat = GL_RGB;
				rval.mFormat = GL_RGB;
				rval.mNumC = 3;
				rval.mBPP = 3;
				break;
			case GL_BGR:
				rval.mInternalFormat = GL_RGB;
				rval.mFormat = GL_BGR;
				rval.mNumC = 3;
				rval.mBPP = 3;
				break;
			case GL_BGRA:
			case GL_RGBA:
				rval.mInternalFormat = GL_RGBA;
				rval.mNumC = 4;
				rval.mBPP = 4;
				break;
			default:
				break;
		}
		switch( typ )
		{
			case GL_UNSIGNED_BYTE:
				break;
			case GL_UNSIGNED_SHORT_5_5_5_1:
				rval.mBPP = 2;
				break;

		}
		OrkAssert(rval.mInternalFormat!=0);
		return rval;
	}

	static void Set2D(GlTextureInterface* txi, GLuint numC, GLuint fmt, GLuint typ, GLuint tgt, int BPP, int inummips, int& iw, int& ih, CFile& file ) //, int& irdptr, const u8* dataBASE )
	{	
		size_t ifilelen = 0;
		EFileErrCode eFileErr = file.GetLength( ifilelen );
	
		//const u8* pimgdata = & dataBASE[irdptr];
		int isize = iw*ih*BPP;
		for( int imip=0; imip<inummips; imip++ )
		{			

			GLuint nfmt = fmt;

			/////////////////////////////////////////////////
			// allocate space for image
 			// see http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Board=3&Number=159972
			// and http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=240547
			// basically you can call glTexImage2D once with the s3tc format as the internalformat
			//  and a null data ptr to let the driver 'allocate' space for the texture
			//  then use the glCompressedTexSubImage2D to overwrite the data in the pre allocated space
			//  this decouples allocation from writing, allowing you to overwrite more efficiently
			/////////////////////////////////////////////////
					
			GLint intfmt = 0;
			printf( "fmt<%04x>\n", fmt );
			int isiz2 = isize;
			printf( "numC<%d> typ<%04x>\n", numC, typ );
			switch( nfmt )
			{
				case GL_BGR:
					intfmt = GL_RGB;
					nfmt = GL_BGR;
					break;
				case GL_BGRA:
					if( (numC==4) && (typ==GL_UNSIGNED_BYTE) )
						intfmt = GL_RGBA;
					break;
				case GL_RGBA:
					if(numC==4) switch(typ)
					{	case GL_UNSIGNED_SHORT_5_5_5_1:
						case GL_UNSIGNED_BYTE:
							intfmt = GL_RGBA;
							break;
						default:
							assert(false);
					}
					break;
				case GL_RGB:
					if( (numC==3) && (typ==GL_UNSIGNED_BYTE) )
						intfmt = GL_RGB;
					break;
				default:
					break;
			}
			OrkAssert(intfmt!=0);
					

			printf( "tgt<%04x> imip<%d> intfmt<%04x> w<%d> h<%d> fmt<%04x> typ<%04x>\n", tgt, imip, intfmt, iw,ih,nfmt,typ);
			GL_ERRORCHECK();

			bool bUSEPBO = false;

			if( bUSEPBO )
			{
				glTexImage2D(	tgt,
								imip,
								intfmt,
								iw, ih,
								0,
								nfmt,
								typ,
								0 );
				GL_ERRORCHECK();
								
				/////////////////////////////
				// imgdata->PBO
				/////////////////////////////

				const GLuint PBOOBJ = txi->GetPBO( isiz2 );
				
				glBindBuffer( GL_PIXEL_UNPACK_BUFFER, PBOOBJ );
				GL_ERRORCHECK();
#if defined(HAVE_MAP_BUFFER_RANGE)
				//void* pgfxmem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );
				u32 map_flags = GL_MAP_WRITE_BIT;
				map_flags |= GL_MAP_INVALIDATE_BUFFER_BIT;
				map_flags |= GL_MAP_UNSYNCHRONIZED_BIT;
				void* pgfxmem = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER,0,isiz2,map_flags);
				printf( "UPDATE IMAGE UNC imip<%d> iw<%d> ih<%d> isiz<%d> pbo<%d> mem<%p>\n", imip, iw, ih, isiz2, PBOOBJ, pgfxmem );
				file.Read( pgfxmem, isiz2 );
				glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
				GL_ERRORCHECK();
#endif
				////////////////////////
				// PBO->texture
				////////////////////////

				glTexSubImage2D(	tgt,
									imip,
									0, 0,
									iw, ih,
									nfmt,
									typ,
									0 );

				GL_ERRORCHECK();

				////////////////////////
				// unbind the PBO
				////////////////////////

				glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );

				GL_ERRORCHECK();
			}
			else // not PBO
			{
				void* pgfxmem = malloc(isiz2);
				file.Read( pgfxmem, isiz2 );

				glTexImage2D(	tgt,
								imip,
								intfmt,
								iw, ih,
								0,
								nfmt,
								typ,
								pgfxmem );
				GL_ERRORCHECK();

				free(pgfxmem);

			}

			////////////////////////

			//irdptr+=isize;
			//pimgdata = & dataBASE[irdptr];

			iw>>=1;
			ih>>=1;
			isize = iw*ih*BPP;
		}
	}
	static void Set3D(GlTextureInterface* txi, /*GLuint numC,*/ GLuint fmt, GLuint typ, GLuint tgt, /*int BPP,*/ int inummips, int& iw, int& ih, int& id, CFile& file ) //, int& irdptr, const u8* dataBASE )
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

			texcfg tc = GetInternalFormat(fmt,typ);

			/////////////////////////////
			// imgdata->PBO
			/////////////////////////////

			int isize = id*iw*ih*tc.mBPP;

			printf( "UPDATE IMAGE 3dUNC imip<%d> iw<%d> ih<%d> id<%d> isiz<%d>\n", imip, iw, ih, id, isize );
			GL_ERRORCHECK();


			bool bUSEPBO = false;
			if( bUSEPBO )
			{
				glTexImage3D(	tgt,
								imip,
								tc.mInternalFormat,
								iw, ih, id,
								0,
								tc.mFormat,
								typ,
								0 );

				const GLuint PBOOBJ = txi->GetPBO( isize );
				
				glBindBuffer( GL_PIXEL_UNPACK_BUFFER, PBOOBJ );
				void* pgfxmem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );
				file.Read( pgfxmem, isize );
				glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

				////////////////////////
				// PBO->texture
				////////////////////////
				
				glTexSubImage3D(	tgt,
									imip,
									0,0,0,
									iw, ih, id,
									tc.mFormat,
									typ,
									0 );

				////////////////////////
				// unbind the PBO
				////////////////////////

				glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
			}
			/////////////////////////////
			else
			{
				void* pgfxmem = malloc(isize);
				file.Read( pgfxmem, isize );

				glTexImage3D(	tgt,
								imip,
								tc.mInternalFormat,
								iw, ih, id,
								0,
								tc.mFormat,
								typ,
								pgfxmem );

				free(pgfxmem);
			}
			GL_ERRORCHECK();
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
			
			bool hasalpha = (fmt==kRGBA_DXT5)||(fmt==kRGBA_DXT3); 
			GLenum extfmt = hasalpha ? GL_RGBA : GL_RGB;

			bool bUSEPBO = false;

			printf("alloc texdata tgt<%d> imip<%d> fmt<%d> iw<%d> ih<%d> extfmt<%d>\n",
				tgt,imip,fmt,iw,ih,extfmt );

			if( false==bUSEPBO )
			{
				static const int kloadbufsize = 16<<20;
				static void* gloadbuf = malloc(kloadbufsize);
				OrkAssert(isize<kloadbufsize);
				file.Read( gloadbuf, isize );
				GL_ERRORCHECK();
				glCompressedTexImage2D(	tgt,
											imip,
											fmt,
											iw, ih,
											0,
											isize,
											gloadbuf
											);

				GL_ERRORCHECK();
			}
			else // PBO
			{
				GL_ERRORCHECK();
				glTexImage2D( tgt, imip, fmt, iw, ih, 0, extfmt, GL_UNSIGNED_BYTE, NULL );
				GL_ERRORCHECK();
				
				/////////////////////////////
				// imgdata->PBO
				/////////////////////////////

				//printf( "UPDATE IMAGE  S3TC\n" );

				const GLuint PBOOBJ = txi->GetPBO( isize );
				
				glBindBuffer( GL_PIXEL_UNPACK_BUFFER, PBOOBJ );
				GL_ERRORCHECK();
				void* pgfxmem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );
				GL_ERRORCHECK();
				OrkAssert(pgfxmem!=0);
				file.Read( pgfxmem, isize );
				glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
				GL_ERRORCHECK();

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

				GL_ERRORCHECK();

				////////////////////////
				// unbind the PBO
				////////////////////////

				glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
				GL_ERRORCHECK();
			}
			////////////////////////

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

///////////////////////////////////////////////////////////////////////////////

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
        
        glBindBuffer( GL_PIXEL_UNPACK_BUFFER, PBOOBJ );
        void* pgfxmem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );
       // mpFile->Read( pgfxmem, miFrameBaseSize );
        memcpy( pgfxmem, pdata, miFrameBaseSize );
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        
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
        
        glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
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
        
        glBindBuffer( GL_PIXEL_UNPACK_BUFFER, PBOOBJ );
        void* pgfxmem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );
//        mpFile->Read( pgfxmem, miFrameBaseSize );
        memcpy( pgfxmem, pdata, miFrameBaseSize );
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        
        ////////////////////////
        // PBO->texture
        ////////////////////////
        
        glCompressedTexSubImage2D( GL_TEXTURE_2D, 0, 0,0, miW, miH, kRGBA_DXT5, miFrameBaseSize, 0 );
        
        ////////////////////////
        // unbind the PBO
        ////////////////////////
        
        glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
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
    
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    if( dxt::IsBGRA8( vta->mpDDSHEADER->ddspf ) )
    {   // allocate uncompressed
        glTexImage2D( GL_TEXTURE_2D, 0, 4, vta->miW, vta->miH, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
    }
    else if( dxt::IsDXT5( vta->mpDDSHEADER->ddspf ) )
    {   // allocate compressed
        glTexImage2D( GL_TEXTURE_2D, 0, kRGBA_DXT5, vta->miW, vta->miH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
    }
    
    return true;
}
    
///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::LoadDDSTextureMainThreadPart(const GlTexLoadReq& req)
{
	mTargetGL.MakeCurrentContext();

	dxt::DDS_HEADER* ddsh = req.ddsh;
	Texture* ptex = req.ptex;
	GLTextureObject* pTEXOBJ = req.pTEXOBJ;
	CFile& TextureFile = *req.pTEXFILE;
	
	int NumMips = (ddsh->dwFlags & dxt::DDSD_MIPMAPCOUNT) ? ddsh->dwMipMapCount : 1;
	int iwidth = ddsh->dwWidth;
	int iheight = ddsh->dwHeight;
	int idepth = ddsh->dwDepth;
	int iBwidth = (iwidth+3)/4;
	int iBheight = (iheight+3)/4;

	bool bVOLUMETEX = (idepth>1);
	
	GLuint TARGET = GL_TEXTURE_2D;
	if( bVOLUMETEX )
	{
		TARGET = GL_TEXTURE_3D;
	}
	pTEXOBJ->mTarget = TARGET;

	//////////////////
	
	GL_ERRORCHECK();

	glGenTextures( 1, & pTEXOBJ->mObject );
	glBindTexture( TARGET, pTEXOBJ->mObject );
	GL_ERRORCHECK();
	
	//printf( "  tex<%s> ORKTEXOBJECT<%p>\n", TextureFile.msFileName.c_str(), pTEXOBJ );
	this->ApplySamplingMode(ptex);

	//printf( "  tex<%s> GLTEXOBJECT<%d>\n", infname.c_str(), int(pTEXOBJ->mObject) );
	////////////////////////////////////////////////////////////////////
	// 
	////////////////////////////////////////////////////////////////////
	
	glTexParameteri(TARGET,GL_TEXTURE_BASE_LEVEL,0);
	glTexParameteri(TARGET,GL_TEXTURE_MAX_LEVEL,NumMips-1);
	
	if( dxt::IsLUM( ddsh->ddspf ) )
	{
		//printf( "  tex<%s> LUM\n", infname.c_str() );
		if( bVOLUMETEX )
			TexSetter::Set3D(	this, GL_RED, GL_UNSIGNED_BYTE, TARGET, 
								NumMips, iwidth, iheight, idepth, TextureFile ); //ireadptr, pdata );
		else
			TexSetter::Set2D(	this, 1, GL_RED, GL_UNSIGNED_BYTE, TARGET, 1, 
								NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
	}
	else if( dxt::IsBGR5A1( ddsh->ddspf ) )
	{
		const dxt::DdsLoadInfo & li = dxt::loadInfoBGR5A1;
		//printf( "  tex<%s> BGR5A1\n", infname.c_str() );
		//printf( "  tex<%s> size<%d>\n", infname.c_str(), 2 );
		if( bVOLUMETEX )
			TexSetter::Set3D(	this, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, TARGET, 
								NumMips, iwidth, iheight, idepth, TextureFile ); // ireadptr, pdata );
		else
			TexSetter::Set2D(	this, 4, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, TARGET, 2, 
								NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
	}
	else if( dxt::IsBGRA8( ddsh->ddspf ) )
	{
		const dxt::DdsLoadInfo & li = dxt::loadInfoBGRA8;
		int size = idepth*iwidth*iheight*4;
		printf( "  tex<%s> BGRA8\n", TextureFile.msFileName.c_str() );
		printf( "  tex<%s> size<%d>\n", TextureFile.msFileName.c_str(), size );
		if( bVOLUMETEX )
			TexSetter::Set3D(	this, GL_RGBA, GL_UNSIGNED_BYTE, TARGET, 
								NumMips, iwidth, iheight, idepth, TextureFile ); // ireadptr, pdata );
		else
			TexSetter::Set2D(	this, 4, GL_RGBA, GL_UNSIGNED_BYTE, TARGET, 4, 
								NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
		GL_ERRORCHECK();
	}
	else if( dxt::IsBGR8( ddsh->ddspf ) )
	{
		int size = idepth*iwidth*iheight*3;
		printf( "  tex<%s> BGR8\n", TextureFile.msFileName.c_str() );
		printf( "  tex<%s> size<%d>\n", TextureFile.msFileName.c_str(), size );
		//printf( "  tex<%s> BGR8\n", infname.c_str() );
		if( bVOLUMETEX )
			TexSetter::Set3D(	this, GL_BGR, GL_UNSIGNED_BYTE, TARGET, 
								NumMips, iwidth, iheight, idepth, TextureFile ); // ireadptr, pdata );
		else
			TexSetter::Set2D(	this, 3, GL_BGR, GL_UNSIGNED_BYTE, TARGET, 3, 
								NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
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
		if( bVOLUMETEX )
			TexSetter::Set3DC(	this, kRGBA_DXT5, TARGET, li.blockBytes, 
								NumMips, iwidth, iheight, idepth, TextureFile ); // ireadptr, pdata );
		else
			TexSetter::Set2DC(	this, kRGBA_DXT5, TARGET, li.blockBytes, 
								NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
		GL_ERRORCHECK();
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
		
		if( bVOLUMETEX )
			TexSetter::Set3DC(	this, kRGBA_DXT3, TARGET, li.blockBytes, 
								NumMips, iwidth, iheight, idepth, TextureFile ); // ireadptr, pdata );
		else
			TexSetter::Set2DC(	this, kRGBA_DXT3, TARGET, li.blockBytes, 
								NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
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
		if( bVOLUMETEX )
			TexSetter::Set3DC(	this, kRGBA_DXT1, TARGET, li.blockBytes, 
								NumMips, iwidth, iheight, idepth, TextureFile ); // ireadptr, pdata );
		else
			TexSetter::Set2DC(	this, kRGBA_DXT1, TARGET, li.blockBytes, 
								NumMips, iwidth, iheight, TextureFile ); // ireadptr, pdata );
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
	GL_ERRORCHECK();

	delete req.pTEXFILE;
}

bool GlTextureInterface::LoadDDSTexture( const AssetPath& infname, Texture *ptex )
{
	AssetPath Filename = infname;
	///////////////////////////////////////////////
	CFile* TextureFile = new CFile( Filename, EFM_READ );
	if( false == TextureFile->IsOpen() )
	{
		return false;
	}
	///////////////////////////////////////////////
	size_t ifilelen = 0;
	EFileErrCode eFileErr = TextureFile->GetLength( ifilelen );
	U8* pdata = (U8*) malloc(ifilelen);
	OrkAssertI( pdata!=0, "out of memory ?" );
	eFileErr = TextureFile->Read( pdata, sizeof( dxt::DDS_HEADER ) );
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
	printf( "  tex<%s> ptex<%p>\n", infname.c_str(), ptex );
	printf( "  tex<%s> width<%d>\n", infname.c_str(), iwidth );
	printf( "  tex<%s> height<%d>\n", infname.c_str(), iheight );
	printf( "  tex<%s> depth<%d>\n", infname.c_str(), idepth );
	printf( "  tex<%s> nummips<%d>\n", infname.c_str(), NumMips );
	printf( "  tex<%s> flgs<%x>\n", infname.c_str(), int(ddsh->dwFlags) );
	printf( "  tex<%s> 4cc<%x>\n", infname.c_str(), int(ddsh->ddspf.dwFourCC) );
	printf( "  tex<%s> bitcnt<%d>\n", infname.c_str(), int(ddsh->ddspf.dwRGBBitCount) );
	printf( "  tex<%s> rmask<0x%x>\n", infname.c_str(), int(ddsh->ddspf.dwRBitMask) );
	printf( "  tex<%s> gmask<0x%x>\n", infname.c_str(), int(ddsh->ddspf.dwGBitMask) );
	printf( "  tex<%s> bmask<0x%x>\n", infname.c_str(), int(ddsh->ddspf.dwBBitMask) );
	///////////////////////////////////////////////
	GLTextureObject* pTEXOBJ = new GLTextureObject;
	ptex->SetTexIH( (void*) pTEXOBJ );
		
	///////////////////////////////////////////////
	GlTexLoadReq load_req;
	load_req.ptex = ptex;
	load_req.ddsh = ddsh;
	load_req.pTEXOBJ = pTEXOBJ;
	load_req.pTEXFILE = TextureFile;
	void_lambda_t lamb = [=]()
	{
		this->LoadDDSTextureMainThreadPart( load_req );
		//ptex->TexSamplingMode().PresetTrilinearWrap();
	};
	MainThreadOpQ().push(lamb,get_backtrace());
	///////////////////////////////////////////////

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

void GlTextureInterface::VRamUpload( Texture *ptex )
{
	GLTextureObject* pTEXOBJ = (GLTextureObject*) ptex->GetTexIH();
    if( pTEXOBJ )
    {
        glBindTexture( GL_TEXTURE_2D, pTEXOBJ->mObject );
        glTexSubImage2D(	GL_TEXTURE_2D,
                            0,
                            0, 0,
                            ptex->GetWidth(), ptex->GetHeight(),
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            ptex->GetTexData() );
    }
}

///////////////////////////////////////////////////////////////////////////////

static auto addrlamb = [&](ETextureAddressMode inp) -> GLenum
{	switch( inp )
	{
 		case ETEXADDR_CLAMP:
#if ! defined(USE_GL3)
			return GL_CLAMP;
			break;
#endif
		case ETEXADDR_WRAP:
			return GL_REPEAT;
			break;
		default:
			return GL_NONE;
			break;
	}
};
//////////////////////////////////////////
static auto magfiltlamb = [&](const TextureSamplingModeData& inp) -> GLenum
{	GLenum rval = GL_NEAREST;
	switch( inp.GetFiltModeMag() )
	{	case ETEXFILT_POINT:
			rval=GL_NEAREST;
			break;
		case ETEXFILT_LINEAR:
			rval=GL_LINEAR;
			break;
		default:
			break;
	}
	return rval;
};
//////////////////////////////////////////
static auto minfiltlamb = [&](const TextureSamplingModeData& inp) -> GLenum
{	GLenum rval = GL_NEAREST;
	switch( inp.GetFiltModeMip() )
	{	case ETEXFILT_POINT:
			switch( inp.GetFiltModeMin() )
			{	case ETEXFILT_POINT:
					rval=GL_NEAREST;
					break;
				case ETEXFILT_LINEAR:
					rval=GL_LINEAR;
					break;
				default:
					break;
			}
			break;
		case ETEXFILT_LINEAR:
			rval=GL_LINEAR_MIPMAP_LINEAR;
			break;
		default:
			break;
	}
	return rval;
};

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::ApplySamplingMode( Texture *ptex )
{
	GLTextureObject* pTEXOBJ = (GLTextureObject*) ptex->GetTexIH();
    if( pTEXOBJ )
    {
   		mTargetGL.MakeCurrentContext();

		const auto& texmode = ptex->TexSamplingMode();

		//printf( "pTEXOBJ<%p> tgt<%p>\n", pTEXOBJ, (void*)pTEXOBJ->mTarget );

		//assert(pTEXOBJ->mTarget == GL_TEXTURE_2D );


		GLenum tgt = (pTEXOBJ->mTarget!=GL_NONE) ? pTEXOBJ->mTarget : GL_TEXTURE_2D;

		GL_ERRORCHECK();
        glBindTexture( tgt, pTEXOBJ->mObject );
		GL_ERRORCHECK();
		glTexParameterf(tgt, GL_TEXTURE_MAG_FILTER, magfiltlamb(texmode));
		GL_ERRORCHECK();
		glTexParameterf(tgt, GL_TEXTURE_MIN_FILTER, minfiltlamb(texmode));
		GL_ERRORCHECK();
		glTexParameterf(tgt, GL_TEXTURE_MAX_LEVEL, 0);
		GL_ERRORCHECK();
		glTexParameterf(tgt, GL_TEXTURE_WRAP_S, addrlamb(texmode.GetAddrModeU()));
		GL_ERRORCHECK();
		glTexParameterf(tgt, GL_TEXTURE_WRAP_T, addrlamb(texmode.GetAddrModeV()));
		GL_ERRORCHECK();
	}
}


///////////////////////////////////////////////////////////////////////////////


}} // namespace ork { namespace lev2 {
