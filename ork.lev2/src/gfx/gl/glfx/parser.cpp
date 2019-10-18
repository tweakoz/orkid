////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
bool ContainerNode::IsTokenOneOfTheBlockTypes(const Token& tok) {
  static const std::regex regex_block(block_regex);
  return std::regex_match(tok.text, regex_block);
}
///////////////////////////////////////////////////////////
ContainerNode::ContainerNode(const AssetPath &pth, const Scanner &s)
  : _path(pth)
  , _scanner(s) {

    std::string typenames = "mat2 mat3 mat4 vec2 vec3 vec4 "
                            "float double half uint int "
#if ! defined(__APPLE__)
                            "int8_t int16_t int32_t int64_t "
                            "uint8_t uint16_t uint32_t uint64_t "
                            "i8vec2 i8vec3 i8vec4 "
                            "i16vec2 i16vec3 i16vec4 "
                            "i32vec2 i32vec3 i32vec4 "
                            "i64vec2 i64vec3 i64vec4 "
                            "u16vec2 u16vec3 u16vec4 "
                            "u32vec2 u32vec3 u32vec4 "
                            "u64vec2 u64vec3 u64vec4 "
#endif
                            "sampler2D sampler3D sampler2DShadow";

    for( auto item : SplitString(typenames, ' ') )
      _validTypeNames.insert(item);

    _validOutputDecorators.insert("perprimitiveNV");
    _validOutputDecorators.insert("taskNV");


}
///////////////////////////////////////////////////////////
bool ContainerNode::validateTypeName(const std::string typeName) const {
  auto it = _validTypeNames.find(typeName);
  return (it!=_validTypeNames.end());
}
///////////////////////////////////////////////////////////
bool ContainerNode::validateMemberName(const std::string typeName) const {
  return true;
}
///////////////////////////////////////////////////////////
bool ContainerNode::isIoAttrDecorator(const std::string typeName) const {
  auto it = _validOutputDecorators.find(typeName);
  return (it!=_validOutputDecorators.end());
}

///////////////////////////////////////////////////////////

void NamedBlockNode::parse(const ScannerView& view) {
  _name  = view.blockName();
  _blocktype = view.token(view._blockType)->text;
}

///////////////////////////////////////////////////////////

void DecoBlockNode::parse(const ScannerView& view) {
  NamedBlockNode::parse(view);
  _container->addBlockNode(this);

  /////////////////////////////
  // fetch block decorators
  /////////////////////////////

  size_t inumdecos = view.numBlockDecorators();

  for (size_t ideco = 0; ideco < inumdecos; ideco++) {
    auto decotok = view.blockDecorator(ideco);
    auto decoref = decotok->text;
    if( decoref=="extension"){
      int decoglobidx = view._blockDecorators[ideco];
      auto extname = view._scanner.token(decoglobidx+2)->text;
      _requiredExtensions.push_back(extname);
    }
    else {
      bool name_ok = _container->validateMemberName(decoref);
      auto it = _decodupecheck.find(decoref);
      assert(it==_decodupecheck.end() or decoref=="extension");
      _decodupecheck.insert(decoref);
      _decorators.push_back(decotok);
    }
  }

}

///////////////////////////////////////////////////////////

void ContainerNode::addBlockNode(DecoBlockNode*node) {
  auto it = _blockNodes.find(node->_name);
  assert(it==_blockNodes.end());
  auto status = _blockNodes.insert(std::make_pair(node->_name,node));
  size_t bncount = _blockNodes.size();
  assert(status.second);
  _orderedBlockNodes.push_back(node);
}

///////////////////////////////////////////////////////////
void ContainerNode::parse() {
  const auto& tokens = _scanner.tokens;

  // printf( "NumTokens<%d>\n", int(tokens.size()) );

  itokidx = 0;

  ScanViewRegex r("(\n)", true);

  while (itokidx < tokens.size()) {
    const Token& tok = tokens[itokidx];
     printf( "token<%d> iline<%d> col<%d> text<%s>\n", itokidx, tok.iline+1,
     tok.icol+1, tok.text.c_str() );

     ScannerView scanview(_scanner, r);
     scanview.scanBlock(itokidx);

     bool advance_block = true;

    if (tok.text == "\n") {
      itokidx++;
      advance_block = false;
    } else if (tok.text == "fxconfig") {
      _configNode = new ConfigNode(this);
      _configNode->parse(scanview);
    } else if (tok.text == "libblock") {
      auto lb = new LibraryBlockNode(this);
      lb->parse(scanview);
    } else if (tok.text == "uniform_set") {
      auto uniset = new UniformSetNode(this);
      uniset->parse(scanview);
    } else if (tok.text == "uniform_block") {
      auto uniblk = new UniformBlockNode(this);
      uniblk->parse(scanview);
    } else if (tok.text == "vertex_interface") {
      auto sif = new VertexInterfaceNode(this);
      sif->parse(scanview);
    } else if (tok.text == "tessctrl_interface") {
      auto sif = new TessCtrlInterfaceNode(this);
      sif->parse(scanview);
    } else if (tok.text == "tesseval_interface") {
      auto sif = new TessEvalInterfaceNode(this);
      sif->parse(scanview);
    } else if (tok.text == "geometry_interface") {
      auto sif = new GeometryInterfaceNode(this);
      sif->parse(scanview);
    } else if (tok.text == "fragment_interface") {
      auto sif = new FragmentInterfaceNode(this);
      sif->parse(scanview);
    } else if (tok.text == "state_block") {
      auto sblock = new StateBlockNode(this);
      sblock->parse(scanview);
      //mpContainer->addStateBlock(psblock);
    } else if (tok.text == "vertex_shader") {
      auto sh = new VertexShaderNode(this);
      sh->parse(scanview);
    } else if (tok.text == "tessctrl_shader") {
      auto sh = new TessCtrlShaderNode(this);
      sh->parse(scanview);
    } else if (tok.text == "tesseval_shader") {
      auto sh = new TessEvalShaderNode(this);
      sh->parse(scanview);
    } else if (tok.text == "geometry_shader") {
      auto sh = new GeometryShaderNode(this);
      sh->parse(scanview);
    } else if (tok.text == "fragment_shader") {
      auto sh = new FragmentShaderNode(this);
      sh->parse(scanview);
#if defined(ENABLE_COMPUTE_SHADERS)
    } else if (tok.text == "compute_shader") {
      auto sh = new ComputeShaderNode(this);
      sh->parse(scanview);
    } else if (tok.text == "compute_interface") {
      auto sif = new ComputeInterfaceNode(this);
      sif->parse(scanview);
#endif
#if defined(ENABLE_NVMESH_SHADERS)
    } else if (tok.text == "nvtask_shader") {
      auto sh = new NvTaskShaderNode(this);
      sh->parse(scanview);
    } else if (tok.text == "nvmesh_shader") {
      auto sh = new NvMeshShaderNode(this);
      sh->parse(scanview);
    } else if (tok.text == "nvtask_interface") {
      auto sif = new NvTaskInterfaceNode(this);
      sif->parse(scanview);
    } else if (tok.text == "nvmesh_interface") {
      auto sif = new NvMeshInterfaceNode(this);
      sif->parse(scanview);
#endif
    } else if (tok.text == "technique") {
      auto tek = new TechniqueNode(this);
      tek->parse(scanview);
    } else {
      printf("Unknown Token<%s>\n", tok.text.c_str());
      OrkAssert(false);
    }
    if( advance_block )
      itokidx = scanview.blockEnd() + 1;
  }
}

