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

const std::map<std::string, int> GlSlFxParser::gattrsorter = {
    {"POSITION", 0},
    {"NORMAL", 1},
    {"COLOR0", 2},
    {"COLOR1", 3},
    {"TEXCOORD0", 4},
    {"TEXCOORD0", 5},
    {"TEXCOORD1", 6},
    {"TEXCOORD2", 7},
    {"TEXCOORD3", 8},
    {"BONEINDICES", 9},
    {"BONEWEIGHTS", 10},
};

///////////////////////////////////////////////////////////
GlSlFxParser::GlSlFxParser(const AssetPath& pth, const Scanner& s)
  : mPath(pth)
  , scanner(s) {
  _rootNode = new ContainerNode(pth,s);
  _rootNode->parse();
}
///////////////////////////////////////////////////////////
bool ContainerNode::IsTokenOneOfTheBlockTypes(const Token& tok) {
  std::regex regex_block(token_regex);
  return std::regex_match(tok.text, regex_block);
}
///////////////////////////////////////////////////////////
ContainerNode::ContainerNode(const AssetPath &pth, const Scanner &s)
  : _path(pth)
  , _scanner(s) {

    std::string typenames = "mat2 mat3 mat4 vec2 vec3 vec4 "
                            "float double half int "
                            "sampler2D sampler3D sampler2DShadow";

    for( auto item : SplitString(typenames, ' ') )
      _validTypeNames.insert(item);

    _validOutputDecorators.insert("perprimitiveNV");
}
///////////////////////////////////////////////////////////
bool ContainerNode::validateTypeName(const std::string typeName) const {
  auto it = _validTypeNames.find(typeName);
  return (it!=_validTypeNames.end());
}
bool ContainerNode::validateMemberName(const std::string typeName) const {
  return true;
}
bool ContainerNode::isOutputDecorator(const std::string typeName) const {
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
    bool name_ok = _container->validateMemberName(decoref);
    auto it = _decodupecheck.find(decoref);
    assert(it==_decodupecheck.end());
    _decodupecheck.insert(decoref);
    _decorators.push_back(decotok);
  }

}

///////////////////////////////////////////////////////////

void ContainerNode::addBlockNode(DecoBlockNode*node) {
  auto it = _blockNodes.find(node->_name);
  assert(it==_blockNodes.end());
  _blockNodes[node->_name]=node;
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
      assert(false);
    } else if (tok.text == "uniform_set") {
      auto uniset = new UniformSetNode(this);
      uniset->parse(scanview);
    } else if (tok.text == "uniform_block") {
      auto uniblk = new UniformBlockNode(this);
      uniblk->parse(scanview);
      assert(false);
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
      assert(false);
    } else if (tok.text == "tesseval_shader") {
      auto sh = new TessEvalShaderNode(this);
      sh->parse(scanview);
      assert(false);
    } else if (tok.text == "geometry_shader") {
      auto sh = new GeometryShaderNode(this);
      sh->parse(scanview);
      assert(false);
    } else if (tok.text == "fragment_shader") {
      auto sh = new FragmentShaderNode(this);
      sh->parse(scanview);
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
  //if (false == bOK) {
//    delete mpContainer;
  //  mpContainer = nullptr;
  //}
  //return mpContainer;
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

//////////////////////////////////////////////////////////////////////////////////

Container* ContainerNode::createContainer() const {
  assert(false);
  //mpContainer = new Container(fxname.c_str());
  //bool bOK    = true;
  return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////

Container* LoadFxFromFile(const AssetPath& pth) {
  Scanner scanner;
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
  GlSlFxParser parser(pth, scanner);
  Container* pcont = parser._rootNode->createContainer();
  ///////////////////////////////////
  return pcont;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
