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

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

int ShaderBody::parse(GlSlFxParser* parser, const ScannerView& view) {
  ShaderLine* out_line = nullptr;
  int ist              = view._start + 1;
  int ien              = view._end - 1;
  bool bnewline        = true;
  int indent           = 1;
  ////////////////////////
  assert(view.token(view._start)->text == "{");
  assert(view.token(view._end)->text == "}");
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
  return view._end + 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void ShaderBody::emit(shaderbuilder::BackEnd& backend) const {
  auto& codegen       = backend._codegen;
  auto topnode = backend._parser->_topNode;
  const auto& stddefs = topnode->_stddefines;
  for (auto l : _lines) {
    codegen.beginLine();
    //////////////////////////////////////////////
    for (int in = 0; in < l->_indent; in++)
      codegen.incIndent();
    //////////////////////////////////////////////
    size_t num_tokens = l->_tokens.size();
    for( size_t i=0; i<num_tokens; i++ ) {
      auto tok0 = l->_tokens[i];
      auto it_def = stddefs.find(tok0->text);
      if (it_def != stddefs.end()) {
        codegen.output(it_def->second + " ");
      } else{
          bool write_tok = true;
          if(i<(num_tokens-3)){
            auto tok1 = l->_tokens[i+1];
            auto tok2 = l->_tokens[i+2];
            auto tok3 = l->_tokens[i+3];
            if (tok1->text == "[" and tok3->text == "]") {
              codegen.output(tok0->text);
              codegen.output(tok1->text);
              codegen.output(tok2->text);
              codegen.output(tok3->text+" ");
              write_tok = false;
              i += 3;
            }
          }
          if(write_tok) {
            codegen.output(tok0->text + " ");
          }
      }
      /////////////////////////
      if (tok0->text == "{") {
        codegen.endLine();
        codegen.incIndent();
        codegen.beginLine();
      } else if (tok0->text == "}") {
        codegen.endLine();
        codegen.decIndent();
        codegen.beginLine();
      }
      /////////////////////////
    }
    //////////////////////////////////////////////
    for (int in = 0; in < l->_indent; in++)
      codegen.decIndent();
    //////////////////////////////////////////////
    codegen.endLine();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
