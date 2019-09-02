////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/util/ddsfile.h>
#include <ork/lev2/gfx/texman.h>
#include <math.h>

namespace ork {
namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

void TextureSamplingModeData::PresetPointAndClamp()
{
	mTexAddrModeU = ETEXADDR_CLAMP;
	mTexAddrModeV = ETEXADDR_CLAMP;
	mTexFiltModeMin = ETEXFILT_POINT;
	mTexFiltModeMag = ETEXFILT_POINT;
	mTexFiltModeMip = ETEXFILT_POINT;
}

///////////////////////////////////////////////////////////////////////////////

void TextureSamplingModeData::PresetTrilinearWrap()
{
	mTexAddrModeU = ETEXADDR_WRAP;
	mTexAddrModeV = ETEXADDR_WRAP;
	mTexFiltModeMin = ETEXFILT_LINEAR;
	mTexFiltModeMag = ETEXFILT_LINEAR;
	mTexFiltModeMip = ETEXFILT_LINEAR;
}

///////////////////////////////////////////////////////////////////////////////

void Texture::RegisterLoaders( void )
{

}

///////////////////////////////////////////////////////////////////////////////

Texture *Texture::LoadUnManaged( const AssetPath& fname )
{
	Texture* ptex = new Texture;
	bool bok = GfxEnv::GetRef().GetLoaderTarget()->TXI()->LoadTexture( fname, ptex );
	return ptex;
}

///////////////////////////////////////////////////////////////////////////////

Texture *Texture::CreateBlank( int iw, int ih, EBufferFormat efmt )
{
	Texture *pTex = new Texture;

	pTex->SetWidth( iw );
	pTex->SetHeight( ih );
	pTex->SetTexClass( Texture::ETEXCLASS_PAINTABLE );

	switch( efmt )
	{
		case EBUFFMT_RGBA32:
		case EBUFFMT_F32:
			pTex->SetBytesPerPixel( 4 );
			pTex->SetTexData( new U8[ iw*ih*4 ] );
			memset( pTex->GetTexData(), 0, iw*ih*4 );
			break;
		case EBUFFMT_RGBA128:
			pTex->SetBytesPerPixel( 16 );
			pTex->SetTexData( new U8[ iw*ih*16 ] );
			memset( pTex->GetTexData(), 0, iw*ih*16 );
			break;
		default:
			assert(false);
	}
	return pTex;
}

///////////////////////////////////////////////////////////////////////////////

Texture::Texture()
	: mMaxMip(0)
	, meTexDest(ETEXDEST_END)
	, meTexType(ETEXTYPE_END)
	, meTexClass(ETEXCLASS_END)
	, meTexFormat(EBUFFMT_END)
	, miWidth(0)
	, miHeight(0)
	, miDepth(0)
	, muvW(0)
	, muvH(0)
	, miBPP(0)
	, mFlags(0)
	, mbDirty(true)
	, miMaxMipUniqueColors( 0 )
	, miTotalUniqueColors( 0 )
	, mpData(nullptr)
	, mpTexAnim(nullptr)
	, mInternalHandle(nullptr)
	, mpImageData(nullptr)
{}

///////////////////////////////////////////////////////////////////////////////

Texture::~Texture()
{
	GfxTarget* pTARG = GfxEnv::GetRef().GetLoaderTarget();
	pTARG->TXI()->DestroyTexture( this );
}

///////////////////////////////////////////////////////////////////////////////

void Texture::Clear( const fcolor4 & color )
{
	float Mul(255.0f);

	u8 ur = (u8) (color.GetX()*Mul);
	u8 ug = (u8) (color.GetY()*Mul);
	u8 ub = (u8) (color.GetZ()*Mul);
	u8 ua = (u8) (color.GetW()*Mul);

	U8* pu8data = (U8*) mpImageData;

	for( int ix=0; ix<miWidth; ix++ )
	{
		for( int iy=0; iy<miHeight; iy++ )
		{
			int idx = 4 * (ix*miHeight + iy);
			pu8data[idx+0] = ub;
			pu8data[idx+1] = ug;
			pu8data[idx+2] = ur;
			pu8data[idx+3] = ua;
		}
	}
	SetDirty(true);
}

///////////////////////////////////////////////////////////////////////////////

void Texture::SetTexel( const fcolor4 & color, const fvec2 & ST )
{
	float Mul(255.0f);
	U8* pu8data = (U8*) mpImageData;

	u8 ur = (u8) (color.GetX()*Mul);
	u8 ug = (u8) (color.GetY()*Mul);
	u8 ub = (u8) (color.GetZ()*Mul);
	u8 ua = (u8) (color.GetW()*Mul);
	int ix = (int) (miWidth * fmod( (float) ST.GetY(), 1.0f ));
	int iy = (int) (miHeight * fmod( (float) ST.GetX(), 1.0f ));
	int idx = (int) (4 * (ix*miHeight + iy));
	pu8data[idx+0] = ub;
	pu8data[idx+1] = ug;
	pu8data[idx+2] = ur;
	pu8data[idx+3] = ua;
	SetDirty(true);
}

///////////////////////////////////////////////////////////////////////////////

}
}
