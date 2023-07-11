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

struct MyParser2 : public Parser {

  MyParser2() {
    _name              = "p2";
    _DEBUG_MATCH       = false;
    _DEBUG_INFO        = false;
    auto scanner_match = this->loadPEGScannerSpec(scanner_spec);
    OrkAssert(scanner_match);
    auto parser_match = this->loadPEGParserSpec(parser_spec);
    OrkAssert(parser_match);
    ///////////////////////////////////////////////////////////
    // parser should be compiled and linked at this point
    ///////////////////////////////////////////////////////////
    auto expression           = rule("expression");
    auto product              = rule("product");
    auto primary              = rule("primary");
    auto variableDeclaration  = rule("variableDeclaration");
    auto variableReference    = rule("variableReference");
    auto funcdef              = rule("funcdef");
    auto semicolon            = rule("SEMICOLON");
    auto assignment_statement = rule("assignment_statement");
    ///////////////////////////////////////////////////////////
    if (1)
      on("variableReference", [=](match_ptr_t match) { //
        auto kwid                    = match->asShared<ClassMatch>()->_token->text;
        auto var_ref                 = match->_user.makeShared<MYAST::VariableReference>();
        var_ref->_name               = kwid;
        auto v                       = match->_view;
        printf(
            "ON variableReference var<%s> st<%zu> en<%zu> matcher<%s>\n", //
            kwid.c_str(),
            v->_start,
            v->_end,                        //
            match->_matcher->_name.c_str());
      });
    ///////////////////////////////////////////////////////////
    on("FLOATING_POINT", [=](match_ptr_t match) { //
      auto ast_node    = match->_user.makeShared<MYAST::FloatLiteral>();
      auto impl        = match->asShared<ClassMatch>();
      ast_node->_value = std::stof(impl->_token->text);
      auto v                       = match->_view;
      printf("ON FLOATING_POINT<%g> st<%zu> en<%zu> \n", //
          ast_node->_value,
          v->_start,
          v->_end );

    });
    ///////////////////////////////////////////////////////////
    on("INTEGER", [=](match_ptr_t match) { //
      auto ast_node    = match->_user.makeShared<MYAST::IntegerLiteral>();
      auto impl        = match->asShared<ClassMatch>();
      ast_node->_value = std::stoi(impl->_token->text);
      auto v                       = match->_view;
      printf("ON INTEGER<%d> st<%zu> en<%zu> \n", //
        ast_node->_value,
        v->_start,
        v->_end);

    });
    ///////////////////////////////////////////////////////////
    on("datatype", [=](match_ptr_t match) { //
      auto selected   = match->asShared<OneOf>()->_selected;
      auto ast_node   = match->userMakeShared<MYAST::DataType>();
      ast_node->_name = selected->asShared<ClassMatch>()->_token->text;
      auto v          = match->_view;
      printf("ON datatype<%s> st<%zu> en<%zu>\n", ast_node->_name.c_str(), v->_start, v->_end);
    });
    ///////////////////////////////////////////////////////////
    on("term", [=](match_ptr_t match) {
      auto ast_node = match->_user.makeShared<MYAST::Term>();
      auto selected = match->asShared<Sequence>()->_items[1];
      auto v          = match->_view;
      printf("ON term<%s> st<%zu> en<%zu> \n", selected->_matcher->_name.c_str(), v->_start, v->_end);
      if (selected->_matcher == expression) {
        ast_node->_subexpression = selected->_user.getShared<MYAST::Expression>();
      }
    });
    ///////////////////////////////////////////////////////////
    on("primary", [=](match_ptr_t match) {
      auto ast_node   = match->_user.makeShared<MYAST::Primary>();
      auto selected   = match->asShared<OneOf>()->_selected;
      ast_node->_impl = selected->_user;
      auto v                       = match->_view;
      printf("ON primary<%s> st<%zu> en<%zu> \n", selected->_matcher->_name.c_str(), v->_start, v->_end);
    });
    ///////////////////////////////////////////////////////////
    on("product", [=](match_ptr_t match) {
      auto ast_node     = match->_user.makeShared<MYAST::Product>();
      auto seq          = match->asShared<Sequence>();
      auto primary1     = seq->_items[0];
      auto primary1_ast = primary1->userAsShared<MYAST::Primary>();
      auto opt          = seq->itemAsShared<Optional>(1);
      auto primary2     = opt->_subitem;

      ast_node->_primaries.push_back(primary1_ast);
      auto v                       = match->_view;

      if (primary2) {
        printf(
            "ON product pri1<%s> * pri2<%s> st<%zu> en<%zu> \n", //
            primary1->_matcher->_name.c_str(),
            primary2->_matcher->_name.c_str(),
            v->_start, v->_end);
        auto seq = primary2->asShared<Sequence>();
        primary2 = seq->_items[1];

        auto primary2_ast = primary2->userAsShared<MYAST::Primary>();
        ast_node->_primaries.push_back(primary2_ast);

      } else {
        printf(
            "ON product pri1<%s> st<%zu> en<%zu> \n", //
            primary1->_matcher->_name.c_str(),
            v->_start, v->_end);
      }
    });
    ///////////////////////////////////////////////////////////
    on("sum", [=](match_ptr_t match) {
      auto ast_node = match->userMakeShared<MYAST::Sum>();
      auto selected = match->asShared<OneOf>()->_selected;
      auto v                       = match->_view;
      printf("ON sum<%s> st<%zu> en<%zu>\n", selected->_matcher->_name.c_str(), v->_start, v->_end);
      if (selected->_matcher == product) {
        ast_node->_left = selected->_user.getShared<MYAST::Product>();
        ast_node->_op   = '_';
      } else if (selected->_matcher->_name == "add1") {
        auto seq         = selected->asShared<Sequence>();
        ast_node->_left  = seq->_items[0]->_user.getShared<MYAST::Product>();
        ast_node->_right = seq->_items[2]->_user.getShared<MYAST::Product>();
        ast_node->_op    = '+';
      } else if (selected->_matcher->_name == "add2") {
        auto seq         = selected->asShared<Sequence>();
        ast_node->_left  = seq->_items[0]->_user.getShared<MYAST::Product>();
        ast_node->_right = seq->_items[2]->_user.getShared<MYAST::Product>();
        ast_node->_op    = '-';
      } else {
        OrkAssert(false);
      }
    });
    ///////////////////////////////////////////////////////////
    on("expression", [=](match_ptr_t match) { //
      auto v        = match->_view;
      auto ast_node = match->userMakeShared<MYAST::Expression>();
      auto impltype = match->_impl.typestr();
      auto seq      = match->asShared<Sequence>();
      auto selected = seq->_items[0];
      // since expression is a sum, then expression match points to sum

      printf(
          "ON expression<%s> st<%zu> end<%zu> impltype<%s>\n", //
          match->_matcher->_name.c_str(),                      //
          v->_start,
          v->_end, //
          impltype.c_str());
    });
    ///////////////////////////////////////////////////////////
    on("assignment_statement", [=](match_ptr_t match) { //
      auto v        = match->_view;
      auto ast_node = match->_user.makeShared<MYAST::AssignmentStatement>();
      auto ass1of   = match->asShared<Sequence>()->itemAsShared<OneOf>(0);
      printf("ON assignment_statement<%s> st<%zu> en<%zu>\n", match->_matcher->_name.c_str(), v->_start, v->_end);
      if (ass1of->_selected->_matcher == variableDeclaration) {
        auto seq      = ass1of->_selected->asShared<Sequence>();
        auto datatype = seq->_items[0]->_user.getShared<MYAST::DataType>();
        auto expr     = match->asShared<Sequence>()->itemAsShared<MYAST::Expression>(2);
      } else if (ass1of->_selected->_matcher == variableReference) {
        auto expr             = match->asShared<Sequence>()->itemAsShared<MYAST::Expression>(2);
        ast_node->_datatype   = nullptr;
        ast_node->_expression = expr;
      } else {
        // OrkAssert(false);
      }
    });
    ///////////////////////////////////////////////////////////
    on("argument_decl", [=](match_ptr_t match) {
      auto v                       = match->_view;
      auto ast_node = match->_user.makeShared<MYAST::ArgumentDeclaration>();
      auto seq      = match->asShared<Sequence>();
      printf("adecl impltype<%s>\n", match->_impl.typestr().c_str());
      ast_node->_variable_name   = seq->_items[1]->asShared<ClassMatch>()->_token->text;
      auto seq0                  = seq->_items[0]->asShared<OneOf>()->_selected;
      auto tok                   = seq0->asShared<ClassMatch>()->_token;
      ast_node->_datatype        = std::make_shared<MYAST::DataType>();
      ast_node->_datatype->_name = tok->text;
    });
    ///////////////////////////////////////////////////////////
    on("funcdef", [=](match_ptr_t match) {
      auto seq     = match->asShared<Sequence>();
      auto funcdef = match->userMakeShared<MYAST::FunctionDef>();
      auto fn_name = seq->itemAsShared<ClassMatch>(1);
      auto args    = seq->itemAsShared<NOrMore>(3);
      auto stas    = seq->itemAsShared<NOrMore>(6);
      auto v       = match->_view;

      funcdef->_name = fn_name->_token->text;

      printf(
          "ON funcdef<%s> function<%s> numargs<%zu> numstatements<%zu> st<%zu> en<%zu>\n", //
          funcdef->_name.c_str(),                                                          //
          fn_name->_token->text.c_str(),                                                   //
          args->_items.size(),                                                             //
          stas->_items.size(),
          v->_start,
          v->_end);

      for (auto arg : args->_items) {
        auto argseq   = arg->asShared<Sequence>();
        auto arg_decl = arg->userAsShared<MYAST::ArgumentDeclaration>();
        funcdef->_arguments.push_back(arg_decl);
        auto argtype = arg_decl->_datatype->_name;
        auto argname = arg_decl->_variable_name;
        printf("  ARG<%s> TYPE<%s>\n", argname.c_str(), argtype.c_str());
      }
      int i = 0;
      for (auto sta : stas->_items) {
        auto stasel = sta->asShared<OneOf>()->_selected;
        if (auto as_seq = stasel->tryAsShared<Sequence>()) {
          auto staseq0         = as_seq.value()->_items[0];
          auto staseq0_matcher = staseq0->_matcher;
          if (staseq0_matcher->_proxy_target) {
            staseq0_matcher = staseq0_matcher->_proxy_target;
          }

          printf(
              "staseq0 <%p> matcher<%p:%s>\n", //
              (void*)staseq0.get(),
              staseq0_matcher.get(),
              staseq0_matcher->_name.c_str());

          if (staseq0_matcher == assignment_statement) {
            printf("GOT ASSIGNMENT STATEMENT\n");
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
    on("funcdefs", [=](match_ptr_t match) {
      printf("ON funcdefs\n");
      auto fndefs_inp = match->asShared<NOrMore>();
      for (auto item : fndefs_inp->_items) {
        auto funcdef = item->userAsShared<MYAST::FunctionDef>();
        printf("GOT FUNCDEF<%s>\n", funcdef->_name.c_str());
      }
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
    auto slv     = std::make_shared<ScannerLightView>(top_view);
    _fns_matcher = findMatcherByName("funcdefs");
    OrkAssert(_fns_matcher);
    auto match = this->match(_fns_matcher, slv);
    return match;
  }

  matcher_ptr_t _fns_matcher;
}; // struct MyParser

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TEST(parser2) {
  printf("P2.TOP.A\n");
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
  printf(
      "P2.TOP.B match<%p> matcher<%p:%s> st<%zu> en<%zu>\n", //
      match.get(),                                           //
      match->_matcher.get(),                                 //
      match->_matcher->_name.c_str(),                        //
      match->_view->_start,                                  //
      match->_view->_end);                                   //

  CHECK(match != nullptr);
}