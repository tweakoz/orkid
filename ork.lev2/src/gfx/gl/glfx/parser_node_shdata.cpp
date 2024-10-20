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
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

void ShaderDataNode::parse(GlSlFxParser* parser, const ScannerView& view) {
  DecoBlockNode::parse(parser,view);


  auto topnode = parser->_topNode;
  
  ////////////////////////

  size_t inumdecos = view.numBlockDecorators();
  for (size_t ideco = 0; ideco < inumdecos; ideco++) {
    auto ptok = view.blockDecorator(ideco);
  }
  ////////////////////////

  size_t ist = view._start + 1;
  size_t ien = view._end - 1;
  size_t i   = ist;
  bool done  = false;

  while (false == done) {
    const Token* dt_tok  = view.token(i);
    const Token* nam_tok = view.token(i + 1);

    bool is_endline = (dt_tok->text == "\n");
    if (is_endline) {
      i++;
    } else {

      interfacelayoutnode_ptr_t layout;
      if (dt_tok->text == "layout") {
        layout = std::make_shared<InterfaceLayoutNode>();
        ScannerView subview(view._scanner, view._filter);
        subview.scanUntil(view.globalTokenIndex(i), ")", true);
        layout->parse(parser,subview);
        i += subview.numTokens();
        dt_tok  = view.token(i);
        nam_tok = view.token(i + 1);
      }

      auto it = _dupenamecheck.find(nam_tok->text);
      assert(it == _dupenamecheck.end()); // make sure there are no duplicate uniforms

      _dupenamecheck.insert(nam_tok->text);

      topnode->validateTypeName(dt_tok->text);
      bool nameok = topnode->validateIdentifierName(nam_tok->text);

      auto unidecl       = std::make_shared<UniformDeclNode>();
      unidecl->_name     = nam_tok->text;
      unidecl->_typeName = dt_tok->text;
      unidecl->_layout   = layout;
      bool is_array      = false;
      if (view.token(i + 2)->text == "[") {
        assert(view.token(i + 4)->text == "]");
        unidecl->_arraySize = atoi(view.token(i + 3)->text.c_str());
        is_array            = true;
      }

      assert(nameok);

      _uniformdecls.push_back(unidecl);

      i += is_array ? 6 : 3;
    }
    done = (i >= ien);
    // printf("ni<%d> ien<%d> done<%d>\n", int(i), int(ien), int(done));
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void UniformDeclNode::emit(shaderbuilder::BackEnd& backend, bool emit_unitxt) const {
  auto& codegen = backend._codegen;

  codegen.beginLine();

  if (_layout)
    _layout->emit(backend);

  if (emit_unitxt)
    codegen.output("uniform ");
  codegen.output(_typeName + " ");
  codegen.output(_name);
  if (_arraySize) {
    codegen.format("[%d]", _arraySize);
  }
  codegen.output(";");
  codegen.endLine();
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void UniformSetNode::emit(shaderbuilder::BackEnd& backend) const {
  auto& codegen = backend._codegen;
  codegen.formatLine("/// begin uniformset");
  for (auto udecl : _uniformdecls)
    udecl->emit(backend, true);
  codegen.formatLine("/// end uniformset");
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void UniformSetNode::_generate2(shaderbuilder::BackEnd& backend) const {

  auto outcon = backend._container;

  assert(_blocktype == "uniform_set");

  UniformSet* uset = new UniformSet;
  uset->_name      = _name;

  for (auto item : _uniformdecls) {
    Uniform* puni                = outcon->MergeUniform(item->_name);
    puni->_typeName              = item->_typeName;
    puni->_arraySize             = item->_arraySize;
    uset->_uniforms[item->_name] = puni;
  }
  outcon->addUniformSet(uset);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void UniformBlockNode::emit(shaderbuilder::BackEnd& backend) const {
  auto& codegen = backend._codegen;
  codegen.formatLine("layout(std140) uniform %s { // uniformblock<%p>", _name.c_str(), this);
  codegen.incIndent();
  for (auto sub : _uniformdecls)
    sub->emit(backend, false);
  codegen.decIndent();
  codegen.formatLine("};");
}
void UniformBlockNode::_generate2(shaderbuilder::BackEnd& backend) const {

  auto outcon = backend._container;

  assert(_blocktype == "uniform_block");

  UniformBlock* ublk = new UniformBlock;
  ublk->_name        = _name;

  for (auto item : _uniformdecls) {
    auto puni        = new Uniform(item->_name);
    puni->_typeName  = item->_typeName;
    puni->_arraySize = item->_arraySize;
    ublk->_subuniforms.push_back(puni);
  }
  outcon->addUniformBlock(ublk);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
