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
#include <ork/util/crc.h>
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////

void StructNode::pregen(shaderbuilder::BackEnd& backend) const {}

/////////////////////////////////////////////////////////////////////////////////////////////////

void StructNode::emit(shaderbuilder::BackEnd& backend) const {
  auto& codegen = backend._codegen;
  if( _emitstructandname )
    codegen.formatLine("struct %s {", _name->text.c_str() );
  else
    codegen.formatLine("{");
  codegen.incIndent();
  for (auto m : _members) {
    codegen.beginLine();
    codegen.output(m->_type->text + " ");
    codegen.output(m->_name->text);
    if (m->_arraySize) {
      codegen.format("[%u]", m->_arraySize);
    }
    codegen.output(";");
    codegen.endLine();
  }
  codegen.decIndent();
  codegen.beginLine();
  codegen.output("}");
  if( _emitstructandname )
    codegen.output(";");
  codegen.endLine();
}

/////////////////////////////////////////////////////////////////////////////////////////////////

int StructNode::parse(GlSlFxParser* parser, const ScannerView& view) {
  int i = 0;
  assert(view.token(i)->text == "{");
  i += 1; // advance {
  int closer            = -1;
  bool done             = false;
  const Token* test_tok = nullptr;
  while (false == done) {
    test_tok = view.token(i++);
    if (test_tok->text == "}") {
      done = true;
    } else {
      auto member   = std::make_shared<StructMemberNode>();
      member->_type = test_tok;
      member->_name = view.token(i++);
      test_tok      = view.token(i);
      if (test_tok->text == "[") { // array ?
        member->_arraySize = atoi(view.token(i + 1)->text.c_str());
        assert(view.token(i + 2)->text == "]");
        assert(view.token(i + 3)->text == ";");
        i += 4;
      } else {
        assert(test_tok->text == ";");
        i++;
      }
      _members.push_back(member);
    }
  }
  return i;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
