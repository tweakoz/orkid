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
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/ui/ui.h>
#include <ork/file/file.h>
#include <ork/math/misc_math.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/debug.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

bool TextureInterface::_loadXTXTexture(texture_ptr_t ptex, datablock_ptr_t datablock) {
  //printf("Loadtex <%p:%s>\n", (void*)ptex.get(), ptex->_debugName.c_str());
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
  void_lambda_t lamb = [=]() {
    this->_loadXTXTextureMainThreadPart(load_req);
  };
  opq::mainSerialQueue()->enqueue(lamb);
  ///////////////////////////////////////////////
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void TextureInterface::_loadXTXTextureMainThreadPart(texloadreq_ptr_t req) {
  OrkAssert(req->_cmipchain.get() != nullptr);
  int inummips = req->_cmipchain->_levels.size();
  OrkAssert(inummips > 0);
  _createFromLoadReq(req); 
  req->ptex->_num_mips = inummips;
  req->ptex->TexSamplingMode().presetTrilinearWrap();
  this->ApplySamplingMode(req->ptex.get());
  req->ptex->_dirty = false;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
