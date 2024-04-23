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
#include <ork/lev2/gfx/gfxvtxbuf.inl>

ImplementReflectionX(ork::lev2::LabeledPointDrawableData, "LabeledPointDrawableData");

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void LabeledPointDrawableData::describeX(object::ObjectClass* clazz) {
}
///////////////////////////////////////////////////////////////////////////////
LabeledPointDrawableData::LabeledPointDrawableData() {
  _font = "i16";
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
  cb_renderable._pickID = _pickID;
  cb_renderable.SetRenderCallback(_rendercb);
  cb_renderable.SetSortKey(0x7fff);
  cb_renderable.SetModColor(renderer->GetTarget()->RefModColor());
}
///////////////////////////////////////////////////////////////////////////////
struct LabeledPointRenderData {
  vtxbufferbase_ptr_t _vtxbuf;
};
///////////////////////////////////////////////////////////////////////////////
LabeledPointDrawable::LabeledPointDrawable(const LabeledPointDrawableData* data)
    : Drawable() {
    _data = data;
    _properties = std::make_shared<varmap::VarMap>();
    auto rdata = _properties->makeSharedForKey<LabeledPointRenderData>("rdata");
    /////////////////////////////////////////////////////////////
    _rendercb = [this,rdata](lev2::RenderContextInstData& RCID) {
        auto context = RCID.context();
        if(nullptr==rdata->_vtxbuf){
            auto vb = VertexBufferBase::CreateVertexBuffer(EVtxStreamFormat::V12,65536,false);
            rdata->_vtxbuf = vb;
            vb->SetRingLock(true);
        }
        /////////////////////////////////////////////////////////////
        size_t num_points = _data->_points_only_mesh->numVertices();
        if(num_points>0){
            VtxWriter<VtxV12> vw;
            vw.Lock(context,rdata->_vtxbuf.get(),num_points);
            for( size_t i=0; i<num_points; i++ ){
                auto inp_vtx = _data->_points_only_mesh->vertex(i);
                const auto& pos = inp_vtx->mPos;
                vw.AddVertex(VtxV12(pos.x,pos.y,pos.z));
            }
            vw.UnLock(context);
            /////////////////////////////////////////////////////////////
            if( _data->_points_pipeline ){
                _data->_points_pipeline->wrappedDrawCall(RCID, [&]() { //
                    auto gbi = context->GBI();
                    gbi->DrawPrimitiveEML(vw, PrimitiveType::POINTS);
                });
            }
            /////////////////////////////////////////////////////////////
            if( true ) { //_data->_text_pipeline ){
                auto mtxi = context->MTXI();
                auto RCFD = RCID.rcfd();
                const auto& CPD             = RCFD->topCPD();
                const CameraMatrices* cmtcs = CPD.cameraMatrices();
                const CameraData& cdata     = cmtcs->_camdat;
                auto renderable = (CallbackRenderable*) RCID._irenderable;
                //auto& current_string = renderable->_drawDataA.get<std::string>();
                const auto& vprect = CPD.mDstRect;

                auto vpmtx = cmtcs->VPMONO();

                mtxi->PushUIMatrix();
                context->PushModColor(fvec4(1,1,1,1));
                FontMan::PushFont(_data->_font.c_str());
                auto font = FontMan::currentFont();
                const auto& fontdesc = font->description();
                font->_use_deferred = RCFD->_renderingmodel.isDeferredPBR();

                
                FontMan::beginTextBlock(context);
                _data->_points_only_mesh->visitAllVertices(
                    [&](meshutil::vertex_const_ptr_t v) {
                        auto hpos = fvec4(dvec3_to_fvec3(v->mPos),1).transform(vpmtx);
                        auto dpos = hpos.xyz() / hpos.w;
                        dpos *= 0.5;
                        dpos += fvec3(0.5,0.5,0.0f);
                        dpos *= fvec3(vprect._w,vprect._h,1.0f);
                        dpos += fvec3( -fontdesc.miCharWidth*0.5f, fontdesc.miCharHeight*0.5f, 0.0f);
                        auto str = FormatString("%d",int(v->_poolindex));
                        FontMan::DrawText(context, dpos.x, vprect._h-dpos.y, str.c_str());
                    }
                );

                FontMan::endTextBlock(context);
                font->_use_deferred = false;
                FontMan::PopFont();
                context->PopModColor();
                mtxi->PopUIMatrix();
                
            }
        }
    };
}
///////////////////////////////////////////////////////////////////////////////
LabeledPointDrawable::~LabeledPointDrawable() {
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
