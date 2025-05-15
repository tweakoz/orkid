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

constexpr bool DEBUG_TEXARRAY2D        = true;
static logchannel_ptr_t logchan_txia2d = logger()->createChannel("GLTEXARRAY", fvec3(0.8, 0.5, 0.2), false);

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::initTextureArray2DFromData(TextureArray* array, TextureArrayInitData tid) {

  bool DEBUG_TEXARRAY2D = false;
  if (DEBUG_TEXARRAY2D) {
    logchan_txia2d->log("///////////////////////////////////////////////////////////");
    logchan_txia2d->log("// GlTextureInterface::initTextureArray2DFromData array<%p>", array);
    logchan_txia2d->log("///////////////////////////////////////////////////////////");
  }

  array->_tex->_texType = ETEXTYPE_2D_ARRAY;
  int num_slices        = int(tid._slices.size());
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
    auto mipchain     = slice._cmipchain;
    if (subimg) {
      array->_images[i] = subimg;
      formats.insert(subimg->_format);
      // auto subimg_cmipc = subimg->compressedMipChainDefault();
      auto subimg_cmipc = subimg->uncompressedMipChain();
      subimagedata[i]   = subimg_cmipc;
      max_levels        = std::max(max_levels, subimg_cmipc->_levels.size());
      max_w             = std::max(max_w, subimg_cmipc->_width);
      max_h             = std::max(max_h, subimg_cmipc->_height);
    } else if (mipchain) {
      subimagedata[i] = mipchain;
      max_levels      = std::max(max_levels, mipchain->_levels.size());
      max_w           = std::max(max_w, mipchain->_width);
      max_h           = std::max(max_h, mipchain->_height);
      formats.insert(mipchain->_format);
    } else {
      OrkAssert(false);
    }
  }
  if (formats.size() > 1) {
    logchan_txia2d->log("TextureArray2D has multiple formats");
    for (auto fmt : formats) {
      auto fmt_str = EBufferFormatToName(fmt);
      logchan_txia2d->log("  format<%s>", fmt_str.c_str());
    }
    OrkAssert(false);
  }

  max_levels -= 1;
  auto format             = *formats.begin();
  array->_tex->_texFormat = format;
  GLFormatTriplet triplet(format);

  ///////////////////////////
  // generate texture object
  ///////////////////////////

  gltexobj_ptr_t glto = array->_tex->_impl.makeShared<GLTextureObject>(this);
  auto texture_target = GL_TEXTURE_2D_ARRAY;
  glto->mTarget       = GL_TEXTURE_2D_ARRAY;
  GL_ERRORCHECK();
  glGenTextures(1, &glto->_textureObject);
  glBindTexture(texture_target, glto->_textureObject);

  _texture_set[glto->_textureObject] = array->_tex.get();

  if (array->_tex->_debugName.length()) {
    mTargetGL.debugLabel(GL_TEXTURE, glto->_textureObject, array->_tex->_debugName);
  }
  array->_tex->_vars->makeValueForKey<GLuint>("gltexobj") = glto->_textureObject;
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
            num_slices,              // depth
            0,                       // border
            size,                    // size
            nullptr);                // data
        GL_ERRORCHECK();
        if (DEBUG_TEXARRAY2D) {
          logchan_txia2d->log(
              "GLCTI3Da obj<%d> target<0x%08x> level<%d> w<%d> h<%d> d<%d> fmt<0x%08x> size<%d> data<%p>",
              int(glto->_textureObject),
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
      case EBufferFormat::RGBA16:
      case EBufferFormat::RGB16:
        GL_ERRORCHECK();
        if (DEBUG_TEXARRAY2D) {
          logchan_txia2d->log(
              "GLCTI3Db obj<%d> target<0x%08x> level<%d> w<%d> h<%d> d<%d> fmt<0x%08x> size<%d> data<%p>",
              int(glto->_textureObject),
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
            num_slices,              // depth
            0,                       // border
            triplet._format,         // format
            triplet._type,           // type
            nullptr);                // data
        GL_ERRORCHECK();
        break;
      case EBufferFormat::RGBA8:
      case EBufferFormat::BGRA8:
      case EBufferFormat::RGB8:
      case EBufferFormat::BGR8:
        GL_ERRORCHECK();
        if (DEBUG_TEXARRAY2D) {
          logchan_txia2d->log(
              "GLCTI3Db obj<%d> target<0x%08x> level<%d> w<%d> h<%d> d<%d> fmt<0x%08x> size<%d> data<%p>",
              int(glto->_textureObject),
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
            num_slices,              // depth
            0,                       // border
            triplet._format,         // format
            triplet._type,           // type
            nullptr);                // data
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
              if (DEBUG_TEXARRAY2D) {
                logchan_txia2d->log(
                    "GLCTSI3Da obj<%d> target<0x%08x> level<%d> x<%d> y<%d> z<%d> w<%d> h<%d> d<%d> fmt<0x%08x> size<%zu> data<%p>",
                    int(glto->_textureObject),
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
              if (DEBUG_TEXARRAY2D) {
                logchan_txia2d->log(
                    "GLCTSI3Db obj<%s> target<0x%08x> level<%d> x<%d> y<%d> z<%d> w<%zu> h<%zu> d<%d> fmt<0x%08x> size<%zu> data<%p>",
                    int(glto->_textureObject),
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
  array->_width           = max_w;
  array->_height          = max_h;
  array->_maxslices       = num_slices;
  array->_tex->_width     = max_w;
  array->_tex->_height    = max_h;
  array->_tex->_depth     = num_slices;
  array->_tex->_texFormat = format;

  array->_tex->_residenceState.fetch_or(1);
  // OrkAssert(num_slices==0);
  GL_ERRORCHECK();

  array->_tex->_dirty = false;
  array->_dirty_slices.clear();
  array->_free_slices.clear();
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::updateTextureArraySlice(TextureArraySliceRef* slice_ref, image_ptr_t img) {

  auto array      = slice_ref->_array;
  int slice_index = slice_ref->_slice;

  bool ok = true;
  ok &= (array->_tex->_texType == ETEXTYPE_2D_ARRAY);
  ok &= (slice_index < array->_tex->_depth);
  ok &= (img != nullptr);
  ok &= (img->_format == array->_tex->_texFormat);
  ok &= (img->_width == array->_tex->_width);
  ok &= (img->_height == array->_tex->_height);
  ok &= (img->_depth == 1);

  if (not ok) {
    printf("not ok\n");
    printf("array<%p> slice<%d> img<%p>\n", array, slice_index, img.get());
    printf("img->_format<%d>\n", int(img->_format));
    printf("img->_width<%d>\n", int(img->_width));
    printf("img->_height<%d>\n", int(img->_height));
    printf("img->_depth<%d>\n", int(img->_depth));
    printf("array->_tex->_texFormat<%d>\n", int(array->_tex->_texFormat));
    printf("array->_tex->_width<%d>\n", int(array->_tex->_width));
    printf("array->_tex->_height<%d>\n", int(array->_tex->_height));
    printf("array->_tex->_depth<%d>\n", int(array->_tex->_depth));
    printf("array->_tex->_texType<%d>\n", int(array->_tex->_texType));
    return;
  }

  auto glto           = array->_tex->_impl.getShared<GLTextureObject>();
  auto texture_target = GL_TEXTURE_2D_ARRAY;

  if (DEBUG_TEXARRAY2D) {
    logchan_txia2d->log("///////////////////////////////////////////////////////////");
    logchan_txia2d->log(
        "// GlTextureInterface::updateTextureArraySlice obt<%d> array<%p> slice<%d> img<%p:%s>",
        int(glto->_textureObject),
        array,
        slice_index,
        img.get(),
        img->_debugName.c_str());
    logchan_txia2d->log("///////////////////////////////////////////////////////////");
  }
  glBindTexture(texture_target, glto->_textureObject);
  auto format = array->_tex->_texFormat;
  GLFormatTriplet triplet(format);
  auto subimg_cmipc = img->uncompressedMipChain();
  int num_levels    = int(subimg_cmipc->_levels.size()) - 1;
  glTexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL, num_levels - 1);
  for (int level = 0; level < num_levels; level++) {
    auto mip      = subimg_cmipc->_levels[level];
    auto mip_w    = mip._width;
    auto mip_h    = mip._height;
    auto mip_data = mip._data;

    if (DEBUG_TEXARRAY2D) {
      logchan_txia2d->log(
          "//   target<0x%08x> level<%d> x<%d> y<%d> z<%d> w<%d> h<%d> d<%d> fmt<0x%08x> size<%zu> data<%p>",
          texture_target,
          level,
          0,
          0,
          slice_index,
          mip_w,
          mip_h,
          1,
          triplet._internalFormat,
          mip_data->length(),
          mip_data->data());
    }

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
            slice_index,             // zoffset (slice)
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
      case EBufferFormat::BGR8: {
        GL_ERRORCHECK();
        glTexSubImage3D(
            texture_target,    // target
            level,             // level
            0,                 // xoffset
            0,                 // yoffset
            slice_index,       // zoffset (slice)
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
// initialize a texture array with blank data
///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::initTextureArray2D(TextureArray* texture_array) {

  if(not texture_array->_isDirty){
    return; 
  } 

  bool w_mips          = texture_array->_requires_mips;
  int w                = texture_array->_width;
  int h                = texture_array->_height;
  int num_slices       = texture_array->_maxslices;
  EBufferFormat format = texture_array->_format;

  OrkAssert(num_slices > 0);

  texture_array->_tex->_texType   = ETEXTYPE_2D_ARRAY;
  texture_array->_tex->_texFormat = format;
  texture_array->_tex->_width     = w;
  texture_array->_tex->_height    = h;
  texture_array->_tex->_depth     = num_slices;
  auto glto                       = texture_array->_tex->_impl.makeShared<GLTextureObject>(this);
  auto texture_target             = GL_TEXTURE_2D_ARRAY;
  glto->mTarget                   = GL_TEXTURE_2D_ARRAY;
  GL_ERRORCHECK();
  glGenTextures(1, &glto->_textureObject);
  glBindTexture(texture_target, glto->_textureObject);

  if (DEBUG_TEXARRAY2D) {
    logchan_txia2d->log("///////////////////////////////////////////////////////////");
    logchan_txia2d->log(
        "// GlTextureInterface::initTextureArray2D obj<%d> array<%p> w<%d> h<%d> d<%d> fmt<%s>",
        int(glto->_textureObject),
        texture_array,
        w,
        h,
        num_slices,
        EBufferFormatToName(format).c_str());
    logchan_txia2d->log("///////////////////////////////////////////////////////////");
  }

  _texture_set[glto->_textureObject] = texture_array->_tex.get();

  if (texture_array->_tex->_debugName.length()) {
    mTargetGL.debugLabel(GL_TEXTURE, glto->_textureObject, texture_array->_tex->_debugName);
  }
  texture_array->_tex->_vars->makeValueForKey<GLuint>("gltexobj") = glto->_textureObject;

  GLFormatTriplet triplet(format);

  //////////////////////////////////////////////////////////////////
  // allocate
  //////////////////////////////////////////////////////////////////

  if (format == EBufferFormat::Z32F) {
    GL_ERRORCHECK();
    if (DEBUG_TEXARRAY2D) {
      auto tgtstr  = GLenumToString(texture_target);
      auto ifmtstr = GLenumToString(triplet._internalFormat);
      logchan_txia2d->log(
          "GLCTI3Db::Z32F obj<%d> target<%s> fmt<%s> w<%d> h<%d> d<%d> numpix<%d>",
          int(glto->_textureObject),
          tgtstr.c_str(),
          ifmtstr.c_str(),
          w,
          h,
          num_slices,
          w * h * num_slices);
    }
    glTexImage3D(
        texture_target,          // target
        0,                       // level
        triplet._internalFormat, // internal format
        w,                       // width
        h,                       // height
        num_slices,              // depth
        0,                       // border
        triplet._format,         // format
        triplet._type,           // type
        nullptr);                // data
    GL_ERRORCHECK();

    glTexParameteri(texture_target, GL_TEXTURE_BASE_LEVEL, 0);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL, 0);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  
  } // Z32F
  else {
    int num_levels = 1;
    int lw         = w;
    int lh         = h;
    while ((lw >= 8) and (lh >= 8)) {
      lw = lw >> 1;
      lh = lh >> 1;
      num_levels++;
    }
    for (int level = 0; level < num_levels; level++) {
      int w1 = w >> level;
      int h1 = h >> level;
      switch (format) {
        case EBufferFormat::RGBA_BPTC_UNORM: {
          int blocked_width  = (w1 + 3) & 0xfffffffc;
          int blocked_height = (h1 + 3) & 0xfffffffc;
          size_t size        = blocked_width * blocked_height * num_slices;
          GL_ERRORCHECK();
          glCompressedTexImage3D(
              texture_target,          // target
              level,                   // level
              triplet._internalFormat, // internal format
              w1,                      // width
              h1,                      // height
              num_slices,              // depth
              0,                       // border
              size,                    // size
              nullptr);                // data
          GL_ERRORCHECK();
          if (DEBUG_TEXARRAY2D) {
            auto tgtstr  = GLenumToString(texture_target);
            auto ifmtstr = GLenumToString(triplet._internalFormat);
            logchan_txia2d->log(
                "GLCTI3Da::RGBA_BPTC_UNORM obj<%d> target<%s> fmt<%s> level<%d> w<%d> h<%d> d<%d> size<%d> data<%p>",
                int(glto->_textureObject),
                tgtstr.c_str(),
                ifmtstr.c_str(),
                level,
                w1,
                h1,
                num_slices,
                blocked_width * blocked_height * num_slices,
                nullptr);
          }
          break;
        }
        case EBufferFormat::RGB8: {
          GL_ERRORCHECK();
          if (DEBUG_TEXARRAY2D) {
            auto tgtstr  = GLenumToString(texture_target);
            auto ifmtstr = GLenumToString(triplet._internalFormat);
            logchan_txia2d->log(
                "GLCTI3Db::RGB8 obj<%d> target<%s> fmt<%s> level<%d> w<%d> h<%d> d<%d> size<%d> data<%p>",
                int(glto->_textureObject),
                tgtstr.c_str(),
                ifmtstr.c_str(),
                level,
                w1,
                h1,
                num_slices,
                w1 * h1 * num_slices,
                nullptr);
          }
          glTexImage3D(
              texture_target,          // target
              level,                   // level
              triplet._internalFormat, // internal format
              w1,                      // width
              h1,                      // height
              num_slices,              // depth
              0,                       // border
              triplet._format,         // format
              triplet._type,           // type
              nullptr);                // data
          GL_ERRORCHECK();
          break;
        }
        case EBufferFormat::RGBA8:
        case EBufferFormat::BGRA8: {
          GL_ERRORCHECK();
          if (DEBUG_TEXARRAY2D) {
            auto tgtstr  = GLenumToString(texture_target);
            auto ifmtstr = GLenumToString(triplet._internalFormat);
            logchan_txia2d->log(
                "GLCTI3Db::RGBA8 obj<%d> target<%s> fmt<%s> level<%d> w<%d> h<%d> d<%d> size<%d> data<%p>",
                int(glto->_textureObject),
                tgtstr.c_str(),
                ifmtstr.c_str(),
                level,
                w1,
                h1,
                num_slices,
                w1 * h1 * num_slices,
                nullptr);
          }
          glTexImage3D(
              texture_target,          // target
              level,                   // level
              triplet._internalFormat, // internal format
              w1,                      // width
              h1,                      // height
              num_slices,              // depth
              0,                       // border
              triplet._format,         // format
              triplet._type,           // type
              nullptr);                // data
          GL_ERRORCHECK();
          break;
        }
        case EBufferFormat::RGB16: {
          GL_ERRORCHECK();
          if (DEBUG_TEXARRAY2D) {
            auto tgtstr  = GLenumToString(texture_target);
            auto ifmtstr = GLenumToString(triplet._internalFormat);
            logchan_txia2d->log(
                "GLCTI3Db::RGB16 obj<%d> target<%s> fmt<%s> level<%d> w<%d> h<%d> d<%d> size<%d> data<%p>",
                int(glto->_textureObject),
                tgtstr.c_str(),
                ifmtstr.c_str(),
                level,
                w1,
                h1,
                num_slices,
                w1 * h1 * num_slices,
                nullptr);
          }
          glTexImage3D(
              texture_target,          // target
              level,                   // level
              triplet._internalFormat, // internal format
              w1,                      // width
              h1,                      // height
              num_slices,              // depth
              0,                       // border
              triplet._format,         // format
              triplet._type,           // type
              nullptr);                // data
          GL_ERRORCHECK();
          break;
        }
        case EBufferFormat::RGBA16: {
          GL_ERRORCHECK();
          if (DEBUG_TEXARRAY2D) {
            auto tgtstr  = GLenumToString(texture_target);
            auto ifmtstr = GLenumToString(triplet._internalFormat);
            logchan_txia2d->log(
                "GLCTI3Db::RGBA16 obj<%d> target<%s> fmt<%s> level<%d> w<%d> h<%d> d<%d> size<%d> data<%p>",
                int(glto->_textureObject),
                tgtstr.c_str(),
                ifmtstr.c_str(),
                level,
                w1,
                h1,
                num_slices,
                w1 * h1 * num_slices,
                nullptr);
          }
          glTexImage3D(
              texture_target,          // target
              level,                   // level
              triplet._internalFormat, // internal format
              w1,                      // width
              h1,                      // height
              num_slices,              // depth
              0,                       // border
              triplet._format,         // format
              triplet._type,           // type
              nullptr);                // data
          GL_ERRORCHECK();
          break;
        }
        default:
          OrkAssert(false);
      } // switch (format) {
    } // for (int level = 0; level < num_levels; level++) {

    glTexParameteri(texture_target, GL_TEXTURE_BASE_LEVEL, 0);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL, num_levels-1);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_SWIZZLE_R, GL_RED);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GL_ERRORCHECK();
    glTexParameteri(texture_target, GL_TEXTURE_WRAP_R, GL_REPEAT);
  
  } // not Z32F
  //////////////////////////////////////////////////////////////////
  // fill in image data
  //////////////////////////////////////////////////////////////////
  glTexParameteri(texture_target, GL_TEXTURE_MIN_LOD, 0);
  GL_ERRORCHECK();
  glTexParameteri(texture_target, GL_TEXTURE_MAX_LOD, num_slices - 1);
  GL_ERRORCHECK();
  glTexParameteri(texture_target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
  GL_ERRORCHECK();

  
  texture_array->_isDirty = false;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
