////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/compositormaterial.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/enum_serializer.inl>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
CompositingMaterial::CompositingMaterial()
    : mBiasA(0.0f, 0.0f, 0.0f, 0.0f)
    , mBiasB(0.0f, 0.0f, 0.0f, 0.0f)
    , mBiasC(0.0f, 0.0f, 0.0f, 0.0f)
    , mLevelA(1.0f, 1.0f, 1.0f, 1.0f)
    , mLevelB(1.0f, 1.0f, 1.0f, 1.0f)
    , mLevelC(1.0f, 1.0f, 1.0f, 1.0f) {
}
/////////////////////////////////////////////////
CompositingMaterial::~CompositingMaterial() {
}
/////////////////////////////////////////////////
void CompositingMaterial::gpuInit(lev2::Context* pTarg) {
  if (0 == _shader) {
    auto mtl_load_req = std::make_shared<asset::LoadRequest>("orkshader://compositor");
    _shaderasset = asset::AssetManager<lev2::FxShaderAsset>::load(mtl_load_req);
    _shader      = _shaderasset->GetFxShader();

    auto fxi = pTarg->FXI();

    hMatMVP = fxi->parameter(_shader, "MatMVP");
    hBiasA  = fxi->parameter(_shader, "BiasA");
    hBiasB  = fxi->parameter(_shader, "BiasB");
    hBiasC  = fxi->parameter(_shader, "BiasC");

    hLevelA = fxi->parameter(_shader, "LevelA");
    hLevelB = fxi->parameter(_shader, "LevelB");
    hLevelC = fxi->parameter(_shader, "LevelC");

    hMapA = fxi->parameter(_shader, "MapA");
    hMapB = fxi->parameter(_shader, "MapB");
    hMapC = fxi->parameter(_shader, "MapC");

    hTekOp2AmulB = fxi->technique(_shader, "Op2AmulB");
    hTekOp2AdivB = fxi->technique(_shader, "Op2AdivB");

    hTekBoverAplusC = fxi->technique(_shader, "BoverAplusC");
    hTekAplusBplusC = fxi->technique(_shader, "AplusBplusC");
    hTekAlerpBwithC = fxi->technique(_shader, "AlerpBwithC");

    hTekAsolo = fxi->technique(_shader, "Asolo");
    hTekBsolo = fxi->technique(_shader, "Bsolo");
    hTekCsolo = fxi->technique(_shader, "Csolo");

    //_rasterstate->SetCullTest(ork::lev2::ECullTest::OFF);
    //_rasterstate->SetAlphaTest(ork::lev2::EALPHATEST_OFF);
    //_rasterstate->SetDepthTest(ork::lev2::EDepthTest::OFF);
  }
}
/////////////////////////////////////////////////
void CompositingMaterial::SetTechnique(const std::string& tek) {
  if (tek == "Op2AmulB")
    hTekCurrent = hTekOp2AmulB;
  if (tek == "Op2AdivB")
    hTekCurrent = hTekOp2AdivB;

  if (tek == "BoverAplusC")
    hTekCurrent = hTekBoverAplusC;
  if (tek == "AplusBplusC")
    hTekCurrent = hTekAplusBplusC;
  if (tek == "AlerpBwithC")
    hTekCurrent = hTekAlerpBwithC;

  if (tek == "Asolo")
    hTekCurrent = hTekAsolo;
  if (tek == "Bsolo")
    hTekCurrent = hTekBsolo;
  if (tek == "Csolo")
    hTekCurrent = hTekCsolo;
}
/////////////////////////////////////////////////
bool CompositingMaterial::BeginPass(lev2::Context* pTarg, int iPass) {
  // printf("CompositorMtl draw\n");

  //pTarg->RSI()->BindRasterState(_rasterstate);
  pTarg->FXI()->BindPass(iPass);
  pTarg->FXI()->BindParamMatrix(hMatMVP, pTarg->MTXI()->RefMVPMatrix());

  pTarg->FXI()->BindParamVect4(hLevelA, mLevelA);
  pTarg->FXI()->BindParamVect4(hLevelB, mLevelB);
  pTarg->FXI()->BindParamVect4(hLevelC, mLevelC);

  pTarg->FXI()->BindParamVect4(hBiasA, mBiasA);
  pTarg->FXI()->BindParamVect4(hBiasB, mBiasB);
  pTarg->FXI()->BindParamVect4(hBiasC, mBiasC);

  if (mCurrentTextureA && hMapA) {
    pTarg->FXI()->BindParamCTex(hMapA, mCurrentTextureA);
  }
  if (mCurrentTextureB && hMapB) {
    pTarg->FXI()->BindParamCTex(hMapB, mCurrentTextureB);
  }
  if (mCurrentTextureC && hMapC) {
    pTarg->FXI()->BindParamCTex(hMapC, mCurrentTextureC);
  }

  pTarg->FXI()->CommitParams();
  return true;
}
/////////////////////////////////////////////////
void CompositingMaterial::EndPass(lev2::Context* pTarg) {
  pTarg->FXI()->EndPass();
}
/////////////////////////////////////////////////
int CompositingMaterial::BeginBlock(lev2::Context* pTarg, const lev2::RenderContextInstData& MatCtx) {
  int inumpasses = pTarg->FXI()->BeginBlock(hTekCurrent, MatCtx);
  return inumpasses;
}
/////////////////////////////////////////////////
void CompositingMaterial::EndBlock(lev2::Context* pTarg) {
  pTarg->FXI()->EndBlock();
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
