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
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::lev2::StringDrawableData, "StringDrawableData");
ImplementReflectionX(ork::lev2::BillboardStringDrawableData, "BillboardStringDrawableData");
ImplementReflectionX(ork::lev2::InstancedBillboardStringDrawableData, "InstancedBillboardStringDrawableData");

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void StringDrawableData::describeX(object::ObjectClass* clazz) {
}
///////////////////////////////////////////////////////////////////////////////
StringDrawableData::StringDrawableData(AssetPath path) {
  _font = "i14";
}
///////////////////////////////////////////////////////////////////////////////
drawable_ptr_t StringDrawableData::createDrawable() const {
  auto rval            = std::make_shared<StringDrawable>(this);
  rval->_rendercb_user = _onRender;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
StringDrawable::StringDrawable(const StringDrawableData* data)
    : Drawable() {
  _data = data;

  _rendercb = [this](lev2::RenderContextInstData& RCID) {
    /////////////////////////////////////
    if (_rendercb_user) {
      _rendercb_user(RCID);
    }
    /////////////////////////////////////
    auto context         = RCID.context();
    auto mtxi            = context->MTXI();
    auto fbi             = context->FBI();
    auto RCFD            = RCID._RCFD;
    const auto& CPD      = RCFD->topCPD();
    auto renderable      = (CallbackRenderable*)RCID._irenderable;
    auto data            = renderable->_drawDataA.get<const StringDrawableData*>();
    auto& current_string = data->_initialString;
    auto fontname        = data->_font;
    if (fontname.empty()) {
      fontname = "i14";
    }

    int w = fbi->GetVPW();
    int h = fbi->GetVPH();
    mtxi->PushUIMatrix(w, h);

    context->PushModColor(data->_color);
    FontMan::PushFont(fontname);
    auto font = FontMan::currentFont();

    font->_use_deferred = RCFD->_renderingmodel.isDeferredPBR();

    auto pos_2d = data->_pos2D;

    FontMan::beginTextBlock(context);
    FontMan::DrawText(context, pos_2d.x, pos_2d.y, current_string.c_str());
    FontMan::endTextBlock(context);
    FontMan::PopFont();
    context->PopModColor();
    mtxi->PopUIMatrix();
  };
}
///////////////////////////////////////////////////////////////////////////////
StringDrawable::~StringDrawable() {
}
///////////////////////////////////////////////////////////////////////////////
void StringDrawable::enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  auto& cb_renderable = renderer->enqueueCallback();
  auto worldmatrix    = item->mXfData._worldTransform->composed();
  cb_renderable.SetMatrix(worldmatrix);
  cb_renderable.SetObject(GetOwner());
  cb_renderable.SetRenderCallback(_rendercb);
  cb_renderable.SetSortKey(0x7fff);
  cb_renderable._drawDataA.set<const StringDrawableData*>(_data);
  cb_renderable.SetModColor(renderer->GetTarget()->RefModColor());
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void BillboardStringDrawableData::describeX(object::ObjectClass* clazz) {
}
///////////////////////////////////////////////////////////////////////////////
BillboardStringDrawableData::BillboardStringDrawableData(AssetPath path) {
  _color = fcolor4::Yellow();
}
///////////////////////////////////////////////////////////////////////////////
drawable_ptr_t BillboardStringDrawableData::createDrawable() const {
  return std::make_shared<BillboardStringDrawable>(this);
}
///////////////////////////////////////////////////////////////////////////////
void BillboardStringDrawable::enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  auto& cb_renderable = renderer->enqueueCallback();
  auto worldmatrix    = item->mXfData._worldTransform->composed();
  cb_renderable.SetMatrix(worldmatrix);
  cb_renderable.SetObject(GetOwner());
  cb_renderable.SetRenderCallback(_rendercb);
  cb_renderable.SetSortKey(0x7fff);
  cb_renderable._drawDataA.set<std::string>(_data->_initialString);
  cb_renderable.SetModColor(renderer->GetTarget()->RefModColor());
}
///////////////////////////////////////////////////////////////////////////////
BillboardStringDrawable::BillboardStringDrawable(const BillboardStringDrawableData* data)
    : Drawable()
    , _data(data) {

  _currentString = _data->_initialString;
  _offset        = _data->_offset;
  _scale         = _data->_scale;
  _upvec         = _data->_upvec;
  _color         = _data->_color;

  static auto tbstate  = std::make_shared<TextBlockState>();
  tbstate->_font       = FontMan::GetFont("i48");
  const auto& FONTDESC = tbstate->_font->description();
  int CHARW            = FONTDESC._3d_char_width;
  int CHARH            = FONTDESC._3d_char_height;
  int CHARUW           = FONTDESC._3d_char_u_width;
  int CHARVH           = FONTDESC._3d_char_v_height;
  int ADVW             = FONTDESC.miAdvanceWidth;
  auto text_rcid = std::make_shared<RenderContextInstData>();

  // todo fix RCID wonkiness

  _rendercb = [this, text_rcid, CHARW, CHARH,ADVW](lev2::RenderContextInstData& RCID) {
    auto context                = RCID.context();
    auto mtxi                   = context->MTXI();
    auto RCFD                   = RCID._RCFD;
    const auto& CPD             = RCFD->topCPD();
    const CameraMatrices* cmtcs = CPD.cameraMatrices();

    auto stereocams = CPD._stereoCameraMatrices;
    auto monocams   = CPD._cameraMatrices;
    auto renderable      = (CallbackRenderable*)RCID._irenderable;
    auto& current_string = renderable->_drawDataA.get<std::string>();
    auto fontman = FontMan::instance();

    bool camrel = _data->_cameraRelativeOffset;

    if (stereocams) {

      auto mcammats = stereocams->_mono;
      const auto& VMATRIX = mcammats->_vmatrix;
      auto IVMATRIX = VMATRIX.inverse();
      auto eyepos = IVMATRIX.translation();

      const auto& VPRECT = CPD.mDstRect;
      fvec2 VP(VPRECT._w>>1, VPRECT._h);

      fvec3 pos = fvec3(0, 0, 0);
      fvec3 pixlen_x, pixlen_y;
      mcammats->GetPixelLengthVectors(pos, VP, pixlen_x, pixlen_y);

      // printf( "pixlen_x<%g %g %g>\n", pixlen_x.x, pixlen_x.y, pixlen_x.z );

      float str_center_x = (current_string.length() * ADVW) * (-0.5f);
      float str_center_y = -0.5f * float(CHARH);
      fmtx4 center_transform;
      center_transform.setTranslation(fvec3(str_center_x, str_center_y, 0));

      auto VL = stereocams->VL();
      auto VR = stereocams->VR();
      auto eyeL = VL.inverse().translation();
      auto eyeR = VR.inverse().translation();
      auto eyeM = (eyeL+eyeR)*0.5f;

      fmtx4 IV2 = IVMATRIX;
      IV2.setColumn(3,fvec4(0,0,0,1));
      text_rcid->_RCFD      = RCFD;
      text_rcid->_genMatrix = [&]() -> fmtx4 {
        fmtx4 text_transform;
        if(camrel){
          auto X = mcammats->_camdat.xNormal();
          auto Y = mcammats->_camdat.yNormal();
          auto Z = mcammats->_camdat.zNormal();
          auto camrelpos = eyeM - (Z * _data->_offset.z)
                                  + (Y * _data->_offset.x)
                                  + (X * _data->_offset.y); // WTF ?
          fmtx4 mtxT, mtxS;
          mtxT.setTranslation(camrelpos);
          mtxS.setScale(_data->_scale / float(CHARW));
          text_transform = mtxT*(IV2*mtxS*center_transform);

          // * (IVMATRIX * center_transform);
        }
        else{
          float scale = pixlen_x.length() * _data->_scale;
          text_transform.compose(_data->_offset, fquat(), fvec3(scale));
          text_transform = text_transform* (IVMATRIX * center_transform);
        }

        return text_transform; 
      };

      tbstate->_overrideRCID = text_rcid;
      tbstate->_blending = _data->_blendmode;

      tbstate->_maxcharcount   = 0; // current_string.size();
      tbstate->_stereo_3d_text = true;
      context->PushModColor(_color);
      fontman->_beginTextBlockWithState(context, tbstate);
      fontman->_enqueueText(
          0,
          0,                      // x, y
          fontman->textwriter(),  // vtxwriter
          current_string.c_str(), // text
          fcolor4::White());
      fontman->_endTextBlockWithState(context, tbstate);
      context->PopModColor();
    } else {

      auto worldmatrix = RCID.worldMatrix();
      fvec3 trans      = worldmatrix.translation() + this->_offset;

      //////////////////////////////////////////////////////

      const CameraData& cdata = cmtcs->_camdat;
      auto PMatrix            = cmtcs->GetPMatrix();
      auto VMatrix            = cmtcs->GetVMatrix();

      fmtx4 bbrotmtx = VMatrix.inverse();
      fmtx4 mtxflipy;
      mtxflipy.setScale(1, -1, 1);

      fmtx4 bbmatrix;
      bbmatrix.compose(trans, fquat(), _scale);
      fontman->_pushFont("i14");
      auto font = FontMan::currentFont();

      //////////////////////////////////////////////////////
      font->_use_deferred = RCFD->_renderingmodel.isDeferredPBR();
      //////////////////////////////////////////////////////

      mtxi->PushMMatrix(fmtx4::multiply_ltor(mtxflipy, bbrotmtx, bbmatrix));
      mtxi->PushVMatrix(VMatrix);
      mtxi->PushPMatrix(PMatrix);

      context->PushModColor(_color);

      FontMan::beginTextBlock(context);
      FontMan::DrawText(context, 0, 0, current_string.c_str());
      FontMan::endTextBlock(context);

      context->PopModColor();
      mtxi->PopMMatrix();
      mtxi->PopVMatrix();
      mtxi->PopPMatrix();

      FontMan::PopFont();

      //////////////////////////////////////////////////////
      font->_use_deferred = false;
      //////////////////////////////////////////////////////
    }
  };
}
///////////////////////////////////////////////////////////////////////////////
BillboardStringDrawable::~BillboardStringDrawable() {
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void InstancedBillboardStringDrawableData::describeX(object::ObjectClass* clazz) {
}

InstancedBillboardStringDrawableData::InstancedBillboardStringDrawableData() {
}
///////////////////////////////////////////////////////////////////////////////
drawable_ptr_t InstancedBillboardStringDrawableData::createDrawable() const {
  auto drw   = std::make_shared<InstancedBillboardStringDrawable>();
  drw->_data = this;
  // drw->_currentString = _initialString;
  drw->_offset = _offset;
  drw->_scale  = _scale;
  drw->_upvec  = _upvec;
  // drw->bindModelAsset(_assetpath);
  // drw->_modcolor = _modcolor;
  return drw;
}
/////////////////////////////////////////////////////////////////////
InstancedBillboardStringDrawable::InstancedBillboardStringDrawable() {
  _rendercb = [this](lev2::RenderContextInstData& RCID) {
    auto context                = RCID.context();
    auto mtxi                   = context->MTXI();
    auto RCFD                   = RCID._RCFD;
    const auto& CPD             = RCFD->topCPD();
    const CameraMatrices* cmtcs = CPD.cameraMatrices();
    const CameraData& cdata     = cmtcs->_camdat;
    auto renderable             = (CallbackRenderable*)RCID._irenderable;
    auto drawable               = renderable->_drawDataB.get<const InstancedBillboardStringDrawable*>();
    auto instance_data          = drawable->_instancedata;

    auto PMatrix = cmtcs->GetPMatrix();
    auto VMatrix = cmtcs->GetVMatrix();

    fmtx4 bbrotmtx;
    fmtx4 bbmatrix;
    fvec3 offset = this->_offset;
    fquat rot    = fquat();

    mtxi->PushVMatrix(VMatrix);
    mtxi->PushPMatrix(PMatrix);
    context->PushModColor(fcolor4::Yellow());
    FontMan::PushFont("i48");
    auto font           = FontMan::currentFont();
    font->_use_deferred = true;

    _text_items.clear();

    for (size_t i = 0; i < instance_data->_count; i++) {
      const auto& miscdata = instance_data->_miscdata[i];
      if (auto md_as_str = miscdata.tryAs<std::string>()) {
        const auto& string = md_as_str.value();
        auto& out_item     = _text_items.emplace_back();

        const auto& worldmatrix = instance_data->_worldmatrices[i];
        auto wtrans             = worldmatrix.translation();
        bbrotmtx.createBillboard2(wtrans, cdata.mEye, this->_upvec);
        fvec3 trans = wtrans + offset;
        bbmatrix.compose(trans, rot, this->_scale);
        out_item._wmatrix = fmtx4::multiply_ltor(bbrotmtx, bbmatrix);
        out_item._text    = string.c_str();
      }
    }
    FontMan::DrawTextItems(context, _text_items);
    font->_use_deferred = false;
    FontMan::PopFont();
    context->PopModColor();
    mtxi->PopVMatrix();
    mtxi->PopPMatrix();
  };
}
InstancedBillboardStringDrawable::~InstancedBillboardStringDrawable() {
}
/////////////////////////////////////////////////////////////////////
void InstancedBillboardStringDrawable::enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  auto& cb_renderable = renderer->enqueueCallback();
  // auto worldmatrix = item->mXfData._worldTransform->composed();
  // cb_renderable.SetMatrix(worldmatrix);
  cb_renderable.SetObject(GetOwner());
  cb_renderable.SetRenderCallback(_rendercb);
  cb_renderable.SetSortKey(0x7fff);
  cb_renderable._drawDataB.set<const InstancedBillboardStringDrawable*>(this);
  cb_renderable.SetModColor(renderer->GetTarget()->RefModColor());
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
