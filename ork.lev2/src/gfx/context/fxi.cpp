////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/lev2_asset.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

FxInterface::FxInterface()
    : _activeShader(0) {
}

void FxInterface::pushRasterState(rasterstate_ptr_t rs){
  _doPushRasterState(rs);
}
rasterstate_ptr_t FxInterface::popRasterState(){
  return _doPopRasterState();
}

void FxInterface::BindParamTex(const FxShaderParam* hpar, const lev2::TextureAsset* texasset) {
  auto texture = (texasset != nullptr) ? texasset->GetTexture().get() : nullptr;
  if (texture)
    BindParamCTex(hpar, texture);
}

///////////////////////////////////////////////////////////////////////////////

void FxInterface::BeginFrame() {
  _doBeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void FxInterface::Reset() {
  auto target = lev2::contextForCurrentThread();
  target->FXI()->DoOnReset();
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
