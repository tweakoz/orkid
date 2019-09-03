////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#if 0 //! defined(_IOS)
#include <ork/lev2/gfx/util/ddsfile.h>
#include <ork/lev2/gfx/texman.h>
//#include "gl/gl.h"
//#include <GL/gl.h>
//#include <GL/glu.h>
#include <IL/il.h>
#include <IL/ilut.h>

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////

namespace devil {

void InitDevIL()
{
	static bool binit = true;

	if( binit )
	{
		ilInit();
		int iver = ilGetInteger( IL_VERSION_NUM);
		if(	(iver < IL_VERSION) )
		{	orkprintf("DevIL version is different...exiting!\n");
			OrkAssert(0);
		}
		binit = false;
	}
}

}

///////////////////////////////////////////////////////////////////////////////
/*
ILboolean ILAPIENTRY orkIL_GLTexImage( Texture *orktex, ILuint ImageName, int SubImage, GLenum Target )
{
	static const int kloadmipbias = 0;

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, IL_FALSE);

	ILuint dxt_format = ilGetInteger( IL_DXTC_DATA_FORMAT );

	if( dxt_format != IL_DXT_NO_COMP )
	{
		static const int kbuffersize = (2<<20);
		static char dxtbuffer[kbuffersize];

		GLenum glformat = 0;
		//switch( dxt_format )
		{
			//case IL_DXT1: glformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
			//case IL_DXT3: glformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
			//case IL_DXT5: glformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
			//default:
			{
				OrkAssert(false);
			}
		}
		

		ilBindImage(ImageName);
		ilActiveImage( SubImage );

		int NumMips = int(ilGetInteger(IL_NUM_MIPMAPS));

		for( int imip=0; imip<=NumMips; imip++ )
		{
			ilBindImage(ImageName);
			ilActiveImage( SubImage );
			ILboolean bok = ilActiveMipmap(imip);
			ILuint Width = ilGetInteger(IL_IMAGE_WIDTH);
			ILuint Height = ilGetInteger(IL_IMAGE_HEIGHT);
			ILuint datasize = ilGetDXTCData( dxtbuffer, kbuffersize, dxt_format );

			//glCompressedTexImage2DARB(	Target,
			//							imip, 
			//							glformat,
			//							Width, Height,
			//							0, datasize, dxtbuffer );
		}
		//glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, kloadmipbias); 
		//glTexParameteri(Target, GL_TEXTURE_MAX_LEVEL, NumMips-kloadmipbias); 
 	}
	else
	{
		struct yo
		{
			static void doit( ILuint ImageName, int SubImage, GLenum Target, int imip )
			{
				ilBindImage(ImageName);
				ilActiveImage( SubImage );
				ILboolean bok = ilActiveMipmap(imip);
				OrkAssert( bok == IL_TRUE );

				ILuint Width = ilGetInteger(IL_IMAGE_WIDTH);
				ILuint Height = ilGetInteger(IL_IMAGE_HEIGHT);
				ILuint datasize = ilGetInteger(IL_IMAGE_SIZE_OF_DATA);
				ILubyte *Data = ilGetData();
				ILuint BPP = ilGetInteger(IL_IMAGE_BPP);
				ILuint Format = ilGetInteger( IL_IMAGE_FORMAT );

				GLenum glformat = 0;
				switch( Format )
				{
					case IL_COLOR_INDEX: glformat=GL_LUMINANCE; break;
					case IL_LUMINANCE: glformat=GL_LUMINANCE; break;
					case IL_RGB: glformat=GL_RGB; break;
					case IL_RGBA: glformat=GL_RGBA; break;
					//case IL_BGR: glformat=GL_BGR; break;
					//case IL_BGRA: glformat=GL_BGRA; break;
					default:
					{
						OrkAssert(false);
					}
				}
				glTexImage2D( Target, imip, BPP, Width, Height, 0, glformat, GL_UNSIGNED_BYTE, Data );
			}
		};
		int NumMips = int(ilGetInteger(IL_NUM_MIPMAPS));
		if( 0 == NumMips )
		{
			iluBuildMipmaps();
			NumMips = ilGetInteger(IL_NUM_MIPMAPS);
		}
		for( int imip=0; imip<=NumMips; imip++ )
		{
			yo::doit( ImageName, SubImage, Target, imip );
		}
		//glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, kloadmipbias); 
		//glTexParameteri(Target, GL_TEXTURE_MAX_LEVEL, NumMips-kloadmipbias); 
	}
	return IL_TRUE;
}

///////////////////////////////////////////////////////////////////////////////
bool orkIL_GLBindTexImage( Texture *orktex, ILuint ImageName )
{
	GLuint	TexID = 0;
	GLuint Target = GL_TEXTURE_2D;

	bool IsCubemap = ( ilGetInteger(IL_IMAGE_CUBEFLAGS)!=0 );

	if(IsCubemap)
	{
		//Target = GL_TEXTURE_CUBE_MAP;
	}

	glGenTextures(1, &TexID);
	glBindTexture(Target, TexID);

	if (Target == GL_TEXTURE_2D) {
		glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
	//else if (Target == GL_TEXTURE_CUBE_MAP) {
	//	glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	//	glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//	glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	//}
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	
	if( IsCubemap )
	{
		for( int i=0; i<6; i++ )
		{
			//orkIL_GLTexImage(orktex,ImageName,i,GL_TEXTURE_CUBE_MAP_POSITIVE_X+i);
		}
	}
	else
	{
		orkIL_GLTexImage(orktex,ImageName,0,Target);
	}

	orktex->SetTexIH( (void*) TexID );

	return true;
}*/

