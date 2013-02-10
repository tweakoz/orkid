////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _GL_DDSFILE_H
#define _GL_DDSFILE_H

////////////////////////////////////////////////////////////////////////////////

#define DDS_RGBA   0x00000041
#define FOURCC_DXT5 0x35545844 //(MAKEFOURCC('D','X','T','5'))
#define FOURCC_DXT4 0x34545844 //(MAKEFOURCC('D','X','T','4'))
#define FOURCC_DXT3 0x33545844 //(MAKEFOURCC('D','X','T','3'))
#define FOURCC_DXT2 0x32545844 //(MAKEFOURCC('D','X','T','2'))
#define FOURCC_DXT1 0x31545844 //(MAKEFOURCC('D','X','T','1'))
#define DDS_ID 0x20534444 //0x44445320 "DDS "

#define DDSD_CAPS 0x00000001 
#define DDSD_HEIGHT 0x00000002 
#define DDSD_WIDTH 0x00000004 
#define DDSD_PITCH 0x00000008 
#define DDSD_PIXELFORMAT 0x00001000 
#define DDSD_MIPMAPCOUNT 0x00020000 
#define DDSD_LINEARSIZE 0x00080000 
#define DDSD_DEPTH 0x00800000 

#define DDSCAPS2_VOLUME 0x00200000 
#define DDSCAPS_COMPLEX 0x00000008 
#define DDSCAPS_TEXTURE 0x00001000 

////////////////////////////////////////////////////////////////////////////////

struct DDS_PIXELFORMAT
{
    unsigned long dwSize;
    unsigned long dwFlags;
    unsigned long dwFourCC;
    unsigned long dwRGBBitCount;
    unsigned long dwRBitMask;
    unsigned long dwGBitMask;
    unsigned long dwBBitMask;
    unsigned long dwABitMask;

	DDS_PIXELFORMAT()
	{
		dwSize = sizeof( DDS_PIXELFORMAT );
		dwFlags = 0;
		dwFourCC = 0;
		dwRGBBitCount = 0;
		dwRBitMask = 0;
		dwGBitMask = 0;
		dwBBitMask = 0;
		dwABitMask = 0;
	}

};

////////////////////////////////////////////////////////////////////////////////

struct DDS_HEADER
{
    unsigned long dwSize;
    unsigned long dwHeaderFlags;
    unsigned long dwHeight;
    unsigned long dwWidth;
    unsigned long dwPitchOrLinearSize;
    unsigned long dwDepth;
    unsigned long dwMipMapCount;
    unsigned long dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    unsigned long dwSurfaceFlags;
    unsigned long dwCubemapFlags;
    unsigned long dwReserved2[3];

	DDS_HEADER()
	{
		dwSize = 0;
		dwHeaderFlags = 0;
		dwHeight = 0;
		dwWidth = 0;
		dwPitchOrLinearSize = 0;
		dwDepth = 0;
		dwMipMapCount = 0;
		dwSurfaceFlags = 0;
		dwCubemapFlags = 0;
		int i;
		for( i=0; i<11; i++ )
		{
			dwReserved1[i] = 0;
		}
		for( i=0; i<3; i++ )
		{
			dwReserved2[i] = 0;
		}
	}
};

////////////////////////////////////////////////////////////////////////////////

class Cddsfile
{	public: //
	   
    DDS_HEADER header;
    void *imagedata;
    
	static void save_volume_texture24( std::string fname, int w, int h, int d, U8 *pData24 );
	static Cddsfile *load_texture( std::string fname );
};    

////////////////////////////////////////////////////////////////////////////////

#endif
