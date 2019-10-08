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
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

void InterfaceNode::parse(const ScannerView& view) {
  DecoBlockNode::parse(view);
  ////////////////////////

  size_t ist = view._start + 1;
  size_t ien = view._end - 1;

  std::set<std::string> output_decorators;

  for (size_t i = ist; i <= ien;) {
    const Token* prv_tok = (i > 0) ? view.token(i - 1) : nullptr;
    const Token* vt_tok  = view.token(i);
    const Token* dt_tok  = view.token(i + 1);
    const Token* nam_tok = view.token(i + 2);
    const auto& named = nam_tok->text;
    printf("  ParseFxInterface VtTok<%s>\n", vt_tok->text.c_str());
    printf("  ParseFxInterface DtTok<%s>\n", dt_tok->text.c_str());
    printf("  ParseFxInterface named<%s>\n", named.c_str());
    ////////////////////////////////////////////////////////////////////////////
    if (vt_tok->text == "layout") {
      std::string layline;
      bool done      = false;
      auto layout = new InterfaceLayoutNode(_container);
      while (false == done) {
        done = vt_tok->text == ";";
        if( false == done ){
          layout->_tokens.push_back(vt_tok);
        }
        i++;
        vt_tok = view.token(i);
      }
      _layouts.push_back(layout);
    }
    ////////////////////////////////////////////////////////////////////////////
    else if (vt_tok->text == "in") {
      auto it = _inputdupecheck.find(named);
      assert(it == _inputdupecheck.end()); // make sure there are no duplicate attrs
      _inputdupecheck.insert(named);
      auto input = new InterfaceInputNode(_container);
      _inputs.push_back(input);
      input->_name = named;
      input->_typeName  = dt_tok->text;

      if (view.token(i + 3)->text == ":") {
        input->_semantic = view.token(i + 4)->text;
        i += 6;
      } else if (view.token(i + 3)->text == ";") {
        i += 4;
      } else if (view.token(i + 3)->text == "[") {
        input->_arraySize = atoi(view.token(i + 4)->text.c_str());
        i += 7;
      } else {
        assert(false);
      }
    }
    ////////////////////////////////////////////////////////////////////////////
    else if (vt_tok->text == "perprimitiveNV") {
      output_decorators.insert("perprimitiveNV");
      i++;
    }
    ////////////////////////////////////////////////////////////////////////////
    else if (vt_tok->text == "out") {
      auto it = _outputdupecheck.find(named);
      assert(it == _outputdupecheck.end()); // make sure there are no duplicate attrs
      _outputdupecheck.insert(named);
      auto output = new InterfaceOutputNode(_container);
      _outputs.push_back(output);
      output->_name = named;
      output->_typeName  = dt_tok->text;
      output->_output_decorators = output_decorators;
      output_decorators.clear();
      ///////////////////////////////////////////
      auto parse_array = [&](int j) -> int {
        if (view.token(j)->text == "[") {
          if (view.token(j + 1)->text == "]") {
            // unsized array
            output->_arraySize = -1;
            j += 2;
          } else {
            assert(view.token(j + 3)->text == "]");
            output->_arraySize = atoi(view.token(j + 2)->text.c_str());
            j += 3;
          }
        }
        return j;
      };
      ///////////////////////////////////////////
      // inline struct type ?
      ///////////////////////////////////////////

      if (view.token(i + 2)->text == "{") {
        output->_inlineStruct = new InterfaceInlineStructNode(_container);
        // struct output
        int closer = -1;
        for (int s = 1; (s < 100) and (closer == -1); s++) {
          const Token* test_tok = view.token(i + s);
          if (test_tok->text == "}") {
            closer = s;
          }
          output->_inlineStruct->_tokens.push_back(test_tok);
        }
        assert(closer > 0);
        int j = parse_array(i + closer + 2);
        if (j > (i + closer + 2)) { // array of inline struct
          i += closer + 5;          // i now points
        } else {
          i += closer + 3; // i now points
        }
      }

      ///////////////////////////////////////////

      if (view.token(i + 3)->text == ";") {
        i += 4;
      } else if (view.token(i + 3)->text == "[") {
        i = parse_array(i + 3);
        assert(i > 0);
      }
      ////////////////////////////////////////////////////////////////////////////
    } else if (vt_tok->text == "\n" or vt_tok->text == ";") {
      i++;
    } else {
      ////////////////////////////////////////////////////////////////////////////
      printf("invalid token<%s>\n", vt_tok->text.c_str());
      OrkAssert(false);
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

StreamInterface* InterfaceNode::_generate(Container* c, GLenum iftype) {


  ////////////////////////

  StreamInterface* psi = new StreamInterface;
  psi->mName           = _name;
  psi->mInterfaceType  = iftype;

  ////////////////////////

  bool is_vtx                 = iftype == GL_VERTEX_SHADER;
  bool is_geo                 = iftype == GL_GEOMETRY_SHADER;

  /////////////////////////////
  // interface inheritance
  /////////////////////////////

  for( auto deconame : _deconames ){

    auto it_uniformset = c->_uniformSets.find(deconame);
    auto it_uniformblk = c->_uniformBlocks.find(deconame);

    if (it_uniformset != c->_uniformSets.end()) {
      psi->_uniformSets.push_back(it_uniformset->second);
    } else if (it_uniformblk != c->_uniformBlocks.end()) {
      psi->_uniformBlocks.push_back(it_uniformblk->second);
    } else if (is_vtx) {
      auto it_vi = c->_vertexInterfaces.find(deconame);
      assert(it_vi != c->_vertexInterfaces.end());
      psi->Inherit(*it_vi->second);
    } else if (is_geo) {
      auto it_fig = c->_geometryInterfaces.find(deconame);
      auto it_fiv = c->_vertexInterfaces.find(deconame);
      auto it_fie = c->_tessEvalInterfaces.find(deconame);
      bool is_geo = (it_fig != c->_geometryInterfaces.end());
      bool is_vtx = (it_fiv != c->_vertexInterfaces.end());
      bool is_tee = (it_fie != c->_tessEvalInterfaces.end());
      assert(is_geo || is_vtx || is_tee);
      auto par = is_geo ? it_fig->second : is_vtx ? it_fiv->second : is_tee ? it_fie->second : nullptr;
      assert(par != nullptr);
      psi->Inherit(*par);
    } else {
      auto it_fi = c->_fragmentInterfaces.find(deconame);
      assert(it_fi != c->_fragmentInterfaces.end());
      psi->Inherit(*it_fi->second);
    }
  } // for (size_t ideco = 0; ideco < inumdecos; ideco++) {

  ////////////////////////

  for( auto layout : _layouts ){
    /*
    std::string layline;
    bool done      = false;
    bool is_input  = false;
    bool has_punc  = false;
    bool is_points = false;
    bool is_lines  = false;
    bool is_tris   = false;

    while (false == done) {
      const auto& txt = vt_tok->text;

      is_input |= (txt == "in");
      is_points |= (txt == "points");
      is_lines |= (txt == "lines");
      is_tris |= (txt == "triangles");

      bool is_punc = (txt == "(") || (txt == ")") || (txt == ",");
      has_punc |= is_punc;

      // if( is_punc )
      layline += " ";
      layline += vt_tok->text;
      // if( is_punc )
      layline += " ";
      done = vt_tok->text == ";";
      i++;
      vt_tok = v.token(i);
    }
    layline += "\n";
    psi->mPreamble.push_back(layline);

    ////////////////////////////////////////////////////////////////////////////
    if (has_punc && is_input) {
      ////////////////////////////////////////////////////////////////////////////
      if (is_points)
        psi->mGsPrimSize = 1;
      if (is_lines)
        psi->mGsPrimSize = 2;
      if (is_tris)
        psi->mGsPrimSize = 3;
    }*/
    }
  for( auto input : _inputs ){
    int iloc          = int(psi->mAttributes.size());
    Attribute* pattr  = new Attribute(input->_name);
    pattr->mTypeName  = input->_typeName;
    pattr->mDirection = "in";
    pattr->mSemantic = input->_semantic;
    psi->mAttributes[input->_name] = pattr;
    pattr->mLocation = int(psi->mAttributes.size());
  }
  for( auto output : _outputs ){
    int iloc                        = int(psi->mAttributes.size());
    Attribute* pattr                = new Attribute(output->_name);
    pattr->mTypeName                = output->_typeName;
    pattr->mDirection               = "out";
    pattr->mLocation                = iloc;
    psi->mAttributes[output->_name] = pattr;
    pattr->_decorators = output->_output_decorators;
  }

  ////////////////////////
  // sort attributes for performance
  //  (see
  //  http://stackoverflow.com/questions/16415037/opengl-core-profile-incredible-slowdown-on-os-x)
  ////////////////////////

  std::multimap<int, Attribute*> attr_sort_map;
  for (const auto& it : psi->mAttributes) {
    auto attr  = it.second;
    auto itloc = GlSlFxParser::gattrsorter.find(attr->mSemantic);
    int isort  = 100;
    if (itloc != GlSlFxParser::gattrsorter.end()) {
      isort = itloc->second;
    }
    attr_sort_map.insert(std::make_pair(isort, attr));
    // pattr->mLocation = itloc->second;
  }

  int isort = 0;
  for (const auto& it : attr_sort_map) {
    auto attr       = it.second;
    attr->mLocation = isort++;
  }

  ////////////////////////

  assert(false);
  return psi;

}

StreamInterface* VertexInterfaceNode::generate(Container*c) {
  return InterfaceNode::_generate(c,GL_VERTEX_SHADER);
}
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
