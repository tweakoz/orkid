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
namespace ork::peg {
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

////////////////////////////////////////////////////////////////////////
namespace AST {
////////////////////////////////////////////////////////////////////////
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
struct VisitContext;
struct RuleRef;
////////////////////////////////////////////////////////////////////////
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
using scanner_rule_pair_t = std::pair<std::string, scanner_rule_ptr_t>;
using astvisitctx_ptr_t   = std::shared_ptr<VisitContext>;
using node_visitor_t      = std::function<void(astnode_ptr_t)>;
using ruleref_ptr_t       = std::shared_ptr<RuleRef>;
using ruleref_list_t      = std::vector<ruleref_ptr_t>;
////////////////////////////////////////////////////////////////////////
struct VisitContext {
  rule_ptr_t _top_rule;
  node_visitor_t _visitor;
  std::vector<astnode_ptr_t> _visit_stack;
};
////////////////////////////////////////////////////////////////////////
struct RuleRef {
  rule_ptr_t _referenced_rule;
  astnode_ptr_t _node;
};
////////////////////////////////////////////////////////////////////////
struct AstNode {
  std::string _name;
  AstNode(Parser* user_parser);
  virtual ~AstNode();
  virtual void dump(dumpctx_ptr_t dctx) = 0;
  static void visit(astnode_ptr_t node, astvisitctx_ptr_t ctx);
  virtual void do_visit(astvisitctx_ptr_t ctx)           = 0;
  virtual matcher_ptr_t createMatcher(std::string named) = 0;
  std::string path() const;
  astnode_ptr_t root() const;
  Parser* _user_parser = nullptr;
  void_lambda_t _on_link;
  astnode_ptr_t _parent;
};
////////////////////////////////////////////////////////////////////////
struct Expression : public AstNode {
  Expression(Parser* user_parser, std::string name = "");
  void dump(dumpctx_ptr_t dctx) final;
  void do_visit(astvisitctx_ptr_t ctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  astnode_ptr_t _expr_selected;
  std::string _expr_name;
};
////////////////////////////////////////////////////////////////////////
struct ExprKWID : public AstNode {
  ExprKWID(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  void do_visit(astvisitctx_ptr_t ctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  std::string _kwid;
  std::string _expr_name;
};
////////////////////////////////////////////////////////////////////////
struct OneOrMore : public AstNode {
  OneOrMore(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  void do_visit(astvisitctx_ptr_t ctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
struct ZeroOrMore : public AstNode {
  ZeroOrMore(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  void do_visit(astvisitctx_ptr_t ctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  expression_ptr_t _subexpression;
};
////////////////////////////////////////////////////////////////////////
struct Select : public AstNode {
  Select(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  void do_visit(astvisitctx_ptr_t ctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
struct Optional : public AstNode {
  Optional(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  void do_visit(astvisitctx_ptr_t ctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  expression_ptr_t _subexpression;
};
////////////////////////////////////////////////////////////////////////
struct Sequence : public AstNode {
  Sequence(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  void do_visit(astvisitctx_ptr_t ctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
struct Group : public AstNode {
  Group(Parser* user_parser);
  void dump(dumpctx_ptr_t dctx) final;
  void do_visit(astvisitctx_ptr_t ctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
struct ParserRule : public AstNode {
  ParserRule(Parser* user_parser, std::string name = "");
  void dump(dumpctx_ptr_t dctx) final;
  void do_visit(astvisitctx_ptr_t ctx) final;
  matcher_ptr_t createMatcher(std::string named) final;
  expression_ptr_t _expression;
  ruleref_list_t _references;
  ruleref_list_t _referenced_by;
};
////////////////////////////////////////////////////////////////////////
} // namespace AST
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::peg
/////////////////////////////////////////////////////////////////////////////////////////////////
