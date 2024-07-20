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

void RequiredExtensionNode::emit(shaderbuilder::BackEnd& backend) {
  backend._codegen.formatLine("#extension %s : enable", _extension.c_str());
}


///////////////////////////////////////////////////////////

void NamedBlockNode::parse(GlSlFxParser* parser, const ScannerView& view) {
  _name      = view.blockName();
  _blocktype = view.token(view._blockType)->text;
}

///////////////////////////////////////////////////////////

void DecoBlockNode::parse(GlSlFxParser* parser, const ScannerView& view) {
  NamedBlockNode::parse(parser,view);

  auto topnode = parser->_topNode;

  // default extensions

  #if !defined(__APPLE__)
  auto extnode        = std::make_shared<RequiredExtensionNode>();
  extnode->_extension = "GL_ARB_gpu_shader_int64";
  _requiredExtensions.push_back(extnode);

  extnode        = std::make_shared<RequiredExtensionNode>();
  extnode->_extension = "GL_NV_gpu_shader5";
  _requiredExtensions.push_back(extnode);
#endif

  /////////////////////////////
  // fetch block decorators
  /////////////////////////////

  size_t inumdecos = view.numBlockDecorators();

  for (size_t ideco = 0; ideco < inumdecos; ideco++) {
    auto decotok = view.blockDecorator(ideco);
    auto decoref = decotok->text;
    if (decoref == "extension") {
      int decoglobidx = view._blockDecorators[ideco];
      assert(view._scanner.token(decoglobidx + 1)->text == "(");
      assert(view._scanner.token(decoglobidx + 3)->text == ")");
      auto extname        = view._scanner.token(decoglobidx + 2)->text;
      auto extnode        = std::make_shared<RequiredExtensionNode>();
      extnode->_extension = extname;
      _requiredExtensions.push_back(extnode);
    } else {
      bool name_ok = topnode->validateIdentifierName(decoref);
      auto it      = _decodupecheck.find(decoref);
      assert(it == _decodupecheck.end() or decoref == "extension");
      _decodupecheck.insert(decoref);
      _decorators.push_back(decotok);
    }
  }
}

///////////////////////////////////////////////////////////
void DecoBlockNode::_pregen(shaderbuilder::BackEnd& backend) const {
  size_t inumdecos = _decorators.size();
  auto parser = backend._parser;
  auto program = parser->_program;

  auto decochildren = std::make_shared<DecoChildren>();
  decochildren->_parent = this;
  backend._decochildrenmap[this] = decochildren;

  for (size_t i = 0; i < inumdecos; i++) {
    auto deco                = _decorators[i]->text;
    auto it_nodedeco         = program->_blockNodes.find(deco);
    bool present = it_nodedeco != program->_blockNodes.end();
    decoblocknode_ptr_t blocknode = present 
                                  ? it_nodedeco->second 
                                  : nullptr;

    if(blocknode==nullptr){
      //printf("BlockNode<%s> not found\n", deco.c_str());
      //OrkAssert(false);
    }

    if (auto as_if = std::dynamic_pointer_cast<InterfaceNode>(blocknode)) {
      decochildren->_interfaceNodes.emplace_back(as_if);
      as_if->_pregen(backend);
    } else if (auto as_lib = std::dynamic_pointer_cast<LibraryBlockNode>(blocknode)) {
      decochildren->_libraryBlocks.emplace_back(as_lib);
      as_lib->_pregen(backend);
    } else if (auto as_uset = std::dynamic_pointer_cast<UniformSetNode>(blocknode)) {
      decochildren->_uniformSets.emplace_back(as_uset);
      as_uset->_pregen(backend);
    } else if (auto as_ublk = std::dynamic_pointer_cast<UniformBlockNode>(blocknode)) {
      decochildren->_uniformBlocks.emplace_back(as_ublk);
      as_ublk->_pregen(backend);
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
