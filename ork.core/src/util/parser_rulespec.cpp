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
  CrcEnum(INTEGER),
  CrcEnum(KW_OR_ID)
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

struct RuleSpecImpl : public Parser {

  RuleSpecImpl() {
    loadScannerRules();
    loadGrammar();
  }
  /////////////////////////////////////////////////////////
  void loadScannerRules() { //
    static constexpr const char* block_regex = "(function|yo|xxx)";
    _scanner                                 = std::make_shared<Scanner>(block_regex);
    _scanner->addEnumClass("\\s+", TokenClass::WHITESPACE);
    _scanner->addEnumClass("[\\n\\r]+", TokenClass::NEWLINE);
    _scanner->addEnumClass("[a-zA-Z_][a-zA-Z0-9_]*", TokenClass::KW_OR_ID);
    _scanner->addEnumClass("=", TokenClass::EQUALS);
    _scanner->addEnumClass(",", TokenClass::COMMA);
    _scanner->addEnumClass(";", TokenClass::SEMICOLON);
    _scanner->addEnumClass("\\(", TokenClass::L_PAREN);
    _scanner->addEnumClass("\\)", TokenClass::R_PAREN);
    _scanner->addEnumClass("\\{", TokenClass::L_CURLY);
    _scanner->addEnumClass("\\}", TokenClass::R_CURLY);
    _scanner->addEnumClass("\\*", TokenClass::STAR);
    _scanner->addEnumClass("\\+", TokenClass::PLUS);
    _scanner->addEnumClass("\\-", TokenClass::MINUS);
    _scanner->addEnumClass("-?(\\d+)", TokenClass::INTEGER);
    _scanner->buildStateMachine();
  }
  /////////////////////////////////////////////////////////
  void loadGrammar() { //
    auto plus      = matcherForTokenClass(TokenClass::PLUS, "plus");
    auto minus     = matcherForTokenClass(TokenClass::MINUS, "minus");
    auto star      = matcherForTokenClass(TokenClass::STAR, "star");
    auto equals    = matcherForTokenClass(TokenClass::EQUALS, "equals");
    auto semicolon = matcherForTokenClass(TokenClass::SEMICOLON, "semicolon");
    auto comma     = matcherForTokenClass(TokenClass::COMMA, "comma");
    auto lparen    = matcherForTokenClass(TokenClass::L_PAREN, "lparen");
    auto rparen    = matcherForTokenClass(TokenClass::R_PAREN, "rparen");
    auto lcurly    = matcherForTokenClass(TokenClass::L_CURLY, "lcurly");
    auto rcurly    = matcherForTokenClass(TokenClass::R_CURLY, "rcurly");
    auto inttok    = matcherForTokenClass(TokenClass::INTEGER, "int");
    auto kworid    = matcherForTokenClass(TokenClass::KW_OR_ID, "kw_or_id");

    _rsi_matcher = zeroOrMore(sequence({kworid}), "rsi");
  }
  /////////////////////////////////////////////////////////
  match_ptr_t parseString(std::string parse_str) {
    _scanner->clear();
    _scanner->scanString(parse_str);
    _scanner->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));

    auto top_view = _scanner->createTopView();
    top_view.dump("top_view");
    auto slv   = std::make_shared<ScannerLightView>(top_view);
    auto match = this->match(slv, _rsi_matcher);
    return match;
  }
  /////////////////////////////////////////////////////////

  scanner_ptr_t _scanner;
  matcher_ptr_t _rsi_matcher;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

using rulespec_impl_ptr_t = std::shared_ptr<RuleSpecImpl>;

rulespec_impl_ptr_t getRuleSpecImpl() {
  rulespec_impl_ptr_t rval;
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void Parser::loadScannerSpec(const std::string& spec) {
  auto rsi = getRuleSpecImpl();
  OrkAssert(false);
}
void Parser::loadParserSpec(const std::string& spec) {
  auto rsi = getRuleSpecImpl();
  OrkAssert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork
/////////////////////////////////////////////////////////////////////////////////////////////////