///////////////////////////////////////////////////////////////////////////////
extern "C" ILboolean ilutGLInit();

bool LoadIL( const AssetPath& fname, Texture *newtex )
{
	AssetPath fullpath = fname.ToAbsolute();

	bool bIsAbsPath = fullpath.IsAbsolute();
	bool bHasExt = fullpath.HasExtension();

	devil::InitDevIL();

	ilutRenderer(ILUT_OPENGL);
	ilSetInteger(IL_KEEP_DXTC_DATA, IL_TRUE);
	ilSetInteger( ILUT_GL_USE_S3TC, IL_TRUE );
	
	/////////////////////////////////////////////////////
	// go thru search paths

	if( FileEnv::GetRef().DoesFileExist( fullpath ) )
	{
		////////////////////////////////////
		// Load Data
		ILuint ImageName;
		ilGenImages(1, &ImageName);
		ilBindImage(ImageName);
		ilutEnable(ILUT_GL_AUTODETECT_TEXTURE_TARGET);
		ilutEnable(ILUT_OPENGL_CONV);
		ilEnable( IL_ORIGIN_SET );
		ILboolean OriginOK = ilOriginFunc( IL_ORIGIN_UPPER_LEFT );

		bool bv = ilLoadImage( (const ILstring) fullpath.c_str() );
		ilActiveImage( 0 );

		ILenum Error = ilGetError(); 

		if( Error == 0 )
		{	
			ILuint Count = ilGetInteger(IL_NUM_IMAGES);
			ILuint Width = ilGetInteger(IL_IMAGE_WIDTH);
			ILuint Height = ilGetInteger(IL_IMAGE_HEIGHT);
			ILuint Depth = ilGetInteger(IL_IMAGE_DEPTH);
			ILuint BPP = ilGetInteger(IL_IMAGE_BPP);
			ILuint datasize = ilGetInteger(IL_IMAGE_SIZE_OF_DATA);
			ILuint dxt_format = ilGetInteger( IL_DXTC_DATA_FORMAT );
			bool IsDXTC = ( dxt_format != IL_DXT_NO_COMP );
			ILubyte *Data = 0; 
			//if( IsDXTC )
			//{
			//	ilSetInteger(IL_KEEP_DXTC_DATA, IL_TRUE);
			//	ilSetInteger( ILUT_GL_USE_S3TC, IL_TRUE );
			//	datasize = ilGetDXTCData( 0, 0, dxt_format );
			//}
			Data = ilGetData();
			ILuint cubeflags = ilGetInteger(IL_IMAGE_CUBEFLAGS);
			bool bCubeMap = (cubeflags&IL_CUBEMAP_POSITIVEX);

			orkprintf( "<DEVIL> [%s] [count %d] [W%d] [H%d] [D%d] [Data %08x] [BPP %d] [IsCube %d] Error %d\n", fname.c_str(), Count, Width, Height, Depth, Data, BPP, bCubeMap, Error );
			orkprintf( "<DEVIL> [dxtformat %08x] [datasize %08x]\n", dxt_format, datasize );
		
			newtex->SetWidth( Width );
			newtex->SetHeight( Height );
			newtex->SetBytesPerPixel( BPP );
			newtex->SetTexData( Data );
			bool bOK = true; //orkIL_GLBindTexImage(newtex,ImageName);
			return bOK;
		}
	}
	////////////////////////////////////*/
 	return false;
}

///////////////////////////////////////////////////////////////////////////////

#endif



