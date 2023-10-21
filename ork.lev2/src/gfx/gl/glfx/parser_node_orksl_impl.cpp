////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/util/logger.h>
#include <ork/util/parser.h>
#include <ork/kernel/string/string.h>

#if defined(USE_ORKSL_LANG)

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

/*
################################################
# OrkSl TOP Level Grammar
################################################

functionDefinition <- L_PAREN params? R_PAREN L_CURLY statements? R_CURLY

################################################
# OrkSl High Level Grammar
################################################

params <- paramDeclaration (COMMA paramDeclaration)*

statements <- (statement)*

statement <- statement_sub? SEMI_COLON

statement_sub <- assignment_statement / functionCall / variableDeclaration / returnStatement

functionCall <- IDENTIFIER L_PAREN arguments? R_PAREN
returnStatement <- 'return' expression
assignment <- IDENTIFIER EQUALS expression
assignment_statement <- variableDeclaration EQUALS expression 
        
expression <- IDENTIFIER / NNUMBER / functionCall / vecMatAccess

vecMatAccess <- IDENTIFIER (L_SQUARE NNUMBER R_SQUARE / DOT component) 

component <- ('x' / 'y' / 'z' / 'w' / 'r' / 'g' / 'b' / 'a')

arguments <- expression (COMMA expression)*

IDENTIFIER <- < [A-Za-z_][-A-Za-z_0-9]* > 

dataType <- BUILTIN_TYPENAME 

variableDeclaration <- dataType IDENTIFIER
paramDeclaration <- dataType IDENTIFIER

################################################
# OrkSl Low Level Grammar
################################################

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
STAR_EQUALS   <- '*='
PLUS_EQUALS   <- '+='
ASSIGNMENT_OP <- (EQUALS/STAR_EQUALS/PLUS_EQUALS)

EQUAL_TO               <- '=='
NOT_EQUAL_TO           <- '!='
LESS_THAN              <- '<'
GREATER_THAN           <- '>'
LESS_THAN_EQUAL_TO     <- '<'
GREATER_THAN_EQUAL_TO  <- '>'

PLUS   <- '+'
MINUS  <- '-'
STAR   <- '*'
SLASH  <- '/'
CARET  <- '^'
EXCLAMATION  <- '^'
TILDE  <- '~'

AMPERSAND    <- '&'
PIPE         <- '|'
LOGICAL_AND  <- '&&'
LOGICAL_OR   <- '||'
LEFT_SHIFT   <- '<<'
RIGHT_SHIFT  <- '>>'

PLUS_PLUS    <- '++'
MINUS_MINUS  <- '--'

INTEGER          <- (HEX_INTEGER/DEC_INTEGER)
FLOAT            <- <MINUS? [0-9]+ '.' [0-9]*>
DEC_INTEGER      <- <MINUS? [0-9]+ 'u'?>
HEX_INTEGER      <- < ('x'/'0x') [0-9a-fA-F]+ 'u'? >
NUMBER           <- (FLOAT/INTEGER)
NNUMBER <- [0-9]+ (DOT [0-9]+)? 

WHITESPACE          <- [ \t\r\n]*
%whitespace         <- WHITESPACE

TYPENAME <- (BUILTIN_TYPENAME)
*/
  /////////////////////////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan         = logger()->createChannel("ORKSLIMPL", fvec3(1, 1, .9), true);
static logchannel_ptr_t logchan_grammar = logger()->createChannel("ORKSLGRAM", fvec3(1, 1, .8), true);
static logchannel_ptr_t logchan_lexer   = logger()->createChannel("ORKSLLEXR", fvec3(1, 1, .7), true);



//////////////////////////////////////////////////////////////

struct _ORKSL_IMPL : public Parser {
  _ORKSL_IMPL(OrkSlFunctionNode* node);

  match_ptr_t match_fndef(const ScannerView& inp_view);
  //

  void loadGrammar();

  matcher_ptr_t _matcher_fndef;
  topnode_ptr_t _topnode;

};

//////////////////////////////////////////////////////////////////////

match_ptr_t _ORKSL_IMPL::match_fndef(const ScannerView& inp_view) {
  auto slv = std::make_shared<ScannerLightView>(inp_view);
  return match(_matcher_fndef,slv);
}

//////////////////////////////////////////////////////////////

#define MATCHER(x) auto x = createMatcher([=](matcher_ptr_t par_matcher,scannerlightview_constptr_t inp_view)->match_ptr_t

