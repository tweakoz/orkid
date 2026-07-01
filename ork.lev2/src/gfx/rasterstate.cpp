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
  rval->_blendingMacro = _blendingMacro;
  rval->_lineWidth = _lineWidth;
  rval->_polygonMode = _polygonMode;
  rval->_frontface = _frontface;
  rval->_depthtest = _depthtest;
  rval->_culltest = _culltest;
  return rval;
}

/////////////////////////////////////////////////////////////////////////
void RasterState::setDepthTest(EDepthTest dt){
  if(dt != _depthtest){
    _impl.clear();
  }
  _depthtest = dt;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setCullTest(ECullTest ct){
  if(ct != _culltest){
    _impl.clear();
  }
  _culltest = ct;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setWriteMaskZ(bool b){
  if(b != _writemaskZ){
    _impl.clear();
  }
  _writemaskZ = b;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setWriteMaskA(bool b){
  if(b != _writemaskA){
    _impl.clear();
  }
  _writemaskA = b;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setWriteMaskRGB(bool b){
  if(b != _writemaskRGB){
    _impl.clear();
  }
  _writemaskRGB = b;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setDepthBiasEnable(bool b){
  if(b != _depthBiasEnable){
    _impl.clear();
  }
  _depthBiasEnable = b;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setDepthBiasSlopeFactor(float f){
  if(f != _depthBiasSlopeFactor){
    _impl.clear();
  }
  _depthBiasSlopeFactor = f;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setDepthBiasConstantFactor(float f){
  if(f != _depthBiasConstantFactor){
    _impl.clear();
  }
  _depthBiasConstantFactor = f;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setDepthBiasClamp(float f){
  if(f != _depthBiasClamp){
    _impl.clear();
  }
  _depthBiasClamp = f;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setDepthClampEnable(bool b){
  if(b != _depthClampEnable){
    _impl.clear();
  }
  _depthClampEnable = b;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setRasterizerDiscard(bool b){
  if(b != _rasterizerDiscard){
    _impl.clear();
  }
  _rasterizerDiscard = b;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setAlphaToCoverage(bool b){
  if(b != _alphaToCoverage){
    _impl.clear();
  }
  _alphaToCoverage = b;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendEnable(bool b){
  _updateBlendingTechnique(false);
  if(b != _blendEnable){
    _impl.clear();
  }
  _blendEnable = b;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendConstant(const fvec4& f){
  _updateBlendingTechnique(false);
  // Blend constants are dynamic state - no impl clear needed for value changes
  _blendConstant = f;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendFactorSrcRGB(BlendingFactor bf){
  _updateBlendingTechnique(false);
  if(bf != _blendFactorSrcRGB){
    _impl.clear();
  }
  _blendFactorSrcRGB = bf;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendFactorDstRGB(BlendingFactor bf){
  _updateBlendingTechnique(false);
  if(bf != _blendFactorDstRGB){
    _impl.clear();
  }
  _blendFactorDstRGB = bf;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendFactorSrcA(BlendingFactor bf){
  _updateBlendingTechnique(false);
  if(bf != _blendFactorSrcA){
    _impl.clear();
  }
  _blendFactorSrcA = bf;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendFactorDstA(BlendingFactor bf){
  _updateBlendingTechnique(false);
  if(bf != _blendFactorDstA){
    _impl.clear();
  }
  _blendFactorDstA = bf;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setLineWidth(float f){
  if(f != _lineWidth){
    _impl.clear();
  }
  _lineWidth = f;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setPolygonMode(EPolygonMode pm){
  if(pm != _polygonMode){
    _impl.clear();
  }
  _polygonMode = pm;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setFrontFace(EFrontFace ff){
  if(ff != _frontface){
    _impl.clear();
  }
  _frontface = ff;
}
/////////////////////////////////////////////////////////////////////////
void RasterState::setBlendingMacro(BlendingMacro bm) {
  bool is_macro = (bm != BlendingMacro::NONE);
  _updateBlendingTechnique(is_macro);

  // Clear impl if macro value is changing
  if(_blendingMacro != bm){
    _impl.clear();
  }

  _blendingMacro = bm;
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
      _blendFactorSrcRGB = BlendingFactor::DST_ALPHA;
      _blendFactorDstRGB = BlendingFactor::ONE_MINUS_DST_ALPHA;
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
    case BlendingMacro::DST_MINUS_SRC:
    case BlendingMacro::SUBTRACTIVE: {
      // dst - src
      _blendFactorSrcRGB = BlendingFactor::ONE;
      _blendFactorDstRGB = BlendingFactor::ONE;
      _blendFactorSrcA   = BlendingFactor::ONE;
      _blendFactorDstA   = BlendingFactor::ONE;
      _blendOpRGB        = BlendingOp::REVERSE_SUBTRACT;
      _blendOpA          = BlendingOp::REVERSE_SUBTRACT;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::SRC_MINUS_DST: {
      // src - dst
      _blendFactorSrcRGB = BlendingFactor::ONE;
      _blendFactorDstRGB = BlendingFactor::ONE;
      _blendFactorSrcA   = BlendingFactor::ONE;
      _blendFactorDstA   = BlendingFactor::ONE;
      _blendOpRGB        = BlendingOp::SUBTRACT;
      _blendOpA          = BlendingOp::SUBTRACT;
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
    case BlendingMacro::INVERSE_SUBTRACTIVE: {
      // src * (1-dst)
      // Note: (1-dst) - src is not possible with Vulkan blend equation
      _blendFactorSrcRGB = BlendingFactor::ONE_MINUS_DST_COLOR;
      _blendFactorDstRGB = BlendingFactor::ZERO;
      _blendFactorSrcA   = BlendingFactor::ONE_MINUS_DST_ALPHA;
      _blendFactorDstA   = BlendingFactor::ZERO;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::SCREEN: {
      // src + dst*(1-src) - Photoshop screen blend
      _blendFactorSrcRGB = BlendingFactor::ONE;
      _blendFactorDstRGB = BlendingFactor::ONE_MINUS_SRC_COLOR;
      _blendFactorSrcA   = BlendingFactor::ONE;
      _blendFactorDstA   = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::DARKEN: {
      // min(src, dst)
      _blendFactorSrcRGB = BlendingFactor::ONE;
      _blendFactorDstRGB = BlendingFactor::ONE;
      _blendFactorSrcA   = BlendingFactor::ONE;
      _blendFactorDstA   = BlendingFactor::ONE;
      _blendOpRGB        = BlendingOp::MIN;
      _blendOpA          = BlendingOp::MIN;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::LIGHTEN: {
      // max(src, dst)
      _blendFactorSrcRGB = BlendingFactor::ONE;
      _blendFactorDstRGB = BlendingFactor::ONE;
      _blendFactorSrcA   = BlendingFactor::ONE;
      _blendFactorDstA   = BlendingFactor::ONE;
      _blendOpRGB        = BlendingOp::MAX;
      _blendOpA          = BlendingOp::MAX;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::MULTIPLY:
    case BlendingMacro::MODULATE: {
      // src * dst
      _blendFactorSrcRGB = BlendingFactor::DST_COLOR;
      _blendFactorDstRGB = BlendingFactor::ZERO;
      _blendFactorSrcA   = BlendingFactor::DST_ALPHA;
      _blendFactorDstA   = BlendingFactor::ZERO;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::ALPHA_MODULATE: {
      // dst * srcAlpha
      _blendFactorSrcRGB = BlendingFactor::ZERO;
      _blendFactorDstRGB = BlendingFactor::SRC_ALPHA;
      _blendFactorSrcA   = BlendingFactor::ZERO;
      _blendFactorDstA   = BlendingFactor::SRC_ALPHA;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::UNDER: {
      // src*(1-dstAlpha) + dst*1 - Porter-Duff UNDER
      _blendFactorSrcRGB = BlendingFactor::ONE_MINUS_DST_ALPHA;
      _blendFactorDstRGB = BlendingFactor::ONE;
      _blendFactorSrcA   = BlendingFactor::ONE_MINUS_DST_ALPHA;
      _blendFactorDstA   = BlendingFactor::ONE;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::ATOP: {
      // src*dstAlpha + dst*(1-srcAlpha) - Porter-Duff ATOP
      _blendFactorSrcRGB = BlendingFactor::DST_ALPHA;
      _blendFactorDstRGB = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendFactorSrcA   = BlendingFactor::DST_ALPHA;
      _blendFactorDstA   = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::XOR: {
      // src*(1-dstAlpha) + dst*(1-srcAlpha) - Porter-Duff XOR
      _blendFactorSrcRGB = BlendingFactor::ONE_MINUS_DST_ALPHA;
      _blendFactorDstRGB = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendFactorSrcA   = BlendingFactor::ONE_MINUS_DST_ALPHA;
      _blendFactorDstA   = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::ERASE: {
      // dst*(1-srcAlpha) - Porter-Duff DST_OUT (erase)
      _blendFactorSrcRGB = BlendingFactor::ZERO;
      _blendFactorDstRGB = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendFactorSrcA   = BlendingFactor::ZERO;
      _blendFactorDstA   = BlendingFactor::ONE_MINUS_SRC_ALPHA;
      _blendOpRGB        = BlendingOp::ADD;
      _blendOpA          = BlendingOp::ADD;
      _blendEnable       = true;
      break;
    }
    case BlendingMacro::ALPHA_WEIGHTED: {
      // src*srcAlpha + dst*dstAlpha - symmetrical alpha blend
      _blendFactorSrcRGB = BlendingFactor::SRC_ALPHA;
      _blendFactorDstRGB = BlendingFactor::DST_ALPHA;
      _blendFactorSrcA   = BlendingFactor::SRC_ALPHA;
      _blendFactorDstA   = BlendingFactor::DST_ALPHA;
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
    case BlendingMacro::NONE: {
      // Non-macro mode - blend state set individually via setters
      break;
    }
    default:
      OrkAssert(false);
      break;
  }
}
/////////////////////////////////////////////////////////////////////////
void RasterState::_updateBlendingTechnique(bool is_macro) {
  bool currently_macro = (_blendingMacro != BlendingMacro::NONE);

  if(is_macro != currently_macro) {
    // Mode transition detected - invalidate cached impl
    _impl.clear();
    if(!is_macro) {
      // Transitioning to manual mode
      _blendingMacro = BlendingMacro::NONE;
    }
    // Note: When transitioning to macro mode, caller sets the specific macro value
  }
}
/////////////////////////////////////////////////////////////////////////
void RasterState::invalidate() {
  _impl.clear();
}
/////////////////////////////////////////////////////////////////////////
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
