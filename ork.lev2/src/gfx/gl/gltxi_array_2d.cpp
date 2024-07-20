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

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::initTextureArray2DFromData(Texture* array_tex, TextureArrayInitData tid) {

  printf( "///////////////////////////////////////////////////////////\n");
  printf( "// GlTextureInterface::initTextureArray2DFromData ptex<%p>\n", array_tex);
  printf( "///////////////////////////////////////////////////////////\n");

  array_tex->_texType = ETEXTYPE_2D_ARRAY;
  int num_subtextures = int(tid._slices.size());
  std::vector<compressedmipchain_ptr_t> subimagedata;
  subimagedata.resize(num_subtextures);
  ///////////////////////////
  // scan present subtextures
  //  extract max width and height
  //  and load mipchains
  ///////////////////////////
  int max_w      = 0;
  int max_h      = 0;
  int max_levels = 0;
  std::unordered_set<EBufferFormat> formats;
  for (int i = 0; i < num_subtextures; i++) {
    const auto& slice = tid._slices[i];
    auto subtex       = slice._subtex;
    if (subtex) {
      auto subtex_cmipc = std::make_shared<CompressedImageMipChain>();
      subimagedata[i]   = subtex_cmipc;

      DataBlockInputStream checkstream(subtex->_final_datablock);
      uint32_t magic = checkstream.getItem<uint32_t>();
      bool ok        = false;
      if (Char4("chkf") == Char4(magic)){
        // XTX
        subtex_cmipc->readXTX(subtex->_final_datablock);
        int W          = subtex_cmipc->_width;
        int H          = subtex_cmipc->_height;
        int num_levels = int(subtex_cmipc->_levels.size());
        auto fmt       = subtex_cmipc->_format;
        formats.insert(fmt);
        max_w      = std::max(max_w, W);
        max_h      = std::max(max_h, H);
        max_levels = std::max(max_levels, num_levels);
        auto subtexname = subtex->_debugName;
        printf("subtex.xtx<%d:%s> w<%d> h<%d> numlev<%d>\n", i, subtexname.c_str(), W, H, num_levels);
      }
      else if (Char4("DDS ") == Char4(magic)) {
        // DDS
        subtex_cmipc->readDDS(subtex->_final_datablock);
        int W          = subtex_cmipc->_width;
        int H          = subtex_cmipc->_height;
        int num_levels = int(subtex_cmipc->_levels.size());
        auto fmt       = subtex_cmipc->_format;
        formats.insert(fmt);
        max_w      = std::max(max_w, W);
        max_h      = std::max(max_h, H);
        max_levels = std::max(max_levels, num_levels);
        auto subtexname = subtex->_debugName;
        printf("subtex.dds<%d:%s> w<%d> h<%d> numlev<%d>\n", i, subtexname.c_str(), W, H, num_levels);
      }
      else {
        // needs convert
        OrkAssert(false);
      }
    }
  }
  OrkAssert(formats.size() == 1);

  max_levels -= 1;
  auto format = *formats.begin();
  array_tex->_texFormat = format;
  GLFormatTriplet triplet(format);

  ///////////////////////////
  // generate texture object
  ///////////////////////////

  gltexobj_ptr_t glto = array_tex->_impl.makeShared<GLTextureObject>(this);
  auto texture_target = GL_TEXTURE_2D_ARRAY;
  glto->mTarget = GL_TEXTURE_2D_ARRAY;
  GL_ERRORCHECK();
  glGenTextures(1, &glto->_textureObject);
  glBindTexture(texture_target, glto->_textureObject);
  if (array_tex->_debugName.length()) {
    mTargetGL.debugLabel(GL_TEXTURE, glto->_textureObject, array_tex->_debugName);
  }
  array_tex->_vars->makeValueForKey<GLuint>("gltexobj") = glto->_textureObject;
  // glTexStorage3D(texture_target, 4, GL_RGBA8, max_w, max_h, num_subtextures);

  static auto clear_tex_data = (const uint8_t*)calloc(1, 256 << 20);

  //////////////////////////////////////////////////////////////////
  // allocate
  //////////////////////////////////////////////////////////////////

  for (int level = 0; level < max_levels; level++) {
    int w = max_w >> level;
    int h = max_h >> level;
    switch (format) {
      case EBufferFormat::RGBA_BPTC_UNORM:{
        int blocked_width  = (w + 3) & 0xfffffffc;
        int blocked_height = (h + 3) & 0xfffffffc;
        size_t size = blocked_width * blocked_height * num_subtextures;
        GL_ERRORCHECK();
        glCompressedTexImage3D(
            texture_target,            // target
            level,                     // level
            triplet._internalFormat,   // internal format
            w,                         // width
            h,                         // height
            num_subtextures,           // depth
            0,                         // border
            size,                      // size
            clear_tex_data);           // data
        GL_ERRORCHECK();
        if(1)printf( "GLCTI3Da target<0x%08x> level<%d> w<%d> h<%d> d<%d> fmt<0x%08x> size<%d> data<%p>\n",
                texture_target, level, w, h, num_subtextures, triplet._internalFormat, blocked_width * blocked_height * num_subtextures, clear_tex_data);
        break;
      }
      case EBufferFormat::RGBA8:
      case EBufferFormat::BGRA8:
      case EBufferFormat::RGB8:
      case EBufferFormat::BGR8:
        GL_ERRORCHECK();
        if(1)printf( "GLCTI3Db target<0x%08x> level<%d> w<%d> h<%d> d<%d> fmt<0x%08x> size<%d> data<%p>\n",
                texture_target, level, w, h, num_subtextures, triplet._internalFormat, w * h * num_subtextures, clear_tex_data);
        glTexImage3D(
            texture_target,          // target
            level,                   // level
            triplet._internalFormat, // internal format
            w,                       // width
            h,                       // height
            num_subtextures,         // depth
            0,                       // border
            triplet._format,         // format
            triplet._type,           // type
            clear_tex_data);         // data
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
  glTexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL, max_levels-1);
  GL_ERRORCHECK();
  glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  GL_ERRORCHECK();
  glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  GL_ERRORCHECK();

  if(1)for (int isub = 0; isub < num_subtextures; isub++) {

    auto subtex_cmipc = subimagedata[isub];
    if (subtex_cmipc) {

      GL_ERRORCHECK();
      int num_levels = int(subtex_cmipc->_levels.size())-1;
        if(num_levels<max_levels){
            glTexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL, num_levels-1);
        }
      for (int level = 0; level < num_levels; level++) {

        auto mip      = subtex_cmipc->_levels[level];
        auto mip_w    = mip._width;
        auto mip_h    = mip._height;
        auto mip_data = mip._data;

        switch (format) {
          case EBufferFormat::RGBA_BPTC_UNORM: {
            GL_ERRORCHECK();
            int blocked_width  = (mip_w + 3) & 0xfffffffc;
            int blocked_height = (mip_h + 3) & 0xfffffffc;
            if(1) printf( "GLCTSI3Da target<0x%08x> level<%d> x<%d> y<%d> z<%d> w<%d> h<%d> d<%d> fmt<0x%08x> size<%d> data<%p>\n",
                    texture_target, level, 0, 0, isub, blocked_width, blocked_height, 1, triplet._internalFormat, mip_data->length(), mip_data->data());
            glCompressedTexSubImage3D(
                texture_target,      // target
                level,               // level
                0,                   // xoffset
                0,                   // yoffset
                isub,                // zoffset (slice)
                mip_w,               // width
                mip_h,               // height
                1,                   // depth (of data for slice)
                triplet._internalFormat,     // format
                mip_data->length(),  // size
                mip_data->data());   // data
            
            GL_ERRORCHECK();
            break;
          }
          default:
            GL_ERRORCHECK();
            if(1) printf( "GLCTSI3Db target<0x%08x> level<%d> x<%d> y<%d> z<%d> w<%d> h<%d> d<%d> fmt<0x%08x> size<%d> data<%p>\n",
                    texture_target, level, 0, 0, isub, mip_w, mip_h, 1, triplet._internalFormat, mip_data->length(), mip_data->data());
            glTexSubImage3D(
                texture_target,      // target
                level,               // level
                0,                   // xoffset
                0,                   // yoffset
                isub,                // zoffset (slice)
                mip_w,               // width
                mip_h,               // height
                1,                   // depth (of data for slice)
                triplet._format,     // format
                triplet._type,       // type
                mip_data->data());   // data
            GL_ERRORCHECK();
            break;
        }
      }
    }
  } //   if(0)for (int isub = 0; isub < num_subtextures; isub++) {

  ///////////////////////////

  printf("TextureArray maxw<%d> maxh<%d> depth<%d>\n", max_w, max_h, num_subtextures);
  printf( "///////////////////////////////////////////////////////////\n");
  array_tex->_residenceState.fetch_or(1);
  // OrkAssert(num_subtextures==0);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
