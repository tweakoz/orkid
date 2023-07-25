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

#include <ork/lev2/gfx/shadlang.h>
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
namespace ork::lev2::shadlang {
/////////////////////////////////////////////////////////////////////////////////////////////////

namespace impl {
static logchannel_ptr_t logchan         = logger()->createChannel("ORKSLIMPL", fvec3(1, 1, .9), true);
static logchannel_ptr_t logchan_grammar = logger()->createChannel("ORKSLGRAM", fvec3(1, 1, .8), true);
static logchannel_ptr_t logchan_lexer   = logger()->createChannel("ORKSLLEXR", fvec3(1, 1, .7), true);

///////////////////////////////////////////////////////////////////////////////

std::string scanner_spec = R"xxx(
    macro(M1)           <- "xyz"
    MULTI_LINE_COMMENT  <- "\/\*([^*]|\*+[^/*])*\*+\/"
    SINGLE_LINE_COMMENT <- "\/\/.*[\n\r]"
    WHITESPACE          <- "\s+"
    NEWLINE             <- "[\n\r]+"
    EQUALS              <- "="
    COMMA               <- ","
    SEMICOLON           <- ";"
    L_PAREN             <- "\("
    R_PAREN             <- "\)"
    L_CURLY             <- "\{"
    R_CURLY             <- "\}"
    STAR                <- "\*"
    PLUS                <- "\+"
    MINUS               <- "\-"
    FLOATING_POINT      <- "-?(\d*\.?)(\d+)([eE][-+]?\d+)?"
    INTEGER             <- "-?(\d+)"
    FUNCTION            <- "function"
    KW_FLOAT            <- "float"
    KW_INT              <- "int"
    KW_OR_ID            <- "[a-zA-Z_][a-zA-Z0-9_]*"
)xxx";

///////////////////////////////////////////////////////////////////////////////

std::string parser_spec = R"xxx(
    datatype <- sel{KW_FLOAT KW_INT}
    number <- sel{FLOATING_POINT INTEGER}
    kw_or_id <- KW_OR_ID
    l_paren <- L_PAREN
    r_paren <- R_PAREN
    plus <- PLUS
    minus <- MINUS
    star <- STAR
    l_curly <- L_CURLY
    r_curly <- R_CURLY
    semicolon <- SEMICOLON
    equals <- EQUALS
    function <- FUNCTION
        
    argument_decl <- [ datatype kw_or_id opt{COMMA} ]
    variableDeclaration <- [datatype kw_or_id]
    variableReference <- kw_or_id
    funcname <- kw_or_id

    product <- [ primary opt{ [star primary] } ]

    sum <- sel{
        [ product plus product ] : "add"
        [ product minus product ] : "sub"
        product : "pro"
    }

    expression <- [ sum ]

    term <- [ l_paren expression r_paren ]

    primary <- sel{ number
                    variableReference
                    term
    }

    assignment_statement <- [
        sel { variableDeclaration variableReference } : "ass1of"
        equals
        expression
    ]

    statement <- sel{ 
        [ assignment_statement semicolon ]
        semicolon
    }

    funcdef <- [
        function
        funcname
        l_paren
        zom{argument_decl} : "args"
        r_paren
        l_curly
        zom{statement} : "statements"
        r_curly
    ]
    
    funcdefs <- zom{funcdef} : "xxx"

)xxx";

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> ast_create(match_ptr_t m) {
  auto sh = std::make_shared<T>();
  m->_uservars.set<SHAST::astnode_ptr_t>("astnode", sh);
  return sh;
}

template <typename T> std::shared_ptr<T> ast_get(match_ptr_t m) {
  auto sh = m->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode").value();
  return std::dynamic_pointer_cast<T>(sh);
}

struct ShadLangParser : public Parser {

