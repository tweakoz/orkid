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

match_results_t UnaryOp::match(FnParseContext ctx) {
  match_results_t rval;
  if (auto m = AddOp::match(ctx))
    rval = m;
  else if (auto m = SubOp::match(ctx))
    rval = m;
  else if (auto m = MulOp::match(ctx))
    rval = m;
  else if (auto m = DivOp::match(ctx))
    rval = m;
  else if (auto m = NotOp::match(ctx))
    rval = m;
  else if (auto m = LtOp::match(ctx))
    rval = m;
  else if (auto m = GtOp::match(ctx))
    rval = m;
  else if (auto m = LtEqOp::match(ctx))
    rval = m;
  else if (auto m = GtEqOp::match(ctx))
    rval = m;
  else if (auto m = BitNotOp::match(ctx))
    rval = m;
  else if (auto m = AndOp::match(ctx))
    rval = m;
  else if (auto m = IncOp::match(ctx))
    rval = m;
  else if (auto m = DecOp::match(ctx))
    rval = m;
  else if (auto m = EqOp::match(ctx))
    rval = m;
  return rval;
}
match_results_t SizeofOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "sizeof") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}

match_results_t DotOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == ".") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}

match_results_t NotOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "!") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t BitNotOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "~") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}

match_results_t IncOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "++") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t DecOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "--") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}

match_results_t AddOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "+") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t SubOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "-") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t MulOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "*") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t DivOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "/") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t ModOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "%") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}

match_results_t LeftOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "<<") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t RightOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == ">>") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}

match_results_t OrOrOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "||") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t OrOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "|") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t XorOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "^") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t LtOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "<") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t LtEqOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "<=") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t GtOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == ">") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t GtEqOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == ">=") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t EqOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "==") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t NeqOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "!=") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t AndAndOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "&&") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}
match_results_t AndOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == "&") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t CommaOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == ",") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}

match_results_t SemicolonOp::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  if (ctx.tokenValue(0) == ";") {
    rval->_start == ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}



/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t InitialAssignmentOperator::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  auto op = ctx.tokenValue(0);
  if (op == "=") {
    rval->_start   = ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}

//InitialAssignmentOperator::parsed_t InitialAssignmentOperator::parse(const match_t& match) {
  //parsed_t rval;
  //assert(false);
  //return rval;
//}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t MutatingAssignmentOperator::match(FnParseContext ctx) {
  match_results_t rval;
  rval.make<match_t>(ctx);
  auto op = ctx.tokenValue(0);
  if (op == "=" or op == "*=" or op == "+=" or op == "-=" or op == "/=" or op == "&=" or op == "|=" or op == "<<=" or op == ">>=" or
      op == "^=") {
    rval->_start   = ctx._startIndex;
    rval->_count   = 1;
    rval->_matched = true;
  }
  return rval;
}

//parsed_t MutatingAssignmentOperator::parse(const match_t& match) {
  //parsed_t rval;
  //assert(false);
  //return rval;
//}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
