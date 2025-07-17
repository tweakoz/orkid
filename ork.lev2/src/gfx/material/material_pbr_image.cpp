///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/util/crc.h>
#include <ork/file/path.h>
#include <ork/file/chunkfile.inl>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/gfx/brdf.inl>
#include <ork/pch.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <OpenImageIO/imageio.h>
#include <ork/kernel/datacache.h>
#include <ork/reflect/properties/registerX.inl>
//
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/util/logger.h>

OIIO_NAMESPACE_USING

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_pbr_img = logger()->configureChannel("mtlpbrIMG", fvec3(0.8, 0.8, 0.1), true);
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// join PBR image set into a texture array
///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::assignImages( lev2::Context* ctx,   //
                                image_ptr_t color,    //
                                image_ptr_t normal,   //
                                image_ptr_t mtlruf,   //
                                image_ptr_t emissive, //
                                image_ptr_t ambocc,   //
                                bool do_conform ) {   //

  OrkAssert(ambocc==nullptr);

  //printf( "assignTextures color<%p> normal<%p> mtlruf<%p> emissive<%p>\n", color.get(), normal.get(), mtlruf.get(), emissive.get() );

  if( do_conform ){
    _image_color = color;
    _image_normal = normal;
    _image_mtlruf = mtlruf;
    _image_emissive = emissive;
    conformImages();
    //printf( "conformed color<%p> normal<%p> mtlruf<%p> emissive<%p>\n", _image_color.get(), _image_normal.get(), _image_mtlruf.get(), _image_emissive.get() );
  }
    
  TextureArrayInitData TID;


  TID._slices.resize(4);
  TID._slices[0] = TextureArrayInitSubItem{"color"_crcu, _image_color};
  TID._slices[1] = TextureArrayInitSubItem{"normal"_crcu, _image_normal};
  TID._slices[2] = TextureArrayInitSubItem{"mtlruf"_crcu, _image_mtlruf};
  TID._slices[3] = TextureArrayInitSubItem{"emissive"_crcu, _image_emissive};
  _texArrayCNMREA = std::make_shared<TextureArray>();
  _texArrayCNMREA->_tex->_debugName = "pbrtexarray";
  auto txi = ctx->TXI();
  txi->initTextureArray2DFromData(_texArrayCNMREA.get(), TID);

}

