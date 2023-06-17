////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/parser.h>
#include <utpp/UnitTest++.h>
#include <ork/util/crc.h>
#include <string.h>
#include <math.h>

using namespace ork;

///////////////////////////////////////////////////////////////////////////////
constexpr const char* block_regex = "(function|yo|xxx)";
#define MATCHER(x) auto x = p->createMatcher([=](scannerlightview_constptr_t inp_view)->scannerlightview_ptr_t
///////////////////////////////////////////////////////////////////////////////

enum class TokenClass : uint64_t {
  CrcEnum(SINGLE_LINE_COMMENT),
  CrcEnum(MULTI_LINE_COMMENT),
  CrcEnum(WHITESPACE),
  CrcEnum(NEWLINE),
  CrcEnum(SEMICOLON),
  CrcEnum(L_PAREN),
  CrcEnum(R_PAREN),
  CrcEnum(L_CURLY),
  CrcEnum(R_CURLY),
  CrcEnum(EQUALS),
  CrcEnum(STAR),
  CrcEnum(PLUS),
  CrcEnum(MINUS),
  CrcEnum(COMMA),
  CrcEnum(FLOATING_POINT),
  CrcEnum(KW_OR_ID)
};

///////////////////////////////////////////////////////////////////////////////

void loadScannerRules(scanner_ptr_t s) { //
  s->addRule("\\/\\*([^*]|\\*+[^/*])*\\*+\\/", uint64_t(TokenClass::MULTI_LINE_COMMENT));
  s->addRule("\\/\\/.*[\\n\\r]", uint64_t(TokenClass::SINGLE_LINE_COMMENT));
  s->addRule("\\s+", uint64_t(TokenClass::WHITESPACE));
  s->addRule("[\\n\\r]+", uint64_t(TokenClass::NEWLINE));
  s->addRule("[a-zA-Z_][a-zA-Z0-9_]*", uint64_t(TokenClass::KW_OR_ID));
  s->addRule("=", uint64_t(TokenClass::EQUALS));
  s->addRule(",", uint64_t(TokenClass::COMMA));
  s->addRule(";", uint64_t(TokenClass::SEMICOLON));
  s->addRule("\\(", uint64_t(TokenClass::L_PAREN));
  s->addRule("\\)", uint64_t(TokenClass::R_PAREN));
  s->addRule("\\{", uint64_t(TokenClass::L_CURLY));
  s->addRule("\\}", uint64_t(TokenClass::R_CURLY));
  s->addRule("\\*", uint64_t(TokenClass::STAR));
  s->addRule("\\+", uint64_t(TokenClass::PLUS));
  s->addRule("\\-", uint64_t(TokenClass::MINUS));
  s->addRule("-?(\\d*\\.?)(\\d+)([eE][-+]?\\d+)?", uint64_t(TokenClass::FLOATING_POINT));

  s->buildStateMachine();
}

///////////////////////////////////////////////////////////////////////////////

matcher_ptr_t loadGrammar(parser_ptr_t p) { //
  auto equals      = p->matcherForTokenClass(TokenClass::EQUALS, "equals");
  auto semicolon   = p->matcherForTokenClass(TokenClass::SEMICOLON, "semicolon");
  auto comma       = p->matcherForTokenClass(TokenClass::COMMA, "comma");
  auto lparen      = p->matcherForTokenClass(TokenClass::L_PAREN, "lparen");
  auto rparen      = p->matcherForTokenClass(TokenClass::R_PAREN, "rparen");
  auto lcurly      = p->matcherForTokenClass(TokenClass::L_CURLY, "lcurly");
  auto rcurly      = p->matcherForTokenClass(TokenClass::R_CURLY, "rcurly");
  auto floattok    = p->matcherForTokenClass(TokenClass::FLOATING_POINT, "float");
  auto kworid      = p->matcherForTokenClass(TokenClass::KW_OR_ID, "kw_or_id");
  auto kw_function = p->matcherForWord("function");
  ///////////////////////////////////////////////////////////
  auto argument_decl = p->sequence(
      "argument_decl",
      {
          //
          kworid,
          kworid,
          p->optional(comma),
      });
  ///////////////////////////////////////////////////////////
  auto seq = p->sequence(
      "funcdef",
      {//
       kw_function,
       kworid,
       lparen,
       p->zeroOrMore(argument_decl),
       rparen});
  ///////////////////////////////////////////////////////////
  seq->_notif = [=](match_ptr_t match) {
    printf("MATCHED sequence<%s>\n", seq->_name.c_str());
    match->_view->dump("seq");
    match->dump(0);
  };
  return seq;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TEST(parser1) {

  auto s = std::make_shared<ork::Scanner>(block_regex);
  auto p = std::make_shared<ork::Parser>();
  loadScannerRules(s);
  auto fn_matcher = loadGrammar(p);

  auto parse_str =
      R"(
        ///////////////////
        // hello world
        ///////////////////
        function abc(int x, float y) {
            b = (a+v)*7.0;
        }
    )";
  s->scanString(parse_str);
  s->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
  s->discardTokensOfClass(uint64_t(TokenClass::SINGLE_LINE_COMMENT));
  s->discardTokensOfClass(uint64_t(TokenClass::MULTI_LINE_COMMENT));
  s->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));

  auto top_view = s->createTopView();
  top_view.dump("top_view");
  auto slv     = std::make_shared<ScannerLightView>(top_view);
  auto match = p->match(slv, fn_matcher);
  OrkAssert(match);
}