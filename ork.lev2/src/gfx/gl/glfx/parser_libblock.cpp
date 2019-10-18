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
void LibraryBlockNode::parse(const ScannerView& view) {
  DecoBlockNode::parse(view);
  _body.parse(view);
}

//////////////////////////////////////////////////////////////////////////////////

void LibraryBlockNode::pregen(shaderbuilder::BackEnd& backend) { DecoBlockNode::_pregen(backend); }

//////////////////////////////////////////////////////////////////////////////////

void LibraryBlockNode::emit(shaderbuilder::BackEnd& backend) const {
  DecoBlockNode::_emit(backend);
  auto& codegen = backend._codegen;
  codegen.formatLine("// libblock<%s> ///////////////////////////////////", _name.c_str());
  for (auto l : _body._lines) {
    codegen.beginLine();
    for (int in = 0; in < l->_indent; in++)
      codegen.incIndent();
    for (auto t : l->_tokens) {
      codegen.output(t->text + " ");
      if (t->text == "{") {
        codegen.endLine();
        codegen.incIndent();
        codegen.beginLine();
      } else if (t->text == "}") {
        codegen.endLine();
        codegen.decIndent();
        codegen.beginLine();
      }
    }
    for (int in = 0; in < l->_indent; in++)
      codegen.decIndent();
    codegen.endLine();
  }
}

//////////////////////////////////////////////////////////////////////////////////

void LibraryBlockNode::generate(shaderbuilder::BackEnd& backend) const {}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
