////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
//  Scanner/Parser
//  this replaces CgFx for OpenGL 3.x and OpenGL ES 2.x
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include "glslfxi_parser.h"
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/pch.h>
#include <ork/util/crc.h>
#include <regex>
#include <stdlib.h>
//#include <parsertl/include/parsertl/generator.hpp>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct _orksl_parser_internals{
  _orksl_parser_internals();

  parsertl::rules _grules;
  parsertl::state_machine _gsm;
  lexertl::rules _lrules;
  lexertl::state_machine _lsm;

  size_t _additive_expression = -1;
  size_t _and_expression = -1;
  size_t _argument_expression_list = -1;

  size_t _assignment_expression = -1;
  size_t _assignment_operator = -1;
  size_t _cast_expression = -1;
  size_t _conditional_expression = -1;
  size_t _equality_expression = -1;
  size_t _exclusive_or_expression = -1;
  size_t _expression = -1;
  size_t _inclusive_or_expression = -1;
  size_t _logical_and_expression = -1;

  size_t _logical_or_expression = -1;
  size_t _multiplicative_expression = -1;
  size_t _postfix_expression = -1;
  size_t _primary_expression = -1;

  size_t _relational_expression = -1;
  size_t _shift_expression = -1;
  size_t _ternary_expression = -1;

  size_t _unary_expression = -1;
  size_t _unary_operator = -1;

  size_t _test_A = -1;
  size_t _test_B = -1;
  size_t _test_C = -1;

};

