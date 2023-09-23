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
#include <ork/lev2/gfx/txi.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/ui.h>
#include <ork/file/file.h>
#include <ork/math/misc_math.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/debug.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

bool TextureInterface::_loadXTXTexture(texture_ptr_t ptex, datablock_ptr_t datablock) {
  auto load_req = std::make_shared<TexLoadReq>();
  load_req->ptex                  = ptex;
  load_req->_inpstream._datablock = datablock;
  load_req->_cmipchain            = std::make_shared<CompressedImageMipChain>();
  load_req->_cmipchain->readXTX(datablock);
  ///////////////////////////////////////////////
  //auto glto = ptex->_impl.makeShared<GLTextureObject>(this);
  ////////////////////////////////////////////////////////////////////
  ptex->_width     = load_req->_cmipchain->_width;
  ptex->_height    = load_req->_cmipchain->_height;
  ptex->_depth     = 1;
  ptex->_texFormat = load_req->_cmipchain->_format;
  ///////////////////////////////////////////////
   auto keys = load_req->_cmipchain->_varmap.dumpkeys();
   //printf("\nxtx w<%lu>\n", ptex->_width);
   //printf("xtx h<%lu>\n", ptex->_height);
   //printf("xtx d<%lu>\n", load_req->_cmipchain->_depth);
   //printf("xtx fmt<%zx>\n", (uint64_t)load_req->_cmipchain->_format);
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
      //auto postblock      = preproc(ptex, &mTargetGL, orig_datablock);
    }
    this->_loadXTXTextureMainThreadPart(load_req);
  };
  opq::mainSerialQueue()->enqueue(lamb);
  ///////////////////////////////////////////////
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void TextureInterface::_loadXTXTextureMainThreadPart(texloadreq_ptr_t req) {
  OrkAssert(req->_cmipchain.get() != nullptr);
  if (req->ptex->_debugName.length()) {
    //mTargetGL.debugLabel(GL_TEXTURE, glto->mObject, req->ptex->_debugName);
  }
  int inummips = req->_cmipchain->_levels.size();
  OrkAssert(inummips > 0);
  //GL_ERRORCHECK();
  printf("inummips<%d>\n", inummips);
  _createFromCompressedLoadReq(req); 
  req->ptex->_num_mips = inummips;
  req->ptex->TexSamplingMode().PresetTrilinearWrap();
  this->ApplySamplingMode(req->ptex.get());
  req->ptex->_dirty = false;
  ////////////////////////////////////////////////
  // done loading texture,
  //  perform postprocessing, if any..
  ////////////////////////////////////////////////
  if(req->ptex->_debugName== "filtenvmap-processed-specular"){
    //OrkAssert(false);
  }
  if (req->ptex->_varmap.hasKey("postproc")) {
    auto dblock    = req->_inpstream._datablock;
    auto postproc  = req->ptex->_varmap.typedValueForKey<Texture::proc_t>("postproc").value();
    auto postblock = postproc(req->ptex, _ctx, dblock);
    OrkAssert(postblock);
  } else {
    // printf("ptex<%p> no postproc\n", ptex);
  }
  req->ptex->_residenceState.fetch_or(1);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