void _ORKSL_IMPL::loadGrammar(){
  auto equals    = matcherForTokenClass(TokenClass::EQUALS);
  auto semicolon = matcherForTokenClass(TokenClass::SEMICOLON);
  auto comma     = matcherForTokenClass(TokenClass::COMMA);
  auto lparen    = matcherForTokenClass(TokenClass::L_PAREN);
  auto rparen    = matcherForTokenClass(TokenClass::R_PAREN);
  auto lcurly    = matcherForTokenClass(TokenClass::L_CURLY);
  auto rcurly    = matcherForTokenClass(TokenClass::R_CURLY);
  auto lsquare   = matcherForTokenClass(TokenClass::L_SQUARE);
  auto rsquare   = matcherForTokenClass(TokenClass::R_SQUARE);
  auto udecint    = matcherForTokenClass(TokenClass::UNSIGNED_DECIMAL_INTEGER);
  auto uhexint    = matcherForTokenClass(TokenClass::HEX_INTEGER);
  auto miscint    = matcherForTokenClass(TokenClass::MISC_INTEGER);
  auto floattok    = matcherForTokenClass(TokenClass::FLOATING_POINT);
  /////////////////////////////////////////////////////
  /*MATCHER(datatype) {
    auto tok0 = inp_view->token(0);
    auto it = _topnode->_validTypeNames.find(tok0->text);
    if( it != _topnode->_validTypeNames.end() ){
      OrkAssert(false);
      return nullptr;
    }
    else{
      printf( "tok0<%s> not a datatype\n", tok0->text.c_str() );
      return nullptr;
    }
  });
  MATCHER(keyword_return){
    auto tok0 = inp_view->token(0);
    if(tok0->text == "return"){
      OrkAssert(false);
      return nullptr;
    }
    else{
      return nullptr;
    }
  });
  /////////////////////////////////////////////////////
  MATCHER(identifier) {
    return nullptr;
  });
  /////////////////////////////////////////////////////
  auto number = oneOf({
    udecint,
    uhexint,
    miscint,
    floattok
  });
  /////////////////////////////////////////////////////
  auto paramDeclaration    = sequence({datatype, identifier});
  auto variableDeclaration = sequence({datatype, identifier});
  /////////////////////////////////////////////////////
  auto params = sequence({
       paramDeclaration,
         zeroOrMore(
           sequence({
             comma, paramDeclaration
           }) // sequence
         ) // zeroOrMore
     }); // sequence
  /////////////////////////////////////////////////////
  auto arguments = sequence({});
       //expression,
         //zeroOrMore(
           //sequence({
             //comma, expression
           //}) // sequence
         //) // zeroOrMore
     //}); // sequence
  /////////////////////////////////////////////////////
  auto functionCall = sequence({
    identifier,
    lparen,
    zeroOrMore(arguments),
    rparen
  }); 
  /////////////////////////////////////////////////////
  auto expression = oneOf({
    identifier,
    number,
    functionCall //,
    //vecMatAccess
  });
  /////////////////////////////////////////////////////
  auto returnStatement = sequence({
    keyword_return,
    expression
  });
  /////////////////////////////////////////////////////
  auto statementSub = oneOf({
    //assignment_statement,
    functionCall,
    variableDeclaration,
    returnStatement
  });
  /////////////////////////////////////////////////////
  auto statement = sequence({optional(statementSub), semicolon});
  /////////////////////////////////////////////////////
  auto statements = zeroOrMore(statement);
  /////////////////////////////////////////////////////
  _matcher_fndef = createMatcher([=](matcher_ptr_t par_matcher, //
                                     scannerlightview_constptr_t inp_view) -> match_ptr_t { //
    auto seq = sequence({lparen, zeroOrMore(params), rparen, lcurly, zeroOrMore(statements), rcurly});
    return match(inp_view,seq);
  });
  */

}

//////////////////////////////////////////////////////////////
using impl_ptr_t = std::shared_ptr<_ORKSL_IMPL>;
//////////////////////////////////////////////////////////////

_ORKSL_IMPL::_ORKSL_IMPL(OrkSlFunctionNode* node) {

  auto glfx_parser = node->_parser;
  _topnode    = glfx_parser->_topNode;

  using str_set_t     = std::set<std::string>;
  using str_map_t     = std::map<std::string, std::string>;
  using struct_map_t  = std::map<std::string, structnode_ptr_t>;
  using import_vect_t = std::vector<importnode_ptr_t>;

  str_set_t valid_typenames      = _topnode->_validTypeNames;
  str_set_t valid_keywords       = _topnode->_keywords;
  str_set_t valid_outdecos       = _topnode->_validOutputDecorators;
  struct_map_t valid_structtypes = _topnode->_structTypes;
  str_map_t valid_defines        = _topnode->_stddefines;
  import_vect_t imports          = _topnode->_imports;

  struct RR {
    RR() {
    }
    void addRule(const char* rule, uint64_t id) {
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
  loadGrammar();

}

/////////////////////////////////////////////////////////////////////////////////////////////////

svar16_t OrkSlFunctionNode::_getimpl(OrkSlFunctionNode* node) {
  static auto _gint = std::make_shared<_ORKSL_IMPL>(node);
  return _gint;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

OrkSlFunctionNode::OrkSlFunctionNode(parser_rawptr_t parser)
    : AstNode(parser) {
}

/////////////////////////////////////////////////////////////////////////////////////////////////

int OrkSlFunctionNode::parse(const ScannerView& view) {

  int i = 0;
  view.dump("OrkSlFunctionNode::start");
  auto open_tok = view.token(i);
  OrkAssert(open_tok->text == "(");
  i++;

  auto impl = _getimpl(this).get<impl_ptr_t>();
  auto match = impl->match_fndef(view);
  OrkAssert(match!=nullptr);

  return 0;
}
void OrkSlFunctionNode::emit(shaderbuilder::BackEnd& backend) const {
  OrkAssert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx::parser
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif