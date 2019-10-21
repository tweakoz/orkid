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

UnaryOp::match_t UnaryOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (auto m=AddOp::match(ctx))
    rval = m;
  else if (auto m=SubOp::match(ctx))
    rval = m;
  else if (auto m=MulOp::match(ctx))
    rval = m;
  else if (auto m=NotOp::match(ctx))
    rval = m;
  else if (auto m=BitNotOp::match(ctx))
    rval = m;
  else if (auto m=AndOp::match(ctx))
    rval = m;
  return rval;
}
SizeofOp::match_t SizeofOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "sizeof") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}

NotOp::match_t NotOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "!") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
BitNotOp::match_t BitNotOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "~") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}

IncOp::match_t IncOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "++") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
DecOp::match_t DecOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "--") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}


AddOp::match_t AddOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "+") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
SubOp::match_t SubOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "-") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
MulOp::match_t MulOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "*") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
DivOp::match_t DivOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "/") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
ModOp::match_t ModOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "%") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}

LeftOp::match_t LeftOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "<<") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
RightOp::match_t RightOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == ">>") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}

OrOrOp::match_t OrOrOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "||") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
OrOp::match_t OrOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "|") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
XorOp::match_t XorOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "^") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
LtOp::match_t LtOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "<") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
LtEqOp::match_t LtEqOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "<=") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
GtOp::match_t GtOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == ">") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
GtEqOp::match_t GtEqOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == ">=") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
EqOp::match_t EqOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "==") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
NeqOp::match_t NeqOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "!=") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

AndAndOp::match_t AndAndOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "&&") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}
AndOp::match_t AndOp::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "&") {
    rval._start == ctx._startIndex;
    rval._count   = 1;
    rval._matched = true;
  }
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

AssignmentOperator::match_t AssignmentOperator::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (ctx.tokenValue(0) == "=") {
    auto op = ctx.tokenValue(1);
    if (op == "*=" or op == "+=" or op == "-=" or op == "/=" or op == "&=" or op == "|=" or op == "<<=" or op == ">>=" or
        op == "^=" ) {
      rval._start   = ctx._startIndex;
      rval._count   = 2;
      rval._matched = true;
    }
  }
  return rval;
}

AssignmentOperator::parsed_t AssignmentOperator::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
