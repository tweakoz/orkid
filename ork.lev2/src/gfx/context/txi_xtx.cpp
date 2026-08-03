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
#include <ork/kernel/async_tracker.h>
#include <ork/kernel/debug.h>
#include <ork/lev2/lev2_asset.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

bool TextureInterface::_loadXTXTexture(texture_ptr_t ptex, datablock_ptr_t datablock) {
  //printf("Loadtex <%p:%s>\n", (void*)ptex.get(), ptex->_debugName.c_str());
  auto load_req = std::make_shared<TexLoadReq>();
  load_req->ptex                  = ptex;
  load_req->_inpstream._datablock = datablock;
  load_req->_cmipchain            = std::make_shared<CompressedImageMipChain>();
  load_req->_cmipchain->readXTX(datablock);

  // Transfer asset load request from texture's asset (if present)
  if (ptex->_asset) {
    load_req->_assetloadreq = ptex->_asset->_load_request;
  }

  ///////////////////////////////////////////////
  //auto glto = ptex->_impl.makeShared<GLTextureObject>(this);
  ////////////////////////////////////////////////////////////////////
  ptex->_width     = load_req->_cmipchain->_width;
  ptex->_height    = load_req->_cmipchain->_height;
  ptex->_depth     = 1;
  ptex->_texFormat = load_req->_cmipchain->_format;
  ///////////////////////////////////////////////
   auto keys = load_req->_cmipchain->_varmap.dumpkeys();
  // Producer-side async-work registration for the opq-routed (non-LoadingPhase)
  // texture-upload path: PNG/EXR/HDR (via txi_imgconvert) and raw XTX all funnel their
  // GPU upload through _loadXTXTextureMainThreadPart on the main serial queue. SAME
  // "texture_upload" tag + contract as submitLoadingPhase (see gfxctx.cpp).
  //
  // End DETERMINISTICALLY at the end of the (exactly-once-run) lambda body — NOT via a
  // captured RAII token. opq's Op storage is a static_variant whose operator= copies
  // WITHOUT destroying the prior held value (svariant.h operator= skips _destroy()), so
  // a token riding in the Op's std::function is never destructed after the op runs and
  // the marker would leak forever (observed: 38 begun, 2 released). opq services every
  // enqueued op exactly once, so Begin-then-End-in-body is balanced; a never-run op is
  // only ever dropped at shutdown (after the capture gate, before exit) where a stranded
  // marker is harmless.
  asyncWorkBegin("texture_upload");
  void_lambda_t lamb = [this, load_req]() {
    this->_loadXTXTextureMainThreadPart(load_req);
    asyncWorkEnd("texture_upload");
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