  ShadLangParser() {
    _name              = "shadlang";
    _DEBUG_MATCH       = true;
    _DEBUG_INFO        = true;
    auto scanner_match = this->loadPEGScannerSpec(scanner_spec);
    OrkAssert(scanner_match);
    auto parser_match = this->loadPEGParserSpec(parser_spec);
    OrkAssert(parser_match);
    OrkAssert(_DEBUG_MATCH);
    ///////////////////////////////////////////////////////////
    // parser should be compiled and linked at this point
    ///////////////////////////////////////////////////////////
    auto expression           = rule("expression");
    auto product              = rule("product");
    auto term                 = rule("term");
    auto primary              = rule("primary");
    auto variableDeclaration  = rule("variableDeclaration");
    auto variableReference    = rule("variableReference");
    auto funcdef              = rule("funcdef");
    auto semicolon            = rule("SEMICOLON");
    auto assignment_statement = rule("assignment_statement");
    ///////////////////////////////////////////////////////////
    onPost("variableReference", [=](match_ptr_t match) { //
        auto kwid      = match->asShared<ClassMatch>()->_token->text;
        auto var_ref   = ast_create<SHAST::VariableReference>(match);
        var_ref->_name = FormatString("VarRef<%s>", kwid.c_str() );
    });
    ///////////////////////////////////////////////////////////
    onPost("FLOATING_POINT", [=](match_ptr_t match) { //
      auto ast_node    = ast_create<SHAST::FloatLiteral>(match);
      auto impl        = match->asShared<ClassMatch>();
      ast_node->_value = std::stof(impl->_token->text);
      ast_node->_name = FormatString("FloatLiteral<%s>", impl->_token->text.c_str() );
    });
    ///////////////////////////////////////////////////////////
    onPost("INTEGER", [=](match_ptr_t match) { //
      auto ast_node    = ast_create<SHAST::IntegerLiteral>(match);
      auto impl        = match->asShared<ClassMatch>();
      ast_node->_value = std::stoi(impl->_token->text);
      ast_node->_name = FormatString("IntLiteral<%s>", impl->_token->text.c_str() );
    });
    ///////////////////////////////////////////////////////////
    onPost("datatype", [=](match_ptr_t match) { //
      auto selected   = match->asShared<OneOf>()->_selected;
      auto ast_node   = ast_create<SHAST::DataType>(match);
      ast_node->_name = FormatString("Datatype<%s>", selected->asShared<ClassMatch>()->_token->text.c_str() );
    });
    ///////////////////////////////////////////////////////////
    onPost("term", [=](match_ptr_t match) {
      auto ast_node = ast_create<SHAST::Term>(match);
      ast_node->_name = "term";
      printf( "ON term\n");
      //OrkAssert(false);
      //auto selected = match->asShared<Sequence>()->_items[1];
      //if (selected->_matcher == expression) {
        //ast_node->_subexpression = ast_get<SHAST::Expression>(selected);
     // }
    });
    ///////////////////////////////////////////////////////////
    onPost("primary", [=](match_ptr_t match) {
      auto ast_node = ast_create<SHAST::Primary>(match);
      ast_node->_name = "primary";
      auto selected = match->asShared<OneOf>()->_selected;
    });
    ///////////////////////////////////////////////////////////
    onPost("product", [=](match_ptr_t match) {
      auto ast_node     = ast_create<SHAST::Product>(match);
      ast_node->_name = "product";
      auto seq          = match->asShared<Sequence>();
      auto primary1     = seq->_items[0];
      auto primary1_ast = ast_get<SHAST::Primary>(primary1);
      auto opt          = seq->itemAsShared<Optional>(1);
      auto primary2     = opt->_subitem;

      ast_node->_primaries.push_back(primary1_ast);
      if (primary2) {
        auto seq = primary2->asShared<Sequence>();
        primary2 = seq->_items[1];
        auto primary2_ast = ast_get<SHAST::Primary>(primary2);
        ast_node->_primaries.push_back(primary2_ast);

      } else { // "prI1" 

      }
    });
    ///////////////////////////////////////////////////////////
    onPost("sum", [=](match_ptr_t match) {
      auto ast_node     = ast_create<SHAST::Sum>(match);
      auto selected = match->asShared<OneOf>()->_selected;
      if (selected->_matcher == product) {
        ast_node->_left = ast_get<SHAST::Product>(selected);
        ast_node->_op   = '_';
      } else if (selected->_matcher->_name == "add1") {
        auto seq         = selected->asShared<Sequence>();
        ast_node->_left = ast_get<SHAST::Product>(seq->_items[0]);
        ast_node->_right = ast_get<SHAST::Product>(seq->_items[2]);
        ast_node->_op    = '+';
      } else if (selected->_matcher->_name == "add2") {
        auto seq         = selected->asShared<Sequence>();
        ast_node->_left = ast_get<SHAST::Product>(seq->_items[0]);
        ast_node->_right = ast_get<SHAST::Product>(seq->_items[2]);
        ast_node->_op    = '-';
      } else {
        //OrkAssert(false);
      }
    });
    ///////////////////////////////////////////////////////////
    onPost("expression", [=](match_ptr_t match) { //
      auto ast_node = ast_create<SHAST::Expression>(match);
      auto impltype = match->_impl.typestr();
      auto seq      = match->asShared<Sequence>();
      auto selected = seq->_items[0];
      // since expression is a sum, then expression match points to sum
    });
    ///////////////////////////////////////////////////////////
    onPost("assignment_statement", [=](match_ptr_t match) { //
      auto ast_node = ast_create<SHAST::AssignmentStatement>(match);
      auto ass1of   = match->asShared<Sequence>()->itemAsShared<OneOf>(0);
      if (ass1of->_selected->_matcher == variableDeclaration) {
        auto seq      = ass1of->_selected->asShared<Sequence>();
        auto datatype = ast_get<SHAST::Product>(seq->_items[0]);
        //auto expr     = match->asShared<Sequence>()->itemAsShared<SHAST::Expression>(2);
      } else if (ass1of->_selected->_matcher == variableReference) {
        auto expr             = match->asShared<Sequence>()->itemAsShared<SHAST::Expression>(2);
        ast_node->_datatype   = nullptr;
        ast_node->_expression = expr;
      } else {
        // OrkAssert(false);
      }
    });
    ///////////////////////////////////////////////////////////
    onPost("argument_decl", [=](match_ptr_t match) {
      auto ast_node = ast_create<SHAST::ArgumentDeclaration>(match);
      auto seq      = match->asShared<Sequence>();
      seq->dump("argument_decl");

      auto varname_proxy = seq->itemAsShared<Proxy>(1);
      auto varname = varname_proxy->followAsShared<ClassMatch>();

      //ast_node->_variable_name   = varname->_token->text;
      //auto seq0                  = seq->itemAsShared<OneOf>(0)->_selected;
      //auto tok                   = seq0->asShared<ClassMatch>()->_token;
      //ast_node->_datatype        = std::make_shared<SHAST::DataType>();
      //ast_node->_datatype->_name = tok->text;
      //ast_node->_name = FormatString("ArgDecl<%s %s>", tok->text.c_str(), ast_node->_variable_name.c_str() );
    });
    ///////////////////////////////////////////////////////////
    onPost("funcdef", [=](match_ptr_t match) {
      auto seq     = match->asShared<Sequence>();
      auto funcdef = ast_create<SHAST::FunctionDef>(match);

      seq->dump("funcdef");

      auto fn_name = seq->_items[1]->followImplAsShared<ClassMatch>();
      //auto fn_name = fn_name_proxy->followAsShared<ClassMatch>();
      funcdef->_name = fn_name->_token->text; //FormatString("FnDef<%s>", fn_name->_token->text.c_str() );

      auto args    = seq->itemAsShared<NOrMore>(3);
      auto stas    = seq->itemAsShared<NOrMore>(6);

      args->dump("args");

      for (auto arg : args->_items) {
        auto argseq   = arg->asShared<Sequence>();
        auto arg_decl = ast_get<SHAST::ArgumentDeclaration>(arg);
        funcdef->_arguments.push_back(arg_decl);
        //auto argtype = arg_decl->_datatype->_name;
        //auto argname = arg_decl->_variable_name;
      }

      stas->dump("stas");

      int i = 0;
      for (auto sta : stas->_items) {
        auto stasel = sta->followImplAsShared<OneOf>()->_selected;
        if (auto as_seq = stasel->tryAsShared<Sequence>()) {
          auto staseq0         = as_seq.value()->_items[0];
          auto staseq0_matcher = staseq0->_matcher;

          printf(
              "staseq0 <%p> matcher<%p:%s>\n", //
              (void*)staseq0.get(),
              staseq0_matcher.get(),
              staseq0_matcher->_name.c_str());

          if (staseq0_matcher == assignment_statement) {
            // GOT ASSIGNMENT STATEMENT
          } else {
            auto statype = staseq0->_impl.typestr();
            printf("unknown staseq0 subtype<%s>\n", statype.c_str());
            OrkAssert(false);
          }
        } else if (auto as_semi = stasel->tryAsShared<Sequence>()) {

        } else {
          auto statype = stasel->_impl.typestr();
          printf("unknown statement item type<%s>\n", statype.c_str());
          OrkAssert(false);
        }
        i++;
      }
      
    });
    ///////////////////////////////////////////////////////////
    onPost("funcdefs", [=](match_ptr_t match) {
      auto ast_node = ast_create<SHAST::FunctionDefs>(match);
      auto fndefs_inp = match->asShared<NOrMore>();
      for (auto item : fndefs_inp->_items) {
        auto funcdef = ast_get<SHAST::FunctionDef>(item);
        auto it = ast_node->_fndefs.find(funcdef->_name);
        OrkAssert(it==ast_node->_fndefs.end());
        ast_node->_fndefs[funcdef->_name] = funcdef;
      }
    });
    ///////////////////////////////////////////////////////////
  }

