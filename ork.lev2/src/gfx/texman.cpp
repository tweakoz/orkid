////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/kernel/spawner.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/gfx/dds.h>
#include <ork/lev2/gfx/texman.h>
#include <math.h>
#include <ispc_texcomp.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

void TextureSamplingModeData::PresetPointAndClamp() {
  mTexAddrModeU   = ETEXADDR_CLAMP;
  mTexAddrModeV   = ETEXADDR_CLAMP;
  mTexFiltModeMin = ETEXFILT_POINT;
  mTexFiltModeMag = ETEXFILT_POINT;
  mTexFiltModeMip = ETEXFILT_POINT;
}

///////////////////////////////////////////////////////////////////////////////

void TextureSamplingModeData::PresetTrilinearWrap() {
  mTexAddrModeU   = ETEXADDR_WRAP;
  mTexAddrModeV   = ETEXADDR_WRAP;
  mTexFiltModeMin = ETEXFILT_LINEAR;
  mTexFiltModeMag = ETEXFILT_LINEAR;
  mTexFiltModeMip = ETEXFILT_LINEAR;
}

///////////////////////////////////////////////////////////////////////////////

void Texture::RegisterLoaders(void) {
}

///////////////////////////////////////////////////////////////////////////////

Texture* Texture::LoadUnManaged(const AssetPath& fname) {
  Texture* ptex = new Texture;
  bool bok      = GfxEnv::GetRef().loadingContext()->TXI()->LoadTexture(fname, ptex);
  return ptex;
}

///////////////////////////////////////////////////////////////////////////////

Texture* Texture::CreateBlank(int iw, int ih, EBufferFormat efmt) {
  Texture* pTex = new Texture;

  pTex->_width  = iw;
  pTex->_height = ih;

  switch (efmt) {
    case EBUFFMT_RGBA8:
    case EBUFFMT_R32F:
      pTex->_data = calloc(iw * ih * 4, 1);
      break;
    case EBUFFMT_RGBA32F:
      pTex->_data = calloc(iw * ih * 16, 1);
      break;
    default:
      assert(false);
  }
  return pTex;
}

///////////////////////////////////////////////////////////////////////////////

Texture::Texture(const TextureAsset* asset)
    : _asset(asset) {
}

///////////////////////////////////////////////////////////////////////////////

Texture::~Texture() {
  Context* pTARG = GfxEnv::GetRef().loadingContext();
  pTARG->TXI()->DestroyTexture(this);
}

///////////////////////////////////////////////////////////////////////////////

MipChain::MipChain(int w, int h, EBufferFormat fmt, ETextureType typ) {
  assert(typ == ETEXTYPE_2D);
  _format = fmt;
  _type   = typ;
  while (w >= 1 and h >= 1) {
    mipchainlevel_t level = std::make_shared<MipChainLevel>();
    _levels.push_back(level);
    level->_width  = w;
    level->_height = h;
    switch (fmt) {
      case EBUFFMT_RGBA32F:
        level->_length = w * h * 4 * sizeof(float);
        break;
      case EBUFFMT_RGBA16F:
        level->_length = w * h * 4 * sizeof(uint16_t);
        break;
      case EBUFFMT_RGBA8:
        level->_length = w * h * 4 * sizeof(uint8_t);
        break;
      case EBUFFMT_R32F:
      case EBUFFMT_Z24S8:
      case EBUFFMT_Z32:
        level->_length = w * h * 4 * sizeof(float);
        break;
      case EBUFFMT_Z16:
        level->_length = w * h * sizeof(uint16_t);
        break;
#if !defined(__APPLE__)
      case EBUFFMT_RGBA_BPTC_UNORM:
        level->_length = w * h;
        break;
      case EBUFFMT_SRGB_ALPHA_BPTC_UNORM:
        level->_length = w * h;
        break;
      case EBUFFMT_RGBA_ASTC_4X4:
        level->_length = w * h;
        break;
      case EBUFFMT_SRGB_ASTC_4X4:
        level->_length = w * h;
        break;
#endif
      case EBUFFMT_DEPTH:
      default:
        assert(false);
    }
    level->_data = malloc(level->_length);
    w >>= 1;
    h >>= 1;
  }
}
MipChain::~MipChain() {
  for (auto l : _levels) {
    free(l->_data);
  }
}

///////////////////////////////////////////////////////////////////////////////
struct Image {

  ~Image() {
    if (_pixels)
      free(_pixels);
  }
  inline void init(size_t w, size_t h, size_t numc) {
    _numcomponents = numc;
    _width         = w;
    _height        = h;
    _pixels        = (uint8_t*)malloc(_width * _height * numc);
  }
  void downsample(Image& imgout) const;

  uint8_t* _pixels      = nullptr;
  size_t _width         = 0;
  size_t _height        = 0;
  size_t _numcomponents = 4; // 3 or 4
};
///////////////////////////////////////////////////////////////////////////////

void Image::downsample(Image& imgout) const {
  OrkAssert(_width & 1 == 0);
  OrkAssert(_height & 1 == 0);
  imgout.init(_width >> 1, _height >> 1, _numcomponents);
  // todo parallelize (CPU(ISPC) or GPU(CUDA))
  for (size_t y = 0; y < imgout._height; y++) {
    size_t ya = y * 2;
    size_t yb = ya + 1;
    for (size_t x = 0; x < imgout._width; x++) {
      size_t xa             = x * 2;
      size_t xb             = xa + 1;
      size_t out_index      = (y * imgout._width + x) * _numcomponents;
      size_t inp_index_xaya = (ya * _width + xa) * _numcomponents;
      size_t inp_index_xbya = (ya * _width + xb) * _numcomponents;
      size_t inp_index_xayb = (yb * _width + xa) * _numcomponents;
      size_t inp_index_xbyb = (yb * _width + xb) * _numcomponents;
      for (size_t c = 0; c < _numcomponents; c++) {
        double xaya                   = double(_pixels[inp_index_xaya + c]);
        double xbya                   = double(_pixels[inp_index_xbya + c]);
        double xayb                   = double(_pixels[inp_index_xayb + c]);
        double xbyb                   = double(_pixels[inp_index_xbyb + c]);
        double avg                    = (xaya + xbya + xayb + xbyb) * 0.25;
        uint8_t uavg                  = uint8_t(avg * 255.0f);
        imgout._pixels[out_index + c] = uavg;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void bc7testcomp() {

  printf("Generating uncompressed texture...\n");

  constexpr size_t DIM = 4096;
  rgba_surface surface;
  surface.ptr     = (uint8_t*)malloc(DIM * DIM * 4);
  auto compressed = (uint8_t*)malloc(DIM * DIM);
  surface.width   = DIM;
  surface.height  = DIM;
  surface.stride  = DIM * 4;
  for (size_t i = 0; i < DIM * DIM * 4; i++) {
    surface.ptr[i] = uint8_t(rand() & 0xff);
  }

  ork::Timer timer;

  bc7_enc_settings settings;

  /////////////////////////////////////////////////////
  // ULTRAFAST
  /////////////////////////////////////////////////////

  printf("STARTING BC7 compression [ULTRAFAST]...\n");
  timer.Start();
  GetProfile_ultrafast(&settings);
  CompressBlocksBC7(&surface, compressed, &settings);
  float time = timer.SecsSinceStart();
  float MPPS = 16.0 / time;
  printf("DONE BC7 compression [ULTRAFAST] time<%g> MPPS<%g>\n", time, MPPS);

  /////////////////////////////////////////////////////
  // FAST
  /////////////////////////////////////////////////////

  printf("STARTING BC7 compression [FAST]...\n");
  timer.Start();
  GetProfile_fast(&settings);
  CompressBlocksBC7(&surface, compressed, &settings);
  time = timer.SecsSinceStart();
  MPPS = 16.0 / time;
  printf("DONE BC7 compression [FAST] time<%g> MPPS<%g>\n", time, MPPS);

  /////////////////////////////////////////////////////
  // BASIC
  /////////////////////////////////////////////////////

  printf("STARTING BC7 compression [BASIC]...\n");
  timer.Start();
  GetProfile_basic(&settings);
  CompressBlocksBC7(&surface, compressed, &settings);
  time = timer.SecsSinceStart();
  MPPS = 16.0 / time;
  printf("DONE BC7 compression [BASIC] time<%g> MPPS<%g>\n", time, MPPS);

  /////////////////////////////////////////////////////
  // SLOW
  /////////////////////////////////////////////////////

  printf("STARTING BC7 compression [SLOW]...\n");
  timer.Start();
  GetProfile_slow(&settings);
  CompressBlocksBC7(&surface, compressed, &settings);
  time = timer.SecsSinceStart();
  MPPS = 16.0 / time;
  printf("DONE BC7 compression [SLOW] time<%g> MPPS<%g>\n", time, MPPS);
}

///////////////////////////////////////////////////////////////////////////////

void astctestcomp() {

  printf("Generating uncompressed texture...\n");

  constexpr size_t DIM = 4096;
  rgba_surface surface;
  surface.ptr     = (uint8_t*)malloc(DIM * DIM * 4);
  auto compressed = (uint8_t*)malloc(DIM * DIM);
  surface.width   = DIM;
  surface.height  = DIM;
  surface.stride  = DIM * 4;
  for (size_t i = 0; i < DIM * DIM * 4; i++) {
    surface.ptr[i] = uint8_t(rand() & 0xff);
  }

  ork::Timer timer;

  astc_enc_settings settings;

  constexpr int block_width  = 8;
  constexpr int block_height = 8;

  /////////////////////////////////////////////////////
  // ULTRAFAST
  /////////////////////////////////////////////////////

  printf("STARTING ASTC compression [FAST]...\n");
  timer.Start();
  GetProfile_astc_alpha_fast(&settings, block_width, block_height);
  CompressBlocksASTC(&surface, compressed, &settings);
  float time = timer.SecsSinceStart();
  float MPPS = 16.0 / time;
  printf("DONE ASTC compression [FAST] time<%g> MPPS<%g>\n", time, MPPS);

  /////////////////////////////////////////////////////
  // FAST
  /////////////////////////////////////////////////////

  printf("STARTING ASTC compression [SLOW]...\n");
  timer.Start();
  GetProfile_astc_alpha_slow(&settings, block_width, block_height);
  CompressBlocksASTC(&surface, compressed, &settings);
  time = timer.SecsSinceStart();
  MPPS = 16.0 / time;
  printf("DONE ASTC compression [SLOW] time<%g> MPPS<%g>\n", time, MPPS);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
