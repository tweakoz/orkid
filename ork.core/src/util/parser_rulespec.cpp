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
    printf("matcher<%s> notif assigned\n", rule_name.c_str());
  } else {
    logerrchannel()->log("matcher<%s> not found", rule_name.c_str());
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
  CrcEnum(PIPE),
  CrcEnum(INTEGER),
  CrcEnum(KW_OR_ID),
  CrcEnum(QUOTED_REGEX),
  CrcEnum(QUOTED_STRING),
  CrcEnum(LEFT_ARROW),
};

/////////////////////////////////////////////////////////////////////////////////////////////////

namespace AST {

struct ScannerRule {
  std::string _name;
  std::string _regex;
};
struct ScannerMacro {
  std::string _name;
  std::string _regex;
};
using scanner_rule_ptr_t  = std::shared_ptr<ScannerRule>;
using scanner_macro_ptr_t = std::shared_ptr<ScannerMacro>;
//
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
//
using astnode_ptr_t    = std::shared_ptr<AstNode>;
using rule_ptr_t = std::shared_ptr<ParserRule>;
using expression_ptr_t = std::shared_ptr<Expression>;
using oneormore_ptr_t  = std::shared_ptr<OneOrMore>;
using zeroormore_ptr_t = std::shared_ptr<ZeroOrMore>;
using select_ptr_t     = std::shared_ptr<Select>;
using optional_ptr_t   = std::shared_ptr<Optional>;
using sequence_ptr_t   = std::shared_ptr<Sequence>;
using group_ptr_t      = std::shared_ptr<Group>;
using expr_kwid_ptr_t  = std::shared_ptr<ExprKWID>;
////////////////////////////////////////////////////////////////////////
struct DumpContext{
  size_t _indent = 0;
};
using dumpctx_ptr_t = std::shared_ptr<DumpContext>;
////////////////////////////////////////////////////////////////////////
struct AstNode {
  std::string _name;
  virtual ~AstNode() {
  }
  match_ptr_t match(matcher_ptr_t par_matcher, scannerlightview_constptr_t& inp_view) {
    OrkAssert(this);
    OrkAssert(_match_fn);
    return _match_fn(par_matcher, inp_view);
  }
  virtual void dump(dumpctx_ptr_t dctx) {
    dctx->_indent++;
    auto indentstr = std::string(dctx->_indent * 2, ' ');
    printf("%s%s\n", indentstr.c_str(), _name.c_str());
    dctx->_indent--;
  }
  
  
  matcher_fn_t _match_fn;

};
////////////////////////////////////////////////////////////////////////
struct ParserRule : public AstNode {
  ParserRule(Parser* user_parser, std::string name = "") {
    _name = "ParserRule";
    if (name != "") {
      _name = name;
    }
  }
  expression_ptr_t _expression;
};
////////////////////////////////////////////////////////////////////////
struct Expression : public AstNode {
  Expression(Parser* user_parser, std::string name = "") {

    _name = "Expression";

    if (name != "") {
      _name = name;
    }

    _match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf("GOT USER EXPR MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end);
      OrkAssert(_expr_selected);
      return _expr_selected->match(par_matcher, slv);
    };

    _user_matcher        = std::make_shared<Matcher>(_match_fn);
    _user_matcher->_name = name;
  }
  void dump(dumpctx_ptr_t dctx) final {
    dctx->_indent++;
    _expr_selected->dump(dctx);
    dctx->_indent--;
  }
  matcher_ptr_t _user_matcher;
  astnode_ptr_t _expr_selected;
  std::string _expr_name;
};
////////////////////////////////////////////////////////////////////////
struct ExprKWID : public AstNode {
  ExprKWID(Parser* user_parser) {
    _name     = "ExprKWID";
    _match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf("GOT USER ExprKWID MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end);
      OrkAssert(false);
      return nullptr;
    };
  }
  std::string _kwid;
  std::string _expr_name;
};
////////////////////////////////////////////////////////////////////////
struct OneOrMore : public AstNode {
  OneOrMore(Parser* user_parser) {
    _name     = "OneOrMore";
    _match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf("GOT USER OOM MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end);
      OrkAssert(false);
      return nullptr;
    };
  }
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
struct ZeroOrMore : public AstNode {
  ZeroOrMore(Parser* user_parser) {
    _name = "ZeroOrMore";
    _match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf("GOT USER ZOM MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end);
      OrkAssert(false);
      return nullptr;
    };
  }
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
struct Select : public AstNode {
  Select(Parser* user_parser) {
    _name     = "Select";
    _match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf("GOT USER SEL MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end);
      OrkAssert(false);
      return nullptr;
    };
  }
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
struct Optional : public AstNode {
  Optional(Parser* user_parser) {
    _name     = "Optional";
    _match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf("GOT USER OPT MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end);
      OrkAssert(false);
      return nullptr;
    };
  }
  expression_ptr_t _subexpression;
};
////////////////////////////////////////////////////////////////////////
struct Sequence : public AstNode {
  Sequence(Parser* user_parser) {
    _name     = "Sequence";
    _match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf("GOT USER SEQ MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end);
      OrkAssert(false);
      return nullptr;
    };
  }
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
struct Group : public AstNode {
  Group(Parser* user_parser) {
    _name     = "Group";
    _match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf("GOT USER GRP MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end);
      OrkAssert(false);
      return nullptr;
    };
  }
  std::vector<expression_ptr_t> _subexpressions;
};
////////////////////////////////////////////////////////////////////////
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
    try {

      _scanner->addEnumClass("\\s+", TokenClass::WHITESPACE);
      _scanner->addEnumClass("[\\n\\r]+", TokenClass::NEWLINE);
      _scanner->addEnumClass("[a-zA-Z_][a-zA-Z0-9_]*", TokenClass::KW_OR_ID);
      _scanner->addEnumClass("=", TokenClass::EQUALS);
      _scanner->addEnumClass(",", TokenClass::COMMA);
      _scanner->addEnumClass(":", TokenClass::COLON);
      _scanner->addEnumClass(";", TokenClass::SEMICOLON);
      _scanner->addEnumClass("\\(", TokenClass::L_PAREN);
      _scanner->addEnumClass("\\)", TokenClass::R_PAREN);
      _scanner->addEnumClass("\\[", TokenClass::L_SQUARE);
      _scanner->addEnumClass("\\]", TokenClass::R_SQUARE);
      _scanner->addEnumClass("\\{", TokenClass::L_CURLY);
      _scanner->addEnumClass("\\}", TokenClass::R_CURLY);
      _scanner->addEnumClass("\\*", TokenClass::STAR);
      _scanner->addEnumClass("\\+", TokenClass::PLUS);
      _scanner->addEnumClass("\\-", TokenClass::MINUS);
      _scanner->addEnumClass("\\|", TokenClass::PIPE);
      _scanner->addEnumClass("-?(\\d+)", TokenClass::INTEGER);
      //_scanner->addMacro("ESCAPED_CHAR", "\\\\[\"\\\\]");
      //_scanner->addMacro("ANY_ESCAPED", "\\\\.");
      //_scanner->addMacro("ASCII", "[\\\\x00-\\\\x21\\\\x23-\\\\x5B\\\\x5D-\\\\x7F]");
      //_scanner->addMacro("ASCII_WITHOUT_DBLQUOTE", "[\\\\x00-\\\\x21\\\\x23-\\\\x7F]");
      _scanner->addEnumClass(R"(\"[^\"]*\")", TokenClass::QUOTED_REGEX);
      _scanner->addEnumClass("<-", TokenClass::LEFT_ARROW);
      printf("Building state machine\n");
      _scanner->buildStateMachine();
      printf("done...\n");
    } catch (std::exception& e) {
      printf("EXCEPTION<%s>\n", e.what());
      OrkAssert(false);
    }
  }
  size_t indent = 0;
  /////////////////////////////////////////////////////////
  AST::oneormore_ptr_t _onOOM(match_ptr_t match) {
    auto indentstr = std::string(indent * 2, ' ');
    // our output AST node
    auto oom_out   = std::make_shared<AST::OneOrMore>(this);
    // our parser DSL input node (containing user language spec)
    //  in this case it is just match
    // rule_oom <- sequence({oom, lcurly, rule_expression, rcurly}, "rule_oom");
    auto oom_inp     = match;
    printf("%s_onOOM<%s>\n", indentstr.c_str(), oom_inp->_matcher->_name.c_str());
    auto sub = _onExpression(oom_inp);
    oom_out->_subexpressions.push_back(sub);
    _retain_astnodes.insert(oom_out);
    return oom_out;
  }
  /////////////////////////////////////////////////////////
  AST::zeroormore_ptr_t _onZOM(match_ptr_t match) {
    auto indentstr = std::string(indent * 2, ' ');
    // our output AST node
    auto zom_out = std::make_shared<AST::ZeroOrMore>(this);
    // our parser DSL input node (containing user language spec)
    auto nom_inp     = match->asShared<NOrMore>();
    OrkAssert(nom_inp->_minmatches == 0);
    printf("%s_onZOM<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str());
    for (auto sub_item : nom_inp->_items) {
      printf("%s zom subitem<%s>\n", indentstr.c_str(), sub_item->_matcher->_name.c_str());
      auto subexpr_out = _onExpression(sub_item);
      zom_out->_subexpressions.push_back(subexpr_out);
    }
    _retain_astnodes.insert(zom_out);
    return zom_out;
  }
  /////////////////////////////////////////////////////////
  AST::select_ptr_t _onSEL(match_ptr_t match) {
    auto indentstr = std::string(indent * 2, ' ');
    // our output AST node
    auto sel_out = std::make_shared<AST::Select>(this);
    // our parser DSL input node (containing user language spec)
    auto nom_inp     = match->asShared<NOrMore>();
    printf("%s _onSEL<%s> nom_inp<%p>\n", indentstr.c_str(), match->_matcher->_name.c_str(), (void*) nom_inp.get() );
    // for each user language spec subexpression
    //   add an AST subexpression
    for (auto sub_item : nom_inp->_items) {
      printf("%s sel subitem<%s>\n", indentstr.c_str(), sub_item->_matcher->_name.c_str());
      auto subexpr_out = _onExpression(sub_item);
      sel_out->_subexpressions.push_back(subexpr_out);
    }
    _retain_astnodes.insert(sel_out);
    return sel_out;
  }
  /////////////////////////////////////////////////////////
  AST::optional_ptr_t _onOPT(match_ptr_t match) {
    auto indentstr = std::string(indent * 2, ' ');
    // our output AST node
    auto opt_out   = std::make_shared<AST::Optional>(this);
    // our parser DSL input node (containing user language spec)
    //  in this case it is just match
    // rule_opt <- sequence({oom, lcurly, rule_expression, rcurly}, "rule_oom");
    auto opt_inp = match;
    printf("%s_onOPT<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str());
    opt_out->_subexpression = _onExpression(opt_inp);
    _retain_astnodes.insert(opt_out);
    return opt_out;
  }
  /////////////////////////////////////////////////////////
  AST::sequence_ptr_t _onSEQ(match_ptr_t match) {
    auto seq_out = std::make_shared<AST::Sequence>(this);
    auto nom     = match->asShared<NOrMore>();
    OrkAssert(nom->_minmatches == 0);
    OrkAssert(nom->_items.size() >= 1);
    auto indentstr = std::string(indent * 2, ' ');
    printf("%s_onSEQ<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str());
    for (auto sub_item : nom->_items) {
      printf("%s seq subitem<%s>\n", indentstr.c_str(), sub_item->_matcher->_name.c_str());
      auto subexpr = _onExpression(sub_item);
      seq_out->_subexpressions.push_back(subexpr);
    }
    _retain_astnodes.insert(seq_out);
    return seq_out;
  }
  /////////////////////////////////////////////////////////
  AST::group_ptr_t _onGRP(match_ptr_t match) {
    auto grp_out = std::make_shared<AST::Group>(this);
    auto nom     = match->asShared<NOrMore>();
    OrkAssert(nom->_minmatches == 0);
    OrkAssert(nom->_items.size() >= 1);
    auto indentstr = std::string(indent * 2, ' ');
    printf("%s_onGRP<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str());
    for (auto item : nom->_items) {
      printf("%s grp subitem<%s>\n", indentstr.c_str(), item->_matcher->_name.c_str());
      auto subexpr = _onExpression(item);
      grp_out->_subexpressions.push_back(subexpr);
    }
    _retain_astnodes.insert(grp_out);
    return grp_out;
  }
  /////////////////////////////////////////////////////////
  AST::expr_kwid_ptr_t _onEXPRKWID(match_ptr_t match) {
    auto kwid_out   = std::make_shared<AST::ExprKWID>(this);
    auto indentstr  = std::string(indent * 2, ' ');
    auto classmatch = match->asShared<ClassMatch>();
    auto token      = classmatch->_token->text;
    kwid_out->_kwid = classmatch->_token->text;
    printf("%s_onEXPRKWID<%s> KWID<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str(), kwid_out->_kwid.c_str());
    _retain_astnodes.insert(kwid_out);
    return kwid_out;
  }
  /////////////////////////////////////////////////////////
  AST::expression_ptr_t _onExpression(match_ptr_t match, std::string named = "") {
    auto expr_out  = std::make_shared<AST::Expression>(this);
    auto indentstr = std::string(indent * 2, ' ');

    indent++;
    auto expression_seq  = match->asShared<Sequence>();
    auto expression_sel  = expression_seq->itemAsShared<OneOf>(0)->_selected;
    auto expression_name = expression_seq->itemAsShared<Optional>(1)->_subitem;

    size_t expression_len = expression_seq->_items.size();
    if (expression_name) {
      expression_name = expression_name->asShared<Sequence>()->_items[1];
      auto xname      = expression_name->asShared<ClassMatch>()->_token->text;
      printf("%s_onExpression<%s> len%zu>  named<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str(), expression_len, xname.c_str());
    } else {
      printf("%s_onExpression<%s> len%zu>\n", indentstr.c_str(), match->_matcher->_name.c_str(), expression_len);
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
          printf("ERR<subseq0 not wordmatch or classmatch> view start<%zu>\n", expression_sel->_view->_start);
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
    return expr_out;
  }
  /////////////////////////////////////////////////////////
  void loadGrammar() { //
    printf("Loading Grammar\n");
    ////////////////////
    // primitives
    ////////////////////
    auto plus         = matcherForTokenClass(TokenClass::PLUS, "plus");
    auto minus        = matcherForTokenClass(TokenClass::MINUS, "minus");
    auto star         = matcherForTokenClass(TokenClass::STAR, "star");
    auto equals       = matcherForTokenClass(TokenClass::EQUALS, "equals");
    auto colon        = matcherForTokenClass(TokenClass::COLON, "colon");
    auto semicolon    = matcherForTokenClass(TokenClass::SEMICOLON, "semicolon");
    auto comma        = matcherForTokenClass(TokenClass::COMMA, "comma");
    auto lparen       = matcherForTokenClass(TokenClass::L_PAREN, "lparen");
    auto rparen       = matcherForTokenClass(TokenClass::R_PAREN, "rparen");
    auto lsquare      = matcherForTokenClass(TokenClass::L_SQUARE, "lsquare");
    auto rsquare      = matcherForTokenClass(TokenClass::R_SQUARE, "rsquare");
    auto lcurly       = matcherForTokenClass(TokenClass::L_CURLY, "lcurly");
    auto rcurly       = matcherForTokenClass(TokenClass::R_CURLY, "rcurly");
    auto pipe         = matcherForTokenClass(TokenClass::PIPE, "pipe");
    auto inttok       = matcherForTokenClass(TokenClass::INTEGER, "int");
    auto kworid       = matcherForTokenClass(TokenClass::KW_OR_ID, "kw_or_id");
    auto left_arrow   = matcherForTokenClass(TokenClass::LEFT_ARROW, "left_arrow");
    auto quoted_regex = matcherForTokenClass(TokenClass::QUOTED_REGEX, "quoted_regex");
    ////////////////////
    auto sel   = matcherForWord("sel");
    auto zom   = matcherForWord("zom");
    auto oom   = matcherForWord("oom");
    auto opt   = matcherForWord("opt");
    auto macro = matcherForWord("macro");
    ////////////////////
    // scanner rules
    ////////////////////
    auto macro_item      = sequence({macro, lparen, kworid, rparen}, "macro_item");
    auto scanner_key     = oneOf({macro_item, kworid}, "scanner_key");
    auto scanner_rule    = sequence({scanner_key, left_arrow, quoted_regex}, "scanner_rule");
    scanner_rule->_notif = [=](match_ptr_t match) {
      auto seq           = match->asShared<Sequence>();
      auto rule_key_item = seq->_items[0]->asShared<OneOf>()->_selected;
      auto qrx           = seq->_items[2]->asShared<ClassMatch>()->_token->text;
      auto rx            = qrx.substr(1, qrx.size() - 2); // remove surrounding quotes
      if (auto as_classmatch = rule_key_item->tryAsShared<ClassMatch>()) {
        auto rule_name = as_classmatch.value()->_token->text;
        auto rule      = std::make_shared<AST::ScannerRule>();
        rule->_name    = rule_name;
        rule->_regex   = rx;
        auto it        = this->_user_scanner_rules.find(rule_name);
        OrkAssert(it == this->_user_scanner_rules.end());
        printf("ADDING SCANNER RULE<%s> <- %s\n", rule_name.c_str(), rx.c_str());
        this->_user_scanner_rules[rule_name] = rule;
        this->_user_parser->matcherForWord(rule_name);
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
        printf("ADDING SCANNER MACRO<%s> <- %s\n", macro_name.c_str(), rx.c_str());
        this->_user_scanner_macros[macro_name] = macro;
      } else {
        OrkAssert(false);
      }
    };
    _rsi_scanner_matcher         = zeroOrMore(scanner_rule, "scanner_rules");
    _rsi_scanner_matcher->_notif = [=](match_ptr_t match) {
      std::string _current_rule_name = "";
      try {
        for (auto item : this->_user_scanner_macros) {
          auto macro         = item.second;
          _current_rule_name = macro->_name;
          printf("IMPLEMENT MACRO <%s : %s>\n", macro->_name.c_str(), macro->_regex.c_str());
          this->_user_scanner->addMacro(macro->_name, macro->_regex);
        }
        for (auto item : this->_user_scanner_rules) {
          auto rule          = item.second;
          uint64_t crc_id    = CrcString(rule->_name.c_str()).hashed();
          _current_rule_name = rule->_name;
          printf("IMPLEMENT EnumClass <%s : %zu : %s>\n", rule->_name.c_str(), crc_id, rule->_regex.c_str());
          this->_user_scanner->addEnumClass(rule->_regex, crc_id);
        }
        this->_user_scanner->buildStateMachine();
      } catch (std::exception& e) {
        printf("EXCEPTION cur_rule<%s>  cause<%s>\n", _current_rule_name.c_str(), e.what());
        OrkAssert(false);
      }
    };
    ////////////////////
    // parser rules
    ////////////////////

    auto rule_expression = declare("rule_expression");
    //
    //
    auto rule_zom      = sequence({zom, lcurly, zeroOrMore(rule_expression), rcurly}, "rule_zom");
    auto rule_oom      = sequence({oom, lcurly, rule_expression, rcurly}, "rule_oom");
    auto rule_sel      = sequence({sel, lcurly, oneOrMore(rule_expression), rcurly}, "rule_sel");
    auto rule_opt      = sequence({opt, lcurly, rule_expression, rcurly}, "rule_opt");
    auto rule_sequence = sequence({lsquare, zeroOrMore(rule_expression), rsquare}, "rule_sequence");
    auto rule_grp      = sequence({lparen, zeroOrMore(rule_expression), rparen}, "rule_grp");
    //
    sequence(
        rule_expression,
        {
            oneOf({
                // load previously declared rule_expression
                rule_zom,
                rule_oom,
                rule_sel,
                rule_opt,
                rule_sequence,
                rule_grp,
                kworid,
            }),
            optional(sequence({colon, quoted_regex}), "expr_name"),
        });
    auto parser_rule = sequence({kworid, left_arrow, rule_expression}, "parser_rule");

    parser_rule->_notif = [=](match_ptr_t match) {
      auto rulename      = match->asShared<Sequence>()->_items[0]->asShared<ClassMatch>()->_token->text;
      auto expr_ast_node = _onExpression(match->asShared<Sequence>()->_items[2], rulename);
      this->_user_parser->_matchers_by_name[rulename] = expr_ast_node->_user_matcher;
      auto ast_rule = std::make_shared<AST::ParserRule>(this,rulename);
      ast_rule->_expression = expr_ast_node;
      _user_parser_rules[rulename] = ast_rule;
    };

    _rsi_parser_matcher = zeroOrMore(parser_rule, "parser_rules");

    _rsi_parser_matcher->_notif = [=](match_ptr_t match) { 
      for( auto rule : _user_parser_rules ){
        auto rule_name = rule.first;
        auto ast_rule = rule.second;
        printf("IMPLEMENT PARSER RULE<%s>\n", rule_name.c_str());
        auto dctx = std::make_shared<AST::DumpContext>();
        ast_rule->dump(dctx);
        //_user_parser->matcherForWord(ast_rule->_name);
      }
      
      printf("IMPLEMENT PARSER RULES\n"); };
  }
  /////////////////////////////////////////////////////////
  match_ptr_t parseScannerSpec(std::string inp_string) {
    try {
      _scanner->clear();
      _scanner->scanString(inp_string);
      _scanner->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
      _scanner->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));
    } catch (std::exception& e) {
      logerrchannel()->log("EXCEPTION<%s>\n", e.what());
      OrkAssert(false);
    }
    auto top_view = _scanner->createTopView();
    top_view.dump("top_view");
    auto slv   = std::make_shared<ScannerLightView>(top_view);
    auto match = this->match(slv, _rsi_scanner_matcher);
    OrkAssert(match);
    OrkAssert(match->_view->_start == top_view._start);
    OrkAssert(match->_view->_end == top_view._end);
    return match;
  }
  /////////////////////////////////////////////////////////
  match_ptr_t parseParserSpec(std::string inp_string) {
    try {
      _scanner->clear();
      _scanner->scanString(inp_string);
      _scanner->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
      _scanner->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));
    } catch (std::exception& e) {
      printf("EXCEPTION<%s>\n", e.what());
      OrkAssert(false);
    }
    auto top_view = _scanner->createTopView();
    top_view.dump("top_view");
    auto slv   = std::make_shared<ScannerLightView>(top_view);
    auto match = this->match(slv, _rsi_parser_matcher);
    OrkAssert(match);
    OrkAssert(match->_view->_start == top_view._start);
    if (match->_view->_end != top_view._end) {
      logerrchannel()->log("Parser :: RULESPEC :: SYNTAX ERROR");
      logerrchannel()->log("  input text num tokens<%zu>", top_view._end);
      logerrchannel()->log("  parse cursor<%zu>", match->_view->_end);
      auto token = _scanner->token(match->_view->_end);
      logerrchannel()->log("  parse token<%s> line<%d>", token->text.c_str(), token->iline + 1);
      OrkAssert(false);
    }
    return match;
  }
  /////////////////////////////////////////////////////////

  void attachUser(Parser* user_parser) {
    _user_parser  = user_parser;
    _user_scanner = user_parser->_scanner;
  }

  /////////////////////////////////////////////////////////

  scanner_ptr_t _user_scanner;
  Parser* _user_parser = nullptr;
  matcher_ptr_t _rsi_scanner_matcher;
  matcher_ptr_t _rsi_parser_matcher;
  std::unordered_set<AST::astnode_ptr_t> _retain_astnodes;

  std::map<std::string, AST::rule_ptr_t> _user_parser_rules;

  std::map<std::string, AST::scanner_rule_ptr_t> _user_scanner_rules;
  std::map<std::string, AST::scanner_macro_ptr_t> _user_scanner_macros;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

using rulespec_impl_ptr_t = std::shared_ptr<RuleSpecImpl>;

rulespec_impl_ptr_t getRuleSpecImpl() {
  static auto rsi = std::make_shared<RuleSpecImpl>();
  return rsi;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void Parser::loadScannerSpec(const std::string& spec) {
  auto rsi = getRuleSpecImpl();
  rsi->attachUser(this);
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
