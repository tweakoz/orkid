////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "parser_lang.inl"

///////////////////////////////////////////////////////////////////////////////

struct MyParser : public Parser {

  MyParser() {
    loadScannerRules();
    loadGrammar();
  }

  void loadScannerRules() { //

    _scanner->addEnumClass("\\/\\*([^*]|\\*+[^/*])*\\*+\\/", TokenClass::MULTI_LINE_COMMENT);
    _scanner->addEnumClass("\\/\\/.*[\\n\\r]", TokenClass::SINGLE_LINE_COMMENT);
    _scanner->addEnumClass("\\s+", TokenClass::WHITESPACE);
    _scanner->addEnumClass("[\\n\\r]+", TokenClass::NEWLINE);
    _scanner->addEnumClass("[a-zA-Z_][a-zA-Z0-9_]*", TokenClass::KW_OR_ID);
    _scanner->addEnumClass("=", TokenClass::EQUALS);
    _scanner->addEnumClass(",", TokenClass::COMMA);
    _scanner->addEnumClass(";", TokenClass::SEMICOLON);
    _scanner->addEnumClass("\\(", TokenClass::L_PAREN);
    _scanner->addEnumClass("\\)", TokenClass::R_PAREN);
    _scanner->addEnumClass("\\{", TokenClass::L_CURLY);
    _scanner->addEnumClass("\\}", TokenClass::R_CURLY);
    _scanner->addEnumClass("\\*", TokenClass::STAR);
    _scanner->addEnumClass("\\+", TokenClass::PLUS);
    _scanner->addEnumClass("\\-", TokenClass::MINUS);
    _scanner->addEnumClass("-?(\\d*\\.?)(\\d+)([eE][-+]?\\d+)?", TokenClass::FLOATING_POINT);
    _scanner->addEnumClass("-?(\\d+)", TokenClass::INTEGER);
    _scanner->buildStateMachine();
  }