ContainerNode::nodevect_t ContainerNode::collectAllNodes() const {
  nodevect_t nodes;
  ////////////////////////////////////////////
  nodes.reserve(128);
  ////////////////////////////////////////////
  // build node list in desired order of processing
  // so do leafy nodes first and nodes
  //  which have dependencies later
  ////////////////////////////////////////////
  collectNodesOfType<LibraryBlockNode>(nodes);
  collectNodesOfType<ShaderDataNode>(nodes);
  collectNodesOfType<StateBlockNode>(nodes);

  collectNodesOfType<VertexInterfaceNode>(nodes);
  collectNodesOfType<TessCtrlInterfaceNode>(nodes);
  collectNodesOfType<TessEvalInterfaceNode>(nodes);
  collectNodesOfType<GeometryInterfaceNode>(nodes);
  collectNodesOfType<FragmentInterfaceNode>(nodes);

  collectNodesOfType<VertexShaderNode>(nodes);
  collectNodesOfType<TessCtrlShaderNode>(nodes);
  collectNodesOfType<TessEvalShaderNode>(nodes);
  collectNodesOfType<GeometryShaderNode>(nodes);
  collectNodesOfType<FragmentShaderNode>(nodes);

  #if defined(ENABLE_COMPUTE_SHADERS)
  collectNodesOfType<ComputeInterfaceNode>(nodes);
  collectNodesOfType<ComputeShaderNode>(nodes);
  #endif
  #if defined(ENABLE_NVMESH_SHADERS)
  collectNodesOfType<NvTaskInterfaceNode>(nodes);
  collectNodesOfType<NvMeshInterfaceNode>(nodes);
  collectNodesOfType<NvTaskShaderNode>(nodes);
  collectNodesOfType<NvMeshShaderNode>(nodes);
  #endif

  collectNodesOfType<TechniqueNode>(nodes);
  return nodes;
}

//////////////////////////////////////////////////////////////////////////////////

void ContainerNode::generate(shaderbuilder::BackEnd& backend) const {
  auto nodes = collectAllNodes();
  for( auto item : nodes )
    item->pregen(backend);
  for( auto item : nodes )
    item->generate(backend);
}

///////////////////////////////////////////////////////////////////////////////

GlSlFxParser::GlSlFxParser(const std::string& pth, const Scanner& s)
  : mPath(pth)
  , scanner(s) {
  _rootNode = new ContainerNode(pth,s);
  _rootNode->parse();
}

///////////////////////////////////////////////////////////

void GlSlFxParser::DumpAllTokens() {
  size_t itokidx     = 0;
  const auto& tokens = scanner.tokens;
  while (itokidx < tokens.size()) {
    const Token& tok = tokens[itokidx++];
    // printf( "tok<%d> <%s>\n", itokidx, tok.text.c_str() );
  }
}

///////////////////////////////////////////////////////////////////////////////

Container* LoadFxFromFile(const AssetPath& pth) {
  Scanner scanner(block_regex);
  ///////////////////////////////////
  File fx_file(pth, EFM_READ);
  OrkAssert(fx_file.IsOpen());
  EFileErrCode eFileErr = fx_file.GetLength(scanner.ifilelen);
  OrkAssert(scanner.ifilelen < scanner.kmaxfxblen);
  eFileErr                           = fx_file.Read(scanner.fxbuffer, scanner.ifilelen);
  scanner.fxbuffer[scanner.ifilelen] = 0;
  ///////////////////////////////////
  scanner.Scan();
  ///////////////////////////////////
  GlSlFxParser parser(pth.c_str(), scanner);
  auto pcont = new Container(pth.c_str());
  shaderbuilder::BackEnd backend(parser._rootNode,pcont);
  bool ok = backend.generate();
  assert(ok);
  ///////////////////////////////////
  return pcont;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
