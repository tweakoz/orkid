////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "parser_lang.inl"

///////////////////////////////////////////////////////////////////////////////

std::string scanner_spec = R"xxx(
    macro(M1)           <- "xyz"
    MULTI_LINE_COMMENT  <- "\/\*([^*]|\*+[^/*])*\*+\/"
    SINGLE_LINE_COMMENT <- "\/\/.*[\n\r]"
    WHITESPACE          <- "\s+"
    NEWLINE             <- "[\n\r]+"
    KW_OR_ID            <- "[a-zA-Z_][a-zA-Z0-9_]*"
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
)xxx";

///////////////////////////////////////////////////////////////////////////////

std::string parser_spec = R"xxx(
    datatype <- sel{KW_FLOAT KW_INT}
    argument_decl <- [ datatype KW_OR_ID opt{COMMA} ]
    number <- sel{FLOATING_POINT INTEGER}
    variableDeclaration <- [datatype KW_OR_ID]
    variableReference <- KW_OR_ID

    product <- [ primary opt{ [STAR primary] } ]

    sum <- sel{
        [ product PLUS product ] : "add"
        [ product MINUS product ] : "sub"
        product : "pro"
    }

    expression <- sum

    term <- [ L_PAREN expression R_PAREN ]

    primary <- sel{ FLOATING_POINT
                    INTEGER
                    variableReference
                    term
    }

    assignment_statement <- [
        sel { variableDeclaration variableReference } : "ass1of"
        EQUALS
        expression
    ]

    statement <- sel{ 
        [ assignment_statement SEMICOLON ]
        SEMICOLON
    }

    funcdef <- [
        FUNCTION
        KW_OR_ID
        L_PAREN
        argument_decl : "args"
        R_PAREN
        L_CURLY
        zom{statement} : "statements"
        R_CURLY
    ]
    
    funcdefs <- zom{funcdef} : "xxx"

)xxx";

///////////////////////////////////////////////////////////////////////////////

struct MyParser2 : public Parser {