  void loadGrammar() { //
    auto plus        = matcherForTokenClass(TokenClass::PLUS, "plus");
    auto minus       = matcherForTokenClass(TokenClass::MINUS, "minus");
    auto star        = matcherForTokenClass(TokenClass::STAR, "star");
    auto equals      = matcherForTokenClass(TokenClass::EQUALS, "equals");
    auto semicolon   = matcherForTokenClass(TokenClass::SEMICOLON, "semicolon");
    auto comma       = matcherForTokenClass(TokenClass::COMMA, "comma");
    auto lparen      = matcherForTokenClass(TokenClass::L_PAREN, "lparen");
    auto rparen      = matcherForTokenClass(TokenClass::R_PAREN, "rparen");
    auto lcurly      = matcherForTokenClass(TokenClass::L_CURLY, "lcurly");
    auto rcurly      = matcherForTokenClass(TokenClass::R_CURLY, "rcurly");
    auto floattok    = matcherForTokenClass(TokenClass::FLOATING_POINT, "float");
    auto inttok      = matcherForTokenClass(TokenClass::INTEGER, "int");
    auto kworid      = matcherForTokenClass(TokenClass::KW_OR_ID, "kw_or_id");
    auto kw_function = matcherForWord("function");
    ///////////////////////////////////////////////////////////
    auto dt_float = matcherForWord("float");
    auto dt_int   = matcherForWord("int");
    ///////////////////////////////////////////////////////////
    floattok->_notif = [=](match_ptr_t match) { //
        auto ast_node = match->_user.makeShared<MYAST::FloatLiteral>();
        auto impl = match->_impl.get<classmatch_ptr_t>();
        ast_node->_value = std::stof(impl->_token->text);
    };
    inttok->_notif   = [=](match_ptr_t match) { //
        auto ast_node = match->_user.makeShared<MYAST::IntegerLiteral>(); 
        auto impl = match->_impl.get<classmatch_ptr_t>();
        ast_node->_value = std::stoi(impl->_token->text);
    };
    ///////////////////////////////////////////////////////////
    auto datatype = oneOf({
        dt_float,
        dt_int,
    });
    //
    datatype->_notif = [=](match_ptr_t match) { //
        auto selected = match->_impl.getShared<OneOf>()->_selected;
        auto ast_node = match->_user.makeShared<MYAST::DataType>();
        ast_node->_name = selected->_impl.get<wordmatch_ptr_t>()->_token->text;
    };
    ///////////////////////////////////////////////////////////
    auto argument_decl = sequence(
        "argument_decl",
        {
            //
            datatype,
            kworid,
            optional(comma),
        });
    ///////////////////////////////////////////////////////////
    auto number = oneOf({
        floattok,
        inttok,
    });
    ///////////////////////////////////////////////////////////
    auto variableDeclaration = sequence("variableDeclaration", {datatype, kworid});
    ///////////////////////////////////////////////////////////
    auto variableReference    = sequence("variableReference", {kworid});
    variableReference->_notif = [=](match_ptr_t match) { //
        auto seq = match->_impl.get<sequence_ptr_t>();
        auto kwid = seq->_items[0]->_impl.getShared<ClassMatch>()->_token->text;
        auto var_ref = match->_user.makeShared<MYAST::VariableReference>(); 
        var_ref->_name = kwid;
    };
    ///////////////////////////////////////////////////////////
    auto expression = declare("expression");
    ///////////////////////////////////////////////////////////
    auto term    = sequence({lparen, expression, rparen}, "term");
    term->_notif = [=](match_ptr_t match) {
      auto ast_node = match->_user.makeShared<MYAST::Term>();
      auto selected = match->_impl.get<sequence_ptr_t>()->_items[1];
      if (selected->_matcher == expression) {
        ast_node->_subexpression = selected->_user.getShared<MYAST::Expression>();
      }
    };
    ///////////////////////////////////////////////////////////
    auto primary = oneOf(
        "primary",
        {
            //
            floattok,
            inttok,
            variableReference,
            term,
        });
    //
    primary->_notif = [=](match_ptr_t match) {
      auto ast_node = match->_user.makeShared<MYAST::Primary>();
      auto selected = match->_impl.get<oneof_ptr_t>()->_selected;
      ast_node->_impl = selected->_user;
    };
    ///////////////////////////////////////////////////////////
    auto mul1sp         = sequence({star, primary}, "mul1sp");
    auto mul1zom        = zeroOrMore(mul1sp, "mul1zom");
    auto product = oneOf(
        "product",
        {
            sequence({primary, mul1zom}, "mul1")
            // sequence({ primary,optional(sequence({slash,primary})) }),
        });
    //
    product->_notif = [=](match_ptr_t match) {
      auto ast_node = match->_user.makeShared<MYAST::Product>();
      auto selected = match->_impl.get<oneof_ptr_t>()->_selected;
      auto sel_seq  = selected->_impl.get<sequence_ptr_t>();
      auto primary  = sel_seq->_items[0]->_user.getShared<MYAST::Primary>();
      auto m1zom  = sel_seq->_items[1]->_impl.get<n_or_more_ptr_t>();
      ast_node->_primaries.push_back(primary);
      if(m1zom->_items.size()){
        for( auto i : m1zom->_items ){
            auto seq = i->_impl.get<sequence_ptr_t>();
            // star is implied...
            primary = seq->_items[1]->_user.getShared<MYAST::Primary>();
            ast_node->_primaries.push_back(primary);
        }
      }
    };
    ///////////////////////////////////////////////////////////
    auto sum = oneOf(
        "sum",
        {//
         sequence({product, plus, product}, "add1"),
         sequence({product, minus, product}, "add2"),
         product});
    //
    sum->_notif = [=](match_ptr_t match) {
      auto ast_node = match->_user.makeShared<MYAST::Sum>();
      auto selected = match->_impl.get<oneof_ptr_t>()->_selected;
      if( selected->_matcher == product ){
        ast_node->_left = selected->_user.getShared<MYAST::Product>();
        ast_node->_op = '_';
      }
      else if( selected->_matcher->_name == "add1" ){
        auto seq = selected->_impl.get<sequence_ptr_t>();
        ast_node->_left = seq->_items[0]->_user.getShared<MYAST::Product>();
        ast_node->_right =  seq->_items[2]->_user.getShared<MYAST::Product>();
        ast_node->_op = '+';
      }
      else if( selected->_matcher->_name == "add2" ){
        auto seq = selected->_impl.get<sequence_ptr_t>();
        ast_node->_left = seq->_items[0]->_user.getShared<MYAST::Product>();
        ast_node->_right =  seq->_items[2]->_user.getShared<MYAST::Product>();
        ast_node->_op = '-';
      }
      else{
        OrkAssert(false);
      }
    };
    ///////////////////////////////////////////////////////////
    sequence(expression, {sum});
    expression->_notif = [=](match_ptr_t match) { //
      auto ast_node = match->_user.makeShared<MYAST::Expression>();
      auto seq = match->_impl.get<sequence_ptr_t>();
      ast_node->_sum = seq->_items[0]->_user.getShared<MYAST::Sum>();
    };
    ///////////////////////////////////////////////////////////
    auto assignment_statement = sequence(
        "assignment_statement",
        {//
         oneOf({variableDeclaration, variableReference}, "ass1of"),
         equals,
         expression});
    //
    assignment_statement->_notif = [=](match_ptr_t match) { //
        auto ast_node = match->_user.makeShared<MYAST::AssignmentStatement>();
        auto ass1of = match->_impl.get<sequence_ptr_t>()->_items[0]->_impl.get<oneof_ptr_t>();
        if( ass1of->_selected->_matcher == variableDeclaration ){
            auto seq = ass1of->_selected->_impl.get<sequence_ptr_t>();
            auto datatype = seq->_items[0]->_user.getShared<MYAST::DataType>();
            //auto kwid = seq->_items[1]->_user.getShared<MYAST::KwOrId>();
            auto expr = match->_impl.get<sequence_ptr_t>()->_items[2]->_user.getShared<MYAST::Expression>();
            //ast_node->_datatype = datatype;
            //ast_node->_name = kwid;
            //ast_node->_expression = expr;
        }
        else if( ass1of->_selected->_matcher == variableReference ){
            //auto kwid = ass1of->_selected->_impl.get<sequence_ptr_t>()->_items[0]->_user.getShared<MYAST::KwOrId>();
            auto expr = match->_impl.get<sequence_ptr_t>()->_items[2]->_user.getShared<MYAST::Expression>();
            ast_node->_datatype = nullptr;
            //ast_node->_name = kwid;
            ast_node->_expression = expr;
        }
        else{
            OrkAssert(false);
        }
    };
    ///////////////////////////////////////////////////////////
    auto statement = oneOf({sequence({assignment_statement, semicolon}), semicolon});
    ///////////////////////////////////////////////////////////
    auto funcdef = sequence(
        "funcdef",
        {//
         kw_function,
         kworid,
         lparen,
         zeroOrMore(argument_decl, "fnd_args"),
         rparen,
         lcurly,
         zeroOrMore(statement, "fnd_statements"),
         rcurly});
    ///////////////////////////////////////////////////////////
    funcdef->_notif = [=](match_ptr_t match) {
      // match->_view->dump("funcdef");
      // match->dump(0);

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
        auto stasel   = sta->_impl.get<oneof_ptr_t>()->_selected;
        if( auto as_seq = stasel->_impl.tryAs<sequence_ptr_t>()){
            auto staseq0 = as_seq.value()->_items[0];
            if( staseq0->_matcher == assignment_statement ){
            }
            else if( staseq0->_matcher == semicolon ){
            }
            else{
                OrkAssert(false);
            }

        }
        i++;
      }
    };
    ///////////////////////////////////////////////////////////
    _fn_matcher = zeroOrMore(funcdef, "funcdefs", true);
    ///////////////////////////////////////////////////////////
  }

  match_ptr_t parseString(std::string parse_str) {
    _scanner->clear();
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
}; // struct MyParser

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TEST(parser1) {

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

  MyParser the_parser;
  auto match = the_parser.parseString(parse_str);
  CHECK(match != nullptr);

  printf( "PARSER PACKRAT cache_hits<%zu> cache_misses<%zu>\n", //
          the_parser._cache_hits,                                //
          the_parser._cache_misses);                             //
}