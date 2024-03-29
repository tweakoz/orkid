////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>

namespace ork::lev2 {

/////////////////////////////////////////////////////////////////////////

SRasterState::SRasterState(){
  _writemaskZ = true;
  _writemaskA = true;
  _writemaskRGB = true;
  _depthBiasEnable = false;
  _depthClampEnable = false;
  _rasterizerDiscard = false;
  _blendEnable = false;
}

/////////////////////////////////////////////////////////////////////////

rasterstate_ptr_t SRasterState::clone() const{
  auto rval = std::make_shared<SRasterState>();
  rval->_writemaskZ = _writemaskZ;
  rval->_writemaskA = _writemaskA;
  rval->_writemaskRGB = _writemaskRGB;
  rval->_depthBiasEnable = _depthBiasEnable;
  rval->_depthBiasSlopeFactor = _depthBiasSlopeFactor;
  rval->_depthBiasConstantFactor = _depthBiasConstantFactor;
  rval->_depthBiasClamp = _depthBiasClamp;
  rval->_depthClampEnable = _depthClampEnable;
  rval->_rasterizerDiscard = _rasterizerDiscard;
  rval->_blendEnable = _blendEnable;
  rval->_blendOpRGB = _blendOpRGB;
  rval->_blendOpA = _blendOpA;
  rval->_blendConstant = _blendConstant;
  rval->_blendFactorSrcRGB = _blendFactorSrcRGB;
  rval->_blendFactorDstRGB = _blendFactorDstRGB;
  rval->_blendFactorSrcA = _blendFactorSrcA;
  rval->_blendFactorDstA = _blendFactorDstA;
  rval->_lineWidth = _lineWidth;
  rval->_polygonMode = _polygonMode;
  rval->_frontface = _frontface;
  rval->_depthtest = _depthtest;
  rval->_culltest = _culltest;
  return rval;
}

/////////////////////////////////////////////////////////////////////////
void SRasterState::setDepthTest(EDepthTest dt){
  _depthtest = dt;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setCullTest(ECullTest ct){
  _culltest = ct;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setWriteMaskZ(bool b){
  _writemaskZ = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setWriteMaskA(bool b){
  _writemaskA = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setWriteMaskRGB(bool b){
  _writemaskRGB = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setDepthBiasEnable(bool b){
  _depthBiasEnable = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setDepthBiasSlopeFactor(float f){
  _depthBiasSlopeFactor = f;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setDepthBiasConstantFactor(float f){
  _depthBiasConstantFactor = f;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setDepthBiasClamp(float f){
  _depthBiasClamp = f;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setDepthClampEnable(bool b){
  _depthClampEnable = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setRasterizerDiscard(bool b){
  _rasterizerDiscard = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setBlendEnable(bool b){
  _blendEnable = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setBlendConstant(const fvec4& f){
  _blendConstant = f;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setBlendFactorSrcRGB(BlendingFactor bf){
  _blendFactorSrcRGB = bf;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setBlendFactorDstRGB(BlendingFactor bf){
  _blendFactorDstRGB = bf;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setBlendFactorSrcA(BlendingFactor bf){
  _blendFactorSrcA = bf;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setBlendFactorDstA(BlendingFactor bf){
  _blendFactorDstA = bf;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setLineWidth(float f){
  _lineWidth = f;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setPolygonMode(EPolygonMode pm){
  _polygonMode = pm;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setFrontFace(EFrontFace ff){
  _frontface = ff;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void SRasterState::setBlendingMacro(BlendingMacro bm) {
  switch (bm) {
    case BlendingMacro::OFF: {
      _blendFactorSrcRGB = BlendingFactor::ONE;
      _blendFactorDstRGB = BlendingFactor::ZERO;
      _blendFactorSrcA   = BlendingFactor::ONE;
      _blendFactorDstA   = BlendingFactor::ZERO;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = false;
      break;
    }
    case BlendingMacro::ALPHA: {
      _blendFactorSrcRGB = BlendingFactor::SRC_ALPHA;
      _blendFactorDstRGB = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendFactorSrcA   = BlendingFactor::ONE;
      _blendFactorDstA   = BlendingFactor::ZERO;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::DSTALPHA: {
      _blendFactorSrcRGB = BlendingFactor::ONE;
      _blendFactorDstRGB = BlendingFactor::ZERO;
      _blendFactorSrcA   = BlendingFactor::DST_ALPHA;
      _blendFactorDstA   = BlendingFactor::ONE_MINUS_DST_ALPHA;
      _blendEnable       = true;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      break;
    }
    case BlendingMacro::ADDITIVE: {
      _blendFactorSrcRGB = BlendingFactor::ONE;
      _blendFactorDstRGB = BlendingFactor::ONE;
      _blendFactorSrcA   = BlendingFactor::ONE;
      _blendFactorDstA   = BlendingFactor::ONE;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::ALPHA_ADDITIVE: {
      _blendFactorSrcRGB = BlendingFactor::SRC_ALPHA;
      _blendFactorDstRGB = BlendingFactor::ONE;
      _blendFactorSrcA   = BlendingFactor::ONE;
      _blendFactorDstA   = BlendingFactor::ONE;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::SUBTRACTIVE: {
      _blendFactorSrcRGB = BlendingFactor::ZERO;
      _blendFactorDstRGB = BlendingFactor::ONE_MINUS_SRC_COLOR;
      _blendFactorSrcA   = BlendingFactor::ZERO;
      _blendFactorDstA   = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::ALPHA_SUBTRACTIVE: {
      _blendFactorSrcRGB = BlendingFactor::ZERO;
      _blendFactorDstRGB = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendFactorSrcA   = BlendingFactor::ZERO;
      _blendFactorDstA   = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::MODULATE: {
      _blendFactorSrcRGB = BlendingFactor::ZERO;
      _blendFactorDstRGB = BlendingFactor::SRC_COLOR;
      _blendFactorSrcA   = BlendingFactor::ZERO;
      _blendFactorDstA   = BlendingFactor::SRC_ALPHA;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::PREMA: {
      _blendFactorSrcRGB = BlendingFactor::ONE;
      _blendFactorDstRGB = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendFactorSrcA   = BlendingFactor::ONE;
      _blendFactorDstA   = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
  }
  _impl.clear();
}
} //namespace ork::lev2 {
