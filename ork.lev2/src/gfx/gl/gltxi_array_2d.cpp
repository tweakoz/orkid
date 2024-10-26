////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <ork/gfx/dds.h>
#include <ork/kernel/debug.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/ui.h>
#include <ork/math/misc_math.h>
#include <ork/pch.h>
#include <ork/util/logger.h>

#include <ork/kernel/memcpy.inl>
#include <ork/profiling.inl>

#include "gl.h"

extern GLuint gLastBoundNonZeroTex;

namespace ork::lev2 {

constexpr bool DEBUG_TEXARRAY2D = true;
static logchannel_ptr_t logchan_txia2d = logger()->createChannel("GLTEXARRAY", fvec3(0.8, 0.5, 0.2), false);

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::initTextureArray2DFromData(Texture* array_tex, TextureArrayInitData tid) {

  if (DEBUG_TEXARRAY2D) {
    logchan_txia2d->log("///////////////////////////////////////////////////////////");
    logchan_txia2d->log("// GlTextureInterface::initTextureArray2DFromData ptex<%p>", array_tex);
    logchan_txia2d->log("///////////////////////////////////////////////////////////");
  }

  array_tex->_texType = ETEXTYPE_2D_ARRAY;
  int num_slices = int(tid._slices.size());
  std::vector<compressedmipchain_ptr_t> subimagedata;
  subimagedata.resize(num_slices);
  ///////////////////////////
  // scan present subimages
  //  extract max width and height
  //  and load mipchains
  ///////////////////////////
  size_t max_w      = 0;
  size_t max_h      = 0;
  size_t max_levels = 0;
  std::unordered_set<EBufferFormat> formats;
  for (int i = 0; i < num_slices; i++) {
    const auto& slice = tid._slices[i];
    auto subimg       = slice._subimg;
    if (subimg) {
      array_tex->_images.push_back(subimg);
      formats.insert(subimg->_format);
      //auto subimg_cmipc = subimg->compressedMipChainDefault();
      auto subimg_cmipc = subimg->uncompressedMipChain();
      subimagedata[i]   = subimg_cmipc;
      max_levels = std::max(max_levels, subimg_cmipc->_levels.size());
      max_w      = std::max(max_w, subimg_cmipc->_width);
      max_h      = std::max(max_h, subimg_cmipc->_height);
    }
  }
  if(formats.size()>1){
    logchan_txia2d->log( "TextureArray2D has multiple formats");
    for( auto fmt : formats){
      auto fmt_str = EBufferFormatToName(fmt);
      logchan_txia2d->log( "  format<%s>", fmt_str.c_str() );
    }
    OrkAssert(false);
  }

  max_levels -= 1;
  auto format           = *formats.begin();
  array_tex->_texFormat = format;
  GLFormatTriplet triplet(format);

  ///////////////////////////
  // generate texture object
  ///////////////////////////

  gltexobj_ptr_t glto = array_tex->_impl.makeShared<GLTextureObject>(this);
  auto texture_target = GL_TEXTURE_2D_ARRAY;
  glto->mTarget       = GL_TEXTURE_2D_ARRAY;
  GL_ERRORCHECK();
  glGenTextures(1, &glto->_textureObject);
  glBindTexture(texture_target, glto->_textureObject);
  if (array_tex->_debugName.length()) {
    mTargetGL.debugLabel(GL_TEXTURE, glto->_textureObject, array_tex->_debugName);
  }
  array_tex->_vars->makeValueForKey<GLuint>("gltexobj") = glto->_textureObject;
  // glTexStorage3D(texture_target, 4, GL_RGBA8, max_w, max_h, num_slices);

  //////////////////////////////////////////////////////////////////
  // allocate
  //////////////////////////////////////////////////////////////////

  for (int level = 0; level < max_levels; level++) {
    int w = max_w >> level;
    int h = max_h >> level;
    switch (format) {
      case EBufferFormat::RGBA_BPTC_UNORM: {
        int blocked_width  = (w + 3) & 0xfffffffc;
        int blocked_height = (h + 3) & 0xfffffffc;
        size_t size        = blocked_width * blocked_height * num_slices;
        GL_ERRORCHECK();
        glCompressedTexImage3D(
            texture_target,          // target
            level,                   // level
            triplet._internalFormat, // internal format
            w,                       // width
            h,                       // height
            num_slices,         // depth
            0,                       // border
            size,                    // size
            nullptr);         // data
        GL_ERRORCHECK();
        if (DEBUG_TEXARRAY2D){
          logchan_txia2d->log(
              "GLCTI3Da target<0x%08x> level<%d> w<%d> h<%d> d<%d> fmt<0x%08x> size<%d> data<%p>",
              texture_target,
              level,
              w,
              h,
              num_slices,
              triplet._internalFormat,
              blocked_width * blocked_height * num_slices,
              nullptr);
        }
        break;
      }
      case EBufferFormat::RGBA8:
      case EBufferFormat::BGRA8:
      case EBufferFormat::RGB8:
      case EBufferFormat::BGR8:
        GL_ERRORCHECK();
        if (DEBUG_TEXARRAY2D){
          logchan_txia2d->log(
              "GLCTI3Db target<0x%08x> level<%d> w<%d> h<%d> d<%d> fmt<0x%08x> size<%d> data<%p>",
              texture_target,
              level,
              w,
              h,
              num_slices,
              triplet._internalFormat,
              w * h * num_slices,
              nullptr);
        }
        glTexImage3D(
            texture_target,          // target
            level,                   // level
            triplet._internalFormat, // internal format
            w,                       // width
            h,                       // height
            num_slices,         // depth
            0,                       // border
            triplet._format,         // format
            triplet._type,           // type
            nullptr);         // data
        GL_ERRORCHECK();
        break;
      default:
        OrkAssert(false);
    }
  } // for (int level = 0; level < max_levels; level++) {

  //////////////////////////////////////////////////////////////////
  // fill in image data
  //////////////////////////////////////////////////////////////////

  glTexParameteri(texture_target, GL_TEXTURE_BASE_LEVEL, 0);
  GL_ERRORCHECK();
  glTexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL, max_levels - 1);
  GL_ERRORCHECK();
  glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  GL_ERRORCHECK();
  glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  GL_ERRORCHECK();

