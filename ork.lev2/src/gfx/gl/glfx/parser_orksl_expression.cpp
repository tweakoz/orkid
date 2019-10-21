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

CastExpression::match_t CastExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  if( auto m = UnaryExpression::match(ctx)){
    rval = m;
  }
  else if( auto m = OpenParen::match(ctx)){
    auto c2 = m.consume();
    if( auto m2 = TypeName::match(c2)){
      auto c3 = (m+m2).consume();
      if( auto m3 = CloseParen::match( c3 )) {
        rval = (m+m2+m3).consume();
      }
    }
  }
  assert(false);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

Expression::match_t Expression::match(FnParseContext ctx) {
  match_t rval(ctx);
  size_t count = 0;
  size_t start = -1;
  bool done    = false;
  bool match   = true;
  while (not done) {
    auto mva = AssignmentExpression::match(ctx);
    if (mva) {
      count += mva._count;
      ctx = mva.consume();
    }
    auto try_tok = ctx.tokenValue(0);
    if (try_tok == ",") {
      ctx._startIndex++;
      count++;
    } else if (try_tok == ";") {
      done = true;
    } else {
      match = false;
    }
  }
  if (match) {
    rval._count   = count;
    rval._start   = start;
    rval._matched = true;
  }
  return rval;
}

Expression::parsed_t Expression::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

AssignmentExpression::match_t AssignmentExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  if (auto mvc = ConditionalExpression::match(ctx)) {
    rval = mvc;
  } else if (auto mvu = UnaryExpression::match(ctx)) {
    size_t count = mvu._count;
    auto ctx2    = mvu.consume();
    if (auto mvo = AssignmentOperator::match(ctx2)) {
      count += mvo._count;
      auto ctx3 = mvo.consume();
      if (auto mva = AssignmentExpression::match(ctx3)) {
        count += mva._count;
        rval._count   = count;
        rval._start   = ctx._startIndex;
        rval._matched = true;
      }
    }
  }
  assert(false);
  return rval;
}

AssignmentExpression::parsed_t AssignmentExpression::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

PrimaryExpression::match_t PrimaryExpression::match(FnParseContext ctx) {
  const auto ctxbase = ctx;
  match_t rval(ctxbase);
  assert(false);
  return rval;
}

ArgumentExpressionList::match_t ArgumentExpressionList::match(FnParseContext ctx) {
  const auto ctxbase = ctx;
  match_t rval(ctxbase);
  assert(false);
  return rval;
}


/////////////////////////////////////////////////////////////////////////////////////////////////

