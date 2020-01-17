////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/file/file.h>
#include <ork/util/endian.h>
#include <ork/gfx/dds.h>

namespace ork::dds {

const DdsLoadInfo loadInfoDXT1 = {
    true,
    false,
    false,
    4,
    8 //, 0 //GL_COMPRESSED_RGBA_S3TC_DXT1
};
const DdsLoadInfo loadInfoDXT3 = {
    true,
    false,
    false,
    4,
    16 //, 0 //GL_COMPRESSED_RGBA_S3TC_DXT3
};
const DdsLoadInfo loadInfoDXT5 = {
    true,
    false,
    false,
    4,
    16 //, 0 //GL_COMPRESSED_RGBA_S3TC_DXT5
};
const DdsLoadInfo loadInfoRGBA8 = {
    false,
    false,
    false,
    1,
    4 //, 0 //GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE
};
const DdsLoadInfo loadInfoRGB8 = {
    false,
    false,
    false,
    1,
    3 //, 0 //GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE
};
const DdsLoadInfo loadInfoBGRA8 = {
    false,
    false,
    false,
    1,
    4 //, 0 //GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE
};
const DdsLoadInfo loadInfoBGR8 = {
    false,
    false,
    false,
    1,
    3 //, 0 //GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE
};
const DdsLoadInfo loadInfoBGR5A1 = {
    false,
    true,
    false,
    1,
    2 //, 0 //GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV
};
const DdsLoadInfo loadInfoBGR565 = {
    false,
    true,
    false,
    1,
    2 //, 0 //GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5
};
const DdsLoadInfo loadInfoIndex8 = {
    false,
    false,
    true,
    1,
    1 //, 0 //GL_RGB8, GL_BGRA, GL_UNSIGNED_BYTE
};

///////////////////////////////////////////////////////////////////////////////

bool IsLUM(const DDS_PIXELFORMAT& pf) {
  bool bv = (pf.dwRGBBitCount == 8) && (pf.dwRBitMask == 0xff0000) && (pf.dwGBitMask == 0xff00) && (pf.dwBBitMask == 0xff); // &&
  //(pf.dwABitMask == 0x8000));
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsBGR5A1(const DDS_PIXELFORMAT& pf) {
  bool bv =
      ((pf.dwFlags & DDSD_RGBA) && (pf.dwRGBBitCount == 16) && (pf.dwRBitMask == 0x7c00) && (pf.dwGBitMask == 0x3e0) &&
       (pf.dwBBitMask == 0x1f) && (pf.dwABitMask == 0x8000));
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsBGR8(const DDS_PIXELFORMAT& pf) {
  bool bv =
      ((pf.dwFlags & DDSD_RGB) && (pf.dwRGBBitCount == 24) && (pf.dwRBitMask == 0xff0000) && (pf.dwGBitMask == 0xff00) &&
       (pf.dwBBitMask == 0xff));
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsBGRA8(const DDS_PIXELFORMAT& pf) {
  bool bv =
      ((pf.dwFlags & DDSD_RGBA) && (pf.dwRGBBitCount == 32) && (pf.dwRBitMask == 0xff0000) && (pf.dwGBitMask == 0xff00) &&
       (pf.dwBBitMask == 0xff) && (pf.dwABitMask == 0xff000000U));
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsDXT1(const DDS_PIXELFORMAT& pf) {
  bool bv = (pf.dwFlags & DDS_FOURCC);
  bv &= (pf.dwFourCC == FOURCC_DXT1);
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsDXT3(const DDS_PIXELFORMAT& pf) {
  bool bv = (pf.dwFlags & DDS_FOURCC);
  bv &= (pf.dwFourCC == FOURCC_DXT3);
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsDXT5(const DDS_PIXELFORMAT& pf) {
  bool bv = (pf.dwFlags & DDS_FOURCC);
  bv &= (pf.dwFourCC == FOURCC_DXT5);
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsABGR8(const DDS_PIXELFORMAT& pf) {
  bool bv =
      ((pf.dwFlags & DDSD_RGBA) && (pf.dwRGBBitCount == 32) && (pf.dwRBitMask == 0x00ff0000) && (pf.dwGBitMask == 0x0000ff00) &&
       (pf.dwBBitMask == 0x000000ff) && (pf.dwABitMask == 0xff000000));
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsRGB8(const DDS_PIXELFORMAT& pf) {
  bool bv =
      ((pf.dwFlags & DDSD_RGB) && (pf.dwRGBBitCount == 24) && (pf.dwRBitMask == 0x0000ff) && (pf.dwGBitMask == 0xff00) &&
       (pf.dwBBitMask == 0xff0000));
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsRGBA8(const DDS_PIXELFORMAT& pf) {
  bool bv =
      ((pf.dwFlags & DDSD_RGBA) && (pf.dwRGBBitCount == 32) && (pf.dwRBitMask == 0x0000ff) && (pf.dwGBitMask == 0xff00) &&
       (pf.dwBBitMask == 0xff0000) && (pf.dwABitMask == 0xff000000U));
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsXBGR8(const DDS_PIXELFORMAT& pf) {
  bool bv =
      ((pf.dwFlags & DDSD_RGB) && (pf.dwRGBBitCount == 32) && (pf.dwRBitMask == 0x00ff0000) && (pf.dwGBitMask == 0x0000ff00) &&
       (pf.dwBBitMask == 0x000000ff) && (pf.dwABitMask == 0x00000000));
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

Image::Image(datablockptr_t dblock) {

  _format = EFMT_END;

  size_t ifilelen = dblock->length();
  DataBlockInputStream inpstream(dblock);

  const auto& ddsh = inpstream.getItem<DDS_HEADER>();
  mHeader          = ddsh;
  ////////////////////////////////////////////////////////////////////
  int NumMips        = (ddsh.dwFlags & DDSD_MIPMAPCOUNT) ? ddsh.dwMipMapCount : 1;
  _width             = ddsh.dwWidth;
  _height            = ddsh.dwHeight;
  _depth             = (ddsh.dwDepth > 1) ? ddsh.dwDepth : 1;
  size_t imagedatlen = ifilelen - sizeof(DDS_HEADER);
  auto imagedata     = malloc(imagedatlen);
  _imagedata         = imagedata;
  memcpy(imagedata, inpstream.current(), imagedatlen);
  inpstream.advance(imagedatlen);
  mbVOLUMETEX = (_depth > 1);
  ////////////////////////////////////////////////////////////////////
  if (IsLUM(ddsh.ddspf)) {
    _format   = EFMT_INDEX8;
    mLoadInfo = loadInfoIndex8;
  } else if (IsDXT1(ddsh.ddspf)) {
    _format   = EFMT_DXT1;
    mLoadInfo = loadInfoDXT1;
  } else if (IsDXT3(ddsh.ddspf)) {
    _format   = EFMT_DXT3;
    mLoadInfo = loadInfoDXT3;
  } else if (IsDXT5(ddsh.ddspf)) {
    _format   = EFMT_DXT5;
    mLoadInfo = loadInfoDXT5;
  } else if (IsBGRA8(ddsh.ddspf)) {
    _format   = EFMT_BGRA8;
    mLoadInfo = loadInfoBGRA8;
  } else if (IsBGR8(ddsh.ddspf)) {
    _format   = EFMT_BGR8;
    mLoadInfo = loadInfoBGR8;
  } else if (IsRGBA8(ddsh.ddspf)) {
    _format   = EFMT_RGBA8;
    mLoadInfo = loadInfoRGBA8;
  } else if (IsRGB8(ddsh.ddspf)) {
    _format   = EFMT_RGB8;
    mLoadInfo = loadInfoRGB8;
  } else if (IsBGR5A1(ddsh.ddspf)) {
    _format   = EFMT_BGR5A1;
    mLoadInfo = loadInfoBGR5A1;
  }
  // else if( IsBGR565( ddsh.ddspf ) ) _format = EFMT_BGR565;
  ////////////////////////////////////////////////////////////////////
  // printf( "  tex<%s> width<%d>\n", pth.c_str(), _width );
  // printf( "  tex<%s> height<%d>\n", pth.c_str(), _height );
  // printf( "  tex<%s> depth<%d>\n", pth.c_str(), _depth );
  // printf( "  tex<%s> format<%d>\n", pth.c_str(), int(_format) );
  // printf( "  tex<%s> blocksize<%d>\n", pth.c_str(), mLoadInfo.blockBytes );
  ////////////////////////////////////////////////////////////////////
}

Image::~Image() {
  if (_imagedata)
    free((void*)_imagedata);
  _imagedata = nullptr;
}
///////////////////////////////////////////////////////////////////////////////

void DDS_HEADER::FixEndian() {
  ork::EndianContext pctx;
  pctx.mendian = ork::EENDIAN_LITTLE;

  ddspf.FixEndian();

  ork::swapbytes_dynamic(dwMagic);
  ork::swapbytes_dynamic(dwSize);
  ork::swapbytes_dynamic(dwFlags);
  ork::swapbytes_dynamic(dwHeight);
  ork::swapbytes_dynamic(dwWidth);

  ork::swapbytes_dynamic(dwPitchOrLinearSize);
  ork::swapbytes_dynamic(dwDepth);
  ork::swapbytes_dynamic(dwMipMapCount);
  ork::swapbytes_dynamic(dwCaps1);
  ork::swapbytes_dynamic(dwCaps2);

  OrkAssert(dwMagic == DDS_MAGIC);
  OrkAssert(dwSize == 124);
  OrkAssert(dwFlags & DDSD_PIXELFORMAT);
}

///////////////////////////////////////////////////////////////////////////////

void DDS_PIXELFORMAT::FixEndian() {
  ork::swapbytes_dynamic(dwSize);

  ork::swapbytes_dynamic(dwFlags);
  ork::swapbytes_dynamic(dwFourCC);
  ork::swapbytes_dynamic(dwRGBBitCount);
  ork::swapbytes_dynamic(dwRBitMask);
  ork::swapbytes_dynamic(dwGBitMask);
  ork::swapbytes_dynamic(dwBBitMask);
  ork::swapbytes_dynamic(dwABitMask);
}

/*
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
    //Flags to indicate valid fields. Always include DDSD_CAPS, DDSD_PIXELFORMAT, DDSD_WIDTH, DDSD_HEIGHT and either DDSD_PITCH or
DDSD_LINEARSIZE.

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
*/
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::dds
