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
/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t Statement::match(FnParseContext ctx) {
  match_results_t rval;;
  if (auto mvi = IterationStatement::match(ctx)) {
    rval = mvi;
  } else if (auto ma = AssignmentStatement::match(ctx)) {
    rval = ma;
  } else if (auto mvi = InstantiationStatement::match(ctx)) {
    rval = mvi;
  } else if (auto mve = ExpressionStatement::match(ctx)) {
    rval = mve;
  } else if (auto mrs = ReturnStatement::match(ctx)) {
    rval = mrs; // empty statement
  }
  rval.dump("sta");
  return rval;
}

void Statement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t InstantiationStatement::match(FnParseContext ctx) {
  match_results_t rval;
  if (auto m = TypeName::match(ctx)) {
    auto ctx2 = m->consume();
    if (auto m2 = Identifier::match(ctx2)) {
      auto ctx3 = (m + m2)->consume();
      if (auto m3 = InitialAssignmentOperator::match(ctx3)) {
        auto ctx4 = (m + m2 + m3)->consume();
        if (auto m4 = Expression::match(ctx4)) {
          auto mfinal = (m + m2 + m3 + m4);
          rval = mfinal;
        }
      }
    }
  }
  rval.dump("sta-inst");
  return rval;
}

void InstantiationStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t AssignmentStatement::match(FnParseContext ctx) {
  match_results_t rval;
  if (auto m = Reference::match(ctx)) {
    auto ctx2 = m->consume();
    if (auto m2 = InitialAssignmentOperator::match(ctx2)) {
      auto ctx3 = (m + m2)->consume();
      if (auto m3 = Expression::match(ctx3)) {
        auto mfinal = (m + m2 + m3);
        rval = mfinal;
      }
    }
  }
  rval.dump("sta-ass");
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t ExpressionStatement::match(FnParseContext ctx) {
  match_results_t rval;
  auto first_tok = ctx.tokenValue(0);
  if (first_tok == ";") {
    rval = std::make_shared<match_t>(ctx);
    rval->_matched = true;
    rval->_start   = ctx._startIndex;
    rval->_count   = 1;
    rval.dump("sta-exp1");
    return rval;
  } else if (auto mve = Expression::match(ctx)) {
    rval = mve;
  }
  rval.dump("sta-exp2");
  return rval;
}

void ExpressionStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t IterationStatement::match(FnParseContext ctx) {
  match_results_t rval;
  if (auto mfs = ForLoopStatement::match(ctx)) {
    rval = mfs;
    return rval;
  }
  rval.dump("sta-iter");
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t ForLoopStatement::match(FnParseContext ctx) {
  FnParseContext basectx = ctx;
  match_results_t rval;
  auto first_tok = ctx.tokenValue(0);
  if (first_tok == "for") {
    match_results_t mf;
    mf = std::make_shared<match_t>(ctx);
    mf->_matched = true;
    mf->_start   = ctx._startIndex;
    mf->_count   = 1;
    rval = std::make_shared<match_t>(ctx);
    rval = rval + mf;
    ctx = rval->consume();
    //////////////////////////////////////
    auto mopen = OpenParen::match(ctx);
    assert(mopen);
    rval = rval + mopen;
    ctx = rval->consume();
    //////////////////////////////////////
    if( auto mtyp = TypeName::match(ctx) ){
      rval = rval + mtyp;
      ctx = rval->consume();
    }
    //////////////////////////////////////
    if( auto mid = Reference::match(ctx) ){
      rval = rval + mid;
      ctx = rval->consume();
    }
    //////////////////////////////////////
    if( auto meq = MutatingAssignmentOperator::match(ctx) ){
      rval = rval + meq;
      ctx = rval->consume();
      if( auto mex = Expression::match(ctx) ){
        rval = rval + mex;
        ctx = rval->consume();
      }
    }
    //////////////////////////////////////
    if( auto msc = SemicolonOp::match(ctx) ){
      rval = rval + msc;
      ctx = rval->consume();
    }
    //////////////////////////////////////
    if( auto mex = Expression::match(ctx) ){
      rval = rval + mex;
      ctx = rval->consume();
    }
    //////////////////////////////////////
    if( auto msc = SemicolonOp::match(ctx) ){
      rval = rval + msc;
      ctx = rval->consume();
    }
    //////////////////////////////////////
    if( auto mex = Expression::match(ctx) ){
      rval = rval + mex;
      ctx = rval->consume();
    }
    //////////////////////////////////////
    auto mclose = CloseParen::match(ctx);
    assert(mclose);
    rval = rval + mclose;
    ctx = rval->consume();
    //////////////////////////////////////
    auto mblock = CompoundStatement::match(ctx);
    rval = rval + mblock;
    ctx = rval->consume();
    rval.dump("sta-for");
    return rval;
  }
  return rval;
}

void ForLoopStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t StatementList::match(FnParseContext ctx) {
  match_results_t rval;
  rval = std::make_shared<match_t>(ctx);
  size_t count = 0;
  size_t start = -1;
  bool done    = false;
  while (not done) {
    if(ctx.tokenValue(0)=="float" and
       ctx.tokenValue(1)=="nz"){
      printf( "yo\n");
    }
    auto sta = Statement::match(ctx);
    if (sta) {
      rval = rval + sta;
      ctx = rval->consume();
      if (auto msemi = SemicolonOp::match(ctx)) {
        rval = rval + msemi;
        ctx = rval->consume();
      }
    } else {
      done = true;
    }
  }
  rval.dump("stalist");
  return rval;
}

void StatementList::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t CompoundStatement::match(FnParseContext ctx) {
  ctx._view->dump("CompoundStatement::");
  match_results_t mfinal;
  ////////////////////////////////////
  auto mopen = OpenCurly::match(ctx);
  mopen.dump("oc");
  if (not mopen)
    return mfinal; // no match
  mfinal = mopen;
  ////////////////////////////////////
  // empty statement ?
  ////////////////////////////////////
  if (auto mcb = CloseCurly::match(mfinal->consume())) {
    mcb.dump("cc");
    return mfinal + mcb;
  }
  ////////////////////////////////////
  // declaration list optional
  ////////////////////////////////////
  if (auto mdl = DeclarationList::match(mfinal->consume())){
    mdl.dump("mdl");
    mfinal = mfinal + mdl;
  }
  ////////////////////////////////////
  // statement list mandatory
  ////////////////////////////////////
  auto msl = StatementList::match(mfinal->consume());
  msl.dump("msl");
  OrkAssert(msl); // statement list non optional in this case
  mfinal = mfinal + msl;
  ////////////////////////////////////
  // closing bracket mandatory
  ////////////////////////////////////
  auto ctxx = mfinal->consume();
  auto mcb = CloseCurly::match(ctxx);
  mcb.dump("mcb");
  mfinal   = mfinal + mcb;
  return mfinal;
}

//parsed_t CompoundStatement::parse(const match_t& match) {
  //parsed_t rval;
  //assert(false);
  //return rval;
//}
void CompoundStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_results_t ReturnStatement::match(FnParseContext ctx) {
  match_results_t rval;
  bool matched = ctx.tokenValue(0) == "return";
  if( matched ){
    auto ctx2 = ctx;
    ctx2._startIndex++;
    if( auto me = ExpressionNode::match(ctx2)){
      rval = me;
      rval->_count++; //  consume return keyword
    }
  }
  rval.dump("sta-ret");
  return rval;
}

//void ReturnStatement::emit(shaderbuilder::BackEnd& backend) const {
  //assert(false);
//}


/////////////////////////////////////////////////////////////////////////////////////////////////
/*
FnMatchResults WhileLoopStatement::match(const FnParseContext& ctx) {
  FnMatchResults rval;
  rval._matched = ctx.tokenValue(0) == "while";
  return rval;
}

int WhileLoopStatement::parse(const FnParseContext& ctx, const FnMatchResults& r) {
  assert(false);
  return 0;
}
void WhileLoopStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

FnMatchResults IfStatement::match(const FnParseContext& ctx) {
  FnMatchResults rval;
  rval._matched = ctx.tokenValue(0) == "if";
  return rval;
}

int IfStatement::parse(const FnParseContext& ctx, const FnMatchResults& r) {
  assert(false);
  return 0;
}
void IfStatement::emit(shaderbuilder::BackEnd& backend) const {
  assert(false);
}
*/
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif
