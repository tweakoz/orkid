////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/parser.inl>
#include <utpp/UnitTest++.h>
#include <ork/util/crc.h>
#include <string.h>
#include <math.h>

using namespace ork;

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

constexpr const char* block_regex = "(function|yo|xxx)";

void loadScannerRules(scanner_ptr_t s) { //
  s->addRule("\\/\\*([^*]|\\*+[^/*])*\\*+\\/", uint64_t(TokenClass::MULTI_LINE_COMMENT));
  s->addRule("\\/\\/.*[\\n\\r]", uint64_t(TokenClass::SINGLE_LINE_COMMENT));
  s->addRule("\\s+", uint64_t(TokenClass::WHITESPACE));
  s->addRule("[\\n\\r]+", uint64_t(TokenClass::NEWLINE));
  s->addRule("[a-zA-Z_]+[a-zA-Z0-9_]+", uint64_t(TokenClass::KW_OR_ID));
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

#define MATCHER(x) auto x = p->createMatcher([=](scannerlightview_constptr_t inp_view)->scannerlightview_ptr_t

matcher_ptr_t loadGrammar(parser_ptr_t p) { //
  auto equals         = p->matcherForTokenClass(TokenClass::EQUALS);
  auto semicolon      = p->matcherForTokenClass(TokenClass::SEMICOLON);
  auto comma          = p->matcherForTokenClass(TokenClass::COMMA);
  auto lparen         = p->matcherForTokenClass(TokenClass::L_PAREN);
  auto rparen         = p->matcherForTokenClass(TokenClass::R_PAREN);
  auto lcurly         = p->matcherForTokenClass(TokenClass::L_CURLY);
  auto rcurly         = p->matcherForTokenClass(TokenClass::R_CURLY);
  auto floattok       = p->matcherForTokenClass(TokenClass::FLOATING_POINT);
  auto kworid         = p->matcherForTokenClass(TokenClass::KW_OR_ID);
  auto kw_function    = p->matcherForWord("function");
  lcurly->_notif      = [=](scannerlightview_ptr_t inp_view) { printf("MATCHED lcurly\n"); };
  kworid->_notif      = [=](scannerlightview_ptr_t inp_view) { printf("MATCHED kworid<%s>\n", inp_view->token(0)->text.c_str()); };
  kw_function->_notif = [=](scannerlightview_ptr_t inp_view) { printf("MATCHED 'function'\n"); };
  auto seq            = p->sequence({kw_function, kworid, lcurly});
  seq->_notif         = [=](scannerlightview_ptr_t inp_view) {
    printf("MATCHED sequence\n");
    inp_view->dump("seq");
  };
  return seq;
}

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
        function abc {
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
  auto matched = fn_matcher->match(slv);
  OrkAssert(matched);
}