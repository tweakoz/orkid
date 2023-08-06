////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/parser_peg.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::peg {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct PegImpl {

  PegImpl(Parser* user_parser);
  void loadPEGScannerRules();
  void loadPEGGrammar();

  void implementUserLanguage();

  AST::oneormore_ptr_t _onOOM(match_ptr_t match);
  AST::zeroormore_ptr_t _onZOM(match_ptr_t match);
  AST::select_ptr_t _onSEL(match_ptr_t match);
  AST::optional_ptr_t _onOPT(match_ptr_t match);
  AST::sequence_ptr_t _onSEQ(match_ptr_t match);
  AST::group_ptr_t _onGRP(match_ptr_t match);
  AST::expr_kwid_ptr_t _onEXPRKWID(match_ptr_t match);
  AST::expression_ptr_t _onExpression(match_ptr_t match, std::string named = "");

  match_ptr_t parseUserScannerSpec(std::string inp_string);
  match_ptr_t parseUserParserSpec(std::string inp_string);
  svar64_t findKWORID(std::string kworid);

  template <typename T, typename... A> std::shared_ptr<T> createAstNode(A&&... args);

  size_t indent = 0;
  scanner_ptr_t _user_scanner;
  Parser* _user_parser = nullptr;
  parser_ptr_t _peg_parser;
  matcher_ptr_t _rsi_scanner_matcher;
  matcher_ptr_t _rsi_parser_matcher;

  std::map<std::string, matcher_ptr_t> _user_matchers_by_name;

  std::vector<matcher_pair_t> _user_scanner_matchers_ordered;
  std::map<std::string, matcher_ptr_t> _user_parser_matchers_by_name;
  std::map<std::string, matcher_ptr_t> _user_scanner_matchers_by_name;

  std::set<AST::astnode_ptr_t> _retain_astnodes;

  std::map<std::string, AST::rule_ptr_t> _user_parser_rules;
  std::map<std::string, AST::rule_ptr_t> _user_scanner_rules_by_name;

  std::vector<AST::scanner_rule_pair_t> _user_scanner_rules_ordered;
  std::map<std::string, AST::scanner_macro_ptr_t> _user_scanner_macros;

  AST::rule_ptr_t _current_rule;
  std::vector<AST::astnode_ptr_t> _ast_buildstack;

  std::set<std::string> _parser_rule_names;
  std::set<std::string> _scanner_rule_names;
};

using pegimpl_ptr_t = std::shared_ptr<PegImpl>;

template <typename T, typename... A> std::shared_ptr<T> PegImpl::createAstNode(A&&... args){
  auto rval = std::make_shared<T>(std::forward<A>(args)...);
  _retain_astnodes.insert(rval);
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::peg
/////////////////////////////////////////////////////////////////////////////////////////////////
