////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::lev2::LabeledPointDrawableData, "LabeledPointDrawableData");

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void LabeledPointDrawableData::describeX(object::ObjectClass* clazz) {
}
///////////////////////////////////////////////////////////////////////////////
LabeledPointDrawableData::LabeledPointDrawableData() {
  _font = "i14";
}
///////////////////////////////////////////////////////////////////////////////
drawable_ptr_t LabeledPointDrawableData::createDrawable() const {
  auto rval            = std::make_shared<LabeledPointDrawable>(this);
  rval->_rendercb_user = _onRender;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void LabeledPointDrawable::enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  auto& cb_renderable = renderer->enqueueCallback();
  auto worldmatrix    = item->mXfData._worldTransform->composed();
  cb_renderable.SetMatrix(worldmatrix);
  cb_renderable.SetObject(GetOwner());
  cb_renderable.SetRenderCallback(_rendercb);
  cb_renderable.SetSortKey(0x7fff);
  cb_renderable.SetModColor(renderer->GetTarget()->RefModColor());
}
///////////////////////////////////////////////////////////////////////////////
struct LabeledPointRenderData {
  vtxbufferbase_ptr_t _vtxbuf;
  fxpipeline_ptr_t _pipeline;
};
///////////////////////////////////////////////////////////////////////////////
LabeledPointDrawable::LabeledPointDrawable(const LabeledPointDrawableData* data)
    : Drawable() {
  _data = data;
  // _font = "i14";
  // _color = fcolor4::Yellow();
  auto rdata = _properties->makeSharedForKey<LabeledPointRenderData>("rdata");
  _rendercb = [this,rdata](lev2::RenderContextInstData& RCID) {
    auto context = RCID.context();
    if(nullptr==rdata->_vtxbuf){
        auto vb = VertexBufferBase::CreateVertexBuffer(EVtxStreamFormat::V12,1024,false);
        rdata->_vtxbuf = vb;
        rdata->_pipeline = nullptr;

    }

    size_t num_points = _data->_points_only_mesh->numVertices();
    VtxWriter<VtxV12> vw;
    vw.Lock(context,rdata->_vtxbuf.get(),num_points);
    for( size_t i=0; i<num_points; i++ ){
        auto inp_vtx = _data->_points_only_mesh->vertex(i);
        const auto& pos = inp_vtx->mPos;
        vw.AddVertex(VtxV12(pos.x,pos.y,pos.z));
    }
    vw.UnLock(context);
    /*
    auto mtxi = context->MTXI();
    auto RCFD = RCID._RCFD;
    const auto& CPD             = RCFD->topCPD();
    const CameraMatrices* cmtcs = CPD.cameraMatrices();
    const CameraData& cdata     = cmtcs->_camdat;
    auto renderable = (CallbackRenderable*) RCID._irenderable;
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
    font->_use_deferred = RCFD->_renderingmodel.isDeferredPBR();

    FontMan::beginTextBlock(context);
    FontMan::DrawText(context, 0, 0, current_string.c_str());
    FontMan::endTextBlock(context);
    font->_use_deferred = false;
    FontMan::PopFont();
    context->PopModColor();
    mtxi->PopMMatrix();
    mtxi->PopVMatrix();
    mtxi->PopPMatrix();
    */
  };
}
///////////////////////////////////////////////////////////////////////////////
LabeledPointDrawable::~LabeledPointDrawable() {
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