_orksl_parser_internals::_orksl_parser_internals()
    : _grules(/*parsertl::enable_captures*/)
    , _gsm()
    , _lrules()
    , _lsm() {

 struct RR {
    RR(){}
    void addRule(const char* rule, int id){
      _id2rule[id] = rule;
    }
    const std::string& rule(TokenClass id) const {
      auto it = _id2rule.find(int(id));
      OrkAssert(it!=_id2rule.end());
      return it->second;
    }
    std::map<int,std::string> _id2rule;
  };
  RR _rr;

  loadScannerRules(_rr);

  ////////////////////////////////////////////////
  // parser
  ////////////////////////////////////////////////

  // terminals (tokens)

  _grules.token("KW_OR_ID TYPENAME");
  _grules.token("FLOAT UINT XINT CONSTANT");
  _grules.token("IDENTIFIER STRING_LITERAL");
  _grules.token("INCREMENT DECREMENT");
  _grules.token("L_PAREN R_PAREN L_CURLY R_CURLY L_SQUARE R_SQUARE");
  _grules.token("LESS_THAN LESS_THAN_EQ GREATER_THAN GREATER_THAN_EQ");
  _grules.token("EQUAL_TO NOT_EQUAL_TO L_SHIFT R_SHIFT");
  _grules.token("COLON SEMICOLON CARET LOGICAL_AND LOGICAL_OR");
  _grules.token("EQUALS COMMA DOT QUESTION_MARK STAR SLASH PERCENT EXCLAMATION AMPERSAND PIPE PLUS MINUS");

  // rules

  _test_C = _grules.push("test_C", //
    "test_A L_CURLY" //
  );

  _test_A = _grules.push("test_A", //
    "L_PAREN R_PAREN " //
    "| L_PAREN test_B R_PAREN" //
  );

  _test_B = _grules.push("test_B", //
    "KW_OR_ID KW_OR_ID " //
    "| test_B COMMA KW_OR_ID KW_OR_ID" //
  );

  printf( "RULE _test_C<%zu>\n", _test_C );
  printf( "RULE _test_A<%zu>\n", _test_A );
  printf( "RULE _test_B<%zu>\n", _test_B );

  /*_grules.push("CONSTANT", //
    "  FLOAT " //
    "| UINT" //
    "| XINT" //
  );*/

  /*_additive_expression = _grules.push("additive_expression", //
    "  multiplicative_expression " //
    "| additive_expression PLUS multiplicative_expression" //
    "| additive_expression MINUS multiplicative_expression" //
  );

  _and_expression = _grules.push("and_expression", //
    "  equality_expression " //
    "| and_expression AMPERSAND equality_expression" //
  );

  _argument_expression_list = _grules.push("argument_expression_list", //
    "  assignment_expression " //
    "| argument_expression_list COMMA assignment_expression" //
  );

  _assignment_expression = _grules.push("assignment_expression", //
    "  conditional_expression " //
    "| unary_expression assignment_operator assignment_expression " //
  );

  _assignment_operator = _grules.push("assignment_operator", //
    "  EQUALS" //
    //"| EQUALS" //
  );

  _cast_expression = _grules.push("cast_expression", //
    "  unary_expression " //
    "| L_PAREN TYPENAME R_PAREN " //
  );


  _conditional_expression = _grules.push("conditional_expression", //
    "  logical_or_expression " //
    "| logical_or_expression ternary_expression " //
  );

  _equality_expression = _grules.push("equality_expression", //
    "  relational_expression " //
    "| equality_expression EQUAL_TO relational_expression" //
    "| equality_expression NOT_EQUAL_TO relational_expression" //
  );

  _exclusive_or_expression = _grules.push("exclusive_or_expression", //
    "  and_expression " //
    "| exclusive_or_expression CARET and_expression " //
  );

  _expression = _grules.push("expression", //
    "  assignment_expression " //
    "| expression COMMA assignment_expression" //
  );

  _inclusive_or_expression = _grules.push("inclusive_or_expression", //
    "  logical_or_expression " //
    "| inclusive_or_expression PIPE exclusive_or_expression " //
  );

  _logical_and_expression = _grules.push("logical_and_expression", //
    "  inclusive_or_expression " //
    "| logical_and_expression LOGICAL_AND inclusive_or_expression " //
  );

  _logical_or_expression = _grules.push("logical_or_expression", //
    "  logical_and_expression " //
    "| logical_or_expression LOGICAL_OR logical_and_expression " //
  );

  _multiplicative_expression = _grules.push("multiplicative_expression", //
    "  cast_expression " //
    "| multiplicative_expression STAR cast_expression " // *
    "| multiplicative_expression SLASH cast_expression " // / 
    "| multiplicative_expression PERCENT cast_expression " // %
  );

  _postfix_expression = _grules.push("postfix_expression", //
    "  primary_expression " //
    "| postfix_expression L_SQUARE expression R_SQUARE " // *
    "| postfix_expression L_PAREN R_PAREN " // *
    "| postfix_expression L_PAREN argument_expression_list R_PAREN " // *
    "| postfix_expression DOT IDENTIFIER " // *
    "| postfix_expression INCREMENT " // *
    "| postfix_expression DECREMENT " // *
  );

  _primary_expression = _grules.push("primary_expression", //
    "| IDENTIFIER " // *
    "| CONSTANT " // *
    "| STRING_LITERAL " // *
    "| L_PAREN expression R_PAREN " // *
  );

  _relational_expression = _grules.push("relational_expression", //
    "  shift_expression " //
    "| relational_expression LESS_THAN shift_expression " //
    "| relational_expression LESS_THAN_EQ shift_expression " //
    "| relational_expression GREATER_THAN shift_expression " //
    "| relational_expression GREATER_THAN_EQ shift_expression " //
  );

  _shift_expression = _grules.push("shift_expression", //
    "  additive_expression " //
    "| shift_expression L_SHIFT additive_expression " //
    "| shift_expression R_SHIFT additive_expression " //
  );

  _ternary_expression = _grules.push("ternary_expression", //
    "QUESTION_MARK expression COLON conditional_expression" //
  );

  _unary_expression = _grules.push("unary_expression", //
    "  postfix_expression " //
    "| INCREMENT unary_expression " //
    "| DECREMENT unary_expression " //
    "| unary_operator cast_expression " //
  );

  _unary_operator = _grules.push("unary_operator", //
    "  AMPERSAND " //
    "| STAR " //
    "| PLUS " //
    "| MINUS " //
    "| EXCLAMATION " //
    //"| TILDE " // ~
  );*/


  parsertl::generator::build(_grules, _gsm);

  ////////////////////////////////////////////////
  // lexer
  ////////////////////////////////////////////////

  if(1){
    _lrules.push("[a-zA-Z_]+[a-zA-Z0-9_]+", _grules.token_id("KW_OR_ID"));
    _lrules.push("\\(", _grules.token_id("L_PAREN"));
    _lrules.push("\\)", _grules.token_id("R_PAREN"));
    _lrules.push("\\{", _grules.token_id("L_CURLY"));
    _lrules.push("\\}", _grules.token_id("R_CURLY"));
    _lrules.push(",", _grules.token_id("COMMA"));
    _lrules.push("\\s+", _lrules.skip());
  }
  else{
    _lrules.push(_rr.rule(TokenClass::KW_OR_ID), _grules.token_id("KW_OR_ID"));
    _lrules.push(_rr.rule(TokenClass::KW_OR_ID), _grules.token_id("TYPENAME"));
    _lrules.push(_rr.rule(TokenClass::KW_OR_ID), _grules.token_id("IDENTIFIER"));
    _lrules.push(_rr.rule(TokenClass::STRING), _grules.token_id("STRING_LITERAL"));
    _lrules.push(_rr.rule(TokenClass::FLOATING_POINT), _grules.token_id("FLOAT"));
    _lrules.push(_rr.rule(TokenClass::UNSIGNED_DECIMAL_INTEGER), _grules.token_id("UINT"));
    _lrules.push(_rr.rule(TokenClass::HEX_INTEGER), _grules.token_id("XINT"));

    _lrules.push(_rr.rule(TokenClass::L_PAREN), _grules.token_id("L_PAREN"));
    _lrules.push(_rr.rule(TokenClass::R_PAREN), _grules.token_id("R_PAREN"));
    _lrules.push(_rr.rule(TokenClass::L_CURLY), _grules.token_id("L_CURLY"));
    _lrules.push(_rr.rule(TokenClass::R_CURLY), _grules.token_id("R_CURLY"));
    _lrules.push(_rr.rule(TokenClass::L_SQUARE), _grules.token_id("L_SQUARE"));
    _lrules.push(_rr.rule(TokenClass::R_SQUARE), _grules.token_id("R_SQUARE"));

    _lrules.push(_rr.rule(TokenClass::LESS_THAN), _grules.token_id("LESS_THAN"));
    _lrules.push(_rr.rule(TokenClass::LESS_THAN_EQ), _grules.token_id("LESS_THAN_EQ"));
    _lrules.push(_rr.rule(TokenClass::GREATER_THAN), _grules.token_id("GREATER_THAN"));
    _lrules.push(_rr.rule(TokenClass::GREATER_THAN_EQ), _grules.token_id("GREATER_THAN_EQ"));

    _lrules.push(_rr.rule(TokenClass::NOT_EQUAL_TO), _grules.token_id("NOT_EQUAL_TO"));
    _lrules.push(_rr.rule(TokenClass::EQUAL_TO), _grules.token_id("EQUAL_TO"));


    _lrules.push(_rr.rule(TokenClass::L_SHIFT), _grules.token_id("L_SHIFT"));
    _lrules.push(_rr.rule(TokenClass::R_SHIFT), _grules.token_id("R_SHIFT"));

    _lrules.push(_rr.rule(TokenClass::COLON), _grules.token_id("COLON"));
    _lrules.push(_rr.rule(TokenClass::SEMICOLON), _grules.token_id("SEMICOLON"));
    _lrules.push(_rr.rule(TokenClass::EQUALS), _grules.token_id("EQUALS"));
    _lrules.push(_rr.rule(TokenClass::COMMA), _grules.token_id("COMMA"));
    _lrules.push(_rr.rule(TokenClass::DOT), _grules.token_id("DOT"));
    _lrules.push(_rr.rule(TokenClass::QUESTION_MARK), _grules.token_id("QUESTION_MARK"));
    _lrules.push(_rr.rule(TokenClass::STAR), _grules.token_id("STAR"));
    _lrules.push(_rr.rule(TokenClass::SLASH), _grules.token_id("SLASH"));
    _lrules.push(_rr.rule(TokenClass::PERCENT), _grules.token_id("PERCENT"));
    _lrules.push(_rr.rule(TokenClass::EXCLAMATION), _grules.token_id("EXCLAMATION"));
    _lrules.push(_rr.rule(TokenClass::AMPERSAND), _grules.token_id("AMPERSAND"));
    _lrules.push(_rr.rule(TokenClass::CARET), _grules.token_id("CARET"));
    _lrules.push(_rr.rule(TokenClass::PIPE), _grules.token_id("PIPE"));
    _lrules.push(_rr.rule(TokenClass::PLUS), _grules.token_id("PLUS"));
    _lrules.push(_rr.rule(TokenClass::MINUS), _grules.token_id("MINUS"));

    _lrules.push(_rr.rule(TokenClass::LOGICAL_OR), _grules.token_id("LOGICAL_OR"));
    _lrules.push(_rr.rule(TokenClass::LOGICAL_AND), _grules.token_id("LOGICAL_AND"));

    _lrules.push(_rr.rule(TokenClass::INCREMENT), _grules.token_id("INCREMENT"));
    _lrules.push(_rr.rule(TokenClass::DECREMENT), _grules.token_id("DECREMENT"));
    _lrules.push("\\s+", _lrules.skip());
  }
  lexertl::generator::build(_lrules, _lsm);

  ////////////////////////////////////////////////
}

