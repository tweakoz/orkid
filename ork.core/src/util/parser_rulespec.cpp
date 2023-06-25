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
    printf( "matcher<%s> notif assigned\n", rule_name.c_str() );
  } else {
    logerrchannel()->log( "matcher<%s> not found", rule_name.c_str() );
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
struct Expression;
struct OneOrMore;
struct ZeroOrMore;
struct Select;
struct Optional;
struct Sequence;
struct Group;
struct ExprKWID;
//
using astnode_ptr_t = std::shared_ptr<AstNode>;
using expression_ptr_t = std::shared_ptr<Expression>;
using oneormore_ptr_t = std::shared_ptr<OneOrMore>;
using zeroormore_ptr_t = std::shared_ptr<ZeroOrMore>;
using select_ptr_t = std::shared_ptr<Select>;
using optional_ptr_t = std::shared_ptr<Optional>;
using sequence_ptr_t = std::shared_ptr<Sequence>;
using group_ptr_t = std::shared_ptr<Group>;
using expr_kwid_ptr_t = std::shared_ptr<ExprKWID>;
//
struct AstNode {
  std::string _name;
  virtual ~AstNode() {
  }
  virtual matcher_fn_t genMatcherFunction() const {
    OrkAssert(false);
    return nullptr;
  }
};
//
struct Expression : public AstNode {
  matcher_fn_t genMatcherFunction() const final {
    return [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf( "GOT USER EXPR MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end );
      OrkAssert(_expr_selected);
      auto submatcher_fn = _expr_selected->genMatcherFunction();
      return submatcher_fn(par_matcher, slv);
    };
  }
  astnode_ptr_t _expr_selected;
  std::string _expr_name;
};
struct ExprKWID : public AstNode {
  matcher_fn_t genMatcherFunction() const final {
    OrkAssert(false);
    return [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf( "GOT USER ExprKWID MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end );
      OrkAssert(false); 
      return nullptr;
    };
  }
  std::string _kwid;
  std::string _expr_name;
};

struct OneOrMore : public AstNode {
  matcher_fn_t genMatcherFunction() const final {
    OrkAssert(false);
    return [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf( "GOT USER OOM MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end );
      OrkAssert(false); 
      return nullptr;
    };
  }
  std::vector<expression_ptr_t> _subexpressions;
};
struct ZeroOrMore : public AstNode {
  matcher_fn_t genMatcherFunction() const final {
    OrkAssert(false);
    return [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf( "GOT USER ZOM MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end );
      OrkAssert(false); 
      return nullptr;
    };
  }
  std::vector<expression_ptr_t> _subexpressions;
};
struct Select : public AstNode {
  matcher_fn_t genMatcherFunction() const final {
    OrkAssert(false);
    return [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf( "GOT USER SEL MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end );
      OrkAssert(false); 
      return nullptr;
    };
  }
  std::vector<expression_ptr_t> _subexpressions;
};
struct Optional : public AstNode {
  matcher_fn_t genMatcherFunction() const final {
    OrkAssert(false);
    return [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf( "GOT USER OPT MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end );
      OrkAssert(false); 
      return nullptr;
    };
  }
  expression_ptr_t _subexpression;
};
struct Sequence : public AstNode {
  matcher_fn_t genMatcherFunction() const final {
    OrkAssert(false);
    return [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf( "GOT USER SEQ MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end );
      OrkAssert(false); 
      return nullptr;
    };
  }
  std::vector<expression_ptr_t> _subexpressions;
};
struct Group : public AstNode {
  matcher_fn_t genMatcherFunction() const final {
    OrkAssert(false);
    return [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
      printf( "GOT USER GRP MATCHER<%s> slv.st<%zu> slv.en<%zu>\n", par_matcher->_name.c_str(), slv->_start, slv->_end );
      OrkAssert(false); 
      return nullptr;
    };
  }
  std::vector<expression_ptr_t> _subexpressions;
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
    auto oom_out = std::make_shared<AST::OneOrMore>();
    auto indentstr = std::string(indent * 2, ' ');
    printf("%s_onOOM<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str());
    _onExpression(match);
    return oom_out;
  }
  /////////////////////////////////////////////////////////
  AST::zeroormore_ptr_t _onZOM(match_ptr_t match) {
    auto zom_out = std::make_shared<AST::ZeroOrMore>();
    auto nom = match->_impl.getShared<NOrMore>();
    OrkAssert(nom->_minmatches == 0);
    auto indentstr = std::string(indent * 2, ' ');
    printf("%s_onZOM<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str());
    for (auto item : nom->_items) {
      printf( "%s zom subitem<%s>\n", indentstr.c_str(), item->_matcher->_name.c_str() );
      _onExpression(item);
    }
    return zom_out;
  }
  /////////////////////////////////////////////////////////
  AST::select_ptr_t _onSEL(match_ptr_t match) {
    auto sel_out = std::make_shared<AST::Select>();
    auto nom = match->_impl.getShared<NOrMore>();
    OrkAssert(nom->_minmatches == 1);
    OrkAssert(nom->_items.size() >= 1);
    auto indentstr = std::string(indent * 2, ' ');
    printf("%s_onSEL<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str());
    for (auto item : nom->_items) {
      printf( "%s sel subitem<%s>\n", indentstr.c_str(), item->_matcher->_name.c_str() );
      _onExpression(item);
    }
    return sel_out;
  }
  /////////////////////////////////////////////////////////
  AST::optional_ptr_t _onOPT(match_ptr_t match) {
    auto opt_out = std::make_shared<AST::Optional>();
    auto indentstr = std::string(indent * 2, ' ');
    printf("%s_onOPT<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str());
    _onExpression(match);
    return opt_out;
  }
  /////////////////////////////////////////////////////////
  AST::sequence_ptr_t _onSEQ(match_ptr_t match) {
    auto seq_out = std::make_shared<AST::Sequence>();
    auto nom = match->_impl.getShared<NOrMore>();
    OrkAssert(nom->_minmatches == 0);
    OrkAssert(nom->_items.size() >= 1);
    auto indentstr = std::string(indent * 2, ' ');
    printf("%s_onSEQ<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str());
    for (auto item : nom->_items) {
      printf( "%s seq subitem<%s>\n", indentstr.c_str(), item->_matcher->_name.c_str() );
      _onExpression(item);
    }
    return seq_out;
  }
  /////////////////////////////////////////////////////////
  AST::group_ptr_t _onGRP(match_ptr_t match) {
    auto grp_out = std::make_shared<AST::Group>();
    auto nom = match->_impl.getShared<NOrMore>();
    OrkAssert(nom->_minmatches == 0);
    OrkAssert(nom->_items.size() >= 1);
    auto indentstr = std::string(indent * 2, ' ');
    printf("%s_onGRP<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str());
    for (auto item : nom->_items) {
      printf( "%s grp subitem<%s>\n", indentstr.c_str(), item->_matcher->_name.c_str() );
      _onExpression(item);
    }
    return grp_out;
  }
  /////////////////////////////////////////////////////////
  AST::expr_kwid_ptr_t _onEXPRKWID(match_ptr_t match){
    auto kwid_out = std::make_shared<AST::ExprKWID>();
    auto indentstr = std::string(indent * 2, ' ');
    auto classmatch = match->_impl.getShared<ClassMatch>();
    auto token = classmatch->_token->text;
    kwid_out->_kwid = classmatch->_token->text;
    printf("%s_onEXPRKWID<%s> KWID<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str(), kwid_out->_kwid.c_str());
    return kwid_out;
  }
  /////////////////////////////////////////////////////////
  AST::expression_ptr_t _onExpression(match_ptr_t match, std::string named = "") {
    auto expr_out = std::make_shared<AST::Expression>();
    auto indentstr = std::string(indent * 2, ' ');

    indent++;
    auto expression_seq  = match->_impl.getShared<Sequence>();
    auto expression_sel  = expression_seq->_items[0]->_impl.getShared<OneOf>()->_selected;
    auto expression_name = expression_seq->_items[1]->_impl.getShared<Optional>()->_subitem;

    if( expression_name ){
      expression_name = expression_name->_impl.getShared<Sequence>()->_items[1];
      auto xname = expression_name->_impl.getShared<ClassMatch>()->_token->text;
      printf("%s_onExpression<%s> named<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str(), xname.c_str() );
    }
    else{
      printf("%s_onExpression<%s>\n", indentstr.c_str(), match->_matcher->_name.c_str() );
    }


    if (expression_sel) {
      if (auto as_seq = expression_sel->_impl.tryAsShared<Sequence>()) {
        auto subseq       = as_seq.value();
        auto subseq0      = subseq->_items[0];
        auto expr_matcher = expression_sel->_matcher;
        ////////////////////////////////////////////////////////////////////////////
        // zom, oom, sel, opt
        ////////////////////////////////////////////////////////////////////////////
        if (auto as_wordmatch = subseq0->_impl.tryAsShared<WordMatch>()) {
          auto wordmatch = as_wordmatch.value();
          auto word      = wordmatch->_token->text;
          if (word == "zom") {
            auto zom = subseq->_items[2];
            expr_out->_expr_selected = _onZOM(zom);
          } else if (word == "oom") {
            auto oom = subseq->_items[2];
            expr_out->_expr_selected = _onOOM(oom);
          } else if (word == "sel") {
            auto sel = subseq->_items[2];
            expr_out->_expr_selected = _onSEL(sel);
          } else if (word == "opt") {
            auto opt = subseq->_items[2];
            expr_out->_expr_selected = _onOPT(opt);
          } else {
            OrkAssert(false);
          }
        } // wordmatch ?
        ////////////////////////////////////////////////////////////////////////////
        // seq, grp
        ////////////////////////////////////////////////////////////////////////////
        else if (auto as_classmatch = subseq0->_impl.tryAsShared<ClassMatch>()) {
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
        OrkAssert(expression_sel->_impl.isShared<ClassMatch>());
        auto classmatch = expression_sel->_impl.getShared<ClassMatch>();
        expr_out->_expr_selected = _onEXPRKWID(expression_sel);
      }
    }
    indent--;
    OrkAssert(expr_out->_expr_selected!=nullptr);
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
      auto seq           = match->_impl.getShared<Sequence>();
      auto rule_key_item = seq->_items[0]->_impl.getShared<OneOf>()->_selected;
      auto qrx           = seq->_items[2]->_impl.getShared<ClassMatch>()->_token->text;
      auto rx            = qrx.substr(1, qrx.size() - 2); // remove surrounding quotes
      if (auto as_classmatch = rule_key_item->_impl.tryAsShared<ClassMatch>()) {
        auto rule_name = as_classmatch.value()->_token->text;
        auto rule      = std::make_shared<AST::ScannerRule>();
        rule->_name    = rule_name;
        rule->_regex   = rx;
        auto it        = this->_user_scanner_rules.find(rule_name);
        OrkAssert(it == this->_user_scanner_rules.end());
        printf("ADDING SCANNER RULE<%s> <- %s\n", rule_name.c_str(), rx.c_str());
        this->_user_scanner_rules[rule_name] = rule;
        this->_user_parser->matcherForWord(rule_name);
      } else if (auto as_seq = rule_key_item->_impl.tryAsShared<Sequence>()) {
        auto sub_seq   = as_seq.value();
        auto macro_str = sub_seq->_items[0]->_impl.getShared<WordMatch>()->_token->text;
        OrkAssert(macro_str == "macro");
        auto macro_name = sub_seq->_items[2]->_impl.getShared<ClassMatch>()->_token->text;
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
       auto rulename = match->_impl.getShared<Sequence>()->_items[0]->_impl.getShared<ClassMatch>()->_token->text;
      auto expr_ast_node = _onExpression(match->_impl.getShared<Sequence>()->_items[2]);
      auto expr_matcher_fn  = expr_ast_node->genMatcherFunction();
      auto user_matcher = std::make_shared<Matcher>([=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
        auto match = expr_matcher_fn(par_matcher,slv);
        return match;
      });
      user_matcher->_name = rulename;
      this->_user_parser->_matchers_by_name[rulename] = user_matcher;
    };

    _rsi_parser_matcher = zeroOrMore(parser_rule, "parser_rules");

    _rsi_parser_matcher->_notif = [=](match_ptr_t match) { printf("IMPLEMENT PARSER RULES\n"); };
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
    if(match->_view->_end != top_view._end){
      logerrchannel()->log( "Parser :: RULESPEC :: SYNTAX ERROR");
      logerrchannel()->log( "  input text num tokens<%zu>", top_view._end );
      logerrchannel()->log( "  parse cursor<%zu>", match->_view->_end );
      auto token = _scanner->token(match->_view->_end);
      logerrchannel()->log( "  parse token<%s> line<%d>", token->text.c_str(), token->iline+1 );
      OrkAssert(false);
    }
    return match;
  }
  /////////////////////////////////////////////////////////

  void attachUser(Parser* user_parser) {
    _user_parser = user_parser;
    _user_scanner = user_parser->_scanner;
  }

  /////////////////////////////////////////////////////////

  scanner_ptr_t _user_scanner;
  Parser* _user_parser = nullptr;
  matcher_ptr_t _rsi_scanner_matcher;
  matcher_ptr_t _rsi_parser_matcher;

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
  auto rsi   = getRuleSpecImpl();
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
