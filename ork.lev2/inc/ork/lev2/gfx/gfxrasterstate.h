////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct SRasterState {

  SRasterState();

  void setDepthTest(EDepthTest dt);
  void setCullTest(ECullTest dt);
  void setWriteMaskZ(bool b);
  void setWriteMaskA(bool b);
  void setWriteMaskRGB(bool b);
  void setDepthBiasEnable(bool b);
  void setDepthBiasSlopeFactor(float f);
  void setDepthBiasConstantFactor(float f);
  void setDepthBiasClamp(float f);
  void setDepthClampEnable(bool b);
  void setRasterizerDiscard(bool b);
  void setBlendEnable(bool b);
  void setBlendConstant(const fvec4& f);
  void setBlendFactorSrcRGB(BlendingFactor bf);
  void setBlendFactorDstRGB(BlendingFactor bf);
  void setBlendFactorSrcA(BlendingFactor bf);
  void setBlendFactorDstA(BlendingFactor bf);
  void setLineWidth(float f);
  void setPolygonMode(EPolygonMode pm);
  void setFrontFace(EFrontFace ff);
  void setBlendingMacro(BlendingMacro bm);

  // Render States
  
  bool _writemaskZ : 1;   
  bool _writemaskA : 1;   
  bool _writemaskRGB : 1;   
  bool _depthBiasEnable : 1;
  bool _depthClampEnable : 1;
  bool _rasterizerDiscard : 1;   
  bool _blendEnable : 1;

  float _depthBiasSlopeFactor = 0.0f;
  float _depthBiasConstantFactor = 0.0f;
  float _depthBiasClamp = 0.0f;
  float _lineWidth = 1.0f;

  /////////////////////////////

  EPolygonMode _polygonMode = EPolygonMode::FILL;
  ECullTest _culltest = ECullTest::OFF;         
  EFrontFace _frontface = EFrontFace::CLOCKWISE;

  EDepthTest _depthtest  = EDepthTest::LEQUALS; 
  fvec4 _blendConstant = fvec4(0,0,0,0);
  BlendingFactor _blendFactorSrcRGB = BlendingFactor::ONE;
  BlendingFactor _blendFactorDstRGB = BlendingFactor::ZERO;
  BlendingFactor _blendFactorSrcA = BlendingFactor::ONE;
  BlendingFactor _blendFactorDstA = BlendingFactor::ZERO;

  // todo: logic ops

  /////////////////////////////

  svar16_t _impl;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
