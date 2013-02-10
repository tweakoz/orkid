////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/file/file.h>
#include<ork/util/endian.h>
#include <ork/lev2/gfx/dxt.h>

namespace ork { namespace lev2 { namespace dxt {

const DdsLoadInfo loadInfoDXT1 = {
  true, false, false, 4, 8 //, 0 //GL_COMPRESSED_RGBA_S3TC_DXT1
};
const DdsLoadInfo loadInfoDXT3 = {
  true, false, false, 4, 16 //, 0 //GL_COMPRESSED_RGBA_S3TC_DXT3
};
const DdsLoadInfo loadInfoDXT5 = {
  true, false, false, 4, 16 //, 0 //GL_COMPRESSED_RGBA_S3TC_DXT5
};
const DdsLoadInfo loadInfoBGRA8 = {
  false, false, false, 1, 4 //, 0 //GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE
};
const DdsLoadInfo loadInfoBGR8 = {
  false, false, false, 1, 3 //, 0 //GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE
};
const DdsLoadInfo loadInfoBGR5A1 = {
  false, true, false, 1, 2 //, 0 //GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV
};
const DdsLoadInfo loadInfoBGR565 = {
  false, true, false, 1, 2 //, 0 //GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5
};
const DdsLoadInfo loadInfoIndex8 = {
  false, false, true, 1, 1 //, 0 //GL_RGB8, GL_BGRA, GL_UNSIGNED_BYTE
};

///////////////////////////////////////////////////////////////////////////////

bool IsLUM( DDS_PIXELFORMAT& pf )
{
	bool bv = 
	(pf.dwRGBBitCount == 8) &&
	(pf.dwRBitMask == 0xff0000) &&
	(pf.dwGBitMask == 0xff00) &&
	(pf.dwBBitMask == 0xff); // &&
	//(pf.dwABitMask == 0x8000));
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsBGR5A1( DDS_PIXELFORMAT& pf )
{
	bool bv = ((pf.dwFlags & DDSD_RGBA) &&
	(pf.dwRGBBitCount == 16) &&
	(pf.dwRBitMask == 0x7c00) &&
	(pf.dwGBitMask == 0x3e0) &&
	(pf.dwBBitMask == 0x1f) &&
	(pf.dwABitMask == 0x8000));
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsBGR8( dxt::DDS_PIXELFORMAT& pf )
{
	bool bv = ((pf.dwFlags & DDSD_RGB) &&
	(pf.dwRGBBitCount == 24) &&
	(pf.dwRBitMask == 0xff0000) &&
	(pf.dwGBitMask == 0xff00) &&
	(pf.dwBBitMask == 0xff));
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsBGRA8( DDS_PIXELFORMAT& pf )
{
	bool bv = ((pf.dwFlags & DDSD_RGBA) &&
	(pf.dwRGBBitCount == 32) &&
	(pf.dwRBitMask == 0xff0000) &&
	(pf.dwGBitMask == 0xff00) &&
	(pf.dwBBitMask == 0xff) &&
	(pf.dwABitMask == 0xff000000U));
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsDXT1( DDS_PIXELFORMAT& pf )
{
	bool bv = (pf.dwFlags & DDS_FOURCC);
	bv &= (pf.dwFourCC == FOURCC_DXT1);
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsDXT3( DDS_PIXELFORMAT& pf )
{
	bool bv = (pf.dwFlags & DDS_FOURCC);
	bv &= (pf.dwFourCC == FOURCC_DXT3);
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsDXT5( DDS_PIXELFORMAT& pf )
{
	bool bv = (pf.dwFlags & DDS_FOURCC);
	bv &= (pf.dwFourCC == FOURCC_DXT5);
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsABGR8( dxt::DDS_PIXELFORMAT& pf )
{
	bool bv = ((pf.dwFlags & dxt::DDSD_RGBA) &&
	(pf.dwRGBBitCount == 32) &&
	(pf.dwRBitMask == 0x00ff0000) &&
	(pf.dwGBitMask == 0x0000ff00) &&
	(pf.dwBBitMask == 0x000000ff) &&
	(pf.dwABitMask == 0xff000000));
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsRGB8( dxt::DDS_PIXELFORMAT& pf )
{
	bool bv = ((pf.dwFlags & dxt::DDSD_RGB) &&
	(pf.dwRGBBitCount == 24) &&
	(pf.dwRBitMask == 0x00ff0000) &&
	(pf.dwGBitMask == 0x0000ff00) &&
	(pf.dwBBitMask == 0x000000ff) &&
	(pf.dwABitMask == 0x00000000));
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool IsXBGR8( dxt::DDS_PIXELFORMAT& pf )
{
	bool bv = ((pf.dwFlags & dxt::DDSD_RGB) &&
	(pf.dwRGBBitCount == 32) &&
	(pf.dwRBitMask == 0x00ff0000) &&
	(pf.dwGBitMask == 0x0000ff00) &&
	(pf.dwBBitMask == 0x000000ff) &&
	(pf.dwABitMask == 0x00000000));
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

DDSFile::DDSFile( const ork::file::Path& pth )
{
	ork::CFile fil;
	size_t ifilelen = 0;
	fil.OpenFile(pth,EFM_READ);
	EFileErrCode eFileErr = fil.GetLength( ifilelen );
	void* pdata = malloc(ifilelen);
	OrkAssertI( pdata!=0, "out of memory ?" );
	eFileErr = fil.Read( pdata, ifilelen );
	dxt::DDS_HEADER* ddsh = (dxt::DDS_HEADER*) pdata;
	mHeader = *ddsh;
	////////////////////////////////////////////////////////////////////		
	int NumMips = (ddsh->dwFlags & dxt::DDSD_MIPMAPCOUNT) ? ddsh->dwMipMapCount : 1;
	miWidth = ddsh->dwWidth;
	miHeight = ddsh->dwHeight;
	miDepth = (ddsh->dwDepth>1) ? ddsh->dwDepth : 1;
	int ireadptr = sizeof( dxt::DDS_HEADER );
	mpData = (const char*) pdata;
	mpImageDataBase = mpData+ireadptr;
	mbVOLUMETEX = (miDepth>1);
	////////////////////////////////////////////////////////////////////		
	if( dxt::IsLUM( ddsh->ddspf ) )
	{	meFormat = EFMT_INDEX8;
		mLoadInfo = loadInfoIndex8;
	}
	else if( dxt::IsDXT1( ddsh->ddspf ) )
	{	meFormat = EFMT_DXT1;
		mLoadInfo = loadInfoDXT1;
	}
	else if( dxt::IsDXT3( ddsh->ddspf ) )
	{	meFormat = EFMT_DXT3;
		mLoadInfo = loadInfoDXT3;
	}
	else if( dxt::IsDXT5( ddsh->ddspf ) )
	{	meFormat = EFMT_DXT5;
		mLoadInfo = loadInfoDXT5;
	}
	else if( dxt::IsBGRA8( ddsh->ddspf ) )
	{	meFormat = EFMT_BGRA8;
		mLoadInfo = loadInfoBGRA8;
	}
	else if( dxt::IsBGR8( ddsh->ddspf ) )
	{	meFormat = EFMT_BGR8;
		mLoadInfo = loadInfoBGR8;
	}
	else if( dxt::IsBGR5A1( ddsh->ddspf ) )
	{	meFormat = EFMT_BGR5A1;
		mLoadInfo = loadInfoBGR5A1;
	}
	//else if( dxt::IsBGR565( ddsh->ddspf ) ) meFormat = EFMT_BGR565;
	////////////////////////////////////////////////////////////////////		
	printf( "  tex<%s> width<%d>\n", pth.c_str(), miWidth );
	printf( "  tex<%s> height<%d>\n", pth.c_str(), miHeight );
	printf( "  tex<%s> depth<%d>\n", pth.c_str(), miDepth );
	printf( "  tex<%s> format<%d>\n", pth.c_str(), int(meFormat) );
	printf( "  tex<%s> blocksize<%d>\n", pth.c_str(), mLoadInfo.blockBytes );
	////////////////////////////////////////////////////////////////////		
	fil.Close();
}

///////////////////////////////////////////////////////////////////////////////

void dxt::DDS_HEADER::FixEndian()
{
	ork::EndianContext pctx;
	pctx.mendian = ork::EENDIAN_LITTLE;

	ddspf.FixEndian();

	ork::swapbytes_dynamic( dwMagic );
	ork::swapbytes_dynamic( dwSize );
	ork::swapbytes_dynamic( dwFlags );
	ork::swapbytes_dynamic( dwHeight );
	ork::swapbytes_dynamic( dwWidth );

	ork::swapbytes_dynamic( dwPitchOrLinearSize );
	ork::swapbytes_dynamic( dwDepth );
	ork::swapbytes_dynamic( dwMipMapCount );
	ork::swapbytes_dynamic( dwCaps1 );
	ork::swapbytes_dynamic( dwCaps2 );

	OrkAssert( dwMagic == dxt::DDS_MAGIC );
	OrkAssert( dwSize == 124 );
	OrkAssert( dwFlags & dxt::DDSD_PIXELFORMAT );
}

///////////////////////////////////////////////////////////////////////////////

void dxt::DDS_PIXELFORMAT::FixEndian()
{
	ork::swapbytes_dynamic( dwSize );

	ork::swapbytes_dynamic( dwFlags );
	ork::swapbytes_dynamic( dwFourCC );
	ork::swapbytes_dynamic( dwRGBBitCount );
	ork::swapbytes_dynamic( dwRBitMask );
	ork::swapbytes_dynamic( dwGBitMask );
	ork::swapbytes_dynamic( dwBBitMask );
	ork::swapbytes_dynamic( dwABitMask );
}

///////////////////////////////////////////////////////////////////////////////

}}}
