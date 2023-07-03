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
#include <csignal>
#include <ork/util/logger.h>
#include <ork/kernel/opq.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork {
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
  CrcEnum(PIPE),
  CrcEnum(INTEGER),
  CrcEnum(KW_OR_ID),
  CrcEnum(QUOTED_REGEX),
  CrcEnum(QUOTED_STRING),
  CrcEnum(LEFT_ARROW),
};

namespace AST {
struct ScannerRule;
struct ScannerMacro;
struct AstNode;
struct ParserRule;
struct Expression;
struct OneOrMore;
struct ZeroOrMore;
struct Select;
struct Optional;
struct Sequence;
struct Group;
struct ExprKWID;
struct DumpContext;
//
using scanner_rule_ptr_t  = std::shared_ptr<ScannerRule>;
using scanner_macro_ptr_t = std::shared_ptr<ScannerMacro>;
using astnode_ptr_t       = std::shared_ptr<AstNode>;
using rule_ptr_t          = std::shared_ptr<ParserRule>;
using expression_ptr_t    = std::shared_ptr<Expression>;
using oneormore_ptr_t     = std::shared_ptr<OneOrMore>;
using zeroormore_ptr_t    = std::shared_ptr<ZeroOrMore>;
using select_ptr_t        = std::shared_ptr<Select>;
using optional_ptr_t      = std::shared_ptr<Optional>;
using sequence_ptr_t      = std::shared_ptr<Sequence>;
using group_ptr_t         = std::shared_ptr<Group>;
using expr_kwid_ptr_t     = std::shared_ptr<ExprKWID>;
using dumpctx_ptr_t       = std::shared_ptr<DumpContext>;

struct AstNode {
  std::string _name;
  AstNode(Parser* user_parser);
  virtual ~AstNode();
  virtual void dump(dumpctx_ptr_t dctx) = 0;
  virtual matcher_ptr_t createMatcher(std::string named) = 0;
  Parser* _user_parser                  = nullptr;
  void_lambda_t _on_link;
};
////////////////////////////////////////////////////////////////////////
struct Expression : public AstNode {
  Expression(Parser* user_parser, std::string name = "");
  void dump(dumpctx_ptr_t dctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  astnode_ptr_t _expr_selected;
  std::string _expr_name;
};
////////////////////////////////////////////////////////////////////////
struct ExprKWID : public AstNode {
  ExprKWID(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  std::string _kwid;
  std::string _expr_name;
};
////////////////////////////////////////////////////////////////////////
struct OneOrMore : public AstNode {
  OneOrMore(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
struct ZeroOrMore : public AstNode {
  ZeroOrMore(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  expression_ptr_t _subexpression;
};
////////////////////////////////////////////////////////////////////////
struct Select : public AstNode {
  Select(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
struct Optional : public AstNode {
  Optional(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  expression_ptr_t _subexpression;
};
////////////////////////////////////////////////////////////////////////
struct Sequence : public AstNode {
  Sequence(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
struct Group : public AstNode {
  Group(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
struct ParserRule : public AstNode {
  ParserRule(Parser* user_parser, std::string name = "");
  void dump(dumpctx_ptr_t dctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  expression_ptr_t _expression;
};
} // namespace AST

struct RuleSpecImpl { // {: public Parser {

  RuleSpecImpl();
  void loadScannerRules();
  void loadGrammar();

  AST::oneormore_ptr_t _onOOM(match_ptr_t match);
  AST::zeroormore_ptr_t _onZOM(match_ptr_t match);
  AST::select_ptr_t _onSEL(match_ptr_t match);
  AST::optional_ptr_t _onOPT(match_ptr_t match);
  AST::sequence_ptr_t _onSEQ(match_ptr_t match);
  AST::group_ptr_t _onGRP(match_ptr_t match);
  AST::expr_kwid_ptr_t _onEXPRKWID(match_ptr_t match);
  AST::expression_ptr_t _onExpression(match_ptr_t match, std::string named = "");

  match_ptr_t parseScannerSpec(std::string inp_string);
  match_ptr_t parseParserSpec(std::string inp_string);
  void attachUser(Parser* user_parser);
  svar64_t findKWORID(std::string kworid);

  size_t indent = 0;
  scanner_ptr_t _user_scanner;
  Parser* _user_parser = nullptr;
  parser_ptr_t _dsl_parser;
  matcher_ptr_t _rsi_scanner_matcher;
  matcher_ptr_t _rsi_parser_matcher;

  std::unordered_map<std::string, matcher_ptr_t> _user_matchers_by_name;

  std::unordered_map<std::string, matcher_ptr_t> _user_scanner_matchers_by_name;
  std::unordered_map<std::string, matcher_ptr_t> _user_parser_matchers_by_name;

  std::unordered_set<AST::astnode_ptr_t> _retain_astnodes;

  std::unordered_map<std::string, AST::rule_ptr_t> _user_parser_rules;

  std::unordered_map<std::string, AST::scanner_rule_ptr_t> _user_scanner_rules;
  std::unordered_map<std::string, AST::scanner_macro_ptr_t> _user_scanner_macros;
  std::unordered_map<std::string, matcher_notif_t> _user_deferred_notifs;

  std::vector<void_lambda_t> _link_ops;
};

using rulespec_impl_ptr_t = std::shared_ptr<RuleSpecImpl>;

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork
/////////////////////////////////////////////////////////////////////////////////////////////////
