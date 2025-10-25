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

#if defined(__APPLE__)
extern bool _macosUseHIDPI;
#endif

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

  printf("Font<%s> load<%s>\n", msFontName.c_str(), msFileName.c_str());
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
  // stereo mode
  //////////////////////////////////////////////////////////////////////

  _fs_material = std::make_shared<FreestyleMaterial>();
  _fs_material->gpuInit(context, "orkshader://ui");
  _fs_material->_rasterstate->_priority = 128;
  _tek_stereo_text = _fs_material->technique("uitext_stereo");
  FxPipelinePermutation permu;
  permu._forced_technique = _tek_stereo_text;
  auto fxcache            = _fs_material->pipelineCache();
  OrkAssert(fxcache);
  _pipe_stereo = fxcache->findPipeline(permu);
  OrkAssert(_pipe_stereo);
  auto paramMVPL     = _fs_material->param("mvp_l");
  auto paramMVPR     = _fs_material->param("mvp_r");
  auto paramColorMap = _fs_material->param("ColorMap");
  auto paramModColor = _fs_material->param("ModColor");
  _pipe_stereo->bindParam(paramMVPL, "RCFD_Camera_MVP_Left"_crcsh);
  _pipe_stereo->bindParam(paramMVPR, "RCFD_Camera_MVP_Right"_crcsh);
  _pipe_stereo->bindParam(paramColorMap, _texture);
  _pipe_stereo->bindParam(paramModColor, "RCFD_MODCOLOR"_crcsh);

  //////////////////////////////////////////////////////////////////////
  // mono mode
  //////////////////////////////////////////////////////////////////////

  mpMaterial->SetTexture(ETEXDEST_DIFFUSE, _texture.get());
  _texture->TexSamplingMode().presetPointAndClamp();
  //context->TXI()->ApplySamplingMode(_texture.get());

  //_materialDeferred->_asset_texcolor = asset::AssetManager<lev2::TextureAsset>::load(apath);
  _materialDeferred->_texColor = _texture; //_materialDeferred->_asset_texcolor.GetTexture();
  //_materialDeferred->gpuInit(context);

#if defined(__APPLE__)
  /*if (_macosUseHIDPI) {
    _fontdesc->miCharWidth *= 2;
    _fontdesc->miCharHeight *= 2;
    _fontdesc->miCharOffsetX *= 2;
    _fontdesc->miCharOffsetY *= 2;
    _fontdesc->miYShift *= 2;
    _fontdesc->miAdvanceWidth *= 2;
    _fontdesc->miAdvanceHeight *= 2;
  }*/
#endif
}

} //namespace ork::lev2 {
