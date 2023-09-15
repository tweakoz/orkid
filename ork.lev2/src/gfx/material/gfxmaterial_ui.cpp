////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxMaterialUI, "MaterialUI")

namespace ork { namespace lev2 {

uimaterial_ptr_t defaultUIMaterial() {
  static auto _g_uimaterial = std::make_shared<GfxMaterialUI>(lev2::contextForCurrentThread());
  return _g_uimaterial;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::Describe() {
}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUI::~GfxMaterialUI() {
}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUI::GfxMaterialUI(Context* pTarg)
    : meType(ETYPE_STANDARD)
    , meUIColorMode(UiColorMode::MOD) {
  miNumPasses = 1;

  auto mtl_load_req = std::make_shared<asset::LoadRequest>();
  mtl_load_req->_asset_path = "orkshader://ui";
  _shaderasset = asset::AssetManager<FxShaderAsset>::load(mtl_load_req);
  _shader      = _shaderasset->GetFxShader();
  // printf( "HMODFX<%p> pTarg<%p>\n", _shader, pTarg );
  OrkAssertI(_shader != 0, "did you copy the shaders folder!\n");

  if (pTarg) {
    gpuInit(pTarg);
  }
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::gpuInit(ork::lev2::Context* pTarg) {
  // printf( "_shader<%p>\n", _shader );

  hTekMod = pTarg->FXI()->technique(_shader, "uidev_modcolor");

  // OrkAssert(hTekMod);
  hTekVtx    = pTarg->FXI()->technique(_shader, "ui_vtx");
  hTekModVtx = pTarg->FXI()->technique(_shader, "ui_vtxmod");
  hTekCircle = pTarg->FXI()->technique(_shader, "uicircle");

  hTransform = pTarg->FXI()->parameter(_shader, "mvp");
  hModColor  = pTarg->FXI()->parameter(_shader, "ModColor");
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUI::BeginBlock(Context* pTarg, const RenderContextInstData& MatCtx) {
  const FxShaderTechnique* htek = 0;

  htek = hTekMod;
  switch (meType) {
    case ETYPE_STANDARD: {
      switch (meUIColorMode) {
        default:
        case UiColorMode::MOD:
          htek = hTekMod;
          break;
        case UiColorMode::VTX:
          htek = hTekVtx;
          break;
        case UiColorMode::MODVTX:
          htek = hTekModVtx;
          break;
      }
      break;
    }
    case ETYPE_CIRCLE:
      htek = hTekCircle;
      break;
    default:
      OrkAssert(false);
  }

  int inumpasses = pTarg->FXI()->BeginBlock(htek, MatCtx);
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::wrappedDraw(Context* context, void_lambda_t drawcb){
  RenderContextInstData RCID;
  this->BeginBlock(context, RCID);
  this->BeginPass(context, 0);
  drawcb();
  this->EndPass(context);
  this->EndBlock(context);
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock();
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::EndPass(Context* pTarg) {
  pTarg->FXI()->EndPass();
}

/////////////////////////////////////////////////////////////////////////

bool GfxMaterialUI::BeginPass(Context* pTarg, int iPass) {
  ///////////////////////////////
  ///////////////////////////////

  const fmtx4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

  ///////////////////////////////

  pTarg->FXI()->BindPass(iPass);
  pTarg->FXI()->BindParamMatrix(hTransform, MatMVP);
  pTarg->FXI()->BindParamVect4(hModColor, pTarg->RefModColor());
  pTarg->FXI()->CommitParams();

  return true;
}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUIText::GfxMaterialUIText(Context* pTarg)
    : hTek(0)
    , hTransform(0)
    , hModColor(0)
    , hColorMap(0) {

  miNumPasses = 1;

  auto mtl_load_req = std::make_shared<asset::LoadRequest>();
  mtl_load_req->_asset_path = "orkshader://ui";

  _shaderasset = asset::AssetManager<FxShaderAsset>::load(mtl_load_req);
  _shader      = _shaderasset->GetFxShader();
  if (pTarg) {
    gpuInit(pTarg);
  }
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::gpuInit(ork::lev2::Context* pTarg) {
  hTek = pTarg->FXI()->technique(_shader, "uitext");

  hTransform = pTarg->FXI()->parameter(_shader, "mvp");
  hModColor  = pTarg->FXI()->parameter(_shader, "ModColor");
  hColorMap  = pTarg->FXI()->parameter(_shader, "ColorMap");
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUIText::BeginBlock(Context* pTarg, const RenderContextInstData& MatCtx) {
  int inumpasses = pTarg->FXI()->BeginBlock(hTek, MatCtx);
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock();
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::EndPass(Context* pTarg) {
  pTarg->FXI()->EndPass();
}

/////////////////////////////////////////////////////////////////////////

bool GfxMaterialUIText::BeginPass(Context* pTarg, int iPass) {
  pTarg->FXI()->BindPass(iPass);
   
  ///////////////////////////////

  const fmtx4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

  pTarg->FXI()->BindParamMatrix(hTransform, MatMVP);

  ///////////////////////////////

  pTarg->FXI()->BindParamCTex(hColorMap, GetTexture(ETEXDEST_DIFFUSE).mpTexture);
  pTarg->FXI()->BindParamVect4(hModColor, pTarg->RefModColor());
  pTarg->FXI()->CommitParams();
  //pTarg->RSI()->BindRasterState( RasterState );

  return true;
}


void GfxMaterialUIText::UpdateMVPMatrix(Context* context) {
  const fmtx4& MatMVP = context->MTXI()->RefMVPMatrix();
  context->FXI()->BindParamMatrix(hTransform, MatMVP);
  context->FXI()->CommitParams();
}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUITextured::GfxMaterialUITextured(Context* pTarg, const std::string& Technique)
    : mTechniqueName(Technique) {
  miNumPasses = 1;

  if (pTarg) {
    gpuInit(pTarg);
  }
}

void GfxMaterialUITextured::ClassInit() {
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EffectInit(void) {
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::gpuInit(ork::lev2::Context* pTarg) {
  if (hTek == nullptr) {

    auto mtl_load_req = std::make_shared<asset::LoadRequest>();
    mtl_load_req->_asset_path = "orkshader://ui";

    _shaderasset = asset::AssetManager<FxShaderAsset>::load(mtl_load_req);
    _shader      = _shaderasset->GetFxShader();

    hTek = pTarg->FXI()->technique(_shader, mTechniqueName);
    printf("HMODFX<%p> pTarg<%p> hTek<%p>\n", (void*) _shader, (void*) pTarg, (void*) hTek);

    hTransform = pTarg->FXI()->parameter(_shader, "mvp");
    hModColor  = pTarg->FXI()->parameter(_shader, "ModColor");
    hColorMap  = pTarg->FXI()->parameter(_shader, "ColorMap");
  }
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::gpuInit(ork::lev2::Context* pTarg, const std::string& Technique) {
  if (hTek == nullptr) {
    mTechniqueName = Technique;
    gpuInit(pTarg);
  }
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUITextured::BeginBlock(Context* pTarg, const RenderContextInstData& MatCtx) {
  int inumpasses = pTarg->FXI()->BeginBlock(hTek, MatCtx);
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock();
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EndPass(Context* pTarg) {
  pTarg->FXI()->EndPass();
}

////////////////////////////////////////////////////////////2/////////////

bool GfxMaterialUITextured::BeginPass(Context* pTarg, int iPass) {

  const fmtx4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

  auto texture = GetTexture(ETEXDEST_DIFFUSE).mpTexture;
  OrkAssert(texture != nullptr);
  
  pTarg->FXI()->BindPass(iPass);
  pTarg->FXI()->BindParamMatrix(hTransform, MatMVP);
  pTarg->FXI()->BindParamCTex(hColorMap, texture);
  pTarg->FXI()->BindParamVect4(hModColor, pTarg->RefModColor());
  pTarg->FXI()->CommitParams();
  return true;
}

}} // namespace ork::lev2
