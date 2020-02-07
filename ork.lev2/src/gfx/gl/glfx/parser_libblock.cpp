////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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

void LibraryBlockNode::parse(const ScannerView& view) {
  DecoBlockNode::parse(view);
  int ist = view._start + 1;
  int ien = view._end - 1;
  for (size_t i = ist; i <= ien;) {
    auto ptok   = view.token(i);
    auto namtok = view.token(i + 1);
    assert(_container->validateIdentifierName(namtok->text));
    if (ptok->text == "struct") {
      // parse struct
      view.checktoken(i + 2, "{");
      i += 2; // advance to {
      ScannerView structview(view, i);
      auto snode   = new StructNode(_container);
      snode->_name = namtok;
      _container->addStructType(snode);
      i += snode->parse(structview);
      _children.push_back(snode);
      view.checktoken(i, ";");
      i++;
    } else { // must be a function
      view.checktoken(i + 2, "(");
      auto returntype = ptok->text;
      _container->validateTypeName(returntype);
      i += 2; // advance to (
      auto fnnode         = new FunctionNode(_container);
      fnnode->_name       = namtok;
      fnnode->_returnType = ptok;
      ScannerView fnview(view, i);
      i += fnnode->parse(fnview);
      _children.push_back(fnnode);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////

void LibraryBlockNode::pregen(shaderbuilder::BackEnd& backend) {
  DecoBlockNode::_pregen(backend);
}

//////////////////////////////////////////////////////////////////////////////////

void LibraryBlockNode::emit(shaderbuilder::BackEnd& backend) const {
  DecoBlockNode::_emit(backend);
  auto& codegen = backend._codegen;
  codegen.formatLine("// begin libblock<%s> ///////////////////////////////////", _name.c_str());
  for (auto item : _children) {
    if (auto as_struct = item.TryAs<StructNode*>()) {
      as_struct.value()->emit(backend);
    } else if (auto as_fn = item.TryAs<FunctionNode*>()) {
      as_fn.value()->emit(backend);
    } else {
      assert(false);
    }
  }

  //_body.emit(backend);
  codegen.formatLine("// end libblock<%s> ///////////////////////////////////", _name.c_str());
}

//////////////////////////////////////////////////////////////////////////////////

void LibraryBlockNode::generate(shaderbuilder::BackEnd& backend) const {
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
