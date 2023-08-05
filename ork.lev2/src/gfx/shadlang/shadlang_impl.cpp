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
#include <ork/kernel/string/string.h>
#include <ork/util/parser.inl>
#include "shadlang_impl.h"

#if defined(USE_ORKSL_LANG)

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::shadlang {
/////////////////////////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan         = logger()->createChannel("ORKSLIMPL", fvec3(1, 1, .9), false);
static logchannel_ptr_t logchan_grammar = logger()->createChannel("ORKSLGRAM", fvec3(1, 1, .8), false);
static logchannel_ptr_t logchan_lexer   = logger()->createChannel("ORKSLLEXR", fvec3(1, 1, .7), false);
/////////////////////////////////////////////////////////////////////////////////////////////////

void SHAST::_dumpAstTreeVisitor( //
    SHAST::astnode_ptr_t node,   //
    int indent,                  //
    std::string& out_str) {      //

  auto indentstr = std::string(indent * 2, ' ');
  out_str += FormatString("%s%s\n", indentstr.c_str(), node->desc().c_str());
  if (node->_descend) {
    for (auto c : node->_children) {
      _dumpAstTreeVisitor(c, indent + 1, out_str);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

std::string toASTstring(SHAST::astnode_ptr_t node) {
  std::string rval;
  SHAST::_dumpAstTreeVisitor(node, 0, rval);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

SHAST::translationunit_ptr_t parse(const std::string& shader_text) {
  auto parser = std::make_shared<impl::ShadLangParser>();
  OrkAssert(parser);
  printf("shader_text<%s>\n", shader_text.c_str());
  OrkAssert(shader_text.length());
  auto topast = parser->parseString(shader_text);
  return topast;
}

///////////////////////////////////////////////////////////////////////////////
namespace impl {
///////////////////////////////////////////////////////////////////////////////

static std::string scanner_spec = "";
static std::string parser_spec  = "";

///////////////////////////////////////////////////////////////////////////////

ShadLangParser::ShadLangParser() {
  _name = "shadlang";
  //_DEBUG_MATCH       = true;
  //_DEBUG_INFO        = true;

  if (0 == scanner_spec.length()) {
    auto scanner_path = ork::file::Path::data_dir() / "grammars" / "shadlang.scanner";
    auto read_result  = ork::File::readAsString(scanner_path);
    scanner_spec      = read_result->_data;
  }
  if (0 == parser_spec.length()) {
    auto parser_path = ork::file::Path::data_dir() / "grammars" / "shadlang.parser";
    auto read_result = ork::File::readAsString(parser_path);
    parser_spec      = read_result->_data;
  }

  ///////////////////////////////////////////////////////////

  preDeclareAstNodes();

  ///////////////////////////////////////////////////////////

  auto scanner_match = this->loadPEGScannerSpec(scanner_spec);
  OrkAssert(scanner_match);
  auto parser_match = this->loadPEGParserSpec(parser_spec);
  OrkAssert(parser_match);

  ///////////////////////////////////////////////////////////
  // parser should be compiled and linked at this point
  //  lets declare the AST node types
  //  and define any custom AST node handlers
  ///////////////////////////////////////////////////////////

  declareAstNodes();
  defineAstHandlers();

  ///////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

SHAST::astnode_ptr_t ShadLangParser::astNodeForMatch(match_ptr_t match) const{
  auto it = _match2astnode.find(match);
  OrkAssert(it != _match2astnode.end());
  return it->second;
}
match_ptr_t ShadLangParser::matchForAstNode(SHAST::astnode_ptr_t astnode) const{
  auto it = _astnode2match.find(astnode);
  OrkAssert(it != _astnode2match.end());
  return it->second;
}

///////////////////////////////////////////////////////////////////////////////

void ShadLangParser::_buildAstTreeVisitor(match_ptr_t the_match) {
  bool has_ast = the_match->_uservars.hasKey("astnode");
  if (has_ast) {
    auto ast = the_match->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode").value();

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

///////////////////////////////////////////////////////////////////////////////

void ShadLangParser::visitAST(      //
    SHAST::astnode_ptr_t node,      //
    SHAST::visitor_ptr_t visitor) { //

  if (visitor->_on_pre) {
    visitor->_on_pre(node);
  }
  visitor->_nodestack.push(node);
  for (auto c : node->_children) {
    visitAST(c, visitor);
  }
  visitor->_nodestack.pop();
  if (visitor->_on_post) {
    visitor->_on_post(node);
  }
}

///////////////////////////////////////////////////////////////////////////////

SHAST::translationunit_ptr_t ShadLangParser::parseString(std::string parse_str) {

  _scanner->scanString(parse_str);
  _scanner->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
  _scanner->discardTokensOfClass(uint64_t(TokenClass::SINGLE_LINE_COMMENT));
  _scanner->discardTokensOfClass(uint64_t(TokenClass::MULTI_LINE_COMMENT));
  _scanner->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));

  auto top_view = _scanner->createTopView();
  // top_view.dump("top_view");
  auto slv    = std::make_shared<ScannerLightView>(top_view);
  _tu_matcher = findMatcherByName("TranslationUnit");
  OrkAssert(_tu_matcher);
  auto match = this->match(_tu_matcher, slv, [this](match_ptr_t m) { _buildAstTreeVisitor(m); });
  OrkAssert(match);
  auto ast_top = match->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode").value();
  ///////////////////////////////////////////
  pruneAST(ast_top);
  semaAST(ast_top);
  ///////////////////////////////////////////
  printf("///////////////////////////////\n");
  printf("// AST TREE\n");
  printf("///////////////////////////////\n");
  std::string ast_str = toASTstring(ast_top);
  printf("%s\n", ast_str.c_str());
  printf("///////////////////////////////\n");
  return std::dynamic_pointer_cast<SHAST::TranslationUnit>(ast_top);
}

} // namespace impl

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif
