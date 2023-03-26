////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxmaterial.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

class GfxMaterialUI final : public GfxMaterial {
  RttiDeclareAbstract(GfxMaterialUI, GfxMaterial);
  //////////////////////////////////////////////////////////////////////////////

public:
  GfxMaterialUI(Context* pTarg = 0);
  ~GfxMaterialUI();

  void Update(void) override {
  }

  void gpuInit(Context* context) override;

  bool BeginPass(Context* pTARG, int iPass = 0) override;
  void EndPass(Context* pTARG) override;
  int BeginBlock(Context* pTARG, const RenderContextInstData& MatCtx) override;
  void EndBlock(Context* pTARG) override;

  void SetUIColorMode(UiColorMode emod) {
    meUIColorMode = emod;
  }
  UiColorMode GetUIColorMode(void) {
    return meUIColorMode;
  }

  enum EType { ETYPE_STANDARD = 0, ETYPE_CIRCLE };

  void SetType(EType etyp) {
    meType = etyp;
  }

  //////////////////////////////////////////////////////////////////////////////

protected:
  fxshaderasset_ptr_t _shaderasset;
  FxShader* _shader                       = nullptr;
  const FxShaderTechnique* hTekMod        = nullptr;
  const FxShaderTechnique* hTekVtx        = nullptr;
  const FxShaderTechnique* hTekModVtx     = nullptr;
  const FxShaderTechnique* hTekCircle     = nullptr;
  const FxShaderParam* hVPW               = nullptr;
  const FxShaderParam* hBias              = nullptr;
  const FxShaderParam* hScale             = nullptr;
  const FxShaderParam* hTransform         = nullptr;
  const FxShaderParam* hModColor          = nullptr;
  const FxShaderParam* hColorMap          = nullptr;
  const FxShaderParam* hCircleInnerRadius = nullptr;
  const FxShaderParam* hCircleOuterRadius = nullptr;

  EType meType;
  UiColorMode meUIColorMode;

  fvec4 PosScale;
  fvec4 PosBias;
  fvec4 Color;
};

using uimaterial_ptr_t = std::shared_ptr<GfxMaterialUI>;

///////////////////////////////////////////////////////////////////////////////

class GfxMaterialUIText final : public GfxMaterial {
  //////////////////////////////////////////////////////////////////////////////

public:
  static void ClassInit() {
  }

  GfxMaterialUIText(Context* pTarg = 0);

  void Update(void) override {
  }
  void gpuInit(Context* context) override;

  bool BeginPass(Context* pTARG, int iPass = 0) override;
  void EndPass(Context* pTARG) override;
  int BeginBlock(Context* pTARG, const RenderContextInstData& MatCtx) override;
  void EndBlock(Context* pTARG) override;

  /////////////////////////
  void UpdateMVPMatrix(Context* pTARG) final;
/////////////////////////////////////////////////////

protected:
  fvec4 PosScale;
  fvec4 PosBias;
  fvec4 TexScale;
  fvec4 TexColor;

  fxshaderasset_ptr_t _shaderasset;
  FxShader* _shader;
  const FxShaderTechnique* hTek;
  const FxShaderParam* hVPW;
  const FxShaderParam* hBias;
  const FxShaderParam* hScale;
  const FxShaderParam* hUVScale;
  const FxShaderParam* hTransform;
  const FxShaderParam* hModColor;
  const FxShaderParam* hColorMap;
};

///////////////////////////////////////////////////////////////////////////////

class GfxMaterialUITextured final : public GfxMaterial {
public:
  static void ClassInit();
  GfxMaterialUITextured(Context* pTarg = 0, const std::string& Technique = "uitextured");
  void gpuInit(Context* context) override;
  void gpuInit(Context* context, const std::string& Technique);
  void Update(void) override {
  }
  bool BeginPass(Context* pTARG, int iPass = 0) override;
  void EndPass(Context* pTARG) override;
  int BeginBlock(Context* pTARG, const RenderContextInstData& MatCtx) override;
  void EndBlock(Context* pTARG) override;

  void EffectInit(void);

protected:
  fxshaderasset_ptr_t _shaderasset;

  const FxShaderTechnique* hTek   = nullptr;
  const FxShaderParam* hVPW       = nullptr;
  const FxShaderParam* hBias      = nullptr;
  const FxShaderParam* hScale     = nullptr;
  const FxShaderParam* hTransform = nullptr;
  const FxShaderParam* hModColor  = nullptr;
  const FxShaderParam* hColorMap  = nullptr;

  FxShader* _shader = nullptr;

  std::string mTechniqueName;

  fvec4 Color;
};

uimaterial_ptr_t defaultUIMaterial();

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
