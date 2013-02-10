////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include<ork/pch.h>
#include<ork/file/file.h>
#include<ork/util/endian.h>
#include<ork/lev2/gfx/dxt.h>

namespace ork { namespace lev2 { namespace dxt {

/*


///////////////////////////////////////////////////////////////////////////////

bool IsDXT1( dxt::DDS_PIXELFORMAT& pf )
{
	bool bv = (pf.dwFlags == dxt::DDS_FOURCC);
	bv &= (pf.dwFourCC == dxt::FOURCC_DXT1);
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsDXT3( dxt::DDS_PIXELFORMAT& pf )
{
	bool bv = (pf.dwFlags == dxt::DDS_FOURCC);
	bv &= (pf.dwFourCC == dxt::FOURCC_DXT3);
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsDXT5( dxt::DDS_PIXELFORMAT& pf )
{
	bool bv = (pf.dwFlags == dxt::DDS_FOURCC);
	bv &= (pf.dwFourCC == dxt::FOURCC_DXT5);
	return bv;
}


*/
}}}

#if 0

#include<GL/ddsfile.h>
#include <stdio.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////

void Cddsfile::save_volume_texture24( string fname, int w, int h, int d, U8 *pData24 )
{
    DDS_HEADER ddsh;

    ddsh.dwSize = 0x7c;
    ddsh.dwWidth = w;
    ddsh.dwHeight = h;
    ddsh.dwDepth = d;
    ddsh.dwPitchOrLinearSize = 0;
    ddsh.dwMipMapCount = 0;

    //ddsh.dwHeaderFlags = DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | DDSD_DEPTH | DDSD_PITCH; // 07108000
    ddsh.dwHeaderFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_DEPTH; // 07108000
    //Flags to indicate valid fields. Always include DDSD_CAPS, DDSD_PIXELFORMAT, DDSD_WIDTH, DDSD_HEIGHT and either DDSD_PITCH or DDSD_LINEARSIZE.

    ddsh.ddspf.dwFlags = DDS_RGBA;
    ddsh.ddspf.dwRGBBitCount = 32;
    ddsh.ddspf.dwRBitMask = 0x00ff0000;
    ddsh.ddspf.dwGBitMask = 0x0000ff00;
    ddsh.ddspf.dwBBitMask = 0x000000ff;
    ddsh.ddspf.dwABitMask = 0xff000000;

    ddsh.dwCubemapFlags = DDSCAPS2_VOLUME;
    ddsh.dwSurfaceFlags = DDSCAPS_TEXTURE | 0x00000002; //DDSCAPS_COMPLEX;

    FILE *fout = fopen( fname.c_str(), "wb" );

    unsigned long int ddsid = DDS_ID;

    fwrite( &ddsid, 4, 1, fout );

    fwrite( &ddsh, sizeof( DDS_HEADER ), 1, fout );

    GLubyte alphabyte = 255;

    for( int iD=0; iD<d; iD++ )
    {
        // write out a plane
        int slicesize = (w*h);
        int index = (slicesize) * iD; // di hj wk
        GLubyte *sliceptr = & pData24[ index ];

        for( int iP=0; iP<slicesize; iP++ )
        {
            GLubyte pix = sliceptr[ iP ];

            fwrite( & pix, 1, 1, fout );
            fwrite( & pix, 1, 1, fout );
            fwrite( & pix, 1, 1, fout );
            fwrite( & alphabyte, 1, 1, fout );
        }


    }

    fclose( fout );

    /////////////////////////////////////////////
    /////////////////////////////////////////////
}

////////////////////////////////////////////////////////////////////////////////



Cddsfile *Cddsfile::load_texture( string fname )
{
    printf( "/////////////////////////////////\n" );
    printf( "DDS_Load %s\n", fname.c_str() );
    Cddsfile *rval = 0;

    FILE *fin = fopen( fname.c_str(), "rb" );

    if( fin )
    {
        unsigned long int ddsid;
        fread( (void *) &ddsid, 4, 1, fin );

        if( ddsid == DDS_ID )
        {
            rval = new Cddsfile;
            fread( (void *) &rval->header, sizeof( DDS_HEADER ), 1, fin );
	    	DDS_HEADER &hdr = rval->header;
	    
	    int w = hdr.dwWidth;
	    int h = hdr.dwHeight;
	    int d = hdr.dwDepth;

	printf( "w %d h %d d %d\n", w, h, d );
        }

        fclose( fin );

    }

    printf( "/////////////////////////////////\n" );

    return rval;
}

#endif