///////////////////////////////////////////////////////////////////////////////
// PBRMaterial::conformImages
//   we need textures to be same size and format
//   so they can go into a texture array 
///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::conformImages(){
  //////////////////////////
  // retain pre-existing images
  //  so they cant get deleted
  //  until we return
  //////////////////////////
  std::unordered_set<image_ptr_t> retain_images;
  retain_images.insert(_image_color);
  retain_images.insert(_image_normal);
  retain_images.insert(_image_mtlruf);
  retain_images.insert(_image_emissive);  
  retain_images.insert(_image_ambocc);  
  //////////////////////////
  // get biggest size
  //  and mark down non-rgb images
  //////////////////////////
  size_t max_w = 64;
  size_t max_h = 64;
  std::set<image_ptr_t> images_to_rgb;
  if (_image_color != nullptr) {
    max_w = std::max(max_w, _image_color->_width);
    max_h = std::max(max_h, _image_color->_height);
    if(_image_color->_format!=EBufferFormat::RGB8){
      images_to_rgb.insert(_image_color);
    }
  }
  if (_image_normal != nullptr) {
    max_w = std::max(max_w, _image_normal->_width);
    max_h = std::max(max_h, _image_normal->_height);
    if(_image_normal->_format!=EBufferFormat::RGB8){
      images_to_rgb.insert(_image_normal);
    }
  }
  if (_image_mtlruf != nullptr) {
    max_w = std::max(max_w, _image_mtlruf->_width);
    max_h = std::max(max_h, _image_mtlruf->_height);
    if(_image_mtlruf->_format!=EBufferFormat::RGB8){
      images_to_rgb.insert(_image_mtlruf);
    }
  }
  if (_image_emissive != nullptr) {
    max_w = std::max(max_w, _image_emissive->_width);
    max_h = std::max(max_h, _image_emissive->_height);
    if(_image_emissive->_format!=EBufferFormat::RGB8){
      images_to_rgb.insert(_image_emissive);
    }
  }
  if (_image_ambocc != nullptr) {
    max_w = std::max(max_w, _image_ambocc->_width);
    max_h = std::max(max_h, _image_ambocc->_height);
    if(_image_ambocc->_format!=EBufferFormat::RGB8){
      images_to_rgb.insert(_image_ambocc);
    }
  }
  //////////////////////////
  std::atomic<int> sync_rgb = 0;
  std::atomic<int> sync_resize = 0;
  std::atomic<int> sync_defaults = 0;
  //////////////////////////
  // convert non-rgb to rgb
  //  overwriting the original images
  //////////////////////////
  for(auto img : images_to_rgb){
    auto rgb = std::make_shared<Image>();
    if(img==_image_color){
      _image_color = rgb;
    }
    if(img==_image_normal){
      _image_normal = rgb;
    }
    if(img==_image_mtlruf){
      _image_mtlruf = rgb;
    }
    if(img==_image_emissive){
      _image_emissive = rgb;
    }
    if(img==_image_ambocc){
      _image_ambocc = rgb;
    }
    sync_rgb++;
    auto OP = [=, &sync_rgb](){
      rgb->convertFromImageToFormat(*img, EBufferFormat::RGB8);
      sync_rgb--;
    };
    opq::concurrentQueue()->enqueue(OP);
  }
  while(sync_rgb>0){
    usleep(1000);
  }
  retain_images.insert(_image_color);
  retain_images.insert(_image_normal);
  retain_images.insert(_image_mtlruf);
  retain_images.insert(_image_emissive);  
  retain_images.insert(_image_ambocc);  
  //////////////////////////
  // now, find out which images need to be resized
  //////////////////////////
  std::set<image_ptr_t> images_to_resize;
  if (_image_color != nullptr) {
    if(_image_color->_width!=max_w || _image_color->_height!=max_h){
      images_to_resize.insert(_image_color);
    }
  }
  if (_image_normal != nullptr) {
    if(_image_normal->_width!=max_w || _image_normal->_height!=max_h){
      images_to_resize.insert(_image_normal);
    }
  }
  if (_image_mtlruf != nullptr) {
    if(_image_mtlruf->_width!=max_w || _image_mtlruf->_height!=max_h){
      images_to_resize.insert(_image_mtlruf);
    }
  }
  if (_image_emissive != nullptr) {
    if(_image_emissive->_width!=max_w || _image_emissive->_height!=max_h){
      images_to_resize.insert(_image_emissive);
    }
  }
  if (_image_ambocc != nullptr) {
    if(_image_ambocc->_width!=max_w || _image_ambocc->_height!=max_h){
      images_to_resize.insert(_image_ambocc);
    }
  }
  //////////////////////////
  // resize the images
  //////////////////////////
  for(auto img : images_to_resize){
    auto resized = std::make_shared<Image>();
    if(img==_image_color){
      _image_color = resized;
    }
    if(img==_image_normal){
      _image_normal = resized;
    }
    if(img==_image_mtlruf){
      _image_mtlruf = resized;
    }
    if(img==_image_emissive){
      _image_emissive = resized;
    }
    if(img==_image_ambocc){
      _image_ambocc = resized;
    }
    sync_resize++;
    auto OP = [=, &sync_resize](){
      resized->resizedOf(*img, max_w, max_h);
      sync_resize--;
    };
    opq::concurrentQueue()->enqueue(OP);
  }
  if(1) while(sync_resize>0){
    usleep(1000);
  }
  retain_images.insert(_image_color);
  retain_images.insert(_image_normal);
  retain_images.insert(_image_mtlruf);
  retain_images.insert(_image_emissive);  
  retain_images.insert(_image_ambocc);  
  //////////////////////////
  // now create defaults if they do not exist
  //////////////////////////
  if (_image_color == nullptr) {
      sync_defaults++;
    auto OP = [=, &sync_defaults](){
      fvec3 color = fvec3(1,1,1);
      _image_color = std::make_shared<Image>();
      _image_color->initRGB8WithColor(max_w, max_h, color, EBufferFormat::RGB8);
      sync_defaults--;
    };
    opq::concurrentQueue()->enqueue(OP);
  }
  if (_image_normal == nullptr) {
      sync_defaults++;
    auto OP = [=, &sync_defaults](){
      fvec3 color = fvec3(0.5,0.5,1);
      _image_normal = std::make_shared<Image>();
      _image_normal->initRGB8WithColor(max_w, max_h, color, EBufferFormat::RGB8);
      sync_defaults--;
    };
    opq::concurrentQueue()->enqueue(OP);
  }
  if (_image_mtlruf == nullptr) {
    sync_defaults++;
    auto OP = [=, &sync_defaults](){
      fvec3 color = (_metallicFactor == 0.0f) //
                  ? fvec3(1,0,0) //
                  : fvec3(1,0,1);
      _image_mtlruf = std::make_shared<Image>();
      _image_mtlruf->initRGB8WithColor(max_w, max_h, color, EBufferFormat::RGB8);
      sync_defaults--;
    };
    opq::concurrentQueue()->enqueue(OP);
  }
  if (_image_emissive == nullptr) {
    sync_defaults++;
    auto OP = [=, &sync_defaults](){
      fvec3 color = fvec3(0,0,0);
      _image_emissive = std::make_shared<Image>();
      _image_emissive->initRGB8WithColor(max_w, max_h, color, EBufferFormat::RGB8);
      sync_defaults--;
    };
    opq::concurrentQueue()->enqueue(OP);
  }
  if (_image_ambocc == nullptr) {
    sync_defaults++;
    auto OP = [=, &sync_defaults](){
      fvec3 color = fvec3(1,1,1);
      _image_ambocc = std::make_shared<Image>();
      _image_ambocc->initRGB8WithColor(max_w, max_h, color, EBufferFormat::RGB8);
      sync_defaults--;
    };
    opq::concurrentQueue()->enqueue(OP);
  }
  if(1)while(sync_defaults>0){
    usleep(1000);
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2