////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::_loadXTXTexture(texture_ptr_t ptex, datablock_ptr_t datablock) {
  GlTexLoadReq load_req;
  load_req.ptex                  = ptex;
  load_req._inpstream._datablock = datablock;
  load_req._cmipchain            = std::make_shared<CompressedImageMipChain>();
  load_req._cmipchain->readXTX(datablock);
  ///////////////////////////////////////////////
  auto glto = ptex->_impl.makeShared<GLTextureObject>(this);
  ////////////////////////////////////////////////////////////////////
  ptex->_width     = load_req._cmipchain->_width;
  ptex->_height    = load_req._cmipchain->_height;
  ptex->_depth     = 1;
  ptex->_texFormat = load_req._cmipchain->_format;
  ///////////////////////////////////////////////
   auto keys = load_req._cmipchain->_varmap.dumpkeys();
   //printf("\nxtx w<%lu>\n", load_req._cmipchain->_width);
   ///printf("xtx h<%lu>\n", load_req._cmipchain->_height);
   //printf("xtx d<%lu>\n", load_req._cmipchain->_depth);
   //printf("xtx fmt<%zx>\n", (uint64_t)load_req._cmipchain->_format);
   //for (auto k : keys) {
   //printf("xtx mipchain varmap-key<%s>\n", k.c_str());
  //}
   //for (auto k : ptex->_varmap.dumpkeys()) {
   //printf("xtx ptex varmap-key<%s>\n", k.c_str());
  //}
  void_lambda_t lamb = [=]() {
    //printf( "XTX MAINTHREAD<%p>\n",ptex);
    /////////////////////////////////////////////
    // texture preprocssing, if any..
    //  on main thread.
    /////////////////////////////////////////////
    if (ptex->_varmap.hasKey("preproc")) {
      auto preproc        = ptex->_varmap.typedValueForKey<Texture::proc_t>("preproc").value();
      auto orig_datablock = datablock;
      auto postblock      = preproc(ptex, &mTargetGL, orig_datablock);
    }
    this->_loadXTXTextureMainThreadPart(load_req);
  };
  opq::mainSerialQueue()->enqueue(lamb);
  ///////////////////////////////////////////////
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::_loadXTXTextureMainThreadPart(GlTexLoadReq req) {
  mTargetGL.makeCurrentContext();
  mTargetGL.debugPushGroup("_loadXTXTextureMainThreadPart");
  OrkAssert(req._cmipchain.get() != nullptr);
  auto glto = req.ptex->_impl.get<gltexobj_ptr_t>();
  glGenTextures(1, &glto->mObject);
  glBindTexture(GL_TEXTURE_2D, glto->mObject);
  GL_ERRORCHECK();
  req.ptex->_varmap.makeValueForKey<GLuint>("gltexobj") = glto->mObject;
  if (req.ptex->_debugName.length()) {
    mTargetGL.debugLabel(GL_TEXTURE, glto->mObject, req.ptex->_debugName);
  }
  int inummips = req._cmipchain->_levels.size();
  OrkAssert(inummips > 0);
  GL_ERRORCHECK();
   //printf("inummips<%d>\n", inummips);
  for (int imip = 0; imip < inummips; imip++) {
    auto& level = req._cmipchain->_levels[imip];
     //printf("mip<%d> w<%ld> h<%ld> len<%zu>\n", imip, level._width, level._height, level._data->length());
    switch (req.ptex->_texFormat) {
      case EBufferFormat::RGBA8:
        glTexImage2D(         //
            GL_TEXTURE_2D,    // target
            imip,             // miplevel
            GL_RGBA8,         // internalformat
            level._width,     // width
            level._height,    // height
            0,                // border
            GL_RGBA,          // format
            GL_UNSIGNED_BYTE, // datatype
            level._data->data());
        break;
#if !defined(__APPLE__)
      case EBufferFormat::RGBA_BPTC_UNORM:
        glCompressedTexImage2D( //
            GL_TEXTURE_2D,      //
            imip,               //
            GL_COMPRESSED_RGBA_BPTC_UNORM,
            level._width,
            level._height,
            0,
            level._data->length(),
            level._data->data());
        break;
#endif
      default:
        OrkAssert(false);
        break;
    }
    GL_ERRORCHECK();
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  GL_ERRORCHECK();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, inummips - 1);
  GL_ERRORCHECK();
  req.ptex->TexSamplingMode().PresetTrilinearWrap();
  this->ApplySamplingMode(req.ptex.get());
  req.ptex->_dirty = false;
  glBindTexture(GL_TEXTURE_2D, 0);
  ////////////////////////////////////////////////
  // done loading texture,
  //  perform postprocessing, if any..
  ////////////////////////////////////////////////

  if(req.ptex->_debugName== "filtenvmap-processed-specular"){
    //OrkAssert(false);
  }

  if (req.ptex->_varmap.hasKey("postproc")) {
    auto dblock    = req._inpstream._datablock;
    auto postproc  = req.ptex->_varmap.typedValueForKey<Texture::proc_t>("postproc").value();
    auto postblock = postproc(req.ptex, &mTargetGL, dblock);
    OrkAssert(postblock);
  } else {
    // printf("ptex<%p> no postproc\n", ptex);
  }
  GL_ERRORCHECK();
  mTargetGL.debugPopGroup();
  req.ptex->_residenceState.fetch_or(1);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