_orksl_parser_internals_ptr_t OrkSlFunctionNode::_get_internals() {
  static auto _gint = std::make_shared<_orksl_parser_internals>();
  return _gint;
}

OrkSlFunctionNode::OrkSlFunctionNode() {
}

int OrkSlFunctionNode::parse(GlSlFxParser* parser, const ScannerView& view) {

  auto internals = _get_internals();

  int i = 0;
  view.dump("OrkSlFunctionNode::start");
  auto open_tok = view.token(i);
  OrkAssert(open_tok->text == "(");
  i++;

  try {
    using capture_vector = std::vector<std::pair<const char*, const char*>>;
    std::vector<capture_vector> captures;

    std::string input = view.asString();

    printf("input<%s>\n", input.c_str());
    printf("input.st<%c>\n", input.c_str()[0]);
    printf("input.en<%c>\n", input.c_str()[input.size()-1]);

    lexertl::citerator iter(
        input.c_str(),                // start
        input.c_str() + input.size(), // end
        internals->_lsm);             // lsm

    parsertl::match_results results(iter->id, internals->_gsm);

    using token = parsertl::token<lexertl::citerator>;
    token::token_vector productions;

    while (results.entry.action != parsertl::eaction::error) { //
      switch (results.entry.action) {
        case parsertl::eaction::error:
          throw std::runtime_error("Parser error");
          break;
        case parsertl::eaction::shift:{
          printf( "shift\n");
          break;
        }
        case parsertl::eaction::go_to:
          printf( "go_to\n");
          break;
        case parsertl::eaction::accept:
          printf( "accept\n");
          break;
        case parsertl::eaction::reduce: {

          printf( "reduce\n");

          std::size_t rule = results.reduce_id();

          if(rule==internals->_test_A){
            printf( "_test_A\n");
          }
          else if(rule==internals->_test_B){
            printf( "_test_B\n");
          }
          else if(rule==internals->_test_C){
            printf( "_test_C\n");
            auto& wtf = results.dollar(internals->_gsm,0,productions);

            const char* wtf_first = wtf.first;
            const char* wtf_second = wtf.second;

            printf( "wtf_first<%s> wtf_second<%s>\n", wtf_first, wtf_second );
            OrkAssert(false);
          }/*
          if(rule==internals->_additive_expression){
            printf( "_additive_expression\n");
          }
          else if(rule==internals->_and_expression){
            printf( "_and_expression\n");
          }
          else if(rule==internals->_argument_expression_list){
            printf( "_argument_expression_list\n");
          }
          else if(rule==internals->_assignment_expression){
            printf( "_assignment_expression\n");
          }
          else if(rule==internals->_assignment_operator){
            printf( "_assignment_operator\n");
          }
          else if(rule==internals->_cast_expression){
            printf( "_cast_expression\n");
          }
          else if(rule==internals->_conditional_expression){
            printf( "_conditional_expression\n");
          }
          else if(rule==internals->_equality_expression){
            printf( "_equality_expression\n");
          }
          else if(rule==internals->_exclusive_or_expression){
            printf( "_exclusive_or_expression\n");
          }
          else if(rule==internals->_expression){
            printf( "_expression\n");
          }
          else if(rule==internals->_logical_and_expression){
            printf( "_logical_and_expression\n");
          }
          else if(rule==internals->_logical_or_expression){
            printf( "_logical_or_expression\n");
          }
          else if(rule==internals->_multiplicative_expression){
            printf( "_multiplicative_expression\n");
          }
          else if(rule==internals->_postfix_expression){
            printf( "_postfix_expression\n");
            auto& wtf = results.dollar(internals->_gsm,0,productions);

            const char* wtf_first = wtf.first;
            const char* wtf_second = wtf.second;

            printf( "wtf_first<%s> wtf_second<%s>\n", wtf_first, wtf_second );
          }
          else if(rule==internals->_primary_expression){
            printf( "_primary_expression\n");
          }
          else if(rule==internals->_relational_expression){
            printf( "_relational_expression\n");
          }
          else if(rule==internals->_shift_expression){
            printf( "_shift_expression\n");
          }
          else if(rule==internals->_ternary_expression){
            printf( "_ternary_expression\n");
          }
          else if(rule==internals->_unary_expression){
            printf( "_unary_expression\n");
          }
          else if(rule==internals->_unary_operator){
            printf( "_unary_operator\n");
          }*/
          else{
            printf( "unknown REDUCE rule<%zu>\n", rule );
            //OrkAssert(false);
          }
          break;
        }
        default:
          OrkAssert(false);
      }

      parsertl::lookup(internals->_gsm, iter, results, productions);

    } // while ((results.entry.action != parsertl::eaction::error)

    printf( "num_productions<%zu>\n", productions.size() );
    OrkAssert(productions.size());

    for( auto p : productions ){
      printf( "item<%s>\n", p.str().c_str() );
    }
    // bool success = parsertl::parse(internals->_gsm, iter, results_);
    // OrkAssert(success);

  } catch (const std::exception& e) {
    std::cout << e.what() << '\n';
    OrkAssert(false);
  }
  OrkAssert(false);
  return 0;
}
void OrkSlFunctionNode::emit(shaderbuilder::BackEnd& backend) const {
  OrkAssert(false);
}

