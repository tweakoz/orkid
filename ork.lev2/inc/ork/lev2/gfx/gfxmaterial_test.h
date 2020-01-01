////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxmaterial.h>

namespace ork { namespace lev2 {
//
///////////////////////////////////////////////////////////////////////////////

class GfxMaterial3DSolid : public GfxMaterial {
  RttiDeclareConcrete(GfxMaterial3DSolid, GfxMaterial);

public:
  static void ClassInit();
  GfxMaterial3DSolid(Context* pTARG = 0);
  GfxMaterial3DSolid(
      Context* pTARG, const char* puserfx, const char* pusertek, bool allowcompilefailure = false, bool unmanaged = false);
  ~GfxMaterial3DSolid() final{};

  void SetVolumeTexture(Texture* ptex) { mVolumeTexture = ptex; }
  void SetTexture(Texture* ptex) { mCurrentTexture = ptex; }
  void SetTexture2(Texture* ptex) { mCurrentTexture2 = ptex; }
  void SetTexture3(Texture* ptex) { mCurrentTexture3 = ptex; }
  void SetTexture4(Texture* ptex) { mCurrentTexture4 = ptex; }

  void SetUser0(const fvec4& vuser) { mUser0 = vuser; }
  void SetUser1(const fvec4& vuser) { mUser1 = vuser; }
  void SetUser2(const fvec4& vuser) { mUser2 = vuser; }
  void SetUser3(const fvec4& vuser) { mUser3 = vuser; }

  void SetUserFx(const char* puserfx, const char* pusertek) {
    mUserFxName  = puserfx;
    mUserTekName = pusertek;
  }

  bool IsUserFxOk() const;

  ////////////////////////////////////////////

  enum EColorMode {
    EMODE_VERTEXMOD_COLOR = 0,
    EMODE_VERTEX_COLOR,
    EMODE_TEX_COLOR,
    EMODE_TEXMOD_COLOR,
    EMODE_TEXTEXMOD_COLOR,
    EMODE_TEXVERTEX_COLOR,
    EMODE_MOD_COLOR,
    EMODE_INTERNAL_COLOR,
    EMODE_USER,
  };

  ////////////////////////////////////////////

  EColorMode GetColorMode() const { return meColorMode; }
  void SetColorMode(EColorMode emode) { meColorMode = emode; }
  void SetColor(const fvec4& color) { Color = color; }
  const fvec4& GetColor() const { return Color; }
  void SetNoiseAmp(const fvec4& color) { mNoiseAmp = color; }
  void SetNoiseFreq(const fvec4& color) { mNoiseFreq = color; }
  void SetNoiseShift(const fvec4& color) { mNoiseShift = color; }

  ////////////////////////////////////////////

  void SetWRotMatrix(const fmtx3& wrot) { mRotMatrix = wrot; }
  void SetAuxMatrix(const fmtx4& mtx) { mMatAux = mtx; }
  void SetAux2Matrix(const fmtx4& mtx) { mMatAux2 = mtx; }

  bool BeginPass(Context* pTARG, int iPass = 0) final;
  void EndPass(Context* pTARG) final;
  int BeginBlock(Context* pTARG, const RenderContextInstData& MatCtx) final;
  void EndBlock(Context* pTARG) final;
  void Init(Context* pTarg) final;

  bool _enablePick = false;

protected:
  void Update(void) final {}
  void SetMaterialProperty(const char* prop, const char* val) final;

  EColorMode meColorMode;
  fvec4 mNoiseAmp;
  fvec4 mNoiseFreq;
  fvec4 mNoiseShift;
  fvec4 Color;
  fvec4 mUser0;
  fvec4 mUser1;
  fvec4 mUser2;
  fvec4 mUser3;
  FxShader* hModFX = nullptr;
  Texture* mVolumeTexture = nullptr;
  Texture* mCurrentTexture = nullptr;
  Texture* mCurrentTexture2 = nullptr;
  Texture* mCurrentTexture3 = nullptr;
  Texture* mCurrentTexture4 = nullptr;
  std::string mUserFxName;
  std::string mUserTekName;
  fmtx4 mMatAux;
  fmtx4 mMatAux2;
  fmtx3 mRotMatrix;
  bool mUnManaged;
  bool mAllowCompileFailure;

  const FxShaderTechnique* hTekUser = nullptr;
  const FxShaderTechnique* hTekUserStereo = nullptr;
  const FxShaderTechnique* hTekTexColor = nullptr;
  const FxShaderTechnique* hTekTexColorStereo = nullptr;
  const FxShaderTechnique* hTekTexModColor = nullptr;
  const FxShaderTechnique* hTekTexTexModColor = nullptr;
  const FxShaderTechnique* hTekTexVertexColor = nullptr;
  const FxShaderTechnique* hTekVertexColor = nullptr;
  const FxShaderTechnique* hTekVertexModColor = nullptr;
  const FxShaderTechnique* hTekModColor = nullptr;
  const FxShaderTechnique* hTekPick = nullptr;

  const FxShaderParam* hMatM = nullptr;
  const FxShaderParam* hMatV = nullptr;
  const FxShaderParam* hMatP = nullptr;
  const FxShaderParam* hMatMV = nullptr;
  const FxShaderParam* hMatMVP = nullptr;
  const FxShaderParam* hMatMVPL = nullptr;
  const FxShaderParam* hMatMVPR = nullptr;
  const FxShaderParam* hMatMVPC = nullptr;
  const FxShaderParam* hMatAux = nullptr;
  const FxShaderParam* hMatAux2 = nullptr;
  const FxShaderParam* hMatRot = nullptr;
  const FxShaderParam* hVolumeMap = nullptr;
  const FxShaderParam* hColorMap = nullptr;
  const FxShaderParam* hColorMap2 = nullptr;
  const FxShaderParam* hColorMap3 = nullptr;
  const FxShaderParam* hColorMap4 = nullptr;
  const FxShaderParam* hParamUser0 = nullptr;
  const FxShaderParam* hParamUser1 = nullptr;
  const FxShaderParam* hParamUser2 = nullptr;
  const FxShaderParam* hParamUser3 = nullptr;
  const FxShaderParam* hParamModColor = nullptr;
  const FxShaderParam* hParamTime = nullptr;
  const FxShaderParam* hParamNoiseShift = nullptr;
  const FxShaderParam* hParamNoiseFreq = nullptr;
  const FxShaderParam* hParamNoiseAmp = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
