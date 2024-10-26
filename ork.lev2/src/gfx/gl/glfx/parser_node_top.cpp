////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
bool TopNode::IsTokenOneOfTheBlockTypes(const Token& tok) {
  static const std::regex regex_block(block_regex);
  return std::regex_match(tok.text, regex_block);
}
///////////////////////////////////////////////////////////
TopNode::TopNode(GlSlFxParser* parser)
    : _parser(parser)
    , _scanner(parser->_scanner)
{
  //printf( "TopNode<%p>::construct()\n", this );

  std::string typenames = "mat2 mat3 mat4 vec2 vec3 vec4 uvec2 uvec3 uvec4 "
                          "ivec2 ivec3 ivec4 "
                          "bvec2 bvec3 bvec4 "
                          "float double half uint int float16_t "
                          "void bool "
#if !defined(__APPLE__)
                          "int8_t int16_t int32_t int64_t "
                          "uint8_t uint16_t uint32_t uint64_t "
                          "i8vec2 i8vec3 i8vec4 "
                          "i16vec2 i16vec3 i16vec4 "
                          "i32vec2 i32vec3 i32vec4 "
                          "i64vec2 i64vec3 i64vec4 "
                          "u16vec2 u16vec3 u16vec4 "
                          "u32vec2 u32vec3 u32vec4 "
                          "u64vec2 u64vec3 u64vec4 "

                          "image1D image2D image3D "
                          "imageCube image2DRect "
                          "imageBuffer image2DMS "
                          "image1DArray image2DArray image2DMSArray"
                          "imageCubeArray "

                          "iimage1D iimage2D iimage3D "
                          "iimageCube iimage2DRect "

                          "uimage1D uimage2D uimage3D "
                          "uimageCube uimage2DRect "
#endif
                          "sampler2D sampler3D sampler2DShadow samplerCube "
                          "usampler2D usampler3D "
                          "sampler1DArray sampler2DArray sampler3DArray";

  for (auto item : SplitString(typenames, ' '))
    _validTypeNames.insert(item);

  _validOutputDecorators.insert("perprimitiveNV");
  _validOutputDecorators.insert("taskNV");
  _validOutputDecorators.insert("flat");
  _validOutputDecorators.insert("varying");

  std::string kws = "for while do struct const if else "
                    "return not and or true false "
                    "uniform layout switch case flat";

  for (auto item : SplitString(typenames, ' '))
    _keywords.insert(item);
  for (auto item : _validOutputDecorators)
    _keywords.insert(item);
  for (auto item : SplitString(kws, ' '))
    _keywords.insert(item);

  _stddefines["PI"]          = "3.141592654";
  _stddefines["PI2"]         = "6.283185307";
  _stddefines["INV_PI"]      = "0.3183098861837907";
  _stddefines["INV_PI2"]     = "0.15915494309189535";
  _stddefines["PIDIV2"]      = "1.5707963267949";
  _stddefines["DEGTORAD"]    = "0.017453";
  _stddefines["RADTODEG"]    = "57.29578";
  _stddefines["E"]           = "2.718281828459";
  _stddefines["SQRT2"]       = "1.4142135623730951";
  _stddefines["GOLDENRATIO"] = "1.6180339887498948482";
  _stddefines["EPSILON"]     = "0.0000001";
  _stddefines["DTOR"]     = "0.017453292519943295";
  _stddefines["RTOD"]     = "57.29577951308232";

  for (auto item : _stddefines) {
    _keywords.insert(item.first);
  }

}
///////////////////////////////////////////////////////////
bool TopNode::validateKeyword(const std::string keyword) const {
  auto it = _keywords.find(keyword);
  return (it != _keywords.end());
}
///////////////////////////////////////////////////////////
void TopNode::validateTypeName(const std::string typeName) const {
  bool typeok = isTypeName(typeName);
  if (false == typeok) {
    deco::printf(fvec3::Red(), "type not found: ");
    deco::printf(fvec3::White(), "%s\n", typeName.c_str());
    assert(false);
  }
}
///////////////////////////////////////////////////////////
bool TopNode::isTypeName(const std::string typeName) const {
  auto it = _validTypeNames.find(typeName);
  return (it != _validTypeNames.end());
}
///////////////////////////////////////////////////////////
bool TopNode::validateIdentifierName(const std::string typeName) const {
  return true;
}
///////////////////////////////////////////////////////////
bool TopNode::isIoAttrDecorator(const std::string typeName) const {
  auto it = _validOutputDecorators.find(typeName);
  return (it != _validOutputDecorators.end());
}
///////////////////////////////////////////////////////////
void TopNode::parse() {
  //printf( "TopNode<%p:%s>::beginparse()\n", this, _parser->_name.c_str() );
  const auto& tokens = _scanner->tokens;

  //printf( "NumTokens<%d>\n", int(tokens.size()) );

  itokidx = 0;

  ScanViewRegex r("(\n)", true);

  auto program = _parser->_program;

  while (itokidx < tokens.size()) {
    const Token& tok = tokens[itokidx];
     //printf("token<%d> iline<%d> col<%d> text<%s>\n", itokidx, tok.iline + 1, tok.icol + 1, tok.text.c_str());

    ScannerView scanview(*_scanner, r);
    scanview.scanBlock(itokidx);

    bool advance_block = true;

    auto tokclass = TokenClass(tok._class);

    switch (tokclass) {
      case TokenClass::KW_OR_ID: {
        if (tok.text == "\n") {
        } else if (tok.text == "fxconfig") {
          assert(_configNode==nullptr);
          _configNode = std::make_shared<ConfigNode>();
          _configNode->parse(_parser,scanview);
        } else if (tok.text == "import") {
          const Token* impnam = _scanner->token(itokidx+1);
          const Token* semi = _scanner->token(itokidx+2);
          const Token* next = _scanner->token(itokidx+3);
          //printf( "IMPORT<%s>\n", impnam->text.c_str());
          //printf( "SEMI<%s>\n", semi->text.c_str());
          //printf( "NEXT<%s>\n", next->text.c_str());
          OrkAssert(semi->text==";");
          std::string p       = impnam->text.substr(1, impnam->text.length() - 2);
          //imports.push_back(p);
          //printf( "IMPORT<%s>\n", p.c_str());
          auto importnode = std::make_shared<ImportNode>(p,this);
          importnode->load();
          _imports.push_back(importnode);
          itokidx += 3;
          advance_block = false;
        } else if (tok.text == "libblock") {
          auto lb = std::make_shared<LibraryBlockNode>();
          lb->parse(_parser,scanview);
          program->addBlockNode(lb);
        } else if (tok.text == "uniform_set") {
          auto uniset = std::make_shared<UniformSetNode>();
          uniset->parse(_parser,scanview);
          program->addBlockNode(uniset);
        } else if (tok.text == "uniform_block") {
          auto uniblk = std::make_shared<UniformBlockNode>();
          uniblk->parse(_parser,scanview);
          program->addBlockNode(uniblk);
        } else if (tok.text == "vertex_interface") {
          auto sif = std::make_shared<VertexInterfaceNode>();
          sif->parse(_parser,scanview);
          program->addBlockNode(sif);
        } else if (tok.text == "tessctrl_interface") {
          auto sif = std::make_shared<TessCtrlInterfaceNode>();
          sif->parse(_parser,scanview);
          program->addBlockNode(sif);
        } else if (tok.text == "tesseval_interface") {
          auto sif = std::make_shared<TessEvalInterfaceNode>();
          sif->parse(_parser,scanview);
          program->addBlockNode(sif);
        } else if (tok.text == "geometry_interface") {
          auto sif = std::make_shared<GeometryInterfaceNode>();
          sif->parse(_parser,scanview);
          program->addBlockNode(sif);
        } else if (tok.text == "fragment_interface") {
          auto sif = std::make_shared<FragmentInterfaceNode>();
          sif->parse(_parser,scanview);
          program->addBlockNode(sif);
        } else if (tok.text == "state_block") {
          auto sblock = std::make_shared<StateBlockNode>();
          sblock->parse(_parser,scanview);
          program->addBlockNode(sblock);
        } else if (tok.text == "vertex_shader") {
          auto sh = std::make_shared<VertexShaderNode>();
          sh->parse(_parser,scanview);
          program->addBlockNode(sh);
        } else if (tok.text == "tessctrl_shader") {
          auto sh = std::make_shared<TessCtrlShaderNode>();
          sh->parse(_parser,scanview);
          program->addBlockNode(sh);
        } else if (tok.text == "tesseval_shader") {
          auto sh = std::make_shared<TessEvalShaderNode>();
          sh->parse(_parser,scanview);
          program->addBlockNode(sh);
        } else if (tok.text == "geometry_shader") {
          auto sh = std::make_shared<GeometryShaderNode>();
          sh->parse(_parser,scanview);
          program->addBlockNode(sh);
        } else if (tok.text == "fragment_shader") {
          auto sh = std::make_shared<FragmentShaderNode>();
          sh->parse(_parser,scanview);
          program->addBlockNode(sh);
        } else if (tok.text == "compute_shader") {
          auto sh = std::make_shared<ComputeShaderNode>();
          sh->parse(_parser,scanview);
//#if defined(ENABLE_COMPUTE_SHADERS)
          program->addBlockNode(sh);
//#endif
        } else if (tok.text == "compute_interface") {
          auto sif = std::make_shared<ComputeInterfaceNode>();
          sif->parse(_parser,scanview);
//#if defined(ENABLE_COMPUTE_SHADERS)
          program->addBlockNode(sif);
//#endif
        } else if (tok.text == "nvtask_shader") {
          auto sh = std::make_shared<NvTaskShaderNode>();
          sh->parse(_parser,scanview);
#if defined(ENABLE_NVMESH_SHADERS)
          program->addBlockNode(sh);
#endif
        } else if (tok.text == "nvmesh_shader") {
          auto sh = std::make_shared<NvMeshShaderNode>();
          sh->parse(_parser,scanview);
#if defined(ENABLE_NVMESH_SHADERS)
          program->addBlockNode(sh);
#endif
        } else if (tok.text == "nvtask_interface") {
          auto sif = std::make_shared<NvTaskInterfaceNode>();
          sif->parse(_parser,scanview);
#if defined(ENABLE_NVMESH_SHADERS)
          program->addBlockNode(sif);
#endif
        } else if (tok.text == "nvmesh_interface") {
          auto sif = std::make_shared<NvMeshInterfaceNode>();
          sif->parse(_parser,scanview);
#if defined(ENABLE_NVMESH_SHADERS)
          program->addBlockNode(sif);
#endif
        } else if (tok.text == "technique") {
          auto tek = std::make_shared<TechniqueNode>();
          tek->parse(_parser,scanview);
          program->addBlockNode(tek);
        } else {
          printf("Unknown Token<%s>\n", tok.text.c_str());
          OrkAssert(false);
        }
        break;
      }
      default:
        printf("Invalid TokenClass<%llu> tok<%s>\n", tok._class, tok.text.c_str());
        OrkAssert(false);
        break;
    }
    if (advance_block)
      itokidx = scanview.blockEnd() + 1;
  }
 // printf( "TopNode<%p:%s>::endparse()\n", this, _parser->_name.c_str() );

}
///////////////////////////////////////////////////////////
void TopNode::addStructType(structnode_ptr_t snode) {
  auto name = snode->_name->text;

  //printf( "TopNode<%p:%s>::addStructType(%p:%s)\n", 
    //       this, _parser->_name.c_str(),
      //     snode.get(), name.c_str() );

  auto it   = _structTypes.find(name);
  assert(it == _structTypes.end());
  auto it2 = _validTypeNames.find(name);
  assert(it2 == _validTypeNames.end());
  _structTypes[name] = snode;
  _validTypeNames.insert(name);
}
///////////////////////////////////////////////////////////

