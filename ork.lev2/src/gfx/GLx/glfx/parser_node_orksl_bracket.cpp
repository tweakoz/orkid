////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
//  Scanner/Parser
//  this replaces CgFx for OpenGL 3.x and OpenGL ES 2.x
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include "glslfxi_parser.h"
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/pch.h>
#include <ork/util/crc.h>
#include <regex>
#include <stdlib.h>

#if defined(USE_ORKSL_LANG)

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t OpenCurly::match(FnParseContext ctx) {
  match_results_t rval;
  if(ctx.tokenValue(0)=="{"){
    rval = std::make_shared<match_t>(ctx);
    rval->_matched = true;
    rval->_count = 1;
    rval->_start = ctx._startIndex;
  }
  //rval.dump("{");
  return rval;
}

void OpenCurly::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t CloseCurly::match(FnParseContext ctx) {
  match_results_t rval;
  if(ctx.tokenValue(0)=="}"){
    rval = std::make_shared<match_t>(ctx);
    rval->_matched = true;
    rval->_count = 1;
    rval->_start = ctx._startIndex;
  }
  //rval.dump("}");
  return rval;
}

void CloseCurly::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

///////////////////////////////////////////////////////////////////////////////

match_results_t OpenSquare::match(FnParseContext ctx) {
  match_results_t rval;
  if(ctx.tokenValue(0)=="["){
    rval.make<match_t>(ctx);
    rval->_matched = true;
    rval->_count = 1;
    rval->_start = ctx._startIndex;
  }
  //rval.dump("[");
  return rval;
}

void OpenSquare::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t CloseSquare::match(FnParseContext ctx) {
  match_results_t rval;
  if(ctx.tokenValue(0)=="]"){
    rval = std::make_shared<match_t>(ctx);
    rval->_matched = true;
    rval->_count = 1;
    rval->_start = ctx._startIndex;
  }
  //rval.dump("]");
  return rval;
}

void CloseSquare::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

///////////////////////////////////////////////////////////////////////////////

match_results_t OpenParen::match(FnParseContext ctx) {
  match_results_t rval;
  if(ctx.tokenValue(0)=="("){
    rval = std::make_shared<match_t>(ctx);
    rval->_matched = true;
    rval->_count = 1;
    rval->_start = ctx._startIndex;
  }
  //rval.dump("(");
  return rval;
}

void OpenParen::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t CloseParen::match(FnParseContext ctx) {
  match_results_t rval;
  if(ctx.tokenValue(0)==")"){
    rval = std::make_shared<match_t>(ctx);
    rval->_matched = true;
    rval->_count = 1;
    rval->_start = ctx._startIndex;
  }
  //rval.dump(")");
  return rval;
}

void CloseParen::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif