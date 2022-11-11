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
#include <ork/kernel/string/string.h>
#include <ork/kernel/string/deco.inl>
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

void performScan(scanner_ptr_t scanner) {

  int tokclass = 1;

  scanner->addRule("\\/\\*([^*]|\\*+[^/*])*\\*+\\/", int(TokenClass::MULTI_LINE_COMMENT));
  scanner->addRule("\\/\\/.*[\\n\\r]", int(TokenClass::SINGLE_LINE_COMMENT));
  scanner->addRule("\\s+", int(TokenClass::WHITESPACE));
  scanner->addRule("[\\n\\r]+", int(TokenClass::NEWLINE));
  /////////
  scanner->addRule("[0-9]+u", int(TokenClass::UNSIGNED_DECIMAL_INTEGER));
  scanner->addRule("0x[0-9a-fA-F]+u?", int(TokenClass::HEX_INTEGER));
  scanner->addRule("-?(\\d+)", int(TokenClass::MISC_INTEGER));
  scanner->addRule("-?(\\d*\\.?)(\\d+)([eE][-+]?\\d+)?", int(TokenClass::FLOATING_POINT));
  /////////
  scanner->addRule("[\"].*[\"]", int(TokenClass::STRING));
  /////////
  scanner->addRule("[a-zA-Z_]+[a-zA-Z0-9_]+", int(TokenClass::KW_OR_ID));
  /////////
  scanner->addRule("[{}]", int(TokenClass::CURLY_BRACKET));
  scanner->addRule("[()]", int(TokenClass::PARENTHESIS));
  scanner->addRule("[\\[\\]]", int(TokenClass::SQUARE_BRACKET));
  /////////
  scanner->addRule(":", int(TokenClass::COLON));
  scanner->addRule(";", int(TokenClass::SEMICOLON));
  scanner->addRule("[,.]", int(TokenClass::COMMA_OR_DOT));
  /////////
  scanner->addRule("<<", int(TokenClass::LEFT_SHIFT));
  scanner->addRule(">>", int(TokenClass::RIGHT_SHIFT));
  /////////
  scanner->addRule("<", int(TokenClass::LESS_THAN));
  scanner->addRule(">", int(TokenClass::GREATER_THAN));
  /////////
  scanner->addRule("<=", int(TokenClass::LESS_THAN_EQ));
  scanner->addRule(">=", int(TokenClass::GREATER_THAN_EQ));
  scanner->addRule("==", int(TokenClass::EQUAL_TO));
  scanner->addRule("!=", int(TokenClass::NOT_EQUAL_TO));
  scanner->addRule("\\+=", int(TokenClass::PLUS_EQ));
  scanner->addRule("\\-=", int(TokenClass::MINUS_EQ));
  scanner->addRule("\\*=", int(TokenClass::TIMES_EQ));
  scanner->addRule("\\/=", int(TokenClass::DIVIDE_EQ));
  scanner->addRule("\\|=", int(TokenClass::OR_EQ));
  scanner->addRule("&=", int(TokenClass::AND_EQ));
  /////////
  scanner->addRule("[*+-/~]", int(TokenClass::ARITHMETIC_OP));
  /////////
  scanner->addRule("\\+\\+", int(TokenClass::INCREMENT));
  scanner->addRule("--", int(TokenClass::DECREMENT));
  /////////
  scanner->addRule("\\|\\|", int(TokenClass::LOGICAL_OR));
  scanner->addRule("&&", int(TokenClass::LOGICAL_AND));

  scanner->buildStateMachine();
  scanner->scan();
  scanner->discardTokensOfClass(int(TokenClass::SINGLE_LINE_COMMENT));
  scanner->discardTokensOfClass(int(TokenClass::MULTI_LINE_COMMENT));
  scanner->discardTokensOfClass(int(TokenClass::WHITESPACE));
  scanner->discardTokensOfClass(int(TokenClass::NEWLINE));
}

void checktoken(const ScannerView& view, int actual_index, std::string expected) {
}

///////////////////////////////////////////////////////////////////////////////

GlSlFxParser::GlSlFxParser(std::string name, 
                           program_ptr_t progam,
                           scanner_constptr_t s)
    : _name(name)
    , _grules(parsertl::enable_captures)
    , _scanner(s)
    , _program(progam){
  _topNode = std::make_shared<TopNode>(this);
  _topNode->parse();
}

///////////////////////////////////////////////////////////

