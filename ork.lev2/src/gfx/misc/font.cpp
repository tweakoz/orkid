///////////////////////////////////////////////////////////
// Copyright 2007, Michael T. Mayers, all rights reserved.
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

namespace ork::lev2 {

const int Font::kMaxChars = 16384;

///////////////////////////////////////////////////////////////////////////////

int FontDesc::stringWidth(int numchars) const {
  return miAdvanceWidth * numchars;
}
int FontDesc::stringHeight(int numlines) const {
  return miAdvanceHeight*numlines;
}

///////////////////////////////////////////////////////////////////////////////

Font::Font(){

}

Font::Font(const std::string& fontname, const std::string& filename)
    : msFileName(filename)
    , msFontName(fontname) {
}

///////////////////////////////////////////////////////////////////////////////

int Font::stringWidth(int numchars) const {
  return _fontdesc->stringWidth(numchars);
}

///////////////////////////////////////////////////////////////////////////////

int Font::stringHeight(int numlines) const {
  return numlines * _fontdesc->miAdvanceHeight;
}

///////////////////////////////////////////////////////////////////////////////

int Font::charHeight() const {
  return _fontdesc->miCharHeight;
}

///////////////////////////////////////////////////////////////////////////////

int Font::centerY(int c) const {
  return c - (_fontdesc->miCharHeight >> 1);
}

///////////////////////////////////////////////////////////////////////////////

GfxMaterial* Font::material() const {
  return _use_deferred ? _materialDeferred.get() : mpMaterial;
}

///////////////////////////////////////////////////////////////////////////////

const FontDesc& Font::description() const {
  return *_fontdesc;
}

///////////////////////////////////////////////////////////////////////////////

void Font::load(Context* context, fontdesc_ptr_t fdesc) {

  //printf("Font<%s> load<%s>\n", msFontName.c_str(), msFileName.c_str());
  auto FXI = context->FXI();

  //////////////////////////////////////////////////////////////////////
  AssetPath apath(msFileName.c_str());
  auto txi                                                 = context->TXI();
  _texture                                                 = std::make_shared<Texture>();
  _texture->_vars->makeValueForKey<bool>("loadimmediate") = true;
  txi->LoadTexture(apath, _texture);
  //////////////////////////////////////////////////////////////////////

  mpMaterial = new GfxMaterialUIText;
  mpMaterial->gpuInit(context);

  _materialDeferred = std::make_shared<PBRMaterial>();

  //////////////////////////////////////////////////////////////////////
  // freestyle text material — ONE technique for mono and stereo alike.
  //  This used to resolve "uitext_stereo", which ui.fxv2 has never defined: the lookup
  //  returned null, the forced-technique pipeline came back non-null with a null
  //  _technique, and the first VR text draw dereferenced it. Stereo text now draws the
  //  ordinary uitext technique, which the multiview shader variant makes per-view.
  //////////////////////////////////////////////////////////////////////

  _fs_material = std::make_shared<FreestyleMaterial>();
  _fs_material->gpuInit(context, "orkshader://ui");
  _fs_material->_rasterstate->_priority = 128;
  _tek_text = _fs_material->technique("uitext");
  OrkAssert(_tek_text);
  FxPipelinePermutation permu;
  permu._forced_technique = _tek_text;
  auto fxcache            = _fs_material->pipelineCache();
  OrkAssert(fxcache);
  _pipe_text = fxcache->findPipeline(permu);
  OrkAssert(_pipe_text);
  auto paramMVP      = _fs_material->param("mvp");
  auto paramColorMap = _fs_material->param("ColorMap");
  auto paramModColor = _fs_material->param("ModColor");
  _pipe_text->bindParam(paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
  _pipe_text->bindParam(paramColorMap, _texture);
  _pipe_text->bindParam(paramModColor, "RCFD_MODCOLOR"_crcsh);

  //////////////////////////////////////////////////////////////////////
  // mono mode
  //////////////////////////////////////////////////////////////////////

  mpMaterial->SetTexture(ETEXDEST_DIFFUSE, _texture.get());
  _texture->TexSamplingMode().presetPointAndClamp();
  //context->TXI()->ApplySamplingMode(_texture.get());

  //_materialDeferred->_asset_texcolor = asset::AssetManager<lev2::TextureAsset>::load(apath);
  _materialDeferred->_texColor = _texture; //_materialDeferred->_asset_texcolor.GetTexture();
  //_materialDeferred->gpuInit(context);

  _fs_material->_rasterstate->_name = "Font";

}

} //namespace ork::lev2 {