  MyParser2() {
    this->loadScannerSpec(scanner_spec);
    this->loadParserSpec(parser_spec);
    ///////////////////////////////////////////////////////////
    auto expression = rule("expression");
    auto product    = rule("product");
    auto variableDeclaration = rule("variableDeclaration");
    auto variableReference = rule("variableReference");
    auto funcdef = rule("funcdef");
    auto semicolon = rule("SEMICOLON");
    auto assignment_statement = rule("assignment_statement");
    ///////////////////////////////////////////////////////////
    on("FLOATING_POINT", [=](match_ptr_t match) { //
      auto ast_node    = match->_user.makeShared<AST::FloatLiteral>();
      auto impl        = match->asShared<ClassMatch>();
      ast_node->_value = std::stof(impl->_token->text);
      printf("ON FLOATING_POINT<%g>\n", ast_node->_value);
    });
    ///////////////////////////////////////////////////////////
    on("INTEGER", [=](match_ptr_t match) { //
      auto ast_node    = match->_user.makeShared<AST::IntegerLiteral>();
      auto impl        = match->asShared<ClassMatch>();
      ast_node->_value = std::stoi(impl->_token->text);
      printf("ON INTEGER<%d>\n", ast_node->_value);
    });
    ///////////////////////////////////////////////////////////
    on("datatype", [=](match_ptr_t match) { //
      auto selected   = match->asShared<OneOf>()->_selected;
      auto ast_node   = match->_user.makeShared<AST::DataType>();
      ast_node->_name = selected->_impl.get<wordmatch_ptr_t>()->_token->text;
      printf("ON datatype<%s>\n", ast_node->_name.c_str());
    });
    ///////////////////////////////////////////////////////////
    on("variableReference", [=](match_ptr_t match) { //
      auto seq       = match->asShared<Sequence>();
      auto kwid      = seq->_items[0]->asShared<ClassMatch>()->_token->text;
      auto var_ref   = match->_user.makeShared<AST::VariableReference>();
      var_ref->_name = kwid;
      printf("ON variableReference<%s>\n", var_ref->_name.c_str());
    });
    ///////////////////////////////////////////////////////////
    on("term", [=](match_ptr_t match) {
      auto ast_node = match->_user.makeShared<AST::Term>();
      auto selected = match->asShared<Sequence>()->_items[1];
      printf("ON term<%s>\n", selected->_matcher->_name.c_str());
      if (selected->_matcher == expression) {
        ast_node->_subexpression = selected->_user.getShared<AST::Expression>();
      }
    });
    ///////////////////////////////////////////////////////////
    on("primary", [=](match_ptr_t match) {
      auto ast_node   = match->_user.makeShared<AST::Primary>();
      auto selected   = match->asShared<OneOf>()->_selected;
      ast_node->_impl = selected->_user;
      printf("ON primary<%s>\n", selected->_matcher->_name.c_str());
    });
    ///////////////////////////////////////////////////////////
    on("product", [=](match_ptr_t match) {
      auto ast_node = match->_user.makeShared<AST::Product>();
      auto selected = match->asShared<OneOf>()->_selected;
      auto sel_seq  = selected->asShared<Sequence>();
      auto primary  = sel_seq->_items[0]->_user.getShared<AST::Primary>();
      auto m1zom    = sel_seq->itemAsShared<NOrMore>(1);
      ast_node->_primaries.push_back(primary);
      printf("ON product<%s>\n", selected->_matcher->_name.c_str());
      if (m1zom->_items.size()) {
        for (auto i : m1zom->_items) {
          auto seq = i->asShared<Sequence>();
          // star is implied...
          primary = seq->_items[1]->_user.getShared<AST::Primary>();
          ast_node->_primaries.push_back(primary);
        }
      }
    });
    ///////////////////////////////////////////////////////////
    on("sum", [=](match_ptr_t match) {
      auto ast_node = match->_user.makeShared<AST::Sum>();
      auto selected = match->asShared<OneOf>()->_selected;
      printf("ON sum<%s>\n", selected->_matcher->_name.c_str());
      if (selected->_matcher == product) {
        ast_node->_left = selected->_user.getShared<AST::Product>();
        ast_node->_op   = '_';
      } else if (selected->_matcher->_name == "add1") {
        auto seq         = selected->asShared<Sequence>();
        ast_node->_left  = seq->_items[0]->_user.getShared<AST::Product>();
        ast_node->_right = seq->_items[2]->_user.getShared<AST::Product>();
        ast_node->_op    = '+';
      } else if (selected->_matcher->_name == "add2") {
        auto seq         = selected->asShared<Sequence>();
        ast_node->_left  = seq->_items[0]->_user.getShared<AST::Product>();
        ast_node->_right = seq->_items[2]->_user.getShared<AST::Product>();
        ast_node->_op    = '-';
      } else {
        OrkAssert(false);
      }
    });
    ///////////////////////////////////////////////////////////
    on("expression", [=](match_ptr_t match) { //
      auto ast_node  = match->_user.makeShared<AST::Expression>();
      auto seq       = match->asShared<Sequence>();
      ast_node->_sum = seq->_items[0]->_user.getShared<AST::Sum>();
      printf("ON expression<%s>\n", match->_matcher->_name.c_str());
    });
    ///////////////////////////////////////////////////////////
    on("assignment_statement", [=](match_ptr_t match) { //
      auto ast_node = match->_user.makeShared<AST::AssignmentStatement>();
      auto ass1of   = match->asShared<Sequence>()->itemAsShared<OneOf>(0);
      printf("ON assignment_statement<%s>\n", match->_matcher->_name.c_str());
      if (ass1of->_selected->_matcher == variableDeclaration) {
        auto seq      = ass1of->_selected->asShared<Sequence>();
        auto datatype = seq->_items[0]->_user.getShared<AST::DataType>();
        // auto kwid = seq->_items[1]->_user.getShared<AST::KwOrId>();
        auto expr = match->asShared<Sequence>()->itemAsShared<AST::Expression>(2);
        // ast_node->_datatype = datatype;
        // ast_node->_name = kwid;
        // ast_node->_expression = expr;
      } else if (ass1of->_selected->_matcher == variableReference) {
        // auto kwid = ass1of->_selected->asShared<Sequence>()->_items[0]->_user.getShared<AST::KwOrId>();
        auto expr           = match->asShared<Sequence>()->itemAsShared<AST::Expression>(2);
        ast_node->_datatype = nullptr;
        // ast_node->_name = kwid;
        ast_node->_expression = expr;
      } else {
        OrkAssert(false);
      }
    });
    ///////////////////////////////////////////////////////////
    on("funcdef", [=](match_ptr_t match) {
      auto seq     = match->asShared<Sequence>();
      auto fn_name = seq->itemAsShared<ClassMatch>(1);
      auto args    = seq->itemAsShared<NOrMore>(3);
      auto stas    = seq->itemAsShared<NOrMore>(6);
      printf(
          "ON funcdef<%s> function<%s> numargs<%d> numstatements<%d>\n", //
          funcdef->_name.c_str(),                                             //
          fn_name->_token->text.c_str(),                                      //
          args->_items.size(),                                                //
          stas->_items.size());

      for (auto arg : args->_items) {
        auto argseq     = arg->asShared<Sequence>();
        auto argtype    = argseq->itemAsShared<OneOf>(0);
        auto argtypeval = argtype->asShared<WordMatch>();
        auto argname    = argseq->itemAsShared<ClassMatch>(1);
        printf("  ARG<%s> TYPE<%s>\n", argname->_token->text.c_str(), argtypeval->_token->text.c_str());
      }
      int i = 0;
      for (auto sta : stas->_items) {
        auto stasel = sta->asShared<OneOf>()->_selected;
        if (auto as_seq = stasel->tryAsShared<Sequence>()) {
          auto staseq0 = as_seq.value()->_items[0];
          if (staseq0->_matcher == assignment_statement) {
          } else if (staseq0->_matcher == semicolon) {
          } else {
            OrkAssert(false);
          }
        }
        i++;
      }
    });
    ///////////////////////////////////////////////////////////
    on("funcdefs", [=](match_ptr_t match) {
      printf( "ON funcdefs\n");
    });
    ///////////////////////////////////////////////////////////
  }
  match_ptr_t parseString(std::string parse_str) {

    _scanner->scanString(parse_str);
    _scanner->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::SINGLE_LINE_COMMENT));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::MULTI_LINE_COMMENT));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));

    auto top_view = _scanner->createTopView();
    top_view.dump("top_view");
    auto slv   = std::make_shared<ScannerLightView>(top_view);
    _fns_matcher = findMatcherByName("funcdefs");
    OrkAssert(_fns_matcher);
    auto match = this->match(slv, _fns_matcher);
    return match;
  }

  matcher_ptr_t _fns_matcher;
}; // struct MyParser

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TEST(parser2) {
  printf( "P2.TOP.A\n");
  auto parse_str =
      R"(
        ///////////////////
        // hello world
        ///////////////////
        function abc(int x, float y) {
            float a = 1.0;
            float v = 2.0;
            float b = (a+v)*7.0;
        }
        function def() {
            float X = (1.0+2.3)*7.0;
        }
    )";
  MyParser2 the_parser;
  auto match = the_parser.parseString(parse_str);
  printf( "P2.TOP.B match<%p>\n", match.get() );
  CHECK(match != nullptr);
}