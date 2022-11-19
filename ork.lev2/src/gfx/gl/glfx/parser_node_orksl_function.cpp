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

struct _ORKSL_IMPL {
  _ORKSL_IMPL(OrkSlFunctionNode* node);
  peg_parser_ptr_t _peg_parser;
};

using impl_ptr_t = std::shared_ptr<_ORKSL_IMPL>;

_ORKSL_IMPL::_ORKSL_IMPL(OrkSlFunctionNode* node) {

  auto glfx_parser = node->_parser;
  auto top_node    = glfx_parser->_topNode;

  using str_set_t     = std::set<std::string>;
  using str_map_t     = std::map<std::string, std::string>;
  using struct_map_t  = std::map<std::string, structnode_ptr_t>;
  using import_vect_t = std::vector<importnode_ptr_t>;

  str_set_t valid_typenames      = top_node->_validTypeNames;
  str_set_t valid_keywords       = top_node->_keywords;
  str_set_t valid_outdecos       = top_node->_validOutputDecorators;
  struct_map_t valid_structtypes = top_node->_structTypes;
  str_map_t valid_defines        = top_node->_stddefines;
  import_vect_t imports          = top_node->_imports;

  struct RR {
    RR() {
    }
    void addRule(const char* rule, int id) {
      _id2rule[id] = rule;
    }
    const std::string& rule(TokenClass id) const {
      auto it = _id2rule.find(int(id));
      OrkAssert(it != _id2rule.end());
      return it->second;
    }
    std::map<int, std::string> _id2rule;
  };
  RR _rr;

  loadScannerRules(_rr);

  ////////////////////////////////////////////////
  // parser rules
  ////////////////////////////////////////////////

  std::string peg_rules = R"(
    # OrkSl Grammar

    TOP  <- ARGUMENT_DECL_LIST L_CURLY

    L_PAREN   <- '('
    R_PAREN   <- ')'
    L_CURLY   <- '{'
    R_CURLY   <- '}'
    L_SQUARE  <- '['
    R_SQUARE  <- ']'

    COMMA  <- ','
    DOT    <- '.'

    COLON         <- ':'
    DOUBLE_COLON  <- '::'
    SEMI_COLON    <- ';'
    QUESTION_MARK <- '?'
    EQUALS        <- '='
    EQUAL_TO      <- '=='
    NOT_EQUAL_TO  <- '!='

    PLUS   <- '+'
    MINUS  <- '-'
    STAR   <- '*'
    SLASH  <- '/'
    CARET  <- '^'

    AMPERSAND    <- '&'
    PIPE         <- '|'
    LOGICAL_AND  <- '&&'
    LOGICAL_OR   <- '||'
    LEFT_SHIFT   <- '<<'
    RIGHT_SHIFT  <- '>>'

    ARGUMENT_DECL_LIST  <- L_PAREN ARG_ITEMS R_PAREN
    ARG_PAIR            <- TYPENAME KW_OR_ID
    ARG_PAIR_COMMA      <- TYPENAME KW_OR_ID COMMA
    ARG_ITEMS           <- (ARG_PAIR / ARG_PAIR_COMMA) +
    %whitespace         <- [ \t]*

    KW_OR_ID <- < [A-Za-z_.][-A-Za-z_.0-9]* >

  )";

  ////////////////////////////////////////////////

  peg_rules += FormatString("    KEYWORD <- (");
  size_t num_keywords = valid_keywords.size();
  int ik = 0;
  for( auto item : valid_keywords  ){
    bool is_last = (ik==(num_keywords-1));
    auto format_str = is_last ? "'%s'" : "'%s'/";
    peg_rules += FormatString(format_str, item.c_str() );
    ik++;
  }
  peg_rules += FormatString(")\n\n");

  ////////////////////////////////////////////////

  peg_rules += FormatString("    TYPENAME <- (");
  size_t num_typenames = valid_typenames.size();
  int it = 0;
  for( auto item : valid_typenames ){
    bool is_last = (it==(num_typenames-1));
    auto format_str = is_last ? "'%s'" : "'%s'/";
    peg_rules += FormatString(format_str, item.c_str() );
    it++;
  }
  peg_rules += FormatString(")\n\n");


  ////////////////////////////////////////////////
  // parser initialization
  ////////////////////////////////////////////////

  printf( "peg_rules: %s\n", peg_rules.c_str() );

  _peg_parser = std::make_shared<peg::parser>();

  _peg_parser->set_logger([](size_t line,               //
                             size_t col,                //
                             const std::string& msg,    //
                             const std::string& rule) { //
    std::cerr << line << ":" << col << ": " << msg << "\n";
  });

  auto& parser = *_peg_parser;

  parser.load_grammar(peg_rules);

  // validate rules

  OrkAssert(static_cast<bool>(parser));

  ///////////////////////////////////////////////////////////
  // keyword / identifier semantic actions
  ///////////////////////////////////////////////////////////

  //parser["KW_OR_ID"] = [](const peg::SemanticValues& vs) {
    //auto tok = vs.token_to_string(0);
    //printf("KW_OR_ID: %s\n", tok.c_str());
  //};
  parser["KEYWORD"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("KEYWORD: %s\n", tok.c_str());
  };
  parser["TYPENAME"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("TYPENAME: %s\n", tok.c_str());
  };
  /*parser["KEYWORD"].predicate = [valid_keywords](const peg::SemanticValues& vs, const std::any& dt, std::string& msg) {
    auto tok = vs.token_to_string(0);
    auto it = valid_keywords.find(tok);
    return (it!=valid_keywords.end());
  };*/

  parser["IDENTIFIER"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("IDENTIFIER: %s\n", tok.c_str());
  };
  //parser["IDENTIFIER"].predicate = [valid_typenames,valid_keywords](const peg::SemanticValues& vs, const std::any& /*dt*/, std::string& msg) {
  //  auto tok = vs.token_to_string(0);
  //  auto itt = valid_typenames.find(tok);
  //  auto itk = valid_keywords.find(tok);
  //  return (itt!=valid_typenames.end()) and (itk!=valid_keywords.end());
  //};
  //parser["TYPENAME"].predicate = [valid_typenames](const peg::SemanticValues& vs, const std::any& /*dt*/, std::string& msg) {
    //auto tok = vs.token_to_string(0);
    //auto it = valid_typenames.find(tok);
    //return (it!=valid_typenames.end());
  //};

  ///////////////////////////////////////////////////////////
  // hierarchy token semantic actions
  ///////////////////////////////////////////////////////////

  parser["L_PAREN"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("L_PAREN: %s\n", tok.c_str());
  };
  parser["R_PAREN"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("R_PAREN: %s\n", tok.c_str());
  };
  parser["L_CURLY"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("L_CURLY: %s\n", tok.c_str());
  };
  parser["R_CURLY"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("R_CURLY: %s\n", tok.c_str());
  };
  parser["L_SQUARE"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("L_SQUARE: %s\n", tok.c_str());
  };
  parser["R_SQUARE"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("R_SQUARE: %s\n", tok.c_str());
  };
  parser["DOT"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("DOT: %s\n", tok.c_str());
  };

  ///////////////////////////////////////////////////////////
  // language construct semantic actions
  ///////////////////////////////////////////////////////////

  parser["ARGUMENT_DECL_LIST"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("ARGUMENT_DECL_LIST: %s\n", tok.c_str());
  };
  parser["ARG_PAIR"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("ARG_PAIR: %s\n", tok.c_str());
  };
  parser["ARG_PAIR_COMMA"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("ARG_PAIR_COMMA: %s\n", tok.c_str());
  };
  parser["ARG_ITEMS"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("ARG_ITEMS: %s\n", tok.c_str());
  };
  parser["TOP"] = [](const peg::SemanticValues& vs) {
    auto tok = vs.token_to_string(0);
    printf("TOP: %s\n", tok.c_str());
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

svar16_t OrkSlFunctionNode::_getimpl(OrkSlFunctionNode* node) {
  static auto _gint = std::make_shared<_ORKSL_IMPL>(node);
  return _gint;
}

OrkSlFunctionNode::OrkSlFunctionNode(parser_rawptr_t parser)
    : AstNode(parser) {
}

int OrkSlFunctionNode::parse(const ScannerView& view) {

  auto internals = _getimpl(this).get<impl_ptr_t>();

  int i = 0;
  view.dump("OrkSlFunctionNode::start");
  auto open_tok = view.token(i);
  OrkAssert(open_tok->text == "(");
  i++;

  try {

    std::string input = view.asString();

    printf("input<%s>\n", input.c_str());

    auto str_start = input.c_str();
    auto str_end   = str_start + 14; // input.size();

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

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx::parser
/////////////////////////////////////////////////////////////////////////////////////////////////
