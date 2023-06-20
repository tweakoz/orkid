////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <regex>
#include <stdlib.h>
#include <stdarg.h>
#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/util/parser.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/crc.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork {
/////////////////////////////////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::rule(const std::string& rule_name) {
  auto it = _matchers_by_name.find(rule_name);
  matcher_ptr_t rval;
  if (it != _matchers_by_name.end()) {
    rval = it->second;
  }
  return rval;
}

void Parser::on(const std::string& rule_name, matcher_notif_t fn) {
  auto it = _matchers_by_name.find(rule_name);
  if (it != _matchers_by_name.end()) {
    matcher_ptr_t matcher = it->second;
    matcher->_notif       = fn;
  } else {
    OrkAssert(false);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

enum class TokenClass : uint64_t {
  CrcEnum(WHITESPACE),
  CrcEnum(NEWLINE),
  CrcEnum(COLON),
  CrcEnum(SEMICOLON),
  CrcEnum(L_PAREN),
  CrcEnum(R_PAREN),
  CrcEnum(L_CURLY),
  CrcEnum(R_CURLY),
  CrcEnum(L_SQUARE),
  CrcEnum(R_SQUARE),
  CrcEnum(EQUALS),
  CrcEnum(STAR),
  CrcEnum(PLUS),
  CrcEnum(MINUS),
  CrcEnum(COMMA),
  CrcEnum(INTEGER),
  CrcEnum(KW_OR_ID),
  CrcEnum(QUOTED_REGEX),
  CrcEnum(QUOTED_STRING),
  CrcEnum(LEFT_ARROW),
};

/////////////////////////////////////////////////////////////////////////////////////////////////

namespace AST {

struct VariableReference;
struct Expression;
using expression_ptr_t = std::shared_ptr<Expression>;
using varref_ptr_t     = std::shared_ptr<VariableReference>;

struct AstNode {
  virtual ~AstNode() {
  }
};

} // namespace AST

/////////////////////////////////////////////////////////////////////////////////////////////////

static constexpr const char* block_regex = "(function|yo|xxx)";

struct RuleSpecImpl : public Parser {

  RuleSpecImpl() {
    loadScannerRules();
    loadGrammar();
  }
  /////////////////////////////////////////////////////////
  void loadScannerRules() { //
    printf("A\n");
    _scanner = std::make_shared<Scanner>(block_regex);
    _scanner->addEnumClass("\\s+", TokenClass::WHITESPACE);
    _scanner->addEnumClass("[\\n\\r]+", TokenClass::NEWLINE);
    _scanner->addEnumClass("[a-zA-Z_][a-zA-Z0-9_]*", TokenClass::KW_OR_ID);
    printf("A2\n");
    _scanner->addEnumClass("=", TokenClass::EQUALS);
    _scanner->addEnumClass(",", TokenClass::COMMA);
    _scanner->addEnumClass(":", TokenClass::COLON);
    _scanner->addEnumClass(";", TokenClass::SEMICOLON);
    _scanner->addEnumClass("\\(", TokenClass::L_PAREN);
    _scanner->addEnumClass("\\)", TokenClass::R_PAREN);
    printf("A3\n");
    _scanner->addEnumClass("\\[", TokenClass::L_SQUARE);
    _scanner->addEnumClass("\\]", TokenClass::R_SQUARE);
    _scanner->addEnumClass("\\{", TokenClass::L_CURLY);
    _scanner->addEnumClass("\\}", TokenClass::R_CURLY);
    _scanner->addEnumClass("\\*", TokenClass::STAR);
    printf("A4\n");
    _scanner->addEnumClass("\\+", TokenClass::PLUS);
    _scanner->addEnumClass("\\-", TokenClass::MINUS);
    _scanner->addEnumClass("-?(\\d+)", TokenClass::INTEGER);
    printf("A5\n");
    _scanner->addEnumClass("\"(\\\\.|[^\"])*\"", TokenClass::QUOTED_REGEX);
    printf("A5A\n");
    _scanner->addEnumClass("<-", TokenClass::LEFT_ARROW);
    printf("A6\n");
    _scanner->buildStateMachine();
    printf("B\n");
  }
  /////////////////////////////////////////////////////////
  void loadGrammar() { //
    printf("C\n");
    ////////////////////
    // primitives
    ////////////////////
    auto plus          = matcherForTokenClass(TokenClass::PLUS, "plus");
    auto minus         = matcherForTokenClass(TokenClass::MINUS, "minus");
    auto star          = matcherForTokenClass(TokenClass::STAR, "star");
    auto equals        = matcherForTokenClass(TokenClass::EQUALS, "equals");
    auto colon         = matcherForTokenClass(TokenClass::COLON, "colon");
    auto semicolon     = matcherForTokenClass(TokenClass::SEMICOLON, "semicolon");
    auto comma         = matcherForTokenClass(TokenClass::COMMA, "comma");
    auto lparen        = matcherForTokenClass(TokenClass::L_PAREN, "lparen");
    auto rparen        = matcherForTokenClass(TokenClass::R_PAREN, "rparen");
    auto lsquare       = matcherForTokenClass(TokenClass::L_SQUARE, "lsquare");
    auto rsquare       = matcherForTokenClass(TokenClass::R_SQUARE, "rsquare");
    auto lcurly        = matcherForTokenClass(TokenClass::L_CURLY, "lcurly");
    auto rcurly        = matcherForTokenClass(TokenClass::R_CURLY, "rcurly");
    auto inttok        = matcherForTokenClass(TokenClass::INTEGER, "int");
    auto kworid        = matcherForTokenClass(TokenClass::KW_OR_ID, "kw_or_id");
    auto left_arrow    = matcherForTokenClass(TokenClass::LEFT_ARROW, "left_arrow");
    auto quoted_regex = matcherForTokenClass(TokenClass::QUOTED_REGEX, "quoted_regex");
    ////////////////////
    auto oneof = matcherForWord("oneOf");
    auto zom   = matcherForWord("zom");
    auto oom   = matcherForWord("oom");
    auto opt   = matcherForWord("opt");
    ////////////////////
    // scanner rules
    ////////////////////
    printf("D\n");
    auto scanner_rule    = sequence({kworid, left_arrow, quoted_regex}, "scanner_rule");
    _rsi_scanner_matcher = zeroOrMore(scanner_rule, "scanner_rules");
    ////////////////////
    // parser rules
    ////////////////////
    printf("E\n");
    auto rule_expression = declare("rule_expression");
    //
    auto rule_zom      = sequence({zom, lcurly, rule_expression, rcurly}, "rule_zom");
    auto rule_oom      = sequence({oom, lcurly, rule_expression, rcurly}, "rule_oom");
    auto rule_1of      = sequence({oneof, lcurly, rule_expression, rcurly}, "rule_oneof");
    auto rule_opt      = sequence({opt, lcurly, rule_expression, rcurly}, "rule_opt");
    auto rule_sequence = sequence({lsquare, zeroOrMore(rule_expression), rsquare}, "rule_sequence");
    auto rule_grp      = sequence({lparen, zeroOrMore(rule_expression), rparen}, "rule_grp");
    //
    sequence(
        rule_expression,
        {
            oneOf({// load previously declared rule_expression
                   rule_zom,
                   rule_oom,
                   rule_1of,
                   rule_opt,
                   rule_sequence,
                   rule_grp}),
            optional(sequence({colon, quoted_regex}), "expr_name"),
        });
    auto parser_rule    = sequence({kworid, left_arrow, rule_expression}, "parser_rule");
    _rsi_parser_matcher = zeroOrMore(parser_rule, "parser_rules");
    printf("F\n");
  }
  /////////////////////////////////////////////////////////
  match_ptr_t parseScannerSpec(std::string inp_string) {
    _scanner->clear();
    _scanner->scanString(inp_string);
    _scanner->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));

    auto top_view = _scanner->createTopView();
    top_view.dump("top_view");
    auto slv   = std::make_shared<ScannerLightView>(top_view);
    auto match = this->match(slv, _rsi_scanner_matcher);
    return match;
  }
  /////////////////////////////////////////////////////////
  match_ptr_t parseParserSpec(std::string inp_string) {
    _scanner->clear();
    _scanner->scanString(inp_string);
    _scanner->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));

    auto top_view = _scanner->createTopView();
    top_view.dump("top_view");
    auto slv   = std::make_shared<ScannerLightView>(top_view);
    auto match = this->match(slv, _rsi_parser_matcher);
    return match;
  }
  /////////////////////////////////////////////////////////

  scanner_ptr_t _scanner;
  matcher_ptr_t _rsi_scanner_matcher;
  matcher_ptr_t _rsi_parser_matcher;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

using rulespec_impl_ptr_t = std::shared_ptr<RuleSpecImpl>;

rulespec_impl_ptr_t getRuleSpecImpl() {
  static auto rsi = std::make_shared<RuleSpecImpl>();
  return rsi;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void Parser::loadScannerSpec(const std::string& spec) {
  auto rsi   = getRuleSpecImpl();
  auto match = rsi->parseScannerSpec(spec);
  OrkAssert(match);
}
void Parser::loadParserSpec(const std::string& spec) {
  auto rsi   = getRuleSpecImpl();
  auto match = rsi->parseParserSpec(spec);
  OrkAssert(match);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork
/////////////////////////////////////////////////////////////////////////////////////////////////
