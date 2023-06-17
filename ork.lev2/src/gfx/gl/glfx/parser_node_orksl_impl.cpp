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
#include <ork/kernel/string/string.h>

#if defined(USE_ORKSL_LANG)

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan = logger()->createChannel("ORKSLIMPL",fvec3(1,1,.9),true);
static logchannel_ptr_t logchan_grammar = logger()->createChannel("ORKSLGRAM",fvec3(1,1,.8),true);
static logchannel_ptr_t logchan_lexer = logger()->createChannel("ORKSLLEXR",fvec3(1,1,.7),true);

using peg_parser_ptr_t = std::shared_ptr<peg::parser>;

struct ScannerLightView{
  ScannerLightView(const ScannerView& inp_view)
    : _input_view(inp_view)
    , _start(inp_view._start)
    , _end(inp_view._end)
  {
  }
  ScannerLightView(const ScannerLightView& oth)
    : _input_view(oth._input_view)
    , _start(oth._start)
    , _end(oth._end)
  {
  }
  const Token* token(size_t i) const{
    return _input_view.token(i);
  }
  TokenClass token_class(size_t i) const{
    auto tok = token(i);
    return (TokenClass) tok->_class;
  }
  const ScannerView& _input_view;
  size_t _start = -1; 
  size_t _end = -1;   
};
using scannerlightview_ptr_t = std::shared_ptr<ScannerLightView>;
using scannerlightview_constptr_t = std::shared_ptr<const ScannerLightView>;
using matcher_fn_t = std::function<scannerlightview_ptr_t(scannerlightview_constptr_t& inp_view)>;

//////////////////////////////////////////////////////////////

struct Matcher {
  Matcher(matcher_fn_t match_fn)
    : _match_fn(match_fn) {
  }
  scannerlightview_ptr_t match(scannerlightview_constptr_t inp_view) const {
    return _match_fn(inp_view);
  }
  matcher_fn_t _match_fn;
};

using matcher_ptr_t = std::shared_ptr<Matcher>;

//////////////////////////////////////////////////////////////

struct OrkslPEG{

  OrkslPEG(){

    auto equals = matcherForTokenClass(TokenClass::EQUALS);
    auto semicolon = matcherForTokenClass(TokenClass::SEMICOLON);
    auto comma = matcherForTokenClass(TokenClass::COMMA);
    auto lparen = matcherForTokenClass(TokenClass::L_PAREN);
    auto rparen = matcherForTokenClass(TokenClass::R_PAREN);
    auto lcurly = matcherForTokenClass(TokenClass::L_CURLY);
    auto rcurly = matcherForTokenClass(TokenClass::R_CURLY);
    auto lsquare = matcherForTokenClass(TokenClass::L_SQUARE);
    auto rsquare = matcherForTokenClass(TokenClass::R_SQUARE);

    auto paramDeclaration = createMatcher([=](scannerlightview_constptr_t inp_view)->scannerlightview_ptr_t{
      return nullptr;
    });

    auto params = createMatcher([=](scannerlightview_constptr_t inp_view)->scannerlightview_ptr_t{
      auto next = lparen->match(inp_view);
      if(next){
        next = paramDeclaration->match(next);

        OrkAssert(false);
        return next;
      }
      return nullptr;
    });

    _matcher_top = createMatcher([=](scannerlightview_constptr_t inp_view)->scannerlightview_ptr_t{
      auto m_lparen = lparen->match(inp_view);
      if(m_lparen){
        auto m_params = params->match(m_lparen);
        if(m_params){
          auto m_rparen = rparen->match(m_params);
          if(m_rparen){
            auto rval = std::make_shared<ScannerLightView>(*inp_view);
            rval->_start = m_lparen->_start;
            rval->_end = m_rparen->_start;
            return rval;
          }
        }
      }
      return nullptr;
    });

  };

  //////////////////////////////////////////////////////////////////////

  scannerlightview_ptr_t match_top(const ScannerView& inp_view){
    auto slv = std::make_shared<ScannerLightView>(inp_view);
    return _matcher_top->match(slv);
  }

  //////////////////////////////////////////////////////////////////////

  matcher_ptr_t matcherForTokenClass(TokenClass tokclass){
    auto match_fn = [tokclass](scannerlightview_constptr_t slv) -> scannerlightview_ptr_t {
      auto slv_tokclass = slv->token_class(0);
      if(slv_tokclass==tokclass){
        auto slv_out = std::make_shared<ScannerLightView>(*slv);
        slv_out->_start++;
        return slv_out;
      }
      return nullptr;
    };
    return createMatcher(match_fn);
  }

  //////////////////////////////////////////////////////////////////////

  matcher_ptr_t matcherSequence(std::vector<matcher_ptr_t> matchers){
    auto match_fn = [matchers](scannerlightview_constptr_t slv) -> scannerlightview_ptr_t {
      auto slv_out = std::make_shared<ScannerLightView>(*slv);
      for(auto m:matchers){
        slv_out = m->match(slv_out);
        if(nullptr==slv_out)
          return nullptr;
      }
      return slv_out;
    };
    return createMatcher(match_fn);
  }

  //////////////////////////////////////////////////////////////////////

  matcher_ptr_t matcherOneOrMore(matcher_ptr_t matcher){
    auto match_fn = [matcher](scannerlightview_constptr_t slv) -> scannerlightview_ptr_t {
      auto slv_out = std::make_shared<ScannerLightView>(*slv);
      while(true){
        auto slv_next = matcher->match(slv_out);
        if(nullptr==slv_next)
          break;
        slv_out = slv_next;
      }
      return slv_out;
    };
    return createMatcher(match_fn);
  }

