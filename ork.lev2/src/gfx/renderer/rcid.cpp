////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>

#include <ork/application/application.h>
#include <ork/kernel/any.h>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/compositor.h>

template class ork::orklut<ork::CrcString, ork::lev2::rendervar_t>;

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

const RenderContextInstData RenderContextInstData::Default(nullptr);

///////////////////////////////////////////////////////////////////////////////

RenderContextInstData::RenderContextInstData(rcfd_ptr_t RCFD)
    : _held_rcfd(RCFD) { //
  if (_held_rcfd) {
    mpActiveRenderer = _held_rcfd->_renderer;
  }

  _genMatrix = [this]()->fmtx4 {
    return _held_rcfd->GetTarget()->MTXI()->RefMMatrix();
  };

}

rcfd_ptr_t RenderContextInstData::rcfd() const {
  return _held_rcfd;
}


///////////////////////////////////////////////////////////////////////////////

rcid_ptr_t RenderContextInstData::create(rcfd_ptr_t the_rcfd){
  return std::make_shared<RenderContextInstData>(the_rcfd);
}

///////////////////////////////////////////////////////////////////////////////

Context* RenderContextInstData::context() const {
  OrkAssert(_held_rcfd);
  return _held_rcfd->GetTarget();
}

///////////////////////////////////////////////////////////////////////////////

void RenderContextInstData::SetRenderer(const IRenderer* rnd) {
  mpActiveRenderer = rnd;
}

///////////////////////////////////////////////////////////////////////////////

void RenderContextInstData::setRenderable(const IRenderable* rnd) {
  _irenderable = rnd;
  if(_irenderable){
    _genMatrix = _irenderable->genMatrixLambda();
  }
}

///////////////////////////////////////////////////////////////////////////////

const IRenderer* RenderContextInstData::GetRenderer(void) const {
  return mpActiveRenderer;
}

///////////////////////////////////////////////////////////////////////////////

const XgmMaterialStateInst* RenderContextInstData::GetMaterialInst() const {
  return mMaterialInst;
}
int RenderContextInstData::GetMaterialIndex(void) const {
  return miMaterialIndex;
}
int RenderContextInstData::GetMaterialPassIndex(void) const {
  return miMaterialPassIndex;
}
void RenderContextInstData::SetMaterialIndex(int idx) {
  miMaterialIndex = idx;
}
void RenderContextInstData::SetMaterialPassIndex(int idx) {
  miMaterialPassIndex = idx;
}
void RenderContextInstData::SetMaterialInst(const XgmMaterialStateInst* mi) {
  mMaterialInst = mi;
}
///////////////////////////////////////////////////////////////////////////////
fmtx4 RenderContextInstData::worldMatrix() const {
  return _genMatrix();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
