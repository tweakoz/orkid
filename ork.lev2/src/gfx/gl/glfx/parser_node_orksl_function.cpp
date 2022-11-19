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
#include <peglib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

using peg_parser_ptr_t = std::shared_ptr<peg::parser>;

struct _orksl_parser_internals{
  _orksl_parser_internals();

  peg_parser_ptr_t _peg_parser;

};

_orksl_parser_internals::_orksl_parser_internals(){

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

  std::string peg_rules = R"(
    # OrkSl Grammar
    TEST_D  <- TEST_A L_CURLY
    KW_OR_ID  <- < [A-Za-z_.][-A-Za-z_.0-9]* >
    COMMA  <- ','
    L_PAREN  <- '('
    R_PAREN  <- ')'
    L_CURLY  <- '{'
    TEST_A  <- L_PAREN TEST_C R_PAREN
    TEST_B  <- KW_OR_ID KW_OR_ID
    TEST_B2 <- KW_OR_ID KW_OR_ID COMMA
    TEST_C  <- (TEST_B / TEST_B2) +
    %whitespace <- [ \t]*
  )";

  _peg_parser = std::make_shared<peg::parser>();

  _peg_parser->set_logger([]( size_t line, //
                              size_t col, //
                              const std::string& msg, //
                              const std::string &rule) { //
    std::cerr << line << ":" << col << ": " << msg << "\n";
  });

  auto& parser = *_peg_parser;

  parser.load_grammar(peg_rules);

  OrkAssert(static_cast<bool>(parser));

  parser["KW_OR_ID"] = [](const peg::SemanticValues &vs){
    auto tok = vs.token_to_string(0);
    printf("KW_OR_ID: %s\n", tok.c_str() );
  };
  parser["L_PAREN"] = [](const peg::SemanticValues &vs){
    auto tok = vs.token_to_string(0);
    printf("L_PAREN: %s\n", tok.c_str() );
  };
  parser["R_PAREN"] = [](const peg::SemanticValues &vs){
    auto tok = vs.token_to_string(0);
    printf("R_PAREN: %s\n", tok.c_str() );
  };
  parser["L_CURLY"] = [](const peg::SemanticValues &vs){
    auto tok = vs.token_to_string(0);
    printf("L_CURLY: %s\n", tok.c_str() );
  };
  parser["TEST_A"] = [](const peg::SemanticValues &vs){
    auto tok = vs.token_to_string(0);
    printf("TEST_A: %s\n", tok.c_str() );
  };
  parser["TEST_B"] = [](const peg::SemanticValues &vs){
    auto tok = vs.token_to_string(0);
    printf("TEST_B: %s\n", tok.c_str() );
  };
  parser["TEST_B2"] = [](const peg::SemanticValues &vs){
    auto tok = vs.token_to_string(0);
    printf("TEST_B2: %s\n", tok.c_str() );
  };
  parser["TEST_C"] = [](const peg::SemanticValues &vs){
    auto tok = vs.token_to_string(0);
    printf("TEST_C: %s\n", tok.c_str() );
  };
  parser["TEST_D"] = [](const peg::SemanticValues &vs){
    auto tok = vs.token_to_string(0);
    printf("TEST_D: %s\n", tok.c_str() );
  };

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

    std::string input = view.asString();

    printf("input<%s>\n", input.c_str());

    auto str_start = input.c_str();
    auto str_end = str_start+14; //input.size();

    printf("input.st<%c>\n", *str_start);
    printf("input.en<%c>\n", *str_end);

    auto ret = internals->_peg_parser->parse(input);

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
