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

ImplementReflectionX(ork::lev2::BillboardStringDrawableData, "BillboardStringDrawableData");
ImplementReflectionX(ork::lev2::InstancedBillboardStringDrawableData, "InstancedBillboardStringDrawableData");

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void BillboardStringDrawableData::describeX(object::ObjectClass* clazz) {}
///////////////////////////////////////////////////////////////////////////////
BillboardStringDrawableData::BillboardStringDrawableData(AssetPath path){

}
///////////////////////////////////////////////////////////////////////////////
drawable_ptr_t BillboardStringDrawableData::createDrawable() const {
  auto rval = std::make_shared<BillboardStringDrawable>();
  rval->_currentString = _initialString;
  rval->_offset = _offset;
  rval->_scale = _scale;
  rval->_upvec = _upvec;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////

void InstancedBillboardStringDrawableData::describeX(object::ObjectClass* clazz){
}

InstancedBillboardStringDrawableData::InstancedBillboardStringDrawableData(){
}
///////////////////////////////////////////////////////////////////////////////
drawable_ptr_t InstancedBillboardStringDrawableData::createDrawable() const {
  auto drw = std::make_shared<InstancedBillboardStringDrawable>();
  drw->_data = this;
  //drw->_currentString = _initialString;
  drw->_offset = _offset;
  drw->_scale = _scale;
  drw->_upvec = _upvec;
  //drw->bindModelAsset(_assetpath);
  //drw->_modcolor = _modcolor;
  return drw;
}
///////////////////////////////////////////////////////////////////////////////
void BillboardStringDrawable::enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const {
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
BillboardStringDrawable::BillboardStringDrawable()
    : Drawable() {

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

    auto PMatrix = cmtcs->GetPMatrix();
    auto VMatrix = cmtcs->GetVMatrix();

    fmtx4 bbrotmtx = VMatrix.inverse();
    fmtx4 mtxflipy;
    mtxflipy.setScale(1,-1,1);


    fvec3 offset = this->_offset;

    fvec3 trans = worldmatrix.translation()+offset;
    fmtx4 bbmatrix;
    bbmatrix.compose(trans,fquat(),_scale);


    mtxi->PushMMatrix((mtxflipy*bbrotmtx)*bbmatrix);
    mtxi->PushVMatrix(VMatrix);
    mtxi->PushPMatrix(PMatrix);
    context->PushModColor(_color);
    FontMan::PushFont("i14");
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
BillboardStringDrawable::~BillboardStringDrawable() {
}
/////////////////////////////////////////////////////////////////////
void InstancedBillboardStringDrawable::enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  auto& cb_renderable = renderer->enqueueCallback();
  //auto worldmatrix = item->mXfData._worldTransform->composed();
  //cb_renderable.SetMatrix(worldmatrix);
  cb_renderable.SetObject(GetOwner());
  cb_renderable.SetRenderCallback(_rendercb);
  cb_renderable.SetSortKey(0x7fff);
  cb_renderable._drawDataB.set<const InstancedBillboardStringDrawable*>(this);
  cb_renderable.SetModColor(renderer->GetTarget()->RefModColor());  
}
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
InstancedBillboardStringDrawable::InstancedBillboardStringDrawable(){
  _rendercb = [this](lev2::RenderContextInstData& RCID){
    auto context = RCID.context();
    auto mtxi = context->MTXI();
    auto RCFD = RCID._RCFD;
    const auto& CPD             = RCFD->topCPD();
    const CameraMatrices* cmtcs = CPD.cameraMatrices();
    const CameraData& cdata     = cmtcs->_camdat;
    auto renderable = (CallbackRenderable*) RCID._dagrenderable;
    //auto instance_data = renderable->_drawDataA.get<instanceddrawinstancedata_ptr_t>();
    auto drawable = renderable->_drawDataB.get<const InstancedBillboardStringDrawable*>();
    auto instance_data = drawable->_instancedata;

    auto PMatrix = cmtcs->GetPMatrix();
    auto VMatrix = cmtcs->GetVMatrix();

    fmtx4 bbrotmtx;
    fmtx4 bbmatrix;
    fvec3 offset = this->_offset;
    fquat rot = fquat();

    mtxi->PushVMatrix(VMatrix);
    mtxi->PushPMatrix(PMatrix);
    context->PushModColor(fcolor4::Yellow());
    FontMan::PushFont("i48");
    auto font = FontMan::currentFont();
    font->_use_deferred = true;

    _text_items.clear();

    for( size_t i=0; i<instance_data->_count; i++ ){
        const auto& miscdata = instance_data->_miscdata[i];
        if( auto md_as_str = miscdata.tryAs<std::string>() ){
          const auto& string = md_as_str.value();
          auto& out_item = _text_items.emplace_back();

          const auto& worldmatrix = instance_data->_worldmatrices[i];
          auto wtrans = worldmatrix.translation();
          bbrotmtx.createBillboard2(wtrans,cdata.mEye,this->_upvec);
          fvec3 trans = wtrans+offset;
          bbmatrix.compose(trans,rot,this->_scale);
          out_item._wmatrix = bbrotmtx*bbmatrix;
          out_item._text = string.c_str();
        }
    }
    FontMan::DrawTextItems(context,_text_items);
    font->_use_deferred = false;
    FontMan::PopFont();
    context->PopModColor();
    mtxi->PopVMatrix();
    mtxi->PopPMatrix();
  };
}
InstancedBillboardStringDrawable::~InstancedBillboardStringDrawable(){

}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
