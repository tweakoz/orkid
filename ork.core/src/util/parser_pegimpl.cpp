////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "parser_pegimpl.h"

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork {
match_ptr_t filtered_match(matcher_ptr_t matcher, match_ptr_t the_match);
/////////////////////////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_rulespec  = logger()->createChannel("PEGSPEC1", fvec3(0.5, 0.8, 0.5), true);
static logchannel_ptr_t logchan_rulespec2 = logger()->createChannel("PEGSPEC2", fvec3(0.5, 0.8, 0.5), true);

void Parser::onPre(const std::string& rule_name, matcher_notif_t fn) {

  auto it = _matchers_by_name.find(rule_name);
  if (it != _matchers_by_name.end()) {
    matcher_ptr_t matcher = it->second;
    matcher->_pre_notif       = fn;
    logchan_rulespec2->log(
        "IMPLEMENT rulenotif<%s> matcher<%p:%s> pre-notif assigned", rule_name.c_str(), (void*)matcher.get(), matcher->_name.c_str());
  } else {
    logerrchannel()->log("IMPLEMENT matcher<%s> not found", rule_name.c_str());
    OrkAssert(false);
  }
}

void Parser::onPost(const std::string& rule_name, matcher_notif_t fn) {

  auto it = _matchers_by_name.find(rule_name);
  if (it != _matchers_by_name.end()) {
    matcher_ptr_t matcher = it->second;
    matcher->_post_notif       = fn;
    logchan_rulespec2->log(
        "IMPLEMENT rulenotif<%s> matcher<%p:%s> post-notif assigned", rule_name.c_str(), (void*)matcher.get(), matcher->_name.c_str());
  } else {
    logerrchannel()->log("IMPLEMENT matcher<%s> not found", rule_name.c_str());
    OrkAssert(false);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
namespace peg {
namespace AST {

struct ScannerRule {
  std::string _name;
  std::string _regex;
};
struct ScannerMacro {
  std::string _name;
  std::string _regex;
};
struct DumpContext {
  size_t _indent = 0;
};

////////////////////////////////////////////////////////////////////////
AstNode::AstNode(Parser* user_parser)
    : _user_parser(user_parser) {
}
////////////////////////////////////////////////////////////////////////
AstNode::~AstNode() {
}
////////////////////////////////////////////////////////////////////////
void AstNode::visit(astnode_ptr_t node, astvisitctx_ptr_t visitctx) {
  visitctx->_visit_stack.push_back(node);
  visitctx->_visitor(node);
  node->do_visit(visitctx);
  visitctx->_visit_stack.pop_back();
}
////////////////////////////////////////////////////////////////////////
std::string AstNode::path() const {
  std::vector<const AstNode*> stack;
  const AstNode* node = this;
  while (node != nullptr) {
    stack.push_back(node);
    node = node->_parent.get();
  }
  std::reverse(stack.begin(), stack.end());
  std::string rval;
  for (auto item : stack) {
    rval += "/";
    rval += item->_name;
  }
  return rval;
}
////////////////////////////////////////////////////////////////////////
astnode_ptr_t AstNode::root() const {
  auto try_node      = _parent;
  astnode_ptr_t rval = try_node;
  while (try_node != nullptr) {
    try_node = rval->_parent;
    if (try_node)
      rval = try_node;
  }
  return rval;
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
Expression::Expression(Parser* user_parser, std::string name)
    : AstNode(user_parser) {

  _name = "PEGExpression";

  if (name != "") {
    _name = name;
  }
}
void Expression::dump(dumpctx_ptr_t dctx) { // final
  dctx->_indent++;
  auto indentstr = std::string(dctx->_indent * 2, ' ');
  // logchan_rulespec->log("%s EXPR(%s)", indentstr.c_str(), _name.c_str() );
  _expr_selected->dump(dctx);
  dctx->_indent--;
}
void Expression::do_visit(astvisitctx_ptr_t visitctx) { // final
  visit(_expr_selected, visitctx);
}
matcher_ptr_t Expression::createMatcher(std::string named) { // final
  return _expr_selected->createMatcher(named);
}
////////////////////////////////////////////////////////////////////////
ExprKWID::ExprKWID(Parser* user_parser)
    : AstNode(user_parser) {
  _name = "PEGExprKWID";
}
void ExprKWID::dump(dumpctx_ptr_t dctx) { // final
  dctx->_indent++;
  auto indentstr = std::string(dctx->_indent * 2, ' ');
  logchan_rulespec->log("%s EKWID(%s) _kwid<%s>", indentstr.c_str(), _name.c_str(), _kwid.c_str());
  dctx->_indent--;
}
void ExprKWID::do_visit(astvisitctx_ptr_t visitctx) { // final
  // visitctx(_expr_selected);
}
matcher_ptr_t ExprKWID::createMatcher(std::string named) { // final
  auto pth      = this->path();
  auto top_rule = std::dynamic_pointer_cast<ParserRule>(root());
  OrkAssert(top_rule != nullptr);
  auto pegimpl = _user_parser->_user.get<PegImpl*>();
  /////////////////////////////////////////////////////////
  logchan_rulespec2->log("CREATE EKWIDPXY(%s) kwid<%s> astnode<%p> toprule<%s>", named.c_str(), _kwid.c_str(), this, top_rule->_name.c_str() );
  /////////////////////////////////////////////////////////

  auto it_scanner = pegimpl->_user_scanner_matchers_by_name.find(_kwid);
  if(it_scanner!=pegimpl->_user_scanner_matchers_by_name.end()){
    auto submatcher = it_scanner->second;
    logchan_rulespec2->log("  from SCANNER matcher<%s:%p>", _kwid.c_str(), (void*) submatcher.get() );
    return submatcher;
  }

  auto it_parser = pegimpl->_user_parser->_matchers_by_name.find(_kwid);
  if(it_parser!=pegimpl->_user_parser->_matchers_by_name.end()){
    auto submatcher = it_parser->second;
    logchan_rulespec2->log("  from PARSER matcher<%s:%p>", _kwid.c_str(), (void*) submatcher.get() );
    return submatcher;
  }

  auto matcher = _user_parser->declare(_kwid);
  matcher->_info = FormatString("EKWIDPXY<%s>", _kwid.c_str());

  matcher->_on_link = [=]() ->bool {
    static int DEPTH = 0;
    /////////////////////////////////////////////////////////
    logchan_rulespec2->log("EKWIDPXY(%s) _kwid<%s> astnode<%p> top_rule<%s> ON-LINK", named.c_str(), _kwid.c_str(), this, top_rule->_name.c_str() );
    /////////////////////////////////////////////////////////
    matcher_ptr_t submatcher;
    auto it_matcher = pegimpl->_user_parser->_matchers_by_name.find(_kwid);
    if (it_matcher != pegimpl->_user_parser->_matchers_by_name.end()) {
      submatcher = it_matcher->second;
      logchan_rulespec2->log("  KWID SUBMATCHER<%p:%s>", (void*) submatcher.get(), submatcher->_name.c_str() );
    }
    else {
      logchan_rulespec2->log("  KWID NO SUBMATCHER" );
      return false;
    }
    if (submatcher->_attempt_match_fn == nullptr) {
      logchan_rulespec2->log(
          "  KWID submatcher<%s> NO SUBMATCHER MATCHFN", named.c_str(), _kwid.c_str(), submatcher->_name.c_str());
      return false;
    }
    _user_parser->_proxy(matcher,submatcher);
    //////////////////////////////////////////////////////////

    return true;
  };
  matcher->_uservars.set<AstNode*>("impl.astnode", this);
  return matcher;
}
////////////////////////////////////////////////////////////////////////
OneOrMore::OneOrMore(Parser* user_parser)
    : AstNode(user_parser) {
  _name = "PEGOneOrMore";
}
void OneOrMore::dump(dumpctx_ptr_t dctx) { // final
  dctx->_indent++;
  auto indentstr = std::string(dctx->_indent * 2, ' ');
  logchan_rulespec->log("%s OOM(%s)", indentstr.c_str(), _name.c_str());
  for (auto e : _subexpressions) {
    e->dump(dctx);
  }
  dctx->_indent--;
}
void OneOrMore::do_visit(astvisitctx_ptr_t visitctx) { // final
  for (auto e : _subexpressions) {
    visit(e, visitctx);
  }
}
matcher_ptr_t OneOrMore::createMatcher(std::string named) { // final
  auto expr0   = _subexpressions[0];
  auto matcher = _user_parser->oneOrMore(expr0->createMatcher(named));
  matcher->_uservars.set<AstNode*>("impl.astnode", this);
  return matcher;
}
////////////////////////////////////////////////////////////////////////
ZeroOrMore::ZeroOrMore(Parser* user_parser)
    : AstNode(user_parser) {
  _name = "PEGZeroOrMore";
}
void ZeroOrMore::dump(dumpctx_ptr_t dctx) { // final
  dctx->_indent++;
  auto indentstr = std::string(dctx->_indent * 2, ' ');
  logchan_rulespec->log("%s ZOM(%s)", indentstr.c_str(), _name.c_str());
  _subexpression->dump(dctx);
  dctx->_indent--;
}
void ZeroOrMore::do_visit(astvisitctx_ptr_t visitctx) { // final
  visit(_subexpression, visitctx);
}
matcher_ptr_t ZeroOrMore::createMatcher(std::string named) { // final
  auto matcher = _user_parser->zeroOrMore(_subexpression->createMatcher(named));
  matcher->_uservars.set<AstNode*>("impl.astnode", this);
  return matcher;
}
////////////////////////////////////////////////////////////////////////
Select::Select(Parser* user_parser)
    : AstNode(user_parser) {
  _name = "PEGSelect";
}
void Select::dump(dumpctx_ptr_t dctx) { // final
  dctx->_indent++;
  auto indentstr = std::string(dctx->_indent * 2, ' ');
  logchan_rulespec->log("%s SEL(%s)", indentstr.c_str(), _name.c_str());
  for (auto e : _subexpressions) {
    e->dump(dctx);
  }
  dctx->_indent--;
}
void Select::do_visit(astvisitctx_ptr_t visitctx) { // final
  for (auto e : _subexpressions) {
    visit(e, visitctx);
  }
}
matcher_ptr_t Select::createMatcher(std::string named) { // final
  std::vector<matcher_ptr_t> sub_matchers;
  for (auto subexp : _subexpressions) {
    sub_matchers.push_back(subexp->createMatcher(named));
  }
  return _user_parser->oneOf(sub_matchers);
}
////////////////////////////////////////////////////////////////////////
Optional::Optional(Parser* user_parser)
    : AstNode(user_parser) {
  _name = "PEGOptional";
}
void Optional::dump(dumpctx_ptr_t dctx) { // final
  dctx->_indent++;
  auto indentstr = std::string(dctx->_indent * 2, ' ');
  logchan_rulespec->log("%s OPT(%s)", indentstr.c_str(), _name.c_str());
  _subexpression->dump(dctx);
  dctx->_indent--;
}
void Optional::do_visit(astvisitctx_ptr_t visitctx) { // final
  visit(_subexpression, visitctx);
}
matcher_ptr_t Optional::createMatcher(std::string named) { // final
  auto subexp  = _subexpression->createMatcher(named);
  auto matcher = _user_parser->optional(subexp);
  matcher->_uservars.set<AstNode*>("impl.astnode", this);
  return matcher;
}
////////////////////////////////////////////////////////////////////////
Sequence::Sequence(Parser* user_parser)
    : AstNode(user_parser) {
  _name = "PEGSequence";
}
void Sequence::dump(dumpctx_ptr_t dctx) { // final
  dctx->_indent++;
  auto indentstr = std::string(dctx->_indent * 2, ' ');
  logchan_rulespec->log("%s SEQ(%s)", indentstr.c_str(), _name.c_str());
  for (auto e : _subexpressions) {
    e->dump(dctx);
  }
  dctx->_indent--;
}
void Sequence::do_visit(astvisitctx_ptr_t visitctx) { // final
  for (auto e : _subexpressions) {
    visit(e, visitctx);
  }
}
matcher_ptr_t Sequence::createMatcher(std::string named) { // final
  std::vector<matcher_ptr_t> sub_matchers;
  for (auto subexp : _subexpressions) {
    sub_matchers.push_back(subexp->createMatcher(named));
  }
  auto matcher = _user_parser->sequence(sub_matchers);
  matcher->_uservars.set<AstNode*>("impl.astnode", this);
  return matcher;
}
////////////////////////////////////////////////////////////////////////
Group::Group(Parser* user_parser)
    : AstNode(user_parser) {
  _name = "PEGGroup";
}
void Group::dump(dumpctx_ptr_t dctx) { // final
  dctx->_indent++;
  auto indentstr = std::string(dctx->_indent * 2, ' ');
  logchan_rulespec->log("%s GRP(%s)", indentstr.c_str(), _name.c_str());
  for (auto e : _subexpressions) {
    e->dump(dctx);
  }
  dctx->_indent--;
}
void Group::do_visit(astvisitctx_ptr_t visitctx) { // final
  for (auto e : _subexpressions) {
    visit(e, visitctx);
  }
}
matcher_ptr_t Group::createMatcher(std::string named) { // final
  std::vector<matcher_ptr_t> sub_matchers;
  for (auto subexp : _subexpressions) {
    sub_matchers.push_back(subexp->createMatcher(named));
  }
  auto matcher = _user_parser->group(sub_matchers);
  matcher->_uservars.set<AstNode*>("impl.astnode", this);
  return matcher;
}
////////////////////////////////////////////////////////////////////////
ParserRule::ParserRule(Parser* user_parser, std::string name)
    : AstNode(user_parser) {
  _name = "PEGParserRule";
  if (name != "") {
    _name = name;
  }
}
void ParserRule::dump(dumpctx_ptr_t dctx) { // final
  _expression->dump(dctx);
}
void ParserRule::do_visit(astvisitctx_ptr_t visitctx) { // final
  visit(_expression, visitctx);
}
matcher_ptr_t ParserRule::createMatcher(std::string named) { // final
  return _expression->createMatcher(named);
}
////////////////////////////////////////////////////////////////////////
} // namespace AST

/////////////////////////////////////////////////////////////////////////////////////////////////

static constexpr const char* block_regex = "(function|yo|xxx)";

PegImpl::PegImpl() {
  _peg_parser        = std::make_shared<Parser>();
  _peg_parser->_DEBUG_MATCH = true;
  _peg_parser->_DEBUG_INFO = true;
  _peg_parser->_name = "gramr";
  loadPEGScannerRules();
  loadPEGGrammar();
}
/////////////////////////////////////////////////////////
void PegImpl::loadPEGScannerRules() { //
  try {
    auto dsl_scanner = _peg_parser->_scanner;
    dsl_scanner->addEnumClass("\\s+", TokenClass::WHITESPACE);
    dsl_scanner->addEnumClass("[\\n\\r]+", TokenClass::NEWLINE);
    dsl_scanner->addEnumClass("[a-zA-Z_][a-zA-Z0-9_]*", TokenClass::KW_OR_ID);
    dsl_scanner->addEnumClass("=", TokenClass::EQUALS);
    dsl_scanner->addEnumClass(",", TokenClass::COMMA);
    dsl_scanner->addEnumClass(":", TokenClass::COLON);
    dsl_scanner->addEnumClass(";", TokenClass::SEMICOLON);
    dsl_scanner->addEnumClass("\\(", TokenClass::L_PAREN);
    dsl_scanner->addEnumClass("\\)", TokenClass::R_PAREN);
    dsl_scanner->addEnumClass("\\[", TokenClass::L_SQUARE);
    dsl_scanner->addEnumClass("\\]", TokenClass::R_SQUARE);
    dsl_scanner->addEnumClass("\\{", TokenClass::L_CURLY);
    dsl_scanner->addEnumClass("\\}", TokenClass::R_CURLY);
    dsl_scanner->addEnumClass("\\*", TokenClass::STAR);
    dsl_scanner->addEnumClass("\\+", TokenClass::PLUS);
    dsl_scanner->addEnumClass("\\-", TokenClass::MINUS);
    dsl_scanner->addEnumClass("\\|", TokenClass::PIPE);
    dsl_scanner->addEnumClass("-?(\\d+)", TokenClass::INTEGER);
    // dsl_scanner->addMacro("ESCAPED_CHAR", "\\\\[\"\\\\]");
    // dsl_scanner->addMacro("ANY_ESCAPED", "\\\\.");
    // dsl_scanner->addMacro("ASCII", "[\\\\x00-\\\\x21\\\\x23-\\\\x5B\\\\x5D-\\\\x7F]");
    // dsl_scanner->addMacro("ASCII_WITHOUT_DBLQUOTE", "[\\\\x00-\\\\x21\\\\x23-\\\\x7F]");
    dsl_scanner->addEnumClass(R"(\"[^\"]*\")", TokenClass::QUOTED_REGEX);
    dsl_scanner->addEnumClass("<-", TokenClass::LEFT_ARROW);
    logchan_rulespec->log("Building state machine");
    dsl_scanner->buildStateMachine();
    logchan_rulespec->log("done...");
  } catch (std::exception& e) {
    logchan_rulespec->log("EXCEPTION<%s>", e.what());
    OrkAssert(false);
  }
}
size_t indent = 0;
/////////////////////////////////////////////////////////
AST::oneormore_ptr_t PegImpl::_onOOM(match_ptr_t match) {
  auto indentstr = std::string(indent * 2, ' ');
  // our output AST node
  auto oom_out     = std::make_shared<AST::OneOrMore>(_user_parser);
  oom_out->_parent = _ast_buildstack.back();
  _ast_buildstack.push_back(oom_out);

  // our parser DSL input node (containing user language spec)
  //  in this case it is just match
  // rule_oom <- sequence({oom, lcurly, rule_expression, rcurly}, "rule_oom");
  auto oom_inp = match;
  logchan_rulespec->log("%s_onOOM<%s>", indentstr.c_str(), oom_inp->_matcher->_name.c_str());
  auto sub = _onExpression(oom_inp);
  oom_out->_subexpressions.push_back(sub);
  _retain_astnodes.insert(oom_out);
  _ast_buildstack.pop_back();
  return oom_out;
}
/////////////////////////////////////////////////////////
AST::zeroormore_ptr_t PegImpl::_onZOM(match_ptr_t match) {
  auto indentstr = std::string(indent * 2, ' ');
  // our output AST node
  auto zom_out     = std::make_shared<AST::ZeroOrMore>(_user_parser);
  zom_out->_parent = _ast_buildstack.back();
  _ast_buildstack.push_back(zom_out);
  // our parser DSL input node (containing user language spec)
  auto nom_inp = match->asShared<NOrMore>();
  OrkAssert(nom_inp->_minmatches == 0);
  logchan_rulespec->log("%s_onZOM<%s>", indentstr.c_str(), match->_matcher->_name.c_str());
  OrkAssert(nom_inp->_items.size() == 1);
  auto sub_item = nom_inp->_items[0];
  logchan_rulespec->log("%s zom subitem<%s>", indentstr.c_str(), sub_item->_matcher->_name.c_str());
  auto subexpr_out        = _onExpression(sub_item);
  zom_out->_subexpression = subexpr_out;
  _retain_astnodes.insert(zom_out);
  _ast_buildstack.pop_back();
  return zom_out;
}
/////////////////////////////////////////////////////////
AST::select_ptr_t PegImpl::_onSEL(match_ptr_t match) {
  auto indentstr = std::string(indent * 2, ' ');
  // our output AST node
  auto sel_out     = std::make_shared<AST::Select>(_user_parser);
  sel_out->_parent = _ast_buildstack.back();
  _ast_buildstack.push_back(sel_out);
  // our parser DSL input node (containing user language spec)
  auto nom_inp = match->asShared<NOrMore>();
  logchan_rulespec->log("%s _onSEL<%s> nom_inp<%p>", indentstr.c_str(), match->_matcher->_name.c_str(), (void*)nom_inp.get());
  // for each user language spec subexpression
  //   add an AST subexpression
  for (auto sub_item : nom_inp->_items) {
    logchan_rulespec->log("%s sel subitem<%s>", indentstr.c_str(), sub_item->_matcher->_name.c_str());
    auto subexpr_out = _onExpression(sub_item);
    sel_out->_subexpressions.push_back(subexpr_out);
  }
  _retain_astnodes.insert(sel_out);
  _ast_buildstack.pop_back();
  return sel_out;
}
/////////////////////////////////////////////////////////
AST::optional_ptr_t PegImpl::_onOPT(match_ptr_t match) {
  auto indentstr = std::string(indent * 2, ' ');
  // our output AST node
  auto opt_out     = std::make_shared<AST::Optional>(_user_parser);
  opt_out->_parent = _ast_buildstack.back();
  _ast_buildstack.push_back(opt_out);
  // our parser DSL input node (containing user language spec)
  //  in this case it is just match
  // rule_opt <- sequence({oom, lcurly, rule_expression, rcurly}, "rule_oom");
  auto opt_inp = match;
  logchan_rulespec->log("%s_onOPT<%s>", indentstr.c_str(), match->_matcher->_name.c_str());
  opt_out->_subexpression = _onExpression(opt_inp);
  _retain_astnodes.insert(opt_out);
  _ast_buildstack.pop_back();
  return opt_out;
}
/////////////////////////////////////////////////////////
AST::sequence_ptr_t PegImpl::_onSEQ(match_ptr_t match) {
  auto seq_out     = std::make_shared<AST::Sequence>(_user_parser);
  seq_out->_parent = _ast_buildstack.back();
  _ast_buildstack.push_back(seq_out);
  auto nom = match->asShared<NOrMore>();
  OrkAssert(nom->_minmatches == 0);
  OrkAssert(nom->_items.size() >= 1);
  auto indentstr = std::string(indent * 2, ' ');
  logchan_rulespec->log("%s_onSEQ<%s>", indentstr.c_str(), match->_matcher->_name.c_str());
  for (auto sub_item : nom->_items) {
    logchan_rulespec->log("%s seq subitem<%s>", indentstr.c_str(), sub_item->_matcher->_name.c_str());
    auto subexpr = _onExpression(sub_item);
    seq_out->_subexpressions.push_back(subexpr);
  }
  _retain_astnodes.insert(seq_out);
  _ast_buildstack.pop_back();
  return seq_out;
}
/////////////////////////////////////////////////////////
AST::group_ptr_t PegImpl::_onGRP(match_ptr_t match) {
  auto grp_out     = std::make_shared<AST::Group>(_user_parser);
  grp_out->_parent = _ast_buildstack.back();
  _ast_buildstack.push_back(grp_out);
  auto nom = match->asShared<NOrMore>();
  OrkAssert(nom->_minmatches == 0);
  OrkAssert(nom->_items.size() >= 1);
  auto indentstr = std::string(indent * 2, ' ');
  logchan_rulespec->log("%s_onGRP<%s>", indentstr.c_str(), match->_matcher->_name.c_str());
  for (auto item : nom->_items) {
    logchan_rulespec->log("%s grp subitem<%s>", indentstr.c_str(), item->_matcher->_name.c_str());
    auto subexpr = _onExpression(item);
    grp_out->_subexpressions.push_back(subexpr);
  }
  _retain_astnodes.insert(grp_out);
  _ast_buildstack.pop_back();
  return grp_out;
}
/////////////////////////////////////////////////////////
AST::expr_kwid_ptr_t PegImpl::_onEXPRKWID(match_ptr_t match) {
  auto kwid_out     = std::make_shared<AST::ExprKWID>(_user_parser);
  kwid_out->_parent = _ast_buildstack.back();
  _ast_buildstack.push_back(kwid_out);

  auto indentstr  = std::string(indent * 2, ' ');
  auto classmatch = match->asShared<ClassMatch>();
  kwid_out->_kwid = classmatch->_token->text;
  logchan_rulespec->log("%s_onEXPRKWID<%s> KWID<%s>", indentstr.c_str(), match->_matcher->_name.c_str(), kwid_out->_kwid.c_str());
  _retain_astnodes.insert(kwid_out);
  _ast_buildstack.pop_back();
  return kwid_out;
}
/////////////////////////////////////////////////////////
AST::expression_ptr_t PegImpl::_onExpression(match_ptr_t match, std::string named) {
  auto expr_out     = std::make_shared<AST::Expression>(_user_parser);
  expr_out->_parent = _ast_buildstack.back();
  _ast_buildstack.push_back(expr_out);
  auto indentstr = std::string(indent * 2, ' ');

  indent++;
  auto expression_seq  = match->asShared<Sequence>();
  auto expression_sel  = expression_seq->itemAsShared<OneOf>(0)->_selected;
  auto expression_name = expression_seq->itemAsShared<Optional>(1)->_subitem;

  size_t expression_len = expression_seq->_items.size();
  if (expression_name) {
    expression_name = expression_name->asShared<Sequence>()->_items[1];
    auto xname      = expression_name->asShared<ClassMatch>()->_token->text;
    logchan_rulespec->log(
        "%s_onExpression<%s> len%zu>  named<%s>", indentstr.c_str(), match->_matcher->_name.c_str(), expression_len, xname.c_str());
  } else {
    logchan_rulespec->log("%s_onExpression<%s> len%zu>", indentstr.c_str(), match->_matcher->_name.c_str(), expression_len);
  }

  if (expression_sel) {
    if (auto as_seq = expression_sel->tryAsShared<Sequence>()) {
      auto subseq       = as_seq.value();
      auto subseq0      = subseq->_items[0];
      auto expr_matcher = expression_sel->_matcher;
      ////////////////////////////////////////////////////////////////////////////
      // zom, oom, sel, opt
      ////////////////////////////////////////////////////////////////////////////
      if (auto as_wordmatch = subseq0->tryAsShared<WordMatch>()) {
        auto wordmatch = as_wordmatch.value();
        auto word      = wordmatch->_token->text;
        if (word == "zom") {
          auto zom                 = subseq->_items[2];
          expr_out->_expr_selected = _onZOM(zom);
        } else if (word == "oom") {
          auto oom                 = subseq->_items[2];
          expr_out->_expr_selected = _onOOM(oom);
        } else if (word == "sel") {
          auto sel                 = subseq->_items[2];
          expr_out->_expr_selected = _onSEL(sel);
        } else if (word == "opt") {
          auto opt                 = subseq->_items[2];
          expr_out->_expr_selected = _onOPT(opt);
        } else {
          OrkAssert(false);
        }
      } // wordmatch ?
      ////////////////////////////////////////////////////////////////////////////
      // seq, grp
      ////////////////////////////////////////////////////////////////////////////
      else if (auto as_classmatch = subseq0->tryAsShared<ClassMatch>()) {
        auto classmatch = as_classmatch.value();
        auto token      = classmatch->_token;
        if (classmatch->_tokclass == uint64_t(TokenClass::L_SQUARE)) {
          expr_out->_expr_selected = _onSEQ(subseq->_items[1]);
        } else if (classmatch->_tokclass == uint64_t(TokenClass::L_PAREN)) {
          expr_out->_expr_selected = _onGRP(subseq->_items[1]);
        } else {
          OrkAssert(false);
        }
      } // classmatch ?
      else {
        logchan_rulespec->log("ERR<subseq0 not wordmatch or classmatch> view start<%zu>", expression_sel->_view->_start);
        OrkAssert(false);
      }
    }
    ////////////////////////////////////////////////////////////////////////////
    // single keyword or identifier
    ////////////////////////////////////////////////////////////////////////////
    else { // kworid
      OrkAssert(expression_sel->isShared<ClassMatch>());
      auto classmatch          = expression_sel->asShared<ClassMatch>();
      expr_out->_expr_selected = _onEXPRKWID(expression_sel);
    }
  }
  indent--;
  OrkAssert(expr_out->_expr_selected != nullptr);
  _retain_astnodes.insert(expr_out);
  _ast_buildstack.pop_back();
  return expr_out;
}
/////////////////////////////////////////////////////////
void PegImpl::loadPEGGrammar() { //
  logchan_rulespec->log("Loading Grammar");
  ////////////////////
  // primitives
  ////////////////////
  auto plus         = _peg_parser->matcherForTokenClass(TokenClass::PLUS, "plus");
  auto minus        = _peg_parser->matcherForTokenClass(TokenClass::MINUS, "minus");
  auto star         = _peg_parser->matcherForTokenClass(TokenClass::STAR, "star");
  auto equals       = _peg_parser->matcherForTokenClass(TokenClass::EQUALS, "equals");
  auto colon        = _peg_parser->matcherForTokenClass(TokenClass::COLON, "colon");
  auto semicolon    = _peg_parser->matcherForTokenClass(TokenClass::SEMICOLON, "semicolon");
  auto comma        = _peg_parser->matcherForTokenClass(TokenClass::COMMA, "comma");
  auto lparen       = _peg_parser->matcherForTokenClass(TokenClass::L_PAREN, "lparen");
  auto rparen       = _peg_parser->matcherForTokenClass(TokenClass::R_PAREN, "rparen");
  auto lsquare      = _peg_parser->matcherForTokenClass(TokenClass::L_SQUARE, "lsquare");
  auto rsquare      = _peg_parser->matcherForTokenClass(TokenClass::R_SQUARE, "rsquare");
  auto lcurly       = _peg_parser->matcherForTokenClass(TokenClass::L_CURLY, "lcurly");
  auto rcurly       = _peg_parser->matcherForTokenClass(TokenClass::R_CURLY, "rcurly");
  auto pipe         = _peg_parser->matcherForTokenClass(TokenClass::PIPE, "pipe");
  auto inttok       = _peg_parser->matcherForTokenClass(TokenClass::INTEGER, "int");
  auto kworid       = _peg_parser->matcherForTokenClass(TokenClass::KW_OR_ID, "kw_or_id");
  auto left_arrow   = _peg_parser->matcherForTokenClass(TokenClass::LEFT_ARROW, "left_arrow");
  auto quoted_regex = _peg_parser->matcherForTokenClass(TokenClass::QUOTED_REGEX, "quoted_regex");
  ////////////////////
  auto sel   = _peg_parser->matcherForWord("sel");
  auto zom   = _peg_parser->matcherForWord("zom");
  auto oom   = _peg_parser->matcherForWord("oom");
  auto opt   = _peg_parser->matcherForWord("opt");
  auto macro = _peg_parser->matcherForWord("macro");
  /////////////////////////////////////////////////////////////////////////////////////////////////////
  // user scanner rules
  /////////////////////////////////////////////////////////////////////////////////////////////////////
  auto macro_item      = _peg_parser->sequence({macro, lparen, kworid, rparen}, "macro_item");
  auto scanner_key     = _peg_parser->oneOf({macro_item, kworid}, "scanner_key");
  auto scanner_rule    = _peg_parser->sequence({scanner_key, left_arrow, quoted_regex}, "scanner_rule");
  scanner_rule->_post_notif = [=](match_ptr_t match) {
    auto seq           = match->asShared<Sequence>();
    auto rule_key_item = seq->_items[0]->asShared<OneOf>()->_selected;
    auto qrx           = seq->_items[2]->asShared<ClassMatch>()->_token->text;
    auto rx            = qrx.substr(1, qrx.size() - 2); // remove surrounding quotes
    if (auto as_classmatch = rule_key_item->tryAsShared<ClassMatch>()) {
      auto match_str = as_classmatch.value()->_token->text;
      auto rule_name = match_str; // + "_scrule";
      auto rule      = std::make_shared<AST::ScannerRule>();
      rule->_name    = rule_name;
      rule->_regex   = rx;
      auto item = std::pair(rule_name, rule);
      this->_user_scanner_rules_ordered.push_back(item);
      logchan_rulespec2->log(
          "RECORD SCANNER->PARSER RULE<%s> literal<%s>", //
          rule_name.c_str(),                             //
          match_str.c_str());

    } else if (auto as_seq = rule_key_item->tryAsShared<Sequence>()) {
      auto sub_seq   = as_seq.value();
      auto macro_str = sub_seq->_items[0]->asShared<WordMatch>()->_token->text;
      OrkAssert(macro_str == "macro");
      auto macro_name = sub_seq->_items[2]->asShared<ClassMatch>()->_token->text;
      auto macro      = std::make_shared<AST::ScannerMacro>();
      macro->_name    = macro_name;
      macro->_regex   = rx;
      auto it         = this->_user_scanner_macros.find(macro_name);
      OrkAssert(it == this->_user_scanner_macros.end());
      logchan_rulespec2->log("ADDING SCANNER->PARSER MACRO literal<%s> <- %s", macro_name.c_str(), rx.c_str());
      this->_user_scanner_macros[macro_name] = macro;
    } else {
      OrkAssert(false);
    }
  };
  _rsi_scanner_matcher         = _peg_parser->zeroOrMore(scanner_rule, "scanner_rules");
  _rsi_scanner_matcher->_post_notif = [=](match_ptr_t match) {
    std::string _current_rule_name = "";
    try {
      for (auto item : this->_user_scanner_macros) {
        auto macro         = item.second;
        _current_rule_name = macro->_name;
        logchan_rulespec2->log("IMPLEMENT SCANNER MACRO <%s : %s>", macro->_name.c_str(), macro->_regex.c_str());
        this->_user_scanner->addMacro(macro->_name, macro->_regex);
      }
      for (auto item : this->_user_scanner_rules_ordered) {
        auto rule          = item.second;
        uint64_t crc_id    = CrcString(rule->_name.c_str()).hashed();
        _current_rule_name = rule->_name;
        this->_user_scanner->addEnumClass(rule->_regex, crc_id);
        logchan_rulespec2->log(
            "IMPLEMENT SCANNER EnumClass<%s : %zu> regex \"%s\" ", //
            rule->_name.c_str(),
            crc_id,
            rule->_regex.c_str());
      }
      this->_user_scanner->buildStateMachine();
    } catch (std::exception& e) {
      logchan_rulespec2->log("EXCEPTION cur_rule<%s>  cause<%s>", _current_rule_name.c_str(), e.what());
      OrkAssert(false);
    }
  };
  /////////////////////////////////////////////////////////////////////////////////////////////////////
  // parser rules
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  auto rule_expression = _peg_parser->declare("rule_expression");
  //
  //
  auto rule_zom      = _peg_parser->sequence({zom, lcurly, _peg_parser->zeroOrMore(rule_expression), rcurly}, "rule_zom");
  auto rule_oom      = _peg_parser->sequence({oom, lcurly, rule_expression, rcurly}, "rule_oom");
  auto rule_sel      = _peg_parser->sequence({sel, lcurly, _peg_parser->oneOrMore(rule_expression), rcurly}, "rule_sel");
  auto rule_opt      = _peg_parser->sequence({opt, lcurly, rule_expression, rcurly}, "rule_opt");
  auto rule_sequence = _peg_parser->sequence({lsquare, _peg_parser->zeroOrMore(rule_expression), rsquare}, "rule_sequence");
  auto rule_grp      = _peg_parser->sequence({lparen, _peg_parser->zeroOrMore(rule_expression), rparen}, "rule_grp");
  //
  _peg_parser->_sequence(
      rule_expression,
      {
          _peg_parser->oneOf({
              // load previously declared rule_expression
              rule_zom,
              rule_oom,
              rule_sel,
              rule_opt,
              rule_sequence,
              rule_grp,
              kworid,
          }),
          _peg_parser->optional(_peg_parser->sequence({colon, quoted_regex}), "expr_name"),
      });
  auto parser_rule = _peg_parser->sequence({kworid, left_arrow, rule_expression}, "parser_rule");

  parser_rule->_post_notif = [=](match_ptr_t match) {
    auto rulename = match->asShared<Sequence>()->_items[0]->asShared<ClassMatch>()->_token->text;
    auto ast_rule = std::make_shared<AST::ParserRule>(_user_parser, rulename);
    _current_rule = ast_rule;
    _ast_buildstack.push_back(ast_rule);
    auto expr_ast_node = _onExpression(match->asShared<Sequence>()->_items[2], rulename);
    _ast_buildstack.pop_back();
    ast_rule->_expression        = expr_ast_node;
    _user_parser_rules[rulename] = ast_rule;
    printf("CREATED AST-RULE<%s>\n", rulename.c_str());
  };

  _rsi_parser_matcher = _peg_parser->zeroOrMore(parser_rule, "parser_rules");

  _rsi_parser_matcher->_post_notif = [=](match_ptr_t match) { printf("MATCHED parser_rules\n"); };
  _peg_parser->link();
}
/////////////////////////////////////////////////////////
match_ptr_t PegImpl::parseUserScannerSpec(std::string inp_string) {
  auto peg_scanner = _peg_parser->_scanner;
  try {
    peg_scanner->clear();
    peg_scanner->scanString(inp_string);
    peg_scanner->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
    peg_scanner->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));
  } catch (std::exception& e) {
    logerrchannel()->log("EXCEPTION<%s>", e.what());
    OrkAssert(false);
  }
  auto top_view = peg_scanner->createTopView();
  top_view.dump("top_view");
  auto slv   = std::make_shared<ScannerLightView>(top_view);
  auto match = _peg_parser->match(_rsi_scanner_matcher, slv);
  OrkAssert(match);
  OrkAssert(match->_view->_start == top_view._start);
  OrkAssert(match->_view->_end == top_view._end);
  return match;
}
/////////////////////////////////////////////////////////
match_ptr_t PegImpl::parseUserParserSpec(std::string inp_string) {
  auto peg_scanner = _peg_parser->_scanner;
  /////////////////////////////////////////////////
  // add user scanner matchers to user matcher
  /////////////////////////////////////////////////
  for (auto item : this->_user_scanner_rules_ordered) {
    auto scanner_rule       = item.second;
    uint64_t crc_id = CrcString(scanner_rule->_name.c_str()).hashed();
    auto tcname     = scanner_rule->_name;
    _scanner_rule_names.insert(tcname);
    auto matcher = _user_parser->matcherForTokenClass(crc_id, tcname);
    auto it      = _user_matchers_by_name.find(tcname);
    OrkAssert(it == _user_matchers_by_name.end());
    logchan_rulespec2->log(
        "IMPLEMENT matcherForScannerTokClass RULE<%s> matcher<%p:%s>",
        scanner_rule->_name.c_str(),
        (void*)matcher.get(),
        matcher->_name.c_str());
    _user_matchers_by_name[tcname] = matcher;
    auto out_item                  = std::pair(tcname, matcher);
    _user_scanner_matchers_ordered.push_back(out_item);
    _user_scanner_matchers_by_name[tcname] = matcher;
  }
  /////////////////////////////////////////////////
  // prepare parser-DSL scanner and scan parser-DSL
  /////////////////////////////////////////////////
  try {
    peg_scanner->clear();
    peg_scanner->scanString(inp_string);
    peg_scanner->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
    peg_scanner->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));
  } catch (std::exception& e) {
    logchan_rulespec->log("EXCEPTION<%s>", e.what());
    OrkAssert(false);
  }
  /////////////////////////////////////////////////
  // parse parser-DSL
  /////////////////////////////////////////////////
  auto top_view = peg_scanner->createTopView();
  top_view.dump("top_view");
  auto slv   = std::make_shared<ScannerLightView>(top_view);
  auto match = _peg_parser->match(_rsi_parser_matcher, slv);
  OrkAssert(match);
  OrkAssert(match->_view->_start == top_view._start);
  this->implementUserLanguage();
  /////////////////////////////////////////////////
  if (match->_view->_end != top_view._end) {
    logerrchannel()->log("Parser :: RULESPEC :: SYNTAX ERROR");
    logerrchannel()->log("  input text num tokens<%zu>", top_view._end);
    logerrchannel()->log("  parse cursor<%zu>", match->_view->_end);
    auto token = peg_scanner->token(match->_view->_end);
    logerrchannel()->log("  parse token<%s> line<%d>", token->text.c_str(), token->iline + 1);
    OrkAssert(false);
  }
  return match;
}
/////////////////////////////////////////////////////////

void PegImpl::attachUser(Parser* user_parser) {
  _user_parser  = user_parser;
  _user_scanner = user_parser->_scanner;
  _user_parser->_user.set<PegImpl*>(this);
}

/////////////////////////////////////////////////////////

svar64_t PegImpl::findKWORID(std::string kworid) {
  return svar64_t();
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void PegImpl::implementUserLanguage() {
  /////////////////////////////////////////////////////////
  // dump matcher phase
  /////////////////////////////////////////////////////////
  if (1) {
    logchan_rulespec2->log("///////////////////////////////////////////////////////////");
    logchan_rulespec2->log("// DUMPING USER MATCHERS");
    logchan_rulespec2->log("///////////////////////////////////////////////////////////");
    for (auto mitem : _user_parser->_matchers_by_name) {
      auto matcher_key = mitem.first;
      auto matcher     = mitem.second;
      logchan_rulespec2->log(
          "// DUMP USER MATCHER KEY<%s> NAME<%s> _info<%s>", matcher_key.c_str(), matcher->_name.c_str(), matcher->_info.c_str());
    }
  }
  /////////////////////////////////////////////////////////
  // dump ast rule phase
  /////////////////////////////////////////////////////////
  if (0) {
    logchan_rulespec->log("///////////////////////////////////////////////////////////");
    logchan_rulespec->log("// DUMPING USER AST-RULES..");
    logchan_rulespec->log("///////////////////////////////////////////////////////////");
    for (auto rule : _user_parser_rules) {
      auto rule_name = rule.first;
      _parser_rule_names.insert(rule_name);
      auto ast_rule = rule.second;
      logchan_rulespec->log("// DUMP USER PARSER RULE<%s>", rule_name.c_str());
      auto dctx = std::make_shared<AST::DumpContext>();
      ast_rule->dump(dctx);
    }
  }
  /////////////////////////////////////////////////////////
  // visit phase
  /////////////////////////////////////////////////////////
  if (1) {
    logchan_rulespec2->log("///////////////////////////////////////////////////////////");
    logchan_rulespec2->log("// LINKING USER AST-RULES..");
    logchan_rulespec2->log("///////////////////////////////////////////////////////////");
    auto visit_ctx      = std::make_shared<AST::VisitContext>();
    visit_ctx->_visitor = [this, visit_ctx](AST::astnode_ptr_t the_node) {
      auto top_rule      = visit_ctx->_top_rule;
      auto top_rule_name = top_rule->_name;
      size_t stacklen    = visit_ctx->_visit_stack.size();
      auto indent        = std::string(stacklen * 2, ' ');
      if (auto as_rule = std::dynamic_pointer_cast<AST::ParserRule>(the_node)) {
        if (as_rule != top_rule) {
          auto rule_name = as_rule->_name;
          logchan_rulespec2->log("nodesubrule<%s>\n", rule_name.c_str());
        }
      } else if (auto as_kwid = std::dynamic_pointer_cast<AST::ExprKWID>(the_node)) {
        auto top_rule_name = top_rule->_name;
        logchan_rulespec2->log("%s kwid<%p:%s>", indent.c_str(), (void*)as_kwid.get(), as_kwid->_kwid.c_str());
        auto it = _user_parser_rules.find(as_kwid->_kwid);
        if (it != _user_parser_rules.end()) {
          auto rule = it->second;
          logchan_rulespec2->log(" subrule<%s>", rule->_name.c_str());

          auto reference              = std::make_shared<AST::RuleRef>();
          reference->_referenced_rule = rule;
          reference->_node            = the_node;
          top_rule->_references.push_back(reference);

          reference                   = std::make_shared<AST::RuleRef>();
          reference->_referenced_rule = top_rule;
          reference->_node            = the_node;
          rule->_referenced_by.push_back(reference);
        }
        logchan_rulespec2->log("\n");
      } else {
        logchan_rulespec2->log("%s node<%p:%s>\n", indent.c_str(), (void*)the_node.get(), the_node->_name.c_str());
      }
    };

    for (auto rule_item : _user_parser_rules) {
      auto rule            = rule_item.second;
      visit_ctx->_top_rule = rule;
      logchan_rulespec2->log("/////////// VISITING RULE<%p:%s> ///////////\n", (void*)rule.get(), rule->_name.c_str());
      AST::AstNode::visit(rule, visit_ctx);
      // rule->_expression->dump();
    }
  }
  /////////////////////////////////////////////////////////
  // visit phase
  /////////////////////////////////////////////////////////
  if (1) {
    logchan_rulespec2->log("///////////////////////////////////////////////////////////");
    logchan_rulespec2->log("// DUMP RULE REFERENCED_BY LIST..");
    logchan_rulespec2->log("///////////////////////////////////////////////////////////");
    for (auto rule_item : _user_parser_rules) {
      auto rule = rule_item.second;
      logchan_rulespec2->log("rule<%s> referenced by [\n", rule->_name.c_str());
      for (auto ref : rule->_referenced_by) {
        auto rule = ref->_referenced_rule;
        auto node = ref->_node;
        logchan_rulespec2->log("  rule(%s) : node(%p)\n", rule->_name.c_str(), (void*)node.get());
      }
      logchan_rulespec2->log("]\n");
    }
  }
  /////////////////////////////////////////////////////////
  // visit phase
  /////////////////////////////////////////////////////////
  if (1) {
    logchan_rulespec2->log("///////////////////////////////////////////////////////////");
    logchan_rulespec2->log("// DUMP RULE REFERENCES LIST..");
    logchan_rulespec2->log("///////////////////////////////////////////////////////////");
    for (auto rule_item : _user_parser_rules) {
      auto rule = rule_item.second;
      logchan_rulespec2->log("rule<%s> references [\n", rule->_name.c_str());
      for (auto ref : rule->_references) {
        auto rule = ref->_referenced_rule;
        auto node = ref->_node;
        logchan_rulespec2->log("  rule(%s) : node(%p)\n", rule->_name.c_str(), (void*)node.get());
      }
      logchan_rulespec2->log("]\n");
    }
  }
  /////////////////////////////////////////////////////////
  // implement phase
  /////////////////////////////////////////////////////////
  if (1) {
    logchan_rulespec2->log("///////////////////////////////////////////////////////////");
    logchan_rulespec2->log("// IMPLEMENTING USER AST-RULES..");
    logchan_rulespec2->log("///////////////////////////////////////////////////////////");
    for (auto rule : _user_parser_rules) {
      auto rule_name = rule.first;
      auto ast_rule  = rule.second;
      auto matcher   = ast_rule->createMatcher(rule_name);
      ast_rule->_matcher = matcher;
      logchan_rulespec2->log(
          "// IMPLEMENT AST-RULE<%s> matcher<%p:%s>", rule_name.c_str(), (void*)matcher.get(), matcher->_name.c_str());
      this->_user_parser->_matchers_by_name[rule_name] = matcher;
      auto it                                          = _user_matchers_by_name.find(rule_name);
      OrkAssert(it == _user_matchers_by_name.end());
      _user_matchers_by_name[rule_name]        = matcher;
      _user_parser_matchers_by_name[rule_name] = matcher;
    }
  }
  /////////////////////////////////////////////////////////
  // link phase
  /////////////////////////////////////////////////////////
  logchan_rulespec2->log("///////////////////////////////////////////////////////////");
  logchan_rulespec2->log("// LINKING USER PARSER");
  logchan_rulespec2->log("///////////////////////////////////////////////////////////");
  _user_parser->link();
  logchan_rulespec2->log("///////////////////////////////////////////////////////////");
}

/////////////////////////////////////////////////////////////////////////////////////////////////

pegimpl_ptr_t getIMPL() {
  static auto the_peg = std::make_shared<PegImpl>();
  return the_peg;
}

} // namespace peg

/////////////////////////////////////////////////////////////////////////////////////////////////

match_ptr_t Parser::loadPEGScannerSpec(const std::string& spec) {
  auto the_peg = peg::getIMPL();
  the_peg->attachUser(this);
  auto match = the_peg->parseUserScannerSpec(spec);
  return match;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

match_ptr_t Parser::loadPEGParserSpec(const std::string& spec) {
  auto the_peg = peg::getIMPL();
  auto match   = the_peg->parseUserParserSpec(spec);
  return match;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork
/////////////////////////////////////////////////////////////////////////////////////////////////