void GlSlFxParser::DumpAllTokens() {
  size_t itokidx     = 0;
  const auto& tokens = _scanner->tokens;
  while (itokidx < tokens.size()) {
    const Token& tok = tokens[itokidx++];
    // printf( "tok<%d> <%s>\n", itokidx, tok.text.c_str() );
  }
}

///////////////////////////////////////////////////////////

Program::Program(const std::string name) : _name(name) {

}
///////////////////////////////////////////////////////////
void Program::addBlockNode(decoblocknode_ptr_t node) {
  //printf( "Program<%p:%s>::addBlockNode(%p:%s)\n", 
  //         this, _name.c_str(),
  //         node.get(), node->_name.c_str() );
  auto it = _blockNodes.find(node->_name);
  if(it != _blockNodes.end()){
    //printf( "adding dup block<%s>\n", node->_name.c_str() );
    OrkAssert(false);
  }
  auto its = node->_name.find("lib_math");
  if(its!=std::string::npos){
    //printf( "WTF\n");
  }
  _blockNodes[node->_name]=node;
  size_t bncount = _blockNodes.size();
  _orderedBlockNodes.push_back(node);
}

//////////////////////////////////////////////////////////////////////////////////
void Program::generate_all(shaderbuilder::BackEnd& backend) {

  NodeSelection nodesel_emittable;
  nodesel_emittable._stateblock_nodes = true;
  nodesel_emittable._interface_nodes = true;
  nodesel_emittable._shader_nodes = true;
  nodesel_emittable._shaderdata_nodes = true;
  nodesel_emittable._technique_nodes = true;

  NodeSelection nodesel_all = nodesel_emittable;
  nodesel_all._lib_nodes = true;

  //auto emit_nodes = collectNodes(nodesel_emittable);
  auto all_nodes = collectNodes(nodesel_all);

  
  backend._parser->_topNode->pregen(backend);

  for (auto item : all_nodes._nodevect)
    item->pregen(backend);
  for (auto item : all_nodes._nodevect)
    item->_generate1(backend);
  for (auto item : all_nodes._nodevect)
    item->_generate2(backend);

}
///////////////////////////////////////////////////////////
NodeCollection Program::collectNodes(const NodeSelection& selection) {
  NodeCollection rval;
  ////////////////////////////////////////////
  rval._nodevect.reserve(256);
  ////////////////////////////////////////////
  // build node list in desired order of processing
  // so do leafy nodes first and nodes
  //  which have dependencies later
  ////////////////////////////////////////////
  if(selection._lib_nodes)
    collectNodesOfType<LibraryBlockNode>(rval);

  if(selection._shaderdata_nodes)
    collectNodesOfType<ShaderDataNode>(rval);

  if(selection._stateblock_nodes)
    collectNodesOfType<StateBlockNode>(rval);

  if(selection._interface_nodes){
    collectNodesOfType<VertexInterfaceNode>(rval);
    collectNodesOfType<TessCtrlInterfaceNode>(rval);
    collectNodesOfType<TessEvalInterfaceNode>(rval);
    collectNodesOfType<GeometryInterfaceNode>(rval);
    collectNodesOfType<FragmentInterfaceNode>(rval);
#if defined(ENABLE_COMPUTE_SHADERS)
  collectNodesOfType<ComputeInterfaceNode>(rval);
#endif
#if defined(ENABLE_NVMESH_SHADERS)
  collectNodesOfType<NvTaskInterfaceNode>(rval);
  collectNodesOfType<NvMeshInterfaceNode>(rval);
#endif
  }

  if(selection._shader_nodes){
    collectNodesOfType<VertexShaderNode>(rval);
    collectNodesOfType<TessCtrlShaderNode>(rval);
    collectNodesOfType<TessEvalShaderNode>(rval);
    collectNodesOfType<GeometryShaderNode>(rval);
    collectNodesOfType<FragmentShaderNode>(rval);
#if defined(ENABLE_COMPUTE_SHADERS)
    collectNodesOfType<ComputeShaderNode>(rval);
#endif
#if defined(ENABLE_NVMESH_SHADERS)
    collectNodesOfType<NvTaskShaderNode>(rval);
    collectNodesOfType<NvMeshShaderNode>(rval);
#endif
  }

  if(selection._technique_nodes){
    collectNodesOfType<TechniqueNode>(rval);
  }

  return rval;
}
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
