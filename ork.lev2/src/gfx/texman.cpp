////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/kernel/spawner.h>
#include <ork/kernel/opq.h>

#include <ork/lev2/config.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/gfx/dds.h>
#include <ork/lev2/gfx/texman.h>
#include <math.h>
#include <ork/lev2/lev2_asset.h>
#if defined(ENABLE_ISPC)
#include <ispc_texcomp.h>
#endif

namespace ork::lev2 {

std::atomic<size_t> Texture::_texture_count = 0;

///////////////////////////////////////////////////////////////////////////////
// TextureProvider implementations
///////////////////////////////////////////////////////////////////////////////

TextureProvider::~TextureProvider() {}

///////////////////////////////////////////////////////////////////////////////

void TextureSamplingModeData::presetPointAndClamp() {
  _texAddrModeS   = TextureAddressMode::CLAMP;
  _texAddrModeT   = TextureAddressMode::CLAMP;
  _texAddrModeR   = TextureAddressMode::CLAMP;
  _texFiltModeMin = ETextureMinifyFilterMode::NEAREST;
  _texFiltModeMag = ETextureMagnifyFilterMode::NEAREST;
}

///////////////////////////////////////////////////////////////////////////////

void TextureSamplingModeData::presetTrilinearWrap() {
  _texAddrModeS   = TextureAddressMode::WRAP;
  _texAddrModeT   = TextureAddressMode::WRAP;
  _texAddrModeR   = TextureAddressMode::WRAP;
  _texFiltModeMin = ETextureMinifyFilterMode::LINEAR_MIPMAP_LINEAR;
  _texFiltModeMag = ETextureMagnifyFilterMode::LINEAR;
}

///////////////////////////////////////////////////////////////////////////////

void Texture::RegisterLoaders(void) {
}

///////////////////////////////////////////////////////////////////////////////

texture_ptr_t Texture::LoadUnManaged(const AssetPath& fname) {
  texture_ptr_t ptex = std::make_shared<Texture>();
  auto target        = lev2::contextForCurrentThread();
  bool bok           = target->TXI()->LoadTexture(fname, ptex);
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
  _vars = std::make_shared<asset::vars_t>();
  _residenceState.store(0);
  _texture_count.fetch_add(1);
  // printf( "_texture_count: %zu\n", _texture_count.load() );
}

Texture::Texture(ipctexture_ptr_t external_memory)
    : _asset(nullptr)
    , _external_memory(external_memory) {
  _vars = std::make_shared<asset::vars_t>();
  _residenceState.store(0);
  int texcount = _texture_count.fetch_add(1);
  // printf( "Texture::_texture_count: %zu\n", texcount+1 );
}

///////////////////////////////////////////////////////////////////////////////

// Phase-6.3 destructor: defer GPU-resource teardown to the main render
// thread when dropped from any other thread.
//
// Rationale: vkDestroy* / GL deletions must run on a thread with a valid
// owning context. The main render thread is the longest-lived graphics
// thread in the process (outlives the loader thread and any worker
// contexts), so its _deferredOps queue is the canonical safe destination.
// This makes "drop this Texture anywhere" Just Work without each caller
// having to think about threads.
//
// Fall-through cases (inline destruction):
//   - no main render context (very early init / post-shutdown)
//   - already on the render thread
//   - the texture never acquired backend state (_impl/_impl_2 both empty)
//
// _impl is a static_variant holding a shared_ptr to a backend impl, so
// copying it just bumps a refcount; the original is then cleared so its
// member-destructor sees an empty variant. _impl_2 follows the same
// pattern (typical backend impls there are refcount-based external-memory
// wrappers); if a future backend introduces a unique-ownership impl in
// _impl_2 we'll need a more careful extraction there.
Texture::~Texture() {
  _texture_count.fetch_add(-1);

  Context* render_ctx = GfxEnv::mainRenderContext();
  bool needs_defer    = render_ctx
                     && (contextForCurrentThread() != render_ctx)
                     && (_impl.isSet() || _impl_2.isSet());
  if (!needs_defer) {
    return; // inline member destruction runs normally
  }

  struct GpuTeardownHolder {
    svarshp_t _impl;
    svar16_t  _impl_2;
  };
  auto holder = std::make_shared<GpuTeardownHolder>();
  holder->_impl   = _impl;
  holder->_impl_2 = _impl_2;
  _impl.clear();
  _impl_2.clear();
  render_ctx->enqueueDeferredOp([holder](Context*) {
    // Holder destructor runs on the render thread → impl shared_ptrs
    // refcount-decrement → backend impl destructors → vkDestroy*.
  });
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
      case EBufferFormat::Z32F:
        level->_length = w * h * 4 * sizeof(float);
        break;
      case EBufferFormat::Z16F:
        level->_length = w * h * sizeof(uint16_t);
        break;
#if defined(ENABLE_ISPC)
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

#if defined(ENABLE_ISPC)
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
#endif

asset::loadrequest_ptr_t Texture::loadRequest() const {
  return _asset ? _asset->_load_request : asset::loadrequest_ptr_t(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

TextureArray::TextureArray() {
  _width     = 0;
  _height    = 0;
  _maxslices = 0;
  _tex       = std::make_shared<Texture>();
}
TextureArray::~TextureArray() {
}
///////////////////////////////////////////////////////////////////////////////

void TextureArray::resize(size_t w, size_t h, size_t maxslices, EBufferFormat efmt) {
  _width     = w;
  _height    = h;
  _maxslices = maxslices;
  _format    = efmt;
  _free_slices.clear();
  _dirty_slices.clear();
  for (size_t i = 0; i < maxslices; i++) {
    _free_slices.insert(i);
  }
}

///////////////////////////////////////////////////////////////////////////////

void TextureArray::_conform(EBufferFormat fmt) {
  /////////////////////
  // note which images need to be converted
  /////////////////////
  std::unordered_set<size_t> images_to_chfmt;
  std::vector<image_ptr_t> final_images;
  size_t index = 0;
  for (auto it_img : _images) {
    size_t map_index = it_img.first;
    OrkAssert(map_index == index);
    auto img = it_img.second;
    final_images.push_back(img);
    index++;
  }
  index = 0;
  for (auto img : final_images) {
    if (img->_format != fmt) {
      images_to_chfmt.insert(index);
    }
    index++;
  }
  // Converted HERE on the calling (non-pool) thread rather than from pool ops:
  // convertFromImageToFormat is itself a fork-join over the concurrentQueue, and a
  // fork-join running ON a pool worker can wait forever for row chunks that have no
  // free worker left to run in. Inline costs nothing in parallelism — each conversion
  // already fans across the whole pool internally, and this thread was doing nothing
  // but spin-waiting for them anyway.
  for (size_t index : images_to_chfmt) {
    auto prvimg         = final_images[index];
    auto newimg         = std::make_shared<Image>();
    final_images[index] = newimg;
    newimg->convertFromImageToFormat(*prvimg, fmt);
  }
  images_to_chfmt.clear();
  /////////////////////
  // resize the images
  /////////////////////
  std::unordered_set<size_t> images_to_resize;
  index = 0;
  for (auto it_img : _images) {
    size_t map_index = it_img.first;
    OrkAssert(map_index == index);
    auto img = it_img.second;
    if (img->_width != _width || img->_height != _height) {
      images_to_resize.insert(index);
    }
    index++;
  }
  std::atomic<int> sync_resize = 0;
  for (size_t index : images_to_resize) {
    auto prvimg         = final_images[index];
    auto newimg         = std::make_shared<Image>();
    final_images[index] = newimg;
    auto OP             = [=, &sync_resize]() {
      newimg->resizedOf(*prvimg, _width, _height);
      sync_resize--;
    };
    opq::concurrentQueue()->enqueue(OP);
  }
  while (sync_resize > 0) {
    usleep(1000);
  }
  images_to_resize.clear();
  //////////////////////////
  _images.clear();
  for (size_t index = 0; index < final_images.size(); index++) {
    auto img = final_images[index];
    OrkAssert(img->_width == _width);
    OrkAssert(img->_height == _height);
    OrkAssert(img->_format == fmt);
    _images[index] = img;
  }
}

///////////////////////////////////////////////////////////////////////////////

texturearraysliceref_ptr_t TextureArray::load(const std::string& path) {
  auto it_first = _free_slices.begin();
  OrkAssert(it_first != _free_slices.end());
  size_t slice_index = *it_first;
  _free_slices.erase(it_first);
  auto new_slice  = std::make_shared<TextureArraySliceRef>(this, slice_index);
  auto temp_image = std::make_shared<Image>();
  temp_image->readFromFile(path);
  auto formatted_image = std::make_shared<Image>();
  formatted_image->convertFromImageToFormat(*temp_image, _format);
  auto resized_image = std::make_shared<Image>();
  resized_image->resizedOf(*formatted_image, _width, _height);
  _images[slice_index] = resized_image;
  _dirty_slices.insert(slice_index);
  //_slices_by_path[path] = slice_index;
  if (0)
    printf(
        "TextureArray<%p>::load slice_index<%zu> path<%s> dim<%zu %zu> new_slice<%p>\n",
        (void*)this,
        slice_index,
        path.c_str(),
        _width,
        _height,
        (void*)new_slice.get());
  return new_slice;
}

///////////////////////////////////////////////////////////////////////////////

texturearraysliceref_ptr_t TextureArray::slice(size_t index) const {
  auto mutable_this = const_cast<TextureArray*>(this);
  auto slice        = std::make_shared<TextureArraySliceRef>(mutable_this, index);
  return slice;
}

///////////////////////////////////////////////////////////////////////////////

TextureArraySliceRef::TextureArraySliceRef(TextureArray* ary, int slice)
    : _array(ary)
    , _slice(slice) {
}

///////////////////////////////////////////////////////////////////////////////

rtgroup_ptr_t TextureArraySliceRef::createRenderTarget(Context* ctx) {
  auto rtg    = std::make_shared<RtGroup>(ctx, _array->_width, _array->_height);
  rtg->_usage = "arrayslice"_crcu;
  rtg->_slice = this;
  rtg->_name  = _array->_debugName + FormatString("[%d]", _slice);
  return rtg;
}

///////////////////////////////////////////////////////////////////////////////
LambdaTextureProvider::LambdaTextureProvider(std::function<texture_ptr_t()> func)
    : _func(func) {
}
texture_ptr_t LambdaTextureProvider::getTexture() {
  return _func();
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