  //////////////////////////////////////////////////////////////////////
  
  matcher_ptr_t createMatcher(matcher_fn_t match_fn){
    auto matcher = std::make_shared<Matcher>(match_fn);
    _matchers.insert(matcher);
    return matcher;
  }

  //////////////////////////////////////////////////////////////////////

  matcher_ptr_t _matcher_top;
  std::unordered_set<matcher_ptr_t> _matchers;
};

//////////////////////////////////////////////////////////////

struct _ORKSL_IMPL {
  _ORKSL_IMPL(OrkSlFunctionNode* node);
  peg_parser_ptr_t _peg_parser;
  OrkslPEG _newpeg;
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

  //////////////////////////////////////////////////////////////////////////////////
  // always put top first
  //////////////////////////////////////////////////////////////////////////////////

  std::string peg_rules = R"(
    ################################################
    # OrkSl TOP Level Grammar
    ################################################

    functionDefinition <- L_PAREN params? R_PAREN L_CURLY statements? R_CURLY

  )";
    

  ////////////////////////////////////////////////
  // parser rules
  ////////////////////////////////////////////////

  peg_rules += R"(

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

  )";

  //////////////////////////////////////////////////////////////////////////////////

  peg_rules += R"(
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

)";

  ////////////////////////////////////////////////
  peg_rules += "    ################################################\n\n";
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
  peg_rules += "    ################################################\n\n";
  peg_rules += FormatString("    BUILTIN_TYPENAME <- (");
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

  logchan_grammar->log( "PEG RULES:");
  auto grammar_lines = SplitString(peg_rules,'\n');
  int iline = 0;
  for( auto l : grammar_lines ){
    logchan_grammar->log( "%03d: %s", iline, l.c_str() );
    iline++;
  }

  logchan_grammar->log( "############################################" );

  _peg_parser = std::make_shared<peg::parser>();

  _peg_parser->set_logger([](size_t line,               //
                             size_t col,                //
                             const std::string& msg,    //
                             const std::string& rule) { //
    std::cerr << line << ":" << col << ": " << rule << " : " << msg << "\n";
  });

  auto& parser = *_peg_parser;

  parser.load_grammar(peg_rules);

  // validate rules

  if( not static_cast<bool>(parser) ){

    auto as_lines = SplitString(peg_rules,'\n');
    int iline = 1;
    for( auto l: as_lines ){
      logchan_grammar->log( "%03d: %s", iline, l.c_str() );
      iline++;
    }
    OrkAssert(false);

  }

  ///////////////////////////////////////////////////////////


  auto impl_default_handler = [&parser](std::string key){
    parser[key.c_str()] = [key](const peg::SemanticValues& vs) {
      auto tok = vs.token_to_string(0);
      logchan->log("%s: %s", key.c_str(), tok.c_str());
    };
  };

  ///////////////////////////////////////////////////////////
  // keyword / typename / identifier semantic actions
  ///////////////////////////////////////////////////////////

  impl_default_handler("KEYWORD");
  impl_default_handler("TYPENAME");
  //impl_default_handler("IDENTIFIER");

  parser["IDENTIFIER"].predicate = [valid_typenames,valid_keywords](const peg::SemanticValues& vs, const std::any& /*dt*/, std::string& msg) {
    auto tok = vs.token_to_string(0);
    auto itt = valid_typenames.find(tok);
    auto itk = valid_keywords.find(tok);
    bool rval = (itt==valid_typenames.end()) and (itk==valid_keywords.end());
    if(rval){
      logchan->log("IDENTIFIER: %s", tok.c_str());
    }
    return rval;
  };

  ///////////////////////////////////////////////////////////
  // hierarchy token semantic actions
  ///////////////////////////////////////////////////////////

  impl_default_handler("BUILTIN_TYPENAME");

  impl_default_handler("L_PAREN");
  impl_default_handler("R_PAREN");
  impl_default_handler("L_CURLY");
  impl_default_handler("R_CURLY");
  impl_default_handler("L_SQUARE");
  impl_default_handler("R_SQUARE");
  impl_default_handler("DOT");
  impl_default_handler("COMMA");
  impl_default_handler("EQUALS");
  impl_default_handler("SEMI_COLON");
  //impl_default_handler("WHITESPACE");
  //impl_default_handler("INTEGER");
  //impl_default_handler("FLOAT");
  
  ///////////////////////////////////////////////////////////
  // language construct semantic actions
  ///////////////////////////////////////////////////////////

  impl_default_handler("functionDefinition");

  impl_default_handler("dataType");
  impl_default_handler("variableDeclaration");
  impl_default_handler("paramDeclaration");
  impl_default_handler("params");
  impl_default_handler("statement");
  impl_default_handler("statements");
  impl_default_handler("functionCall");
  impl_default_handler("returnStatement");
  impl_default_handler("assignment");
  impl_default_handler("expression");
  impl_default_handler("vecMatAccess");
  impl_default_handler("component");
  impl_default_handler("arguments");

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

    internals->_newpeg.match_top(view);
    OrkAssert(false);
    /*std::string input = view.asString(true);

    auto str_start = input.c_str();
    auto str_end   = str_start + input.size();

    printf("input<%s>\n", input.c_str() );
    printf("input.st<%c>\n", *str_start);
    printf("input.en<%c>\n", *str_end);

    bool ret = internals->_peg_parser->parse(input);

    OrkAssert(ret==true);*/

  } catch (const std::exception& e) {
    //std::cout << e.what() << '\n';
    //OrkAssert(false);
  }
  return 0;
}
void OrkSlFunctionNode::emit(shaderbuilder::BackEnd& backend) const {
  OrkAssert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx::parser
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif