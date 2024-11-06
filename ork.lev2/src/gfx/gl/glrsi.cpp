////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"

#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>

// #include <UI/UI.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

GlRasterStateInterface::GlRasterStateInterface(Context& context)
    : _context(context) { //
}

void GlRasterStateInterface::beginFrame(){
  _currentState = RasterState();
  _currentState._force = true;
  
}

void GlRasterStateInterface::apply(const RasterState& newstate) {

  bool force = _currentState._force;

  bool do_write_z = force or (newstate._writemaskZ != _currentState._writemaskZ);
  bool do_write_a = force or (newstate._writemaskA != _currentState._writemaskA);
  bool do_write_rgb = force or (newstate._writemaskRGB != _currentState._writemaskRGB);

  /////////////////////////////////
  // RGBAZ writemasks
  /////////////////////////////////

  GL_ERRORCHECK();
  if (do_write_z) {
    GLenum zmask = newstate._writemaskZ ? GL_TRUE : GL_FALSE;
    glDepthMask(zmask);
    _currentState._writemaskZ = newstate._writemaskZ;
  }
  GL_ERRORCHECK();
  if (do_write_rgb or do_write_a ) {
    GLenum rgbmask = newstate._writemaskRGB ? GL_TRUE : GL_FALSE;
    GLenum amask = newstate._writemaskA ? GL_TRUE : GL_FALSE;
    glColorMask(rgbmask, rgbmask, rgbmask, amask);
    _currentState._writemaskRGB = newstate._writemaskRGB;
    _currentState._writemaskA = newstate._writemaskA;
  }

  /////////////////////////////////
  // culling
  /////////////////////////////////

  bool do_culltest = force or (newstate._culltest != _currentState._culltest);
       do_culltest |= (newstate._frontface != _currentState._frontface);

  GL_ERRORCHECK();
  if (do_culltest) {
    switch (newstate._frontface) {
				case EFrontFace::COUNTER_CLOCKWISE:
					glFrontFace(GL_CCW);
					break;
				case EFrontFace::CLOCKWISE:
					glFrontFace(GL_CW);
					break;
  		default:
	  		break;
			}
    switch (newstate._culltest) {
      case ECullTest::PASS_FRONT:
			case ECullTest::PASS_BACK:
				glEnable(GL_CULL_FACE);
				break;
			case ECullTest::OFF:
				glDisable(GL_CULL_FACE);
				break;
  		default:
	  		break;
		}

    _currentState._culltest = newstate._culltest;
    _currentState._frontface = newstate._frontface;
  }

  /////////////////////////////////
	// depth test
  /////////////////////////////////

  bool do_depthtest = force or (newstate._depthtest != _currentState._depthtest);

	GL_ERRORCHECK();

	if (do_depthtest) {
		switch (newstate._depthtest) {
			case EDepthTest::OFF:
				glDisable(GL_DEPTH_TEST);
				break;
			case EDepthTest::LESS:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LESS);
				break;
			case EDepthTest::LEQUALS:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LEQUAL);
				break;
			case EDepthTest::GREATER:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_GREATER);
				break;
			case EDepthTest::GEQUALS:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_GEQUAL);
				break;
			case EDepthTest::EQUALS:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_EQUAL);
				break;
			case EDepthTest::ALWAYS:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_ALWAYS);
				break;
		}
		_currentState._depthtest = newstate._depthtest;
	}

  GL_ERRORCHECK();
  if (force or (newstate._polygonMode != _currentState._polygonMode)) {
    switch (newstate._polygonMode) {
      case EPolygonMode::FILL:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        break;
      case EPolygonMode::LINE:
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        break;
      case EPolygonMode::POINT:
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        break;
    }
    _currentState._polygonMode = newstate._polygonMode;
  }

  if (force or (newstate._depthBiasEnable != _currentState._depthBiasEnable)) {
    if (newstate._depthBiasEnable) {
      glEnable(GL_POLYGON_OFFSET_FILL);
    } else {
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
    _currentState._depthBiasEnable = newstate._depthBiasEnable;
  }
  GL_ERRORCHECK();
  if (force or (newstate._depthBiasSlopeFactor != _currentState._depthBiasSlopeFactor ||
      newstate._depthBiasConstantFactor != _currentState._depthBiasConstantFactor ||
      newstate._depthBiasClamp != _currentState._depthBiasClamp)) {
    glPolygonOffset(newstate._depthBiasSlopeFactor, newstate._depthBiasConstantFactor);
    _currentState._depthBiasSlopeFactor    = newstate._depthBiasSlopeFactor;
    _currentState._depthBiasConstantFactor = newstate._depthBiasConstantFactor;
    _currentState._depthBiasClamp          = newstate._depthBiasClamp;
  }
  GL_ERRORCHECK();
  if (force or newstate._depthClampEnable != _currentState._depthClampEnable) {
    if (newstate._depthClampEnable) {
      glEnable(GL_DEPTH_CLAMP);
    } else {
      glDisable(GL_DEPTH_CLAMP);
    }
    _currentState._depthClampEnable = newstate._depthClampEnable;
  }
  GL_ERRORCHECK();
  if (force or newstate._rasterizerDiscard != _currentState._rasterizerDiscard) {
    if (newstate._rasterizerDiscard) {
      glEnable(GL_RASTERIZER_DISCARD);
    } else {
      glDisable(GL_RASTERIZER_DISCARD);
    }
    _currentState._rasterizerDiscard = newstate._rasterizerDiscard;
  }
  GL_ERRORCHECK();
  if (force or newstate._blendEnable != _currentState._blendEnable) {
    if (newstate._blendEnable) {
      glEnable(GL_BLEND);
    } else {
      glDisable(GL_BLEND);
    }
    _currentState._blendEnable = newstate._blendEnable;
  }

  bool do_blend = force or (newstate._blendEnable != _currentState._blendEnable);
  do_blend |= (newstate._blendFactorSrcRGB != _currentState._blendFactorSrcRGB);
  do_blend |= (newstate._blendFactorDstRGB != _currentState._blendFactorDstRGB);
  do_blend |= (newstate._blendFactorSrcA != _currentState._blendFactorSrcA);
  do_blend |= (newstate._blendFactorDstA != _currentState._blendFactorDstA);
  do_blend |= (newstate._blendOpRGB != _currentState._blendOpRGB);
  do_blend |= (newstate._blendOpA != _currentState._blendOpA);
  do_blend |= (newstate._blendConstant != _currentState._blendConstant);

  GL_ERRORCHECK();
  if( do_blend) {
    GLenum srcrgb, dstrgb, srca, dsta;
    switch (newstate._blendFactorSrcRGB) {
      case BlendingFactor::ZERO:
        srcrgb = GL_ZERO;
        break;
      case BlendingFactor::ONE:
        srcrgb = GL_ONE;
        break;
      case BlendingFactor::SRC_COLOR:
        srcrgb = GL_SRC_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_SRC_COLOR:
        srcrgb = GL_ONE_MINUS_SRC_COLOR;
        break;
      case BlendingFactor::DST_COLOR:
        srcrgb = GL_DST_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_DST_COLOR:
        srcrgb = GL_ONE_MINUS_DST_COLOR;
        break;
      case BlendingFactor::SRC_ALPHA:
        srcrgb = GL_SRC_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_SRC_ALPHA:
        srcrgb = GL_ONE_MINUS_SRC_ALPHA;
        break;
      case BlendingFactor::DST_ALPHA:
        srcrgb = GL_DST_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_DST_ALPHA:
        srcrgb = GL_ONE_MINUS_DST_ALPHA;
        break;
      case BlendingFactor::CONSTANT_COLOR:
        srcrgb = GL_CONSTANT_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_CONSTANT_COLOR:
        srcrgb = GL_ONE_MINUS_CONSTANT_COLOR;
        break;
      case BlendingFactor::CONSTANT_ALPHA:
        srcrgb = GL_CONSTANT_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_CONSTANT_ALPHA:
        srcrgb = GL_ONE_MINUS_CONSTANT_ALPHA;
        break;
      case BlendingFactor::SRC_ALPHA_SATURATE:
        srcrgb = GL_SRC_ALPHA_SATURATE;
        break;
    }
    switch (newstate._blendFactorDstRGB) {
      case BlendingFactor::ZERO:
        dstrgb = GL_ZERO;
        break;
      case BlendingFactor::ONE:
        dstrgb = GL_ONE;
        break;
      case BlendingFactor::SRC_COLOR:
        dstrgb = GL_SRC_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_SRC_COLOR:
        dstrgb = GL_ONE_MINUS_SRC_COLOR;
        break;
      case BlendingFactor::DST_COLOR:
        dstrgb = GL_DST_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_DST_COLOR:
        dstrgb = GL_ONE_MINUS_DST_COLOR;
        break;
      case BlendingFactor::SRC_ALPHA:
        dstrgb = GL_SRC_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_SRC_ALPHA:
        dstrgb = GL_ONE_MINUS_SRC_ALPHA;
        break;
      case BlendingFactor::DST_ALPHA:
        dstrgb = GL_DST_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_DST_ALPHA:
        dstrgb = GL_ONE_MINUS_DST_ALPHA;
        break;
      case BlendingFactor::CONSTANT_COLOR:
        dstrgb = GL_CONSTANT_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_CONSTANT_COLOR:
        dstrgb = GL_ONE_MINUS_CONSTANT_COLOR;
        break;
      case BlendingFactor::CONSTANT_ALPHA:
        dstrgb = GL_CONSTANT_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_CONSTANT_ALPHA:
        dstrgb = GL_ONE_MINUS_CONSTANT_ALPHA;
        break;
      case BlendingFactor::SRC_ALPHA_SATURATE:
        dstrgb = GL_SRC_ALPHA_SATURATE;
        break;
    }
    switch (newstate._blendFactorSrcA) {
      case BlendingFactor::ZERO:
        srca = GL_ZERO;
        break;
      case BlendingFactor::ONE:
        srca = GL_ONE;
        break;
      case BlendingFactor::SRC_COLOR:
        srca = GL_SRC_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_SRC_COLOR:
        srca = GL_ONE_MINUS_SRC_COLOR;
        break;
      case BlendingFactor::DST_COLOR:
        srca = GL_DST_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_DST_COLOR:
        srca = GL_ONE_MINUS_DST_COLOR;
        break;
      case BlendingFactor::SRC_ALPHA:
        srca = GL_SRC_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_SRC_ALPHA:
        srca = GL_ONE_MINUS_SRC_ALPHA;
        break;
      case BlendingFactor::DST_ALPHA:
        srca = GL_DST_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_DST_ALPHA:
        srca = GL_ONE_MINUS_DST_ALPHA;
        break;
      case BlendingFactor::CONSTANT_COLOR:
        srca = GL_CONSTANT_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_CONSTANT_COLOR:
        srca = GL_ONE_MINUS_CONSTANT_COLOR;
        break;
      case BlendingFactor::CONSTANT_ALPHA:
        srca = GL_CONSTANT_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_CONSTANT_ALPHA:
        srca = GL_ONE_MINUS_CONSTANT_ALPHA;
        break;
      case BlendingFactor::SRC_ALPHA_SATURATE:
        srca = GL_SRC_ALPHA_SATURATE;
        break;
    }
    switch (newstate._blendFactorDstA) {
      case BlendingFactor::ZERO:
        dsta = GL_ZERO;
        break;
      case BlendingFactor::ONE:
        dsta = GL_ONE;
        break;
      case BlendingFactor::SRC_COLOR:
        dsta = GL_SRC_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_SRC_COLOR:
        dsta = GL_ONE_MINUS_SRC_COLOR;
        break;
      case BlendingFactor::DST_COLOR:
        dsta = GL_DST_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_DST_COLOR:
        dsta = GL_ONE_MINUS_DST_COLOR;
        break;
      case BlendingFactor::SRC_ALPHA:
        dsta = GL_SRC_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_SRC_ALPHA:
        dsta = GL_ONE_MINUS_SRC_ALPHA;
        break;
      case BlendingFactor::DST_ALPHA:
        dsta = GL_DST_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_DST_ALPHA:
        dsta = GL_ONE_MINUS_DST_ALPHA;
        break;
      case BlendingFactor::CONSTANT_COLOR:
        dsta = GL_CONSTANT_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_CONSTANT_COLOR:
        dsta = GL_ONE_MINUS_CONSTANT_COLOR;
        break;
      case BlendingFactor::CONSTANT_ALPHA:
        dsta = GL_CONSTANT_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_CONSTANT_ALPHA:
        dsta = GL_ONE_MINUS_CONSTANT_ALPHA;
        break;
      case BlendingFactor::SRC_ALPHA_SATURATE:
        dsta = GL_SRC_ALPHA_SATURATE;
        break;
    }

    glBlendFuncSeparate(srcrgb, dstrgb, srca, dsta);

    switch (newstate._blendOpRGB) {
      case BlendingOp::ADD:
        OrkAssert(newstate._blendOpA==BlendingOp::ADD);
        glBlendEquation(GL_FUNC_ADD);
        break;
      case BlendingOp::SUBTRACT:
        OrkAssert(newstate._blendOpA==BlendingOp::SUBTRACT);
        glBlendEquation(GL_FUNC_SUBTRACT);
        break;
      case BlendingOp::REVSUBTRACT:
        OrkAssert(newstate._blendOpA==BlendingOp::REVSUBTRACT);
        glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        break;
      case BlendingOp::MIN:
        OrkAssert(newstate._blendOpA==BlendingOp::MIN);
        glBlendEquation(GL_MIN);
        break;
      case BlendingOp::MAX:
        OrkAssert(newstate._blendOpA==BlendingOp::MAX);
        glBlendEquation(GL_MAX);
        break;
    }

    _currentState._blendFactorSrcRGB = newstate._blendFactorSrcRGB;
    _currentState._blendFactorDstRGB = newstate._blendFactorDstRGB;
    _currentState._blendFactorSrcA   = newstate._blendFactorSrcA;
    _currentState._blendFactorDstA   = newstate._blendFactorDstA;
  }
  GL_ERRORCHECK();
  if (force or newstate._lineWidth != _currentState._lineWidth) {
    glLineWidth(newstate._lineWidth);
    _currentState._lineWidth = newstate._lineWidth;
  }
  GL_ERRORCHECK();

  _currentState._force = false;
}

} // namespace ork::lev2
