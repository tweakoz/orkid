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
static logchannel_ptr_t logchan_pbr_lm = logger()->createChannel("mtlpbrLM", fvec3(0.8, 0.8, 0.1), true);
///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::setActiveLightMap(std::string name, fvec3 c ){
  auto it = _lightmap_indices.find(name);
  if(it!=_lightmap_indices.end()){
    int index = it->second;
    _lightmapColors[index] = c;
  }
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::conformLightmaps() {
  if(_lightmap_image_assets.size()==0){
    auto img = std::make_shared<Image>();
    img->initRGB8WithColor(64, 64, fvec3(1,1,1),EBufferFormat::RGB8);
    _lightmap_image_assets["default"] = img;
  }

  ////////////////////////////////
  // retain and find biggest size
  ////////////////////////////////
  size_t max_w = 64;
  size_t max_h = 64;
  std::set<image_ptr_t> retain_lmaps;
  std::unordered_map<std::string,image_ptr_t> lmap_by_name;
  for(auto lmitem : _lightmap_image_assets){
    auto name = lmitem.first;
    auto lm = lmitem.second;
    max_w = std::max(max_w, lm->_width);
    max_h = std::max(max_h, lm->_height);
    retain_lmaps.insert(lm);
    lmap_by_name[name] = lm;
    printf("conformLightmaps st0 name<%s> fmt<%s>\n", name.c_str(), EBufferFormatToName(lm->_format).c_str());
  }
  ////////////////////////////////
  // convert to RGB8
  ////////////////////////////////
  std::unordered_map<std::string,image_ptr_t> images_to_rgb;
  std::atomic<int> sync_rgb = 0;
  for(auto lmitem : _lightmap_image_assets){
    auto name = lmitem.first;
    auto lm = lmitem.second;
    max_w = std::max(max_w, lm->_width);
    max_h = std::max(max_h, lm->_height);
    if(lm->_format!=EBufferFormat::RGB8){
      auto rgb = std::make_shared<Image>();
      images_to_rgb[name] = rgb;
      sync_rgb++;
      auto OP = [=, &sync_rgb](){
        rgb->convertFromImageToFormat(*lm, EBufferFormat::RGB8);
        sync_rgb--;
      };
      opq::concurrentQueue()->enqueue(OP);
    }
  }
  while(sync_rgb>0){
    usleep(1000);
  }
  ////////////////////////////////
  for(auto lmitem : images_to_rgb){
    auto name = lmitem.first;
    auto lm = lmitem.second;
    lmap_by_name[name] = lm;
    OrkAssert(lm->_format==EBufferFormat::RGB8);
  }
  ////////////////////////////////
  // conform size to largest 
  ////////////////////////////////
  std::atomic<int> sync_resize = 0;
  std::unordered_map<std::string,image_ptr_t> resized_lmaps;
  for(auto lmitem : lmap_by_name){
    auto name = lmitem.first;
    auto lm = lmitem.second;
    if(lm->_width!=max_w || lm->_height!=max_h){
      auto resized = std::make_shared<Image>();
      resized_lmaps[name] = resized;
        sync_resize++;
        auto OP = [=, &sync_resize](){
          resized->resizedOf(*lm, max_w, max_h);
          sync_resize--;
        };
        opq::concurrentQueue()->enqueue(OP);
    }
  }
  while(sync_resize>0){
    usleep(1000);
  }
  ////////////////////////////////
  for(auto lmitem : resized_lmaps){
    auto name = lmitem.first;
    auto lm = lmitem.second;
    lmap_by_name[name] = lm;
  }
  ////////////////////////////////
  for( auto item : lmap_by_name ){
    auto name = item.first;
    auto img = item.second;
    _lightmap_image_assets[name] = img;
  }
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::assignLightmaps(Context* ctx){
  printf("beg PBRMaterial::assignLightmaps\n");
  conformLightmaps();
  if(_lightmap_image_assets.size()){
    TextureArrayInitData TID;
    TID._slices.resize(_lightmap_image_assets.size());
    uint32_t idx = 0;
    for( auto item : _lightmap_image_assets ){
      auto name = item.first;
      auto img = item.second;
      TID._slices[idx] = TextureArrayInitSubItem{idx, img};
      _lightmap_indices[name] = idx;
      idx++;
    }
    ////////////////////////////////
    _texLightMapArray = std::make_shared<Texture>();
    _texLightMapArray->_debugName = "pbrLMtexarray";
    ctx->TXI()->initTextureArray2DFromData(_texLightMapArray.get(), TID);
    ////////////////////////////////
  }
  printf("end PBRMaterial::assignLightmaps\n");
}

} //namespace ork::lev2 {
