////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

OpenCurly::match_t OpenCurly::match(FnParseContext ctx) {
  match_t rval(ctx);
  if(ctx.tokenValue(0)=="{"){
    rval._matched = true;
    rval._count = 1;
    rval._start = ctx._startIndex;
  }
  return rval;
}

OpenCurly::parsed_t OpenCurly::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void OpenCurly::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

CloseCurly::match_t CloseCurly::match(FnParseContext ctx) {
  match_t rval(ctx);
  if(ctx.tokenValue(0)=="}"){
    rval._matched = true;
    rval._count = 1;
    rval._start = ctx._startIndex;
  }
  return rval;
}

CloseCurly::parsed_t CloseCurly::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void CloseCurly::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

///////////////////////////////////////////////////////////////////////////////

OpenSquare::match_t OpenSquare::match(FnParseContext ctx) {
  match_t rval(ctx);
  if(ctx.tokenValue(0)=="["){
    rval._matched = true;
    rval._count = 1;
    rval._start = ctx._startIndex;
  }
  return rval;
}

OpenSquare::parsed_t OpenSquare::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void OpenSquare::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

CloseSquare::match_t CloseSquare::match(FnParseContext ctx) {
  match_t rval(ctx);
  if(ctx.tokenValue(0)=="]"){
    rval._matched = true;
    rval._count = 1;
    rval._start = ctx._startIndex;
  }
  return rval;
}

CloseSquare::parsed_t CloseSquare::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void CloseSquare::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

///////////////////////////////////////////////////////////////////////////////

OpenParen::match_t OpenParen::match(FnParseContext ctx) {
  match_t rval(ctx);
  if(ctx.tokenValue(0)=="("){
    rval._matched = true;
    rval._count = 1;
    rval._start = ctx._startIndex;
  }
  return rval;
}

OpenParen::parsed_t OpenParen::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void OpenParen::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

CloseParen::match_t CloseParen::match(FnParseContext ctx) {
  match_t rval(ctx);
  if(ctx.tokenValue(0)==")"){
    rval._matched = true;
    rval._count = 1;
    rval._start = ctx._startIndex;
  }
  return rval;
}

CloseParen::parsed_t CloseParen::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
void CloseParen::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
