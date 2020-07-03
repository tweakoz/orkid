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
  mTexAddrModeU   = TextureAddressMode::CLAMP;
  mTexAddrModeV   = TextureAddressMode::CLAMP;
  mTexFiltModeMin = ETEXFILT_POINT;
  mTexFiltModeMag = ETEXFILT_POINT;
  mTexFiltModeMip = ETEXFILT_POINT;
}

///////////////////////////////////////////////////////////////////////////////

void TextureSamplingModeData::PresetTrilinearWrap() {
  mTexAddrModeU   = TextureAddressMode::WRAP;
  mTexAddrModeV   = TextureAddressMode::WRAP;
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

texture_ptr_t Texture::createBlank(int iw, int ih, EBufferFormat efmt) {
  auto texture = std::make_shared<Texture>();

  texture->_width  = iw;
  texture->_height = ih;

  switch (efmt) {
    case EBufferFormat::RGBA8:
    case EBufferFormat::R32F:
      texture->_data = calloc(iw * ih * 4, 1);
      break;
    case EBufferFormat::RGBA16UI:
      texture->_data = calloc(iw * ih * 8, 1);
      break;
    case EBufferFormat::RGBA32F:
      texture->_data = calloc(iw * ih * 16, 1);
      break;
    default:
      assert(false);
  }
  return texture;
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
      case EBufferFormat::RGBA32F:
        level->_length = w * h * 4 * sizeof(float);
        break;
      case EBufferFormat::RGBA16F:
        level->_length = w * h * 4 * sizeof(uint16_t);
        break;
      case EBufferFormat::RGBA8:
        level->_length = w * h * 4 * sizeof(uint8_t);
        break;
      case EBufferFormat::R32F:
      case EBufferFormat::Z24S8:
      case EBufferFormat::Z32:
        level->_length = w * h * 4 * sizeof(float);
        break;
      case EBufferFormat::Z16:
        level->_length = w * h * sizeof(uint16_t);
        break;
#if !defined(__APPLE__)
      case EBufferFormat::RGBA_BPTC_UNORM:
        level->_length = w * h;
        break;
      case EBufferFormat::SRGB_ALPHA_BPTC_UNORM:
        level->_length = w * h;
        break;
      case EBufferFormat::RGBA_ASTC_4X4:
        level->_length = w * h;
        break;
      case EBufferFormat::SRGB_ASTC_4X4:
        level->_length = w * h;
        break;
#endif
      case EBufferFormat::DEPTH:
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
