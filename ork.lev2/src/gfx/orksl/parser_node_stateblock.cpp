////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/orksl/parser.h>

namespace ork::lev2::orksl::parser {
///////////////////////////////////////////////////////////////////////////////

void StateBlockNode::parse(OrkSlParser* parser, const ScannerView& view) {
  DecoBlockNode::parse(parser,view);
  //////////////////////
  int ist = view._start + 1;
  int ien = view._end - 1;
  //////////////////////
  for (size_t i = ist; i <= ien;) {
    const Token* vt_tok = view.token(i);
    if (vt_tok->text == "CullTest") {
      _culltest = view.token(i + 2)->text;
      i += 4;
    } else if (vt_tok->text == "DepthMask") {
      _depthmask = view.token(i + 2)->text;
      i += 4;
    } else if (vt_tok->text == "DepthTest") {
      _depthtest = view.token(i + 2)->text;
      i += 4;
    } else if (vt_tok->text == "BlendMode") {
      _blendmode = view.token(i + 2)->text;
      i += 4;
    } else if (vt_tok->text == "\n") {
      i++;
    } else {
      OrkAssert(false);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void StateBlockNode::_generate2(shaderbuilder::BackEnd& backend) const {
  auto psb      = new StateBlock;
  auto c        = backend._container;
  psb->mName    = _name;
  auto& apptors = psb->mApplicators;

  //////////////////////
  // inherit stateblock
  //////////////////////

  size_t inumdecos = _decorators.size();

  assert(inumdecos < 2);

  for (const auto deco : _decorators) {
    StateBlock* parent = c->GetStateBlock(deco->text);
    assert(parent != nullptr);
    psb->mApplicators = parent->mApplicators; // inherit applicators
  }

  //////////////////////

  if (_culltest != "") {
    if (_culltest == "OFF")
      psb->addStateFn([=](Context* t) { t->RSI()->SetCullTest(lev2::ECULLTEST_OFF); });
    else if (_culltest == "PASS_FRONT")
      psb->addStateFn([=](Context* t) { t->RSI()->SetCullTest(lev2::ECULLTEST_PASS_FRONT); });
    else if (_culltest == "PASS_BACK")
      psb->addStateFn([=](Context* t) { t->RSI()->SetCullTest(lev2::ECULLTEST_PASS_BACK); });
  }
  if (_depthmask != "") {
    bool bena = (_depthmask == "true");
    psb->addStateFn([=](Context* t) { t->RSI()->SetZWriteMask(bena); });
  }
  if (_depthtest != "") {
    if (_depthtest == "OFF")
      psb->addStateFn([=](Context* t) { t->RSI()->SetDepthTest(lev2::EDEPTHTEST_OFF); });
    else if (_depthtest == "LESS")
      psb->addStateFn([=](Context* t) { t->RSI()->SetDepthTest(lev2::EDEPTHTEST_LESS); });
    else if (_depthtest == "LEQUALS")
      psb->addStateFn([=](Context* t) { t->RSI()->SetDepthTest(lev2::EDEPTHTEST_LEQUALS); });
    else if (_depthtest == "GREATER")
      psb->addStateFn([=](Context* t) { t->RSI()->SetDepthTest(lev2::EDEPTHTEST_GREATER); });
    else if (_depthtest == "GEQUALS")
      psb->addStateFn([=](Context* t) { t->RSI()->SetDepthTest(lev2::EDEPTHTEST_GEQUALS); });
    else if (_depthtest == "EQUALS")
      psb->addStateFn([=](Context* t) { t->RSI()->SetDepthTest(lev2::EDEPTHTEST_EQUALS); });
  }
  if (_blendmode != "") {
    if (_blendmode == "ADDITIVE")
      psb->addStateFn([=](Context* t) { t->RSI()->SetBlending(lev2::Blending::ADDITIVE); });
    else if (_blendmode == "ALPHA_ADDITIVE")
      psb->addStateFn([=](Context* t) { t->RSI()->SetBlending(lev2::Blending::ALPHA_ADDITIVE); });
    else if (_blendmode == "ALPHA")
      psb->addStateFn([=](Context* t) { t->RSI()->SetBlending(lev2::Blending::ALPHA); });
  }

  //////////////////////

  backend._container->addStateBlock(psb);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
