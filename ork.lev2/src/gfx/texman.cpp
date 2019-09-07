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

	pTex->_width = iw;
	pTex->_height = ih;

	switch( efmt )
	{
		case EBUFFMT_RGBA32:
		case EBUFFMT_F32:
			pTex->_data = calloc(iw*ih*4,1);
			break;
		case EBUFFMT_RGBA128:
      pTex->_data = calloc(iw*ih*16,1);
			break;
		default:
			assert(false);
	}
	return pTex;
}

///////////////////////////////////////////////////////////////////////////////

Texture::Texture()
{}

///////////////////////////////////////////////////////////////////////////////

Texture::~Texture()
{
	GfxTarget* pTARG = GfxEnv::GetRef().GetLoaderTarget();
	pTARG->TXI()->DestroyTexture( this );
}

///////////////////////////////////////////////////////////////////////////////

MipChain::MipChain(int w, int h,EBufferFormat fmt,ETextureType typ){
  assert(typ==ETEXTYPE_2D);
  _format = fmt;
  _type = typ;
  while(w>=1 and h>=1){
    mipchainlevel_t level = std::make_shared<MipChainLevel>();
    _levels.push_back(level);
    level->_width = w;
    level->_height = h;
    switch(fmt){
      case EBUFFMT_RGBA128:
        level->_length = w*h*4*sizeof(float);
        break;
      case EBUFFMT_RGBA64:
        level->_length = w*h*4*sizeof(uint16_t);
        break;
      case EBUFFMT_RGBA32:
        level->_length = w*h*4*sizeof(uint8_t);
        break;
      case EBUFFMT_F32:
      case EBUFFMT_Z24S8:
      case EBUFFMT_Z32:
        level->_length = w*h*4*sizeof(float);
        break;
      case EBUFFMT_Z16:
        level->_length = w*h*sizeof(uint16_t);
        break;
      case EBUFFMT_DEPTH:
      default:
        assert(false);
    }
    level->_data = malloc(level->_length);
    w>>=1;
    h>>=1;
  }
}
MipChain::~MipChain(){
  for( auto l : _levels ){
    free(l->_data);
  }
}



///////////////////////////////////////////////////////////////////////////////

}
}
