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

RasterState::RasterState() {
  _frontface = EFrontFace::COUNTER_CLOCKWISE;
}

/////////////////////////////////////////////////////////////////////////

rasterstate_ptr_t RasterState::clone() const{
  auto rval = std::make_shared<RasterState>();
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
void RasterState::setDepthTest(EDepthTest dt){
  _depthtest = dt;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setCullTest(ECullTest ct){
  _culltest = ct;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setWriteMaskZ(bool b){
  _writemaskZ = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setWriteMaskA(bool b){
  _writemaskA = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setWriteMaskRGB(bool b){
  _writemaskRGB = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setDepthBiasEnable(bool b){
  _depthBiasEnable = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setDepthBiasSlopeFactor(float f){
  _depthBiasSlopeFactor = f;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setDepthBiasConstantFactor(float f){
  _depthBiasConstantFactor = f;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setDepthBiasClamp(float f){
  _depthBiasClamp = f;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setDepthClampEnable(bool b){
  _depthClampEnable = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setRasterizerDiscard(bool b){
  _rasterizerDiscard = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendEnable(bool b){
  _blendEnable = b;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendConstant(const fvec4& f){
  _blendConstant = f;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendFactorSrcRGB(BlendingFactor bf){
  _blendFactorSrcRGB = bf;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendFactorDstRGB(BlendingFactor bf){
  _blendFactorDstRGB = bf;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendFactorSrcA(BlendingFactor bf){
  _blendFactorSrcA = bf;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendFactorDstA(BlendingFactor bf){
  _blendFactorDstA = bf;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setLineWidth(float f){
  _lineWidth = f;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setPolygonMode(EPolygonMode pm){
  _polygonMode = pm;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setFrontFace(EFrontFace ff){
  _frontface = ff;
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendingMacro(BlendingMacro bm) {
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
    default:
      OrkAssert(false);
      break;
  }
  _impl.clear();
}

void RasterState::dump() const {
  printf("RASTERSTATE<%p:%s>\n", this, _name.c_str());
  printf("  LINEWIDTH<%f>\n", _lineWidth);
  printf("  POLYMODE<0x%08x>\n", uint32_t(_polygonMode));
  printf("  CULLTEST<0x%08x>\n", uint32_t(_culltest));
  printf("  FRONTFACE<0x%08x>\n", uint32_t(_frontface));
  printf("  DEPTHTEST<0x%08x>\n", uint32_t(_depthtest));
  printf("   WRITEMASKZ<%d>\n", int(_writemaskZ));
  printf("   WRITEMASKRGB<%d>\n", int(_writemaskRGB));
  printf("   WRITEMASKA<%d>\n", int(_writemaskA));
  printf("  DEPTHCLAMPENABLE<%d>\n", int(_depthClampEnable));
  printf("  DEPTHBIASENABLE<%d>\n", int(_depthBiasEnable));
  printf("  DEPTHBIASSLOPEFACTOR<%f>\n", _depthBiasSlopeFactor);
  printf("  DEPTHBIASCONSTANTFACTOR<%f>\n", _depthBiasConstantFactor);
  printf("  DEPTHBIASCLAMP<%f>\n", _depthBiasClamp);
  printf("    RASTERIZERDISCARD<%d>\n", int(_rasterizerDiscard));
  printf("    BLENDENABLE<%d>\n", int(_blendEnable));
  if (_blendEnable) {
    printf("      BLENDCONSTANT<%f %f %f %f>\n", _blendConstant.x, _blendConstant.y, _blendConstant.z, _blendConstant.w);
    printf("      BLENDFACTORSRCRGB<0x%08x>\n", uint32_t(_blendFactorSrcRGB));
    printf("      BLENDFACTORDSTRGB<0x%08x>\n", uint32_t(_blendFactorDstRGB));
    printf("      BLENDFACTORSRCA<0x%08x>\n", uint32_t(_blendFactorSrcA));
    printf("      BLENDFACTORDSTA<0x%08x>\n", uint32_t(_blendFactorDstA));
    printf("      BLENDOPRGB<0x%08x>\n", uint32_t(_blendOpRGB));
    printf("      BLENDOPA<0x%08x>\n", uint32_t(_blendOpA));
  }
}

} //namespace ork::lev2 {