/*
std::string FnParseContext::tokenValue(size_t offset) const {
  return _view->token(_startIndex + offset)->text;
}

FnParseContext::FnParseContext(GlSlFxParser* parser, const ScannerView* v)
    : _parser(parser)
    , _view(v) {
}
FnParseContext::FnParseContext(const FnParseContext& oth)
    : _parser(oth._parser)
    , _startIndex(oth._startIndex)
    , _view(oth._view) {
}
FnParseContext& FnParseContext::operator=(const FnParseContext& oth) {
  _parser     = oth._parser;
  _startIndex = oth._startIndex;
  _view       = oth._view;
  return *this;
}
FnParseContext FnParseContext::advance(size_t count) const {
  FnParseContext rval(*this);
  rval._startIndex = count;
  return rval;
}
void FnParseContext::dump(const std::string dumpid) const{
  printf( "FPC<%p:%s> idx<%zd>\n", this, dumpid.c_str(), _startIndex );
}

/////////////////////////////////////////////////////////////////////////////////////////////////

int ParsedFunctionNode::parse(GlSlFxParser* parser, const ork::ScannerView& view) {
  int i         = 0;
  view.dump("pfnstart");
  auto open_tok = view.token(i);
  OrkAssert(open_tok->text == "(");
  i++;
  /////////////////////////////////
  // arguments
  /////////////////////////////////
  bool args_done       = false;
  const Token* dirspec = nullptr;
  while (false == args_done) {
    auto argtype_tok = view.token(i);
    if (argtype_tok->text == ")") {
      args_done = true;
      i++;
    } else if (argtype_tok->text == "in") {
      dirspec = argtype_tok;
      i++;
    } else if (argtype_tok->text == "out") {
      dirspec = argtype_tok;
      i++;
    } else if (argtype_tok->text == "inout") {
      dirspec = argtype_tok;
      i++;
    } else {
      i++;
      auto nam_tok = view.token(i);
      i++;
      auto argnode        = std::make_shared<FunctionArgumentNode>();
      argnode->_type      = argtype_tok;
      argnode->_name      = nam_tok;
      argnode->_direction = dirspec;
      dirspec             = nullptr;
      //_arguments.push_back(argnode);
      auto try_comma = view.token(i)->text;
      if (try_comma == ",") {
        i++;
      }
    }
  }

  /////////////////////////////////
  // body
  /////////////////////////////////

  auto open_body_tok = view.token(i);
  assert(open_body_tok->text == "{");
  bool done = false;
  ScannerView body_view(view,i);
  body_view.dump("pfnbody");
  FnParseContext pctx(parser, &body_view);
  int j = 0;
  while (not done) {
    auto try_tok     = body_view.token(j)->text;
    if (auto m = VariableDeclaration::match(pctx)) {
      auto parsed = m->parse();
      j += m->_count;
      //_elements.push_back(parsed._node);
    } else if (auto m = CompoundStatement::match(pctx)) {
      auto parsed = m->parse();
      j += m->_count;
      //_elements.push_back(parsed._node);
    } else {
      body_view.dump("ParsedFunctionNode::XXX");
      OrkAssert(false);
    }
    done = j >= body_view._indices.size();
  }
  i+=j;
  auto close_tok = view.token(i - 1);
  assert(close_tok->text == "}");
  return i;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void ParsedFunctionNode::emit(ork::lev2::glslfx::shaderbuilder::BackEnd& backend) const {
  for (auto elem : _elements)
    elem->emit(backend);
  assert(false); // not implemented yet...
}


/////////////////////////////////////////////////////////////////////////////////////////////////
*/
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx::parser
/////////////////////////////////////////////////////////////////////////////////////////////////
