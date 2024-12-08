////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/memcpy.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/gfx/dds.h>
#include "gl.h"
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/ui.h>
#include <ork/file/file.h>
#include <ork/math/misc_math.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/debug.h>
#include <ork/kernel/datacache.h>

namespace ork::lev2 {

void GlTextureInterface::_createFromCompressedLoadReq(texloadreq_ptr_t req) {

  auto assreq = req->_assetloadreq;

  if(assreq and assreq->_on_event){
    assreq->_on_event("beginLoadMainThread"_crcu,nullptr);
  }

  mTargetGL.makeCurrentContext();
  mTargetGL.debugPushGroup("loadDDSTextureMainThreadPart",fvec4::White());

  printf("GlTextureInterface : Loading compressed texture...\n");
  auto ptex = req->ptex;
  auto GLTO = ptex->_impl.makeShared<GLTextureObject>(this);
  GLTO->_txi = this;

  auto chain       = req->_cmipchain;
  size_t num_mips  = chain->_levels.size();
  auto format      = chain->_format;
  int iwidth       = chain->_width;
  int iheight      = chain->_height;
  int idepth       = chain->_depth;

  bool is_volume_texture = (idepth > 1);

  GLuint TARGET = is_volume_texture ? GL_TEXTURE_3D : GL_TEXTURE_2D;
  GLTO->mTarget = TARGET;

  GL_ERRORCHECK();
  glGenTextures(1, &GLTO->_textureObject);
  glBindTexture(TARGET, GLTO->_textureObject);
  GL_ERRORCHECK();

  _texture_set[GLTO->_textureObject] = ptex.get();

  if (ptex->_debugName.length()) {
    mTargetGL.debugLabel(GL_TEXTURE, GLTO->_textureObject, ptex->_debugName);
  }

  ptex->_vars->makeValueForKey<GLuint>("gltexobj") = GLTO->_textureObject;

  auto infname = req->_texname;

  for (int ilevel = 0; ilevel < num_mips; ilevel++) {
    auto& level         = chain->_levels[ilevel];
    int level_width     = level._width;
    int level_height    = level._height;
    int level_depth    = level._depth;
    auto level_data     = level._data->data(0);
    size_t level_length = level._data->length();
    printf("  level<%d> w<%d> h<%d> d<%d> len<%zu>\n", ilevel, level_width, level_height, level_depth, level_length);

    if( is_volume_texture){
      switch(format){
        case EBufferFormat::S3TC_DXT1:
          glCompressedTexImage3D(TARGET, ilevel, kRGBA_DXT1, level_width, level_height, level_depth, 0, level_length, level_data);
          break;
        case EBufferFormat::S3TC_DXT3:
          glCompressedTexImage3D(TARGET, ilevel, kRGBA_DXT3, level_width, level_height, level_depth, 0, level_length, level_data);
          break;
        case EBufferFormat::S3TC_DXT5:
          glCompressedTexImage3D(TARGET, ilevel, kRGBA_DXT5, level_width, level_height, level_depth, 0, level_length, level_data);
          break;
        case EBufferFormat::R8:
          glTexImage3D(TARGET, ilevel, GL_RED, level_width, level_height, level_depth, 0, GL_RED, GL_UNSIGNED_BYTE, level_data);
          break;
        case EBufferFormat::RGBA8:
          glTexImage3D(TARGET, ilevel, GL_RGBA, level_width, level_height, level_depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, level_data);
          break;
        case EBufferFormat::BGRA8:
          glTexImage3D(TARGET, ilevel, GL_RGBA, level_width, level_height, level_depth, 0, GL_BGRA, GL_UNSIGNED_BYTE, level_data);
          break;
        case EBufferFormat::RGB8:
          glTexImage3D(TARGET, ilevel, GL_RGB, level_width, level_height, level_depth, 0, GL_RGB, GL_UNSIGNED_BYTE, level_data);
          break;
        case EBufferFormat::BGR8:
          glTexImage3D(TARGET, ilevel, GL_RGB, level_width, level_height, level_depth, 0, GL_BGR, GL_UNSIGNED_BYTE, level_data);
          break;            
        default:
          OrkAssert(false);
          break;
      }
    }
    else{
      switch(format){
        case EBufferFormat::S3TC_DXT1:
          glCompressedTexImage2D(TARGET, ilevel, kRGBA_DXT1, level_width, level_height, 0, level_length, level_data);
          break;
        case EBufferFormat::S3TC_DXT3:
          glCompressedTexImage2D(TARGET, ilevel, kRGBA_DXT3, level_width, level_height, 0, level_length, level_data);
          break;
        case EBufferFormat::S3TC_DXT5:
          glCompressedTexImage2D(TARGET, ilevel, kRGBA_DXT5, level_width, level_height, 0, level_length, level_data);
          break;
        case EBufferFormat::RGBA_BPTC_UNORM:
          #if defined(__APPLE__)
          OrkAssert(false);
          #else
          glCompressedTexImage2D(TARGET, ilevel, GL_COMPRESSED_RGBA_BPTC_UNORM, level_width, level_height, 0, level_length, level_data);
          #endif
          break;
        case EBufferFormat::BGR8:
          glTexImage2D(TARGET, ilevel, GL_RGB, level_width, level_height, 0, GL_BGR, GL_UNSIGNED_BYTE, level_data);
          break;
        case EBufferFormat::RGB8:
          glTexImage2D(TARGET, ilevel, GL_RGB, level_width, level_height, 0, GL_RGB, GL_UNSIGNED_BYTE, level_data);
          break;
        case EBufferFormat::RGBA8:
          glTexImage2D(TARGET, ilevel, GL_RGBA, level_width, level_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, level_data);
          break;
        case EBufferFormat::BGRA8:
          glTexImage2D(TARGET, ilevel, GL_RGBA, level_width, level_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, level_data);
          break;
        case EBufferFormat::R8:
          glTexImage2D(TARGET, ilevel, GL_RED, level_width, level_height, 0, GL_RED, GL_UNSIGNED_BYTE, level_data);
          break;
        default:
          OrkAssert(false);
          break;
      }
    }
  }

  glTexParameteri(TARGET, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(TARGET, GL_TEXTURE_MAX_LEVEL, num_mips - 1);

  if (is_volume_texture) {
    ptex->TexSamplingMode().PresetTrilinearWrap();
  }
  else{
    if (num_mips > 3) {
      ptex->TexSamplingMode().PresetTrilinearWrap();
    }
  }
  this->ApplySamplingMode(ptex.get());

  ptex->_dirty = false;

  glBindTexture(TARGET, 0);
  GL_ERRORCHECK();

  ////////////////////////////////////////////////
  // done loading texture,
  //  perform postprocessing, if any..
  ////////////////////////////////////////////////

  if (ptex->_vars->hasKey("postproc")) {
    auto dblock    = req->_inpstream._datablock;
    auto postproc  = ptex->_vars->typedValueForKey<Texture::proc_t>("postproc").value();
    auto postblock = postproc(ptex, &mTargetGL, dblock);
    OrkAssert(postblock);
  } else {
    // printf("ptex<%p> no postproc\n", ptex);
  }

  mTargetGL.debugPopGroup();

  if(assreq and assreq->_on_event){
    assreq->_on_event("endLoadMainThread"_crcu,nullptr);
  }
  if(assreq and assreq->_on_event){
    auto data = std::make_shared<varmap::VarMap>();
    data->makeValueForKey<std::string>("infname") = assreq->_asset_path.c_str();
    data->makeValueForKey<std::string>("loader") = "_loadDDSTexture";
    assreq->_on_event("loadComplete"_crcu,data);
  }
}

} //namespace ork::lev2 {
