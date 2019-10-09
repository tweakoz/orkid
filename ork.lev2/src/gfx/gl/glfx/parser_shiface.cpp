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

const std::map<std::string, int> gattrsorter = {
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

int InterfaceLayoutNode::parse(const ScannerView& view) {
  int i               = view._start;
  const Token* vt_tok = view.token(i);
  bool done           = false;
  while (false == done) {
    done = (vt_tok->text == ";");

    if( done )
        this->_standaloneLayout = true;
    else
      this->_tokens.push_back(vt_tok);
    done |= (vt_tok->text == ")");

    i++;
    vt_tok = view.token(i);
  }
  return i;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void InterfaceNode::parseInputs(const ScannerView& view) {
  size_t ist = view._start + 1;
  size_t ien = view._end - 1;
  for (size_t i = ist; i <= ien;) {
    const Token* prv_tok = (i > 0) ? view.token(i - 1) : nullptr;
    const Token* dt_tok  = view.token(i);
    const Token* nam_tok = view.token(i + 1);
    std::string named = nam_tok ? nam_tok->text : "";
    printf("  parseInputs DtTok<%s>\n", dt_tok->text.c_str());
    printf("  parseInputs named<%s>\n", named.c_str());

    if (dt_tok->text == "layout") {
      std::string layline;
      auto layout = new InterfaceLayoutNode(_container);
      int j       = layout->parse(view);
      assert(j > i);
      i = j;
      _inputlayouts.push_back(layout);
      continue;
    }

    bool typeok = _container->validateTypeName(dt_tok->text);
    bool nameok = _container->validateMemberName(named);


    assert(typeok and nameok);
    auto it = _inputdupecheck.find(named);
    assert(it == _inputdupecheck.end()); // make sure there are no duplicate attrs
    _inputdupecheck.insert(named);
    auto input = new InterfaceInputNode(_container);
    _inputs.push_back(input);
    input->_name     = named;
    input->_typeName = dt_tok->text;
    if (view.token(i + 2)->text == ":") {
      input->_semantic = view.token(i + 3)->text;
      i += 5;
    } else if (view.token(i + 2)->text == ";") {
      i += 3;
    } else if (view.token(i + 2)->text == "[") {
      input->_arraySize = atoi(view.token(i + 3)->text.c_str());
      i += 6;
    } else {
      assert(false);
    }
  }
}
void InterfaceNode::parseOutputs(const ScannerView& view) {
  size_t ist = view._start + 1;
  size_t ien = view._end - 1;
  std::set<std::string> outputdecos;
  for (size_t i = ist; i <= ien;) {
    const Token* prv_tok = (i > 0) ? view.token(i - 1) : nullptr;
    const Token* dt_tok  = view.token(i );
    const Token* nam_tok = view.token(i + 1);

    //////////////////////////////////
    // layout
    //////////////////////////////////

    if (dt_tok->text == "layout") {
      std::string layline;
      // todo PARSE layout until closing )
      //  if tokens follow layout before the ;
      //  then the layout scope is for the following output tokens
      //  otherwise it must be block scope
      auto layout = new InterfaceLayoutNode(_container);
      ScannerView subview(view._scanner,view._filter);
      subview.scanUntil(view.globalTokenIndex(i),")",true);
      layout->parse(subview);
      i += subview.numTokens();
      if(layout->_standaloneLayout)
          _interfacelayouts.push_back(layout);
      else{
          assert(_outputlayout==nullptr);
          _outputlayout = layout;
      }
      continue;
    }

    //////////////////////////////////
    // check for output decorators
    //////////////////////////////////

    if(_container->isOutputDecorator(dt_tok->text)){
      outputdecos.insert(dt_tok->text);
      i++;
      continue;
    }

    std::string named = nam_tok ? nam_tok->text : "";
    printf("  parseOutputs DtTok<%s>\n", dt_tok->text.c_str());
    printf("  parseOutputs named<%s>\n", named.c_str());
    _container->validateTypeName(dt_tok->text);
    auto it = _outputdupecheck.find(named);
    assert(it == _outputdupecheck.end()); // make sure there are no duplicate attrs
    _outputdupecheck.insert(named);
    auto output = new InterfaceOutputNode(_container);
    _outputs.push_back(output);
    output->_name              = named;
    output->_typeName          = dt_tok->text;
    output->_output_decorators = outputdecos;
    outputdecos.clear();
    if( _outputlayout ) {
        output->_layout = _outputlayout;
        _outputlayout = nullptr;
    }
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

    if (view.token(i + 2)->text == ";") {
      i += 3;
    } else if (view.token(i + 2)->text == "[") {
      i = parse_array(i + 2);
      assert(i > 0);
    }
    ////////////////////////////////////////////////////////////////////////////
  }
}

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
    const auto& named    = nam_tok->text;
    printf("  ParseFxInterface VtTok<%zu:%s>\n", i, vt_tok->text.c_str());
    printf("  ParseFxInterface DtTok<%s>\n", dt_tok->text.c_str());
    printf("  ParseFxInterface named<%s>\n", named.c_str());
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    if (vt_tok->text == "inputs") {
      assert(view.token(i + 1)->text == "{");
      ScannerView inputsview(view,i+1);
      i += inputsview.numTokens() + 1;
      parseInputs(inputsview);
    } else if (vt_tok->text == "outputs") {
      assert(view.token(i + 1)->text == "{");
      ScannerView outputsview(view,i+1);
      i += outputsview.numTokens() + 1;
      parseOutputs(outputsview);
    }
    else {
      ////////////////////////////////////////////////////////////////////////////
      printf("invalid token<%s>\n", vt_tok->text.c_str());
      assert(false);
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

  bool is_vtx = iftype == GL_VERTEX_SHADER;
  bool is_geo = iftype == GL_GEOMETRY_SHADER;

  /////////////////////////////
  // interface inheritance
  /////////////////////////////

  for (auto decotok : _decorators) {

    auto deconame = decotok->text;

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

  //for (auto layout : _layouts) {
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
  //}
  for (auto input : _inputs) {
    int iloc                       = int(psi->mAttributes.size());
    Attribute* pattr               = new Attribute(input->_name);
    pattr->mTypeName               = input->_typeName;
    pattr->mDirection              = "in";
    pattr->mSemantic               = input->_semantic;
    psi->mAttributes[input->_name] = pattr;
    pattr->mLocation               = int(psi->mAttributes.size());
  }
  for (auto output : _outputs) {
    int iloc                        = int(psi->mAttributes.size());
    Attribute* pattr                = new Attribute(output->_name);
    pattr->mTypeName                = output->_typeName;
    pattr->mDirection               = "out";
    pattr->mLocation                = iloc;
    psi->mAttributes[output->_name] = pattr;
    pattr->_decorators              = output->_output_decorators;
  }

  ////////////////////////
  // sort attributes for performance
  //  (see
  //  http://stackoverflow.com/questions/16415037/opengl-core-profile-incredible-slowdown-on-os-x)
  ////////////////////////

  std::multimap<int, Attribute*> attr_sort_map;
  for (const auto& it : psi->mAttributes) {
    auto attr  = it.second;
    auto itloc = gattrsorter.find(attr->mSemantic);
    int isort  = 100;
    if (itloc != gattrsorter.end()) {
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

  return psi;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

StreamInterface* VertexInterfaceNode::generate(Container* c) {
  auto sif = InterfaceNode::_generate(c, GL_VERTEX_SHADER);
  c->addVertexInterface(sif);
  return sif;
}
StreamInterface* FragmentInterfaceNode::generate(Container* c) {
  auto sif = InterfaceNode::_generate(c, GL_FRAGMENT_SHADER);
  c->addFragmentInterface(sif);
  return sif;
}
StreamInterface* TessCtrlInterfaceNode::generate(Container* c) {
  auto sif = InterfaceNode::_generate(c, GL_TESS_CONTROL_SHADER);
  c->addTessCtrlInterface(sif);
  return sif;
}
StreamInterface* TessEvalInterfaceNode::generate(Container* c) {
  auto sif = InterfaceNode::_generate(c, GL_TESS_EVALUATION_SHADER);
  c->addTessEvalInterface(sif);
  return sif;
}
StreamInterface* GeometryInterfaceNode::generate(Container* c) {
  auto sif = InterfaceNode::_generate(c, GL_GEOMETRY_SHADER);
  c->addGeometryInterface(sif);
  return sif;
}
#if defined(ENABLE_NVMESH_SHADERS)
StreamInterface* NvTaskInterfaceNode::generate(Container* c) {
  auto sif = InterfaceNode::_generate(c, GL_TASK_SHADER_NV);
  c->addNvTaskInterface(sif);
  return sif;
}
StreamInterface* NvMeshInterfaceNode::generate(Container* c) {
  auto sif = InterfaceNode::_generate(c, GL_MESH_SHADER_NV);
  c->addNvMeshInterface(sif);
  return sif;
}
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
