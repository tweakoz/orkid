////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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

bool GlTextureInterface::LoadXTXTexture(Texture* ptex, datablockptr_t datablock) {
  GlTexLoadReq load_req;
  load_req.ptex                  = ptex;
  load_req._inpstream._datablock = datablock;
  load_req._cmipchain            = std::make_shared<CompressedImageMipChain>();
  load_req._cmipchain->readXTX(datablock);
  ///////////////////////////////////////////////
  GLTextureObject* pTEXOBJ = new GLTextureObject;
  ptex->_internalHandle    = (void*)pTEXOBJ;
  ///////////////////////////////////////////////
  auto keys = load_req._cmipchain->_varmap.dumpkeys();
  printf("\nxtx w<%lu>\n", load_req._cmipchain->_width);
  printf("xtx h<%lu>\n", load_req._cmipchain->_height);
  printf("xtx d<%lu>\n", load_req._cmipchain->_depth);
  printf("xtx fmt<%zx>\n", (uint64_t)load_req._cmipchain->_format);
  for (auto k : keys) {
    printf("xtx varmap-key<%s>\n", k.c_str());
  }

  void_lambda_t lamb = [=]() {
    /////////////////////////////////////////////
    // texture preprocssing, if any..
    //  on main thread.
    /////////////////////////////////////////////
    if (ptex->_varmap.hasKey("preproc")) {
      auto preproc        = ptex->_varmap.typedValueForKey<Texture::proc_t>("preproc").value();
      auto orig_datablock = datablock;
      auto postblock      = preproc(ptex, &mTargetGL, orig_datablock);
    }
    this->LoadXTXTextureMainThreadPart(load_req);
  };
  opq::mainSerialQueue().enqueue(lamb);
  ///////////////////////////////////////////////
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::LoadXTXTextureMainThreadPart(GlTexLoadReq req) {
  OrkAssert(req._cmipchain.get() != nullptr);
  auto teximpl = (GLTextureObject*)req.ptex->_internalHandle;
  glGenTextures(1, &teximpl->mObject);
  glBindTexture(GL_TEXTURE_2D, teximpl->mObject);
  GL_ERRORCHECK();
  if (req.ptex->_debugName.length()) {
    mTargetGL.debugLabel(GL_TEXTURE, teximpl->mObject, req.ptex->_debugName);
  }
  int inummips = req._cmipchain->_levels.size();
  OrkAssert(inummips > 0);
  GL_ERRORCHECK();
  for (int imip = 0; imip < inummips; imip++) {
    auto& level = req._cmipchain->_levels[imip];
    glCompressedTexImage2D( //
        GL_TEXTURE_2D,      //
        imip,               //
        GL_COMPRESSED_RGBA_BPTC_UNORM,
        level._width,
        level._height,
        0,
        level._data->length(),
        level._data->data());
    GL_ERRORCHECK();
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  GL_ERRORCHECK();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, inummips - 1);
  GL_ERRORCHECK();
  req.ptex->TexSamplingMode().PresetTrilinearWrap();
  this->ApplySamplingMode(req.ptex);
  req.ptex->_dirty = false;
  glBindTexture(GL_TEXTURE_2D, 0);
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
