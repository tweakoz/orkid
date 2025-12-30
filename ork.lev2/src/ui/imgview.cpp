#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
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
void ImageView::setTextureProvider(lev2::texture_provider_ptr_t texprovider) {
  _texprovider = texprovider;
}
///////////////////////////////////////////////////////////////////////////////
void ImageView::setTexture(lev2::texture_ptr_t tex) {
  _texture = tex;
  _texprovider = nullptr;  // Direct texture assignment, not from provider
  _imgprovider = nullptr;  // Clear image provider too
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
    std::string technique = _fs_antialias ? "uitextured_aa" : "uitextured";
    _tex_material = std::make_shared<lev2::GfxMaterialUITextured>(tgt, technique);
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
  // update texture (GPU-direct or image-based)
  //////////////////////////////////

  // GPU-direct path: texture_provider (e.g., VideoToolbox → IOSurface → Vulkan)
  if(_texprovider){
    auto new_texture = _texprovider->getTexture();
    if(new_texture){
      _texture = new_texture;
      // Texture dimensions might have changed
      _active_image = nullptr;  // Mark as having valid texture content
    }
  }
  // Direct texture assignment with embedded update provider (MOVIE textures)
  else if(_texture && _texture->_update_provider){
    // Poll provider to trigger frame updates (updates _impl_2)
    _texture->_update_provider->getTexture();
  }
  // CPU path: image_provider
  else if(_imgprovider){
    _pending_image = _imgprovider->_func();
  }

  // Upload image to texture if changed (CPU path only)
  if(!_texprovider && _active_image!=_pending_image){
    _active_image = _pending_image;
    if(_active_image){
      txi->initTextureFromImage(_texture.get(),_active_image,_generate_mipmaps);
    }
  }

  //////////////////////////////////
  // if we have content, draw it
  //////////////////////////////////

  // For GPU-direct, we have a texture even without _active_image
  // Also handle direct texture assignment (no provider)
  bool has_direct_texture = _texture && _texture->_width > 0 && !_texprovider && !_imgprovider;
  bool has_content = _active_image || (_texprovider && _texture) || has_direct_texture;
  if(!has_content){
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
    // Use texture dimensions (works for both GPU-direct and image-based)
    float iw = _active_image ? float(_active_image->_width) : float(_texture->_width);
    float ih = _active_image ? float(_active_image->_height) : float(_texture->_height);

    if(_invert_aspect){
      std::swap(iw,ih);
    }

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

  if(_img_mesh){
    auto rcfd = std::make_shared<lev2::RenderContextFrameData>(tgt);
    auto rcid = std::make_shared<lev2::RenderContextInstData>(rcfd);
    if(_pipeline_override){
      _pipeline_override->wrappedDrawCall(*rcid,[&](){
        _img_mesh->renderEML(tgt);
      });
    }
    else{
      _tex_material->BeginBlock(tgt,*rcid);
      _img_mesh->renderEML(tgt);
      _tex_material->EndBlock(tgt);
    }
  }
  else {
    if(_pipeline_override){
      auto rcfd = std::make_shared<lev2::RenderContextFrameData>(tgt);
      auto rcid = std::make_shared<lev2::RenderContextInstData>(rcfd);
      _pipeline_override->wrappedDrawCall(*rcid,[&](){
        if( _image_rot_180 ){
          pri->RenderEMLQuadAtZV16T16C16(
            ix2,  // x0
            ix1,  // x1
            iy2,  // y0
            iy1,  // y1
            0.0f, // z
            0.0f,
            1.0f, // u0, u1
            0.0f,
            1.0f // v0, v1
          );
        }
        else {
          pri->RenderEMLQuadAtZV16T16C16(
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
        }
      });
    }
    else {
      if( _image_rot_180 ){
        pri->RenderQuadAtZ(
          _tex_material.get(),
          ix2,  // x0
          ix1,  // x1
          iy2,  // y0
          iy1,  // y1
          0.0f, // z
          0.0f,
          1.0f, // u0, u1
          0.0f,
          1.0f // v0, v1
        );
      }
      else {
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
      }
    }
  }

  //////////////////////////////////

  tgt->PopModColor();
  mtxi->PopUIMatrix();
}
} // namespace ork::ui
