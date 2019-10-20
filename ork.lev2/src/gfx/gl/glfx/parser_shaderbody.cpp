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

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

int ShaderBody::parse(const ScannerView& view) {
  ShaderLine* out_line = nullptr;
  int ist              = view._start + 1;
  int ien              = view._end - 1;
  bool bnewline        = true;
  int indent           = 1;
  ////////////////////////
  assert( view.token(view._start)->text == "{" );
  assert( view.token(view._end)->text == "}" );
  ////////////////////////
  for (size_t i = ist; i <= ien; i++) {
    ////////////////////////
    // create a new line if its a new line...
    ////////////////////////
    if (bnewline) {
      out_line          = new ShaderLine;
      out_line->_indent = indent;
      _lines.push_back(out_line);
    }
    bnewline = false;
    ////////////////////////
    auto ptok                  = view.token(i);
    const std::string& cur_tok = ptok->text;
    ////////////////////////
    if (cur_tok != "\n")
      out_line->_tokens.push_back(ptok);
    ////////////////////////
    if (cur_tok == "\n" or cur_tok == ";") {
      bnewline = true;
    } else if (cur_tok == "{")
      indent++;
    else if (cur_tok == "}")
      indent--;
    ////////////////////////
  }
  return view._end+1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void ShaderBody::emit(shaderbuilder::BackEnd& backend) const {
  auto& codegen = backend._codegen;
  for (auto l : _lines) {
    codegen.beginLine();
    for (int in = 0; in < l->_indent; in++)
      codegen.incIndent();
    for (auto t : l->_tokens) {
      codegen.output(t->text + " ");
      if (t->text == "{") {
        codegen.endLine();
        codegen.incIndent();
        codegen.beginLine();
      }
      else if (t->text == "}") {
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

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
