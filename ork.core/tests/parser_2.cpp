////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "parser_lang.inl"

///////////////////////////////////////////////////////////////////////////////

std::string scanner_spec = R"xxx(
    MULTI_LINE_COMMENT <- "\/\*([^*]|\\*+[^/*])*\*+\/"
    SINGLE_LINE_COMMENT <- "\/\/.*[\n\r]"
    WHITESPACE          <- "\s+"
    NEWLINE             <- "[\n\r]+"
    KW_OR_ID            <- "[a-zA-Z_][a-zA-Z0-9_]*"
    EQUALS              <- "="
    COMMA               <- ","
    SEMICOLON           <- ";"
    L_PAREN             <- "("
    R_PAREN             <- ")"
    L_CURLY             <- "{"
    R_CURLY             <- "}"
    STAR                <- "*"
    PLUS                <- "+"
    MINUS               <- "-"
    FLOATING_POINT      <- "-?(\d*\.?)(\d+)([eE][-+]?\d+)?"
    INTEGER             <- "-?(\d+)"
    FUNCTION            <- "function"
    DT_FLOAT            <- "float"
    DT_INT              <- "int
)xxx";

///////////////////////////////////////////////////////////////////////////////

std::string parser_spec = R"xxx(
    datatype <- DT_FLOAT | DT_INT
    argument_decl <- [ datatype KW_OR_ID {COMMA} ]
    number <- FLOATING_POINT | INTEGER
    variableDeclaration <- [datatype KW_OR_ID]
    variableReference <- KW_OR_ID

    product <- [ primary zom{ [STAR primary] } ]

    sum <- oneOf {
        [ product PLUS product ] : "add"
        [ product MINUS product ] : "sub"
        product : "pro"
    }

    expression <- sum

    term <- [ L_PAREN expression R_PAREN ]

    primary <- oneOf { FLOATING_POINT
                       INTEGER
                       variableReference
                       term
    }

    assignment_statement <- [
        oneOf { variableDeclaration variableReference } : "ass1of"
        EQUALS
        expression
    ]

    statement <- oneOf { 
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
        zom{ statement } : "statements"
        R_CURLY
    ]
    
    funcdefs <- zom{ funcdef }

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
      auto impl        = match->_impl.get<classmatch_ptr_t>();
      ast_node->_value = std::stof(impl->_token->text);
    });
    ///////////////////////////////////////////////////////////
    on("INTEGER", [=](match_ptr_t match) { //
      auto ast_node    = match->_user.makeShared<AST::IntegerLiteral>();
      auto impl        = match->_impl.get<classmatch_ptr_t>();
      ast_node->_value = std::stoi(impl->_token->text);
    });
    ///////////////////////////////////////////////////////////
    on("datatype", [=](match_ptr_t match) { //
      auto selected   = match->_impl.getShared<OneOf>()->_selected;
      auto ast_node   = match->_user.makeShared<AST::DataType>();
      ast_node->_name = selected->_impl.get<wordmatch_ptr_t>()->_token->text;
    });
    ///////////////////////////////////////////////////////////
    on("variableReference", [=](match_ptr_t match) { //
      auto seq       = match->_impl.get<sequence_ptr_t>();
      auto kwid      = seq->_items[0]->_impl.getShared<ClassMatch>()->_token->text;
      auto var_ref   = match->_user.makeShared<AST::VariableReference>();
      var_ref->_name = kwid;
    });
    ///////////////////////////////////////////////////////////
    on("term", [=](match_ptr_t match) {
      auto ast_node = match->_user.makeShared<AST::Term>();
      auto selected = match->_impl.get<sequence_ptr_t>()->_items[1];
      if (selected->_matcher == expression) {
        ast_node->_subexpression = selected->_user.getShared<AST::Expression>();
      }
    });
    ///////////////////////////////////////////////////////////
    on("primary", [=](match_ptr_t match) {
      auto ast_node   = match->_user.makeShared<AST::Primary>();
      auto selected   = match->_impl.get<oneof_ptr_t>()->_selected;
      ast_node->_impl = selected->_user;
    });
    ///////////////////////////////////////////////////////////
    on("product", [=](match_ptr_t match) {
      auto ast_node = match->_user.makeShared<AST::Product>();
      auto selected = match->_impl.get<oneof_ptr_t>()->_selected;
      auto sel_seq  = selected->_impl.get<sequence_ptr_t>();
      auto primary  = sel_seq->_items[0]->_user.getShared<AST::Primary>();
      auto m1zom    = sel_seq->_items[1]->_impl.get<n_or_more_ptr_t>();
      ast_node->_primaries.push_back(primary);
      if (m1zom->_items.size()) {
        for (auto i : m1zom->_items) {
          auto seq = i->_impl.get<sequence_ptr_t>();
          // star is implied...
          primary = seq->_items[1]->_user.getShared<AST::Primary>();
          ast_node->_primaries.push_back(primary);
        }
      }
    });
    ///////////////////////////////////////////////////////////
    on("sum", [=](match_ptr_t match) {
      auto ast_node = match->_user.makeShared<AST::Sum>();
      auto selected = match->_impl.get<oneof_ptr_t>()->_selected;
      if (selected->_matcher == product) {
        ast_node->_left = selected->_user.getShared<AST::Product>();
        ast_node->_op   = '_';
      } else if (selected->_matcher->_name == "add1") {
        auto seq         = selected->_impl.get<sequence_ptr_t>();
        ast_node->_left  = seq->_items[0]->_user.getShared<AST::Product>();
        ast_node->_right = seq->_items[2]->_user.getShared<AST::Product>();
        ast_node->_op    = '+';
      } else if (selected->_matcher->_name == "add2") {
        auto seq         = selected->_impl.get<sequence_ptr_t>();
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
      auto seq       = match->_impl.get<sequence_ptr_t>();
      ast_node->_sum = seq->_items[0]->_user.getShared<AST::Sum>();
    });
    ///////////////////////////////////////////////////////////
    on("assignment_statement", [=](match_ptr_t match) { //
      auto ast_node = match->_user.makeShared<AST::AssignmentStatement>();
      auto ass1of   = match->_impl.get<sequence_ptr_t>()->_items[0]->_impl.get<oneof_ptr_t>();
      if (ass1of->_selected->_matcher == variableDeclaration) {
        auto seq      = ass1of->_selected->_impl.get<sequence_ptr_t>();
        auto datatype = seq->_items[0]->_user.getShared<AST::DataType>();
        // auto kwid = seq->_items[1]->_user.getShared<AST::KwOrId>();
        auto expr = match->_impl.get<sequence_ptr_t>()->_items[2]->_user.getShared<AST::Expression>();
        // ast_node->_datatype = datatype;
        // ast_node->_name = kwid;
        // ast_node->_expression = expr;
      } else if (ass1of->_selected->_matcher == variableReference) {
        // auto kwid = ass1of->_selected->_impl.get<sequence_ptr_t>()->_items[0]->_user.getShared<AST::KwOrId>();
        auto expr           = match->_impl.get<sequence_ptr_t>()->_items[2]->_user.getShared<AST::Expression>();
        ast_node->_datatype = nullptr;
        // ast_node->_name = kwid;
        ast_node->_expression = expr;
      } else {
        OrkAssert(false);
      }
    });
    ///////////////////////////////////////////////////////////
    on("funcdef", [=](match_ptr_t match) {
      auto seq     = match->_impl.get<sequence_ptr_t>();
      auto fn_name = seq->_items[1]->_impl.get<classmatch_ptr_t>();
      auto args    = seq->_items[3]->_impl.get<n_or_more_ptr_t>();
      auto stas    = seq->_items[6]->_impl.get<n_or_more_ptr_t>();
      printf(
          "MATCHED funcdef<%s> function<%s> numargs<%d> numstatements<%d>\n", //
          funcdef->_name.c_str(),                                             //
          fn_name->_token->text.c_str(),                                      //
          args->_items.size(),                                                //
          stas->_items.size());

      for (auto arg : args->_items) {
        auto argseq     = arg->_impl.get<sequence_ptr_t>();
        auto argtype    = argseq->_items[0]->_impl.get<oneof_ptr_t>();
        auto argtypeval = argtype->_selected->_impl.get<wordmatch_ptr_t>();
        auto argname    = argseq->_items[1]->_impl.get<classmatch_ptr_t>();
        printf("  ARG<%s> TYPE<%s>\n", argname->_token->text.c_str(), argtypeval->_token->text.c_str());
      }
      int i = 0;
      for (auto sta : stas->_items) {
        auto stasel = sta->_impl.get<oneof_ptr_t>()->_selected;
        if (auto as_seq = stasel->_impl.tryAs<sequence_ptr_t>()) {
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
    auto match = this->match(slv, _fn_matcher);
    return match;
  }

  matcher_ptr_t _fn_matcher;
  scanner_ptr_t _scanner;
}; // struct MyParser

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TEST(parser2) {

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
  CHECK(match != nullptr);
}