  if (1)
    for (int isub = 0; isub < num_slices; isub++) {

      auto subimg_cmipc = subimagedata[isub];
      if (subimg_cmipc) {

        GL_ERRORCHECK();
        int num_levels = int(subimg_cmipc->_levels.size()) - 1;
        if (num_levels < max_levels) {
          glTexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL, num_levels - 1);
        }
        for (int level = 0; level < num_levels; level++) {

          auto mip      = subimg_cmipc->_levels[level];
          auto mip_w    = mip._width;
          auto mip_h    = mip._height;
          auto mip_data = mip._data;

          switch (format) {
            case EBufferFormat::RGBA_BPTC_UNORM: {
              GL_ERRORCHECK();
              int blocked_width  = (mip_w + 3) & 0xfffffffc;
              int blocked_height = (mip_h + 3) & 0xfffffffc;
              if (DEBUG_TEXARRAY2D){
                logchan_txia2d->log(
                    "GLCTSI3Da target<0x%08x> level<%d> x<%d> y<%d> z<%d> w<%d> h<%d> d<%d> fmt<0x%08x> size<%zu> data<%p>",
                    texture_target,
                    level,
                    0,
                    0,
                    isub,
                    blocked_width,
                    blocked_height,
                    1,
                    triplet._internalFormat,
                    mip_data->length(),
                    mip_data->data());
              }
              glCompressedTexSubImage3D(
                  texture_target,          // target
                  level,                   // level
                  0,                       // xoffset
                  0,                       // yoffset
                  isub,                    // zoffset (slice)
                  mip_w,                   // width
                  mip_h,                   // height
                  1,                       // depth (of data for slice)
                  triplet._internalFormat, // format
                  mip_data->length(),      // size
                  mip_data->data());       // data

              GL_ERRORCHECK();
              break;
            }
            default:
              GL_ERRORCHECK();
              if (DEBUG_TEXARRAY2D){
                logchan_txia2d->log(
                    "GLCTSI3Db target<0x%08x> level<%d> x<%d> y<%d> z<%d> w<%zu> h<%zu> d<%d> fmt<0x%08x> size<%zu> data<%p>",
                    texture_target,
                    level,
                    0,
                    0,
                    isub,
                    mip_w,
                    mip_h,
                    1,
                    triplet._internalFormat,
                    mip_data->length(),
                    mip_data->data());
              }
              glTexSubImage3D(
                  texture_target,    // target
                  level,             // level
                  0,                 // xoffset
                  0,                 // yoffset
                  isub,              // zoffset (slice)
                  mip_w,             // width
                  mip_h,             // height
                  1,                 // depth (of data for slice)
                  triplet._format,   // format
                  triplet._type,     // type
                  mip_data->data()); // data
              GL_ERRORCHECK();
              break;
          }
        }
      }
    } //   if(0)for (int isub = 0; isub < num_slices; isub++) {

  ///////////////////////////

  if (DEBUG_TEXARRAY2D) {
    logchan_txia2d->log("TextureArray maxw<%d> maxh<%d> depth<%d>", max_w, max_h, num_slices);
    logchan_txia2d->log("///////////////////////////////////////////////////////////");
  }
  array_tex->_width  = max_w;
  array_tex->_height = max_h;
  array_tex->_depth  = num_slices;
  array_tex->_texFormat = format;

  array_tex->_residenceState.fetch_or(1);
  // OrkAssert(num_slices==0);
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::updateTextureArraySlice(Texture* array_tex, int slice, image_ptr_t img) {
  bool ok = true;
  ok &= (array_tex->_texType == ETEXTYPE_2D_ARRAY);
  ok &= (slice < array_tex->_depth);
  ok &= (img != nullptr);
  ok &= (img->_format == array_tex->_texFormat);
  ok &= (img->_width == array_tex->_width);
  ok &= (img->_height == array_tex->_height);
  ok &= (img->_depth == 1);

  if( not ok ){
    return;
  }

  auto glto = array_tex->_impl.getShared<GLTextureObject>();
  auto texture_target = GL_TEXTURE_2D_ARRAY;
  glBindTexture(texture_target, glto->_textureObject);
  auto format = array_tex->_texFormat;
  GLFormatTriplet triplet(format);
  auto subimg_cmipc = img->uncompressedMipChain();
  int num_levels = int(subimg_cmipc->_levels.size()) - 1;
  glTexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL, num_levels - 1);
  for (int level = 0; level < num_levels; level++) {
    auto mip      = subimg_cmipc->_levels[level];
    auto mip_w    = mip._width;
    auto mip_h    = mip._height;
    auto mip_data = mip._data;
    switch (format) {
      case EBufferFormat::RGBA_BPTC_UNORM: {
        GL_ERRORCHECK();
        int blocked_width  = (mip_w + 3) & 0xfffffffc;
        int blocked_height = (mip_h + 3) & 0xfffffffc;
        glCompressedTexSubImage3D(
            texture_target,          // target
            level,                   // level
            0,                       // xoffset
            0,                       // yoffset
            slice,                   // zoffset (slice)
            mip_w,                   // width
            mip_h,                   // height
            1,                       // depth (of data for slice)
            triplet._internalFormat, // format
            mip_data->length(),      // size
            mip_data->data());       // data
        GL_ERRORCHECK();
        break;
      }
      case EBufferFormat::RGBA8:
      case EBufferFormat::BGRA8:
      case EBufferFormat::RGB8:
      case EBufferFormat::BGR8:{
        GL_ERRORCHECK();
        glTexSubImage3D(
            texture_target,    // target
            level,             // level
            0,                 // xoffset
            0,                 // yoffset
            slice,             // zoffset (slice)
            mip_w,             // width
            mip_h,             // height
            1,                 // depth (of data for slice)
            triplet._format,   // format
            triplet._type,     // type
            mip_data->data()); // data
        GL_ERRORCHECK();
        break;
      }
      default:
        OrkAssert(false);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
