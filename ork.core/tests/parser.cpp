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
  CrcEnum(INTEGER),
  CrcEnum(KW_OR_ID)
};

///////////////////////////////////////////////////////////////////////////////

void loadScannerRules(scanner_ptr_t s) { //
  s->addEnumClass("\\/\\*([^*]|\\*+[^/*])*\\*+\\/", TokenClass::MULTI_LINE_COMMENT);
  s->addEnumClass("\\/\\/.*[\\n\\r]", TokenClass::SINGLE_LINE_COMMENT);
  s->addEnumClass("\\s+", TokenClass::WHITESPACE);
  s->addEnumClass("[\\n\\r]+", TokenClass::NEWLINE);
  s->addEnumClass("[a-zA-Z_][a-zA-Z0-9_]*", TokenClass::KW_OR_ID);
  s->addEnumClass("=", TokenClass::EQUALS);
  s->addEnumClass(",", TokenClass::COMMA);
  s->addEnumClass(";", TokenClass::SEMICOLON);
  s->addEnumClass("\\(", TokenClass::L_PAREN);
  s->addEnumClass("\\)", TokenClass::R_PAREN);
  s->addEnumClass("\\{", TokenClass::L_CURLY);
  s->addEnumClass("\\}", TokenClass::R_CURLY);
  s->addEnumClass("\\*", TokenClass::STAR);
  s->addEnumClass("\\+", TokenClass::PLUS);
  s->addEnumClass("\\-", TokenClass::MINUS);
  s->addEnumClass("-?(\\d*\\.?)(\\d+)([eE][-+]?\\d+)?", TokenClass::FLOATING_POINT);
  s->addEnumClass("-?(\\d+)", TokenClass::INTEGER);

  s->buildStateMachine();
}

///////////////////////////////////////////////////////////////////////////////

matcher_ptr_t loadGrammar(parser_ptr_t p) { //
printf( "A\n" );
  auto plus      = p->matcherForTokenClass(TokenClass::PLUS, "plus");
  auto minus      = p->matcherForTokenClass(TokenClass::MINUS, "minus");
  auto star      = p->matcherForTokenClass(TokenClass::STAR, "star");
  auto equals      = p->matcherForTokenClass(TokenClass::EQUALS, "equals");
  auto semicolon   = p->matcherForTokenClass(TokenClass::SEMICOLON, "semicolon");
  auto comma       = p->matcherForTokenClass(TokenClass::COMMA, "comma");
  auto lparen      = p->matcherForTokenClass(TokenClass::L_PAREN, "lparen");
  auto rparen      = p->matcherForTokenClass(TokenClass::R_PAREN, "rparen");
  auto lcurly      = p->matcherForTokenClass(TokenClass::L_CURLY, "lcurly");
  auto rcurly      = p->matcherForTokenClass(TokenClass::R_CURLY, "rcurly");
  auto floattok    = p->matcherForTokenClass(TokenClass::FLOATING_POINT, "float");
  auto inttok      = p->matcherForTokenClass(TokenClass::INTEGER, "int");
  auto kworid      = p->matcherForTokenClass(TokenClass::KW_OR_ID, "kw_or_id");
  auto kw_function = p->matcherForWord("function");
  ///////////////////////////////////////////////////////////
  auto dt_float = p->matcherForWord("float");
  auto dt_int   = p->matcherForWord("int");
  ///////////////////////////////////////////////////////////
printf( "B\n" );
  auto datatype = p->oneOf({
      dt_float,
      dt_int,
  });
  ///////////////////////////////////////////////////////////
  auto argument_decl = p->sequence(
      "argument_decl",
      {
          //
          datatype,
          kworid,
          p->optional(comma),
      });
  ///////////////////////////////////////////////////////////
  auto number = p->oneOf({
      floattok,
      inttok,
  });
  ///////////////////////////////////////////////////////////
  auto variableDeclaration = p->sequence(
      "variableDeclaration",
      { datatype,
        kworid
      });
printf( "C\n" );
  ///////////////////////////////////////////////////////////
  auto variableReference = p->sequence(
      "variableReference",
      { kworid
      });
  ///////////////////////////////////////////////////////////
  auto expression = p->declare( "expression" );
  ///////////////////////////////////////////////////////////
  auto primary = p->oneOf("primary",{
    floattok,
    inttok,
    variableReference,
    p->sequence({ lparen,expression,rparen }),
  });
  ///////////////////////////////////////////////////////////
  auto multiplicative = p->oneOf("multiplicative",{
     p->sequence({ primary,p->zeroOrMore(p->sequence({star,primary})) })
     //p->sequence({ primary,p->optional(p->sequence({slash,primary})) }),
  });
printf( "D\n" );
  ///////////////////////////////////////////////////////////
  auto additive = p->oneOf("additive",{
      p->sequence({ multiplicative,plus,multiplicative }),
      p->sequence({ multiplicative,minus,multiplicative }),
      multiplicative
  });
  ///////////////////////////////////////////////////////////
  p->sequence( expression, { additive });
  ///////////////////////////////////////////////////////////
  auto assignment_statement = p->sequence(
      "assignment_statement",
      {
          //
          p->oneOf({variableDeclaration,variableReference}),
          equals,
          expression
      });
printf( "E\n" );
  ///////////////////////////////////////////////////////////
  auto statement = p->sequence({
    p->optional(assignment_statement),
    p->optional(semicolon)
  });
  ///////////////////////////////////////////////////////////
  auto seq = p->sequence(
      "funcdef",
      {//
       kw_function,
       kworid,
       lparen,
       p->zeroOrMore(argument_decl,"fnd_args"),
       rparen,
       lcurly,
       p->zeroOrMore(statement,"fnd_statements"),
       rcurly});
printf( "F\n" );
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
        function def() {
        //    float X = (1.0+2.3)*7.0;
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