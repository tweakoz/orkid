////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/reflect/properties/registerX.inl>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
void OverlayStringDrawable::enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  auto& cb_renderable = renderer->enqueueCallback();
  auto worldmatrix = item->mXfData._worldTransform->composed();
  cb_renderable.SetMatrix(worldmatrix);
  cb_renderable.SetObject(GetOwner());
  cb_renderable.SetRenderCallback(_rendercb);
  cb_renderable.SetSortKey(0x7fff);
  cb_renderable._drawDataA.set<std::string>(_currentString);
  cb_renderable.SetModColor(renderer->GetTarget()->RefModColor());
}
///////////////////////////////////////////////////////////////////////////////
OverlayStringDrawable::OverlayStringDrawable()
    : Drawable() {

      _font = "i14";
      _color = fcolor4::Yellow();

  _rendercb = [this](lev2::RenderContextInstData& RCID){
    auto context = RCID.context();
    auto mtxi = context->MTXI();
    auto RCFD = RCID._RCFD;
    const auto& CPD             = RCFD->topCPD();
    const CameraMatrices* cmtcs = CPD.cameraMatrices();
    const CameraData& cdata     = cmtcs->_camdat;
    auto renderable = (CallbackRenderable*) RCID._dagrenderable;
    auto worldmatrix = renderable->_worldMatrix;
    auto& current_string = renderable->_drawDataA.get<std::string>();
    const auto& vprect = CPD.mDstRect;


    auto PMatrix = mtxi->Ortho(vprect._x, vprect._x+vprect._w, vprect._y, vprect._y+vprect._h, 0,1);
    fmtx4 wmatrix;
    wmatrix.compose(fvec3(_position.x,_position.y,0),fquat(),_scale);

    mtxi->PushMMatrix(wmatrix);
    mtxi->PushVMatrix(fmtx4::Identity());
    mtxi->PushPMatrix(PMatrix);
    context->PushModColor(_color);
    FontMan::PushFont(_font);
    auto font = FontMan::currentFont();
    font->_use_deferred = not (RCFD->_renderingmodel=="FORWARD"_crcu);

    FontMan::beginTextBlock(context);
    FontMan::DrawText(context, 0, 0, current_string.c_str());
    FontMan::endTextBlock(context);
    font->_use_deferred = false;
    FontMan::PopFont();
    context->PopModColor();
    mtxi->PopMMatrix();
    mtxi->PopVMatrix();
    mtxi->PopPMatrix();

  };
}
///////////////////////////////////////////////////////////////////////////////
OverlayStringDrawable::~OverlayStringDrawable() {
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