PostFixExpression::match_t PostFixExpression::match(FnParseContext ctx) {
  const auto ctxbase = ctx;
  match_t rval(ctxbase);
  size_t count = 0;
  bool done = false;
  while(not done){
    if (auto m = PrimaryExpression::match(ctx)) {
      ctx = m.consume();
      count += m._count;
    }
    else if (auto m = OpenParen::match(ctx)) {
      ctx = m.consume();
      count += m._count;
      if (auto m = CloseParen::match(ctx)) {
        ctx = m.consume();
        count += m._count;
      }
      else if (auto m = ArgumentExpressionList::match(ctx)) {
        ctx = m.consume();
        count += m._count;
        if (auto m = CloseParen::match(ctx)) {
          ctx = m.consume();
          count += m._count;
        }
        else
          return match_t(ctxbase);
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

UnaryExpression::match_t UnaryExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  if( auto m = PostFixExpression::match(ctx)){
    return m;
  }
  else if( auto m = IncOp::match(ctx)){
    auto c2 = m.consume();
    if( auto m2 = UnaryExpression::match(c2)){
      return (m+m2);
    }
  }
  else if( auto m = DecOp::match(ctx)){
    auto c2 = m.consume();
    if( auto m2 = UnaryExpression::match(c2)){
      return (m+m2);
    }
  }
  else if( auto m = UnaryOp::match(ctx)){
    auto c2 = m.consume();
    if( auto m2 = CastExpression::match(c2)){
      return (m+m2);
    }
  }
  else if( auto m = SizeofOp::match(ctx)){
    auto c2 = m.consume();
    if( auto m2 = UnaryExpression::match(c2)){
      return (m+m2);
    }
    else if( auto m2 = OpenParen::match(c2)){
      auto c3 = (m+m2).consume();
      if( auto m3 = TypeName::match(c3)){
        auto c4 = (m+m2+m3).consume();
        if( auto m4 = CloseParen::match(c4)){
          return m+m2+m3+m4;
        }
      }
    }
  }
  return rval;
}

UnaryExpression::parsed_t UnaryExpression::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

ConditionalExpression::match_t ConditionalExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  if( auto mte = TernaryExpression::match(ctx)){
    rval = mte;
  }
  else if( auto mtl = LogicalOrExpression::match(ctx)){
    rval = mtl;
  }
  return rval;
}

ConditionalExpression::parsed_t ConditionalExpression::parse(const match_t& match) {
  parsed_t rval;

  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

TernaryExpression::match_t TernaryExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  if( auto mvl = LogicalOrExpression::match(ctx)){
    if(ctx.tokenValue(mvl.end())=="?"){
      auto ctx2 = mvl.consume();
      ctx2._startIndex++; // consume ?
      if(auto mve = Expression::match(ctx2)){
        if(ctx.tokenValue(mve.end())==":"){
          auto ctx3 = mve.consume();
          ctx3._startIndex++; // consume :
          if( auto mvx = ConditionalExpression::match(ctx3)){
            rval._matched = true;
            assert(false);
          }
        }
      }

    }
  }
  return rval;
}

TernaryExpression::parsed_t TernaryExpression::parse(const match_t& match) {
  parsed_t rval;

  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

LogicalOrExpression::match_t LogicalOrExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  bool done = false;
  bool danglingor = false;
  while(not done){
    if( auto mla = LogicalAndExpression::match(ctx)){
      ctx = mla.consume();
      rval = rval+mla;
      danglingor = false;
    }
    if( auto moo = OrOrOp::match(ctx)){
      ctx = moo.consume();
      rval = rval+moo;
      danglingor = true;
    }
    else {
      done = true;
    }
  }
  assert(false==danglingor);
  return rval;
}

LogicalOrExpression::parsed_t LogicalOrExpression::parse(const match_t& match) {
  parsed_t rval;
  assert(false);
  return rval;
}
// void Expression::emit(shaderbuilder::BackEnd& backend) const {
// assert(false);
//}

/////////////////////////////////////////////////////////////////////////////////////////////////

LogicalAndExpression::match_t LogicalAndExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  bool done = false;
  bool danglingand = false;
  while(not done){
    if( auto mio = InclusiveOrExpression::match(ctx)){
      ctx = mio.consume();
      rval = rval+mio;
      danglingand = false;
    }
    if( auto mao = AndAndOp::match(ctx)){
      ctx = mao.consume();
      rval = rval+mao;
      danglingand = true;
    }
    else {
      done = true;
    }
  }
  assert(false==danglingand);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

InclusiveOrExpression::match_t InclusiveOrExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  bool done = false;
  bool danglingor = false;
  while(not done){
    if( auto meo = ExclusiveOrExpression::match(ctx)){
      ctx = meo.consume();
      rval = rval+meo;
      danglingor = false;
    }
    if( auto moo = OrOp::match(ctx)){
      ctx = moo.consume();
      rval = rval+moo;
      danglingor = true;
    }
    else {
      done = true;
    }
  }
  assert(false==danglingor);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

ExclusiveOrExpression::match_t ExclusiveOrExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  bool done = false;
  bool danglingxor = false;
  while(not done){
    if( auto meo = AndExpression::match(ctx)){
      ctx = meo.consume();
      rval = rval+meo;
      danglingxor = false;
    }
    if( auto xoo = XorOp::match(ctx)){
      ctx = xoo.consume();
      rval = rval+xoo;
      danglingxor = true;
    }
    else {
      done = true;
    }
  }
  assert(false==danglingxor);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

AndExpression::match_t AndExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  bool done = false;
  bool danglingand = false;
  while(not done){
    if( auto eeo = EqualityExpression::match(ctx)){
      ctx = eeo.consume();
      rval = rval+eeo;
      danglingand = false;
    }
    if( auto mao = AndOp::match(ctx)){
      ctx = mao.consume();
      rval = rval+mao;
      danglingand = true;
    }
    else {
      done = true;
    }
  }
  assert(false==danglingand);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

EqualityExpression::match_t EqualityExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  bool done = false;
  bool danglingop = false;
  while(not done){
    if( auto mro = RelationalExpression::match(ctx)){
      ctx = mro.consume();
      rval = rval+mro;
      danglingop = false;
    }
    if( auto meo = EqOp::match(ctx)){
      ctx = meo.consume();
      rval = rval+meo;
      danglingop = true;
    }
    else if( auto mno = NeqOp::match(ctx)){
      ctx = mno.consume();
      rval = rval+mno;
      danglingop = true;
    }
    else {
      done = true;
    }
  }
  assert(false==danglingop);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

RelationalExpression::match_t RelationalExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  bool done = false;
  bool danglingop = false;
  while(not done){
    if( auto mso = ShiftExpression::match(ctx)){
      ctx = mso.consume();
      rval = rval+mso;
      danglingop = false;
    }
    if( auto mlt = LtOp::match(ctx)){
      ctx = mlt.consume();
      rval = rval+mlt;
      danglingop = true;
    }
    else if( auto mlte = LtEqOp::match(ctx)){
      ctx = mlte.consume();
      rval = rval+mlte;
      danglingop = true;
    }
    else if( auto mgt = GtOp::match(ctx)){
      ctx = mgt.consume();
      rval = rval+mgt;
      danglingop = true;
    }
    else if( auto mgte = GtEqOp::match(ctx)){
      ctx = mgte.consume();
      rval = rval+mgte;
      danglingop = true;
    }
    else {
      done = true;
    }
  }
  assert(false==danglingop);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

ShiftExpression::match_t ShiftExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  bool done = false;
  bool danglingop = false;
  while(not done){
    if( auto mso = AdditiveExpression::match(ctx)){
      ctx = mso.consume();
      rval = rval+mso;
      danglingop = false;
    }
    if( auto mlt = LeftOp::match(ctx)){
      ctx = mlt.consume();
      rval = rval+mlt;
      danglingop = true;
    }
    else if( auto mrt = RightOp::match(ctx)){
      ctx = mrt.consume();
      rval = rval+mrt;
      danglingop = true;
    }
    else {
      done = true;
    }
  }
  assert(false==danglingop);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

AdditiveExpression::match_t AdditiveExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  bool done = false;
  bool danglingop = false;
  while(not done){
    if( auto mso = MultiplicativeExpression::match(ctx)){
      ctx = mso.consume();
      rval = rval+mso;
      danglingop = false;
    }
    if( auto mlt = AddOp::match(ctx)){
      ctx = mlt.consume();
      rval = rval+mlt;
      danglingop = true;
    }
    else if( auto mrt = SubOp::match(ctx)){
      ctx = mrt.consume();
      rval = rval+mrt;
      danglingop = true;
    }
    else {
      done = true;
    }
  }
  assert(false==danglingop);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

MultiplicativeExpression::match_t MultiplicativeExpression::match(FnParseContext ctx) {
  match_t rval(ctx);
  bool done = false;
  bool danglingop = false;
  while(not done){
    if( auto mso = CastExpression::match(ctx)){
      ctx = mso.consume();
      rval = rval+mso;
      danglingop = false;
    }
    if( auto mlt = MulOp::match(ctx)){
      ctx = mlt.consume();
      rval = rval+mlt;
      danglingop = true;
    }
    else if( auto mrt = DivOp::match(ctx)){
      ctx = mrt.consume();
      rval = rval+mrt;
      danglingop = true;
    }
    else if( auto mrt = ModOp::match(ctx)){
      ctx = mrt.consume();
      rval = rval+mrt;
      danglingop = true;
    }
    else {
      done = true;
    }
  }
  assert(false==danglingop);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
