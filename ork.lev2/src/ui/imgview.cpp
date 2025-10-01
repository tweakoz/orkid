#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/ui/imgview.h>

namespace ork::ui {
///////////////////////////////////////////////////////////////////////////////
ImageView::ImageView(
    const std::string& name)
    : Widget(name){
  _default_color = fvec4(0,0,0,0);
  _texture = std::make_shared<lev2::Texture>();
}
///////////////////////////////////////////////////////////////////////////////
void ImageView::setImage(lev2::image_ptr_t img) {
  _pending_image = img;
}
///////////////////////////////////////////////////////////////////////////////
void ImageView::setImageProvider(lev2::image_provider_ptr_t imgprovider) {
  _imgprovider = imgprovider;
}
///////////////////////////////////////////////////////////////////////////////
void ImageView::DoDraw(drawevent_constptr_t drwev) {

  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto pri = tgt->PRI();
  auto txi    = tgt->TXI();
  auto defmtl = lev2::defaultUIMaterial();

  //////////////////////////////////
  // initial coords (full rect)
  //////////////////////////////////

  int ix1, iy1, ix2, iy2, ixc, iyc;
  LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;
    ixc = ix1 + (_geometry._w >> 1);
    iyc = iy1 + (_geometry._h >> 1);

  //////////////////////////////////
  // lazy init tex material
  //////////////////////////////////

  if(_tex_material==nullptr){
    _tex_material = std::make_shared<lev2::GfxMaterialUITextured>(tgt);
  }

  mtxi->PushUIMatrix();

  //////////////////////////////////
  // draw background quad
  //////////////////////////////////

  defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
  defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
  defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
  tgt->PushModColor(_default_color);

  pri->RenderQuadAtZ(
      defmtl.get(),
      ix1,  // x0
      ix2,  // x1
      iy1,  // y0
      iy2,  // y1
      0.0f, // z
      0.0f,
      1.0f, // u0, u1
      0.0f,
      1.0f // v0, v1
  );

  tgt->PopModColor();

  //////////////////////////////////
  // update image if applicable
  //////////////////////////////////

  if(_imgprovider){
    _pending_image = _imgprovider->_func();
  }
  if(_active_image!=_pending_image){
    _active_image = _pending_image;
    if(_active_image){
      txi->initTextureFromImage(_texture.get(),_active_image);
    }
  }

  //////////////////////////////////
  // if we have an image, draw it
  //////////////////////////////////

  if(not _active_image){
    mtxi->PopUIMatrix();
    return;
  }

  tgt->PushModColor(fvec4(1,1,1,1));
  _tex_material->SetTexture(lev2::ETEXDEST_DIFFUSE, _texture.get());
  _tex_material->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
  _tex_material->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);

  //////////////////////////////////
  // setup coords
  //////////////////////////////////

  if(_maintain_aspect_ratio) {
    // letterbox (or pillarbox)
    float fw = float(_geometry._w);
    float fh = float(_geometry._h);
    float iw = float(_active_image->_width);
    float ih = float(_active_image->_height);
    float fr = fw / fh;
    float ir = iw / ih;
    if(ir>fr){
      // pillarbox
      float nh = fh * (fr/ir);
      ix2 = ix1 + _geometry._w;
      iy1 = iy1 + int((fh-nh)*0.5f);
      iy2 = iy1 + int(nh);
      ixc = ix1 + (_geometry._w >> 1);
      iyc = iy1 + (int(nh) >> 1);
    }
    else{
      // letterbox
      float nw = fw * (ir/fr);
      ix1 = ix1 + int((fw-nw)*0.5f);
      ix2 = ix1 + int(nw);
      iy2 = iy1 + _geometry._h;
      ixc = ix1 + (int(nw) >> 1);
      iyc = iy1 + (_geometry._h >> 1);
    } 
  }

  //////////////////////////////////
  // draw textured quad
  //////////////////////////////////

  pri->RenderQuadAtZ(
      _tex_material.get(),
      ix1,  // x0
      ix2,  // x1
      iy1,  // y0
      iy2,  // y1
      0.0f, // z
      0.0f,
      1.0f, // u0, u1
      0.0f,
      1.0f // v0, v1
  );

  //////////////////////////////////

  tgt->PopModColor();
  mtxi->PopUIMatrix();
}
} // namespace ork::ui
