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

void GlTextureInterface::initTextureArray2DFromData(Texture* array_tex, TextureArrayInitData tid){
  array_tex->_texType = ETEXTYPE_2D_ARRAY;
  int num_subtextures = int(tid._slices.size());
  std::vector<compressedmipchain_ptr_t> subimagedata;
  subimagedata.resize(num_subtextures);
  ///////////////////////////
  // scan present subtextures
  //  extract max width and height
  //  and load mipchains
  ///////////////////////////
  int max_w = 0;
  int max_h = 0;
  int max_levels = 0;
  std::unordered_set<EBufferFormat> formats;
  for(int i=0; i<num_subtextures; i++){
    const auto& slice = tid._slices[i];
    auto subtex = slice._subtex;
    if(subtex){
      auto subtex_cmipc = std::make_shared<CompressedImageMipChain>();
      subimagedata[i] = subtex_cmipc;
      subtex_cmipc->readXTX(subtex->_final_datablock);
      int W = subtex_cmipc->_width;
      int H = subtex_cmipc->_height;
      int num_levels = int(subtex_cmipc->_levels.size());
      auto fmt = subtex_cmipc->_format;
      formats.insert(fmt);
      max_w = std::max(max_w, W);
      max_h = std::max(max_h, H);
      max_levels = std::max(max_levels,num_levels);
      printf( "subtex<%d> w<%d> h<%d> numlev<%d>\n", i, W, H, num_levels );
    }
    else{
      printf( "subtex<%d> empty\n", i );
    }
  }
  OrkAssert(formats.size()==1);

  GLFormatTriplet triplet(*formats.begin());

  ///////////////////////////
  // generate texture object 
  ///////////////////////////

  gltexobj_ptr_t glto = array_tex->_impl.makeShared<GLTextureObject>(this);
  auto texture_target = GL_TEXTURE_2D_ARRAY;
  GL_ERRORCHECK();
  glGenTextures(1, &glto->_textureObject);
  glBindTexture(texture_target, glto->_textureObject);
  if (array_tex->_debugName.length()) {
    mTargetGL.debugLabel(GL_TEXTURE, glto->_textureObject, array_tex->_debugName);
  }
  array_tex->_vars->makeValueForKey<GLuint>("gltexobj") = glto->_textureObject;
  //glTexStorage3D(texture_target, 4, GL_RGBA8, max_w, max_h, num_subtextures);

  for( int level=0; level<max_levels; level++ ){
    int w = max_w >> level;
    int h = max_h >> level;
    glTexImage3D( texture_target,                  // target
                  level ,                          // level 
                  triplet._internalFormat,         // internal format 
                  w, h, num_subtextures,           // width, height, depth
                  0,                               // border
                  triplet._format,                 // format
                  triplet._type,                   // type
                  NULL);                           // data
  }
  GL_ERRORCHECK();
  for( int i=0; i<num_subtextures; i++ ){


        

    auto subtex_cmipc = subimagedata[i];
    if(subtex_cmipc){

        int num_levels = int(subtex_cmipc->_levels.size());

        for( int level=0; level<num_levels; level++ ){

            auto mip = subtex_cmipc->_levels[level];
            auto mip_w = mip._width;
            auto mip_h = mip._height;
            auto mip_data = mip._data;

            glTexSubImage3D( texture_target,      // target
                             level,               // level
                             0, 0, i,             // xoffset, yoffset, zoffset
                             mip_w, mip_h, 1,     // width, height, depth (of data for slice)
                             triplet._format,     // format
                             triplet._type,       // type
                             mip_data->data() );  // data

        }

    }
  }
  ///////////////////////////
  // fill in blank slices
  ///////////////////////////
  for( int i=0; i<num_subtextures; i++ ){
    auto subtex_cmipc = subimagedata[i];
    if(subtex_cmipc == nullptr){
      printf( "subtex<%d> create blank <%dx%d>\n", i, max_w, max_h );
    }
  }
  ///////////////////////////

  printf( "TextureArray maxw<%d> maxh<%d> depth<%d>\n", max_w, max_h, num_subtextures );
  GL_ERRORCHECK();
  //OrkAssert(num_subtextures==0);
}

///////////////////////////////////////////////////////////////////////////////

} //namespace ork::lev2 {
