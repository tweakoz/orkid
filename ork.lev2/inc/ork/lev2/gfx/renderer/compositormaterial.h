////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/dataflow/dataflow.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/renderer/frametek.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

class CompositingMaterial final : public lev2::GfxMaterial {
public:
  CompositingMaterial();
  ~CompositingMaterial();
  /////////////////////////////////////////////////
  void Update(void) {
  }
  void gpuInit(lev2::Context* context) final;
  bool BeginPass(lev2::Context* pTARG, int iPass = 0) final;
  void EndPass(lev2::Context* pTARG) final;
  int BeginBlock(lev2::Context* pTARG, const lev2::RenderContextInstData& MatCtx) final;
  void EndBlock(lev2::Context* pTARG) final;
  /////////////////////////////////////////////////
  void SetTextureA(lev2::Texture* ptex) {
    mCurrentTextureA = ptex;
  }
  void SetTextureB(lev2::Texture* ptex) {
    mCurrentTextureB = ptex;
  }
  void SetTextureC(lev2::Texture* ptex) {
    mCurrentTextureC = ptex;
  }
  void SetLevelA(const fvec4& la) {
    mLevelA = la;
  }
  void SetLevelB(const fvec4& lb) {
    mLevelB = lb;
  }
  void SetLevelC(const fvec4& lc) {
    mLevelC = lc;
  }
  void SetBiasA(const fvec4& ba) {
    mBiasA = ba;
  }
  void SetBiasB(const fvec4& bb) {
    mBiasB = bb;
  }
  void SetBiasC(const fvec4& bc) {
    mBiasC = bc;
  }
  void SetTechnique(const std::string& tek);
  /////////////////////////////////////////////////
  lev2::Texture* mCurrentTextureA;
  lev2::Texture* mCurrentTextureB;
  lev2::Texture* mCurrentTextureC;
  fvec4 mLevelA;
  fvec4 mLevelB;
  fvec4 mLevelC;
  fvec4 mBiasA;
  fvec4 mBiasB;
  fvec4 mBiasC;

  const lev2::FxShaderTechnique* hTekOp2AmulB;
  const lev2::FxShaderTechnique* hTekOp2AdivB;

  const lev2::FxShaderTechnique* hTekBoverAplusC;
  const lev2::FxShaderTechnique* hTekAplusBplusC;
  const lev2::FxShaderTechnique* hTekAlerpBwithC;
  const lev2::FxShaderTechnique* hTekAsolo;
  const lev2::FxShaderTechnique* hTekBsolo;
  const lev2::FxShaderTechnique* hTekCsolo;

  const lev2::FxShaderTechnique* hTekCurrent;

  const lev2::FxShaderParam* hMapA;
  const lev2::FxShaderParam* hMapB;
  const lev2::FxShaderParam* hLevelA;
  const lev2::FxShaderParam* hLevelB;
  const lev2::FxShaderParam* hLevelC;
  const lev2::FxShaderParam* hBiasA;
  const lev2::FxShaderParam* hBiasB;
  const lev2::FxShaderParam* hBiasC;
  const lev2::FxShaderParam* hMapC;
  const lev2::FxShaderParam* hMatMVP;
  lev2::FxShader* _shader;
  fxshaderasset_ptr_t _shaderasset;
};

} // namespace ork::lev2