void TopNode::pregen(shaderbuilder::BackEnd& backend) const {
  static int level = 0;
  //printf( "TopNode<%d,%p:%s>::pregen()\n", level, this, _parser->_name.c_str() );
  level++;
  for( auto import : _imports ){
    auto imported_parser = import->_parser;
    auto imported_topnode = imported_parser->_topNode;
    imported_topnode->pregen(backend);
  }
  this->_enumerateValidationData(backend);
  level--;
}
///////////////////////////////////////////////////////////
void TopNode::_enumerateValidationData(shaderbuilder::BackEnd& backend) const {

  //printf( "TopNode<%p:%s>::_enumerateValidationData()\n", this, _parser->_name.c_str() );
  ///////////////////////////////////////////////////////
  // struct types
  ///////////////////////////////////////////////////////
  for (auto i : _structTypes) {
    auto k                     = i.first;
    auto v                     = i.second;
    /* printf("TopNode<%p:%s> IMPORT struct<%s::%s>\n",
             this, 
             _parser->_name.c_str(),
             _parser->_name.c_str(),
             k.c_str());*/

     auto it = backend._structTypes.find(k);
     OrkAssert(it==backend._structTypes.end());
     backend._structTypes[k] = v;
  }
  ///////////////////////////////////////////////////////
  // valid type names
  ///////////////////////////////////////////////////////
  for (auto vtn : _validTypeNames) {
     auto it = backend._validTypeNames.find(vtn);
     backend._validTypeNames.insert(vtn);
  }
}
///////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
