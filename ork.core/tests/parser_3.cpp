////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "parser_lang.inl"
#include <utpp/UnitTest++.h>
#include <ork/util/parser.inl>

namespace ork::unittest::parser3 {
///////////////////////////////////////////////////////////////////////////////

static std::string scanner_spec = R"xxx(

MULTI_LINE_COMMENT  <| "\/\*([^*]|\*+[^/*])*\*+\/" |>
SINGLE_LINE_COMMENT <| "\/\/.*[\n\r]" |>
WHITESPACE          <| "\s+" |>
NEWLINE             <| "[\n\r]+" |>
SEMICOLON           <| ";" |>
L_PAREN             <| "\(" |>
R_PAREN             <| "\)" |>
FLOATING_POINT      <| "-?(\d*\.?)(\d+)([eE][-+]?\d+)?" |>
INTEGER             <| "-?(\d+)" |>
KW_FLOAT            <| "float" |>
KW_INT              <| "int" |>
KW_OR_ID            <| "[a-zA-Z_][a-zA-Z0-9_]*" |>

)xxx";

///////////////////////////////////////////////////////////////////////////////

static std::string parser_spec = R"xxx(

datatype <| sel{KW_FLOAT KW_INT} |>
number <| sel{FLOATING_POINT INTEGER} |>
    
test_right_recursion <| opt{
    sel{ 
        [ L_PAREN datatype R_PAREN test_right_recursion ] : "nta"
        [ L_PAREN number R_PAREN test_right_recursion ] : "ntb"
        [ SEMICOLON test_right_recursion ] : "ntsemi"
    }
}|>

program <| zom{ test_right_recursion } |>

)xxx";

///////////////////////////////////////////////////////////////////////////////

static std::string test_text = R"xxx(

         // hello world
(int);   // a
(float); // a
(1.0)    // b
(2);     // b

)xxx";

///////////////////////////////////////////////////////////////////////////////

static std::string expected_ast_str = R"xxx(
program
  nta
    ntsemi
      nta
        ntsemi
          ntb
            ntb
              ntsemi

)xxx";

///////////////////////////////////////////////////////////////////////////////

struct AstNode;
using astnode_ptr_t = std::shared_ptr<AstNode>;

struct AstNode{
    AstNode(std::string name) : _name(name) {}
    virtual ~AstNode(){}
    std::string _name;
    std::vector<astnode_ptr_t> _children;
    astnode_ptr_t _parent;
};
struct NodeTypeA : public AstNode{ NodeTypeA() : AstNode("nta"){} };
struct NodeTypeB : public AstNode{ NodeTypeB() : AstNode("ntb"){} };
struct NodeTypeSemi : public AstNode{ NodeTypeSemi() : AstNode("ntsemi"){} };
struct NodeTypeNil : public AstNode{NodeTypeNil() : AstNode("ntnil"){} };
struct Program : public AstNode{Program() : AstNode("program"){} };

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> ast_create(match_ptr_t m) {
    auto sh = std::make_shared<T>();
    m->_uservars.set<astnode_ptr_t>("astnode", sh);
    return sh;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> ast_get(match_ptr_t m) {
    auto sh = m->_uservars.typedValueForKey<astnode_ptr_t>("astnode").value();
    return std::dynamic_pointer_cast<T>(sh);
}

///////////////////////////////////////////////////////////////////////////////

struct Parser : public ::ork::Parser {

  Parser() {
    _name              = "p3";
    _DEBUG_MATCH       = true;
    _DEBUG_INFO        = true;
    bool OK = this->loadPEGSpec(scanner_spec,parser_spec);
    OrkAssert(OK);
    ///////////////////////////////////////////////////////////
    onPost("program", [=](match_ptr_t match) {
      auto ast_node = ast_create<Program>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("nta", [=](match_ptr_t match) {
      auto ast_node = ast_create<NodeTypeA>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("ntb", [=](match_ptr_t match) {
      auto ast_node = ast_create<NodeTypeB>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("ntsemi", [=](match_ptr_t match) {
      auto ast_node = ast_create<NodeTypeSemi>(match);
    });
  }

  ///////////////////////////////////////////////////////////////////////////////

  void _buildAstTreeVisitor(match_ptr_t the_match) {
    bool has_ast = the_match->_uservars.hasKey("astnode");
    if (has_ast) {
      auto ast = the_match->_uservars.typedValueForKey<astnode_ptr_t>("astnode").value();

      if (_astnodestack.size() > 0) {
        auto parent = _astnodestack.back();
        parent->_children.push_back(ast);
        ast->_parent = parent;
      }
      _astnodestack.push_back(ast);
    }

    for (auto c : the_match->_children) {
      _buildAstTreeVisitor(c);
    }
    if (has_ast) {
      _astnodestack.pop_back();
    }
  }

  /////////////////////////////////////////////////////////////////

  void _dumpAstTreeVisitor(astnode_ptr_t node, int indent, std::string& out_str) {
    auto indentstr = std::string(indent*2, ' ');
    out_str += FormatString("%s%s\n", indentstr.c_str(), node->_name.c_str());
    for (auto c : node->_children) {
      _dumpAstTreeVisitor(c, indent+1, out_str);
    }
  }

  /////////////////////////////////////////////////////////////////

  match_ptr_t parseString(std::string str_to_parse) {

    _scanner->scanString(str_to_parse);
    _scanner->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::SINGLE_LINE_COMMENT));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::MULTI_LINE_COMMENT));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));

    auto top_view = _scanner->createTopView();
    top_view.dump("top_view");
    auto slv     = std::make_shared<ScannerLightView>(top_view);
    _matcher = findMatcherByName("program");
    OrkAssert(_matcher);
    auto match = this->match(_matcher, slv);
    OrkAssert(match);

    return match;
  }

  /////////////////////////////////////////////////////////////////

  matcher_ptr_t _matcher;
  std::vector<astnode_ptr_t> _astnodestack;
}; // struct Parser

} // namespace ork::unittest::parser3

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TEST(parser3) {
  using namespace ork::unittest;

  printf("P3.TOP.A\n");
  auto the_parser = std::make_shared<parser3::Parser>();
  auto match      = the_parser->parseString(parser3::test_text);
  printf(
      "P3.TOP.B match<%p> matcher<%p:%s> st<%zu> en<%zu>\n", //
      match.get(),                                           //
      match->_matcher.get(),                                 //
      match->_matcher->_name.c_str(),                        //
      match->_view->_start,                                  //
      match->_view->_end);                                   //

  CHECK(match != nullptr);

  the_parser->_buildAstTreeVisitor(match);
  std::string ast_str = "\n";
  auto top_node = parser3::ast_get<parser3::Program>(match);
  the_parser->_dumpAstTreeVisitor(top_node, 0, ast_str);
  ast_str += "\n";

  printf("/////////////////////\n");
  printf( "aststr: %s", ast_str.c_str() );
  printf("/////////////////////\n");
  printf( "expected_ast_str: %s", parser3::expected_ast_str.c_str() );
  printf("/////////////////////\n");

  CHECK(parser3::expected_ast_str==ast_str);
}