  /////////////////////////////////////////////////////////////////

  void _buildAstTreeVisitor(match_ptr_t the_match) {
    bool has_ast = the_match->_uservars.hasKey("astnode");
    if (has_ast){
      auto ast = the_match->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode").value();

      if(_astnodestack.size()>0){
        auto parent = _astnodestack.back();
        parent->_children.push_back(ast);
        ast->_parent = parent;
      }
      _astnodestack.push_back(ast);
    }

    for( auto c : the_match->_children ){
      _buildAstTreeVisitor(c);
    }
    if (has_ast){
      _astnodestack.pop_back();
    }
  }

  /////////////////////////////////////////////////////////////////

  void _dumpAstTreeVisitor(SHAST::astnode_ptr_t node, int indent) {
    auto indentstr = std::string(indent*2, ' ');
    printf("%s%s\n", indentstr.c_str(), node->_name.c_str());
    for (auto c : node->_children) {
      _dumpAstTreeVisitor(c, indent+1);
    }
  }

  /////////////////////////////////////////////////////////////////

  SHAST::fndefs_ptr_t parseString(std::string parse_str) {

    _scanner->scanString(parse_str);
    _scanner->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::SINGLE_LINE_COMMENT));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::MULTI_LINE_COMMENT));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));

    auto top_view = _scanner->createTopView();
    top_view.dump("top_view");
    auto slv     = std::make_shared<ScannerLightView>(top_view);
    _fns_matcher = findMatcherByName("funcdefs");
    OrkAssert(_fns_matcher);
    auto match = this->match(_fns_matcher, slv);
    OrkAssert(match);
    _buildAstTreeVisitor(match);
    auto ast_top = match->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode").value();
    printf( "///////////////////////////////\n");
    printf( "// AST TREE\n");
    printf( "///////////////////////////////\n");
    _dumpAstTreeVisitor(ast_top, 0);
    printf( "///////////////////////////////\n");
    return std::dynamic_pointer_cast<SHAST::FunctionDefs>(ast_top);
  }

  /////////////////////////////////////////////////////////////////

  matcher_ptr_t _fns_matcher;
  std::vector<SHAST::astnode_ptr_t> _astnodestack;
}; // struct ShadLangParser

} //namespace impl {

SHAST::fndefs_ptr_t parse_fndefs( const std::string& shader_text ){
    auto parser = std::make_shared<impl::ShadLangParser>();
    auto topast = parser->parseString(shader_text);
    return topast;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx::parser
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif