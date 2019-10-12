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

/////////////////////////////////////////////////////////////////////////////////////////////////

int InterfaceLayoutNode::parse(const ScannerView& view) {
  int i               = view._start;
  const Token* vt_tok = view.token(i);
  bool done           = false;
  while (false == done) {
    _tokens.push_back(vt_tok);
    done |= (vt_tok->text == ")");

    i++;
    vt_tok = view.token(i);
  }
  return i;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void InterfaceNode::parseIos(const ScannerView& view,IoContainer& ioc) {
  size_t ist = view._start + 1;
  size_t ien = view._end - 1;
  std::set<std::string> decos;

  for (size_t i = ist; i <= ien;) {
    const Token* prv_tok = (i > 0) ? view.token(i - 1) : nullptr;
    const Token* dt_tok  = view.token(i);

    if (dt_tok->text == ";") {
      i++;
      continue;
    }

    //////////////////////////////////
    // layout
    //////////////////////////////////

    if (dt_tok->text == "layout") {
      std::string layline;
      auto layout = new InterfaceLayoutNode(_container);
      ScannerView subview(view._scanner,view._filter);
      subview.scanUntil(view.globalTokenIndex(i),")",true);
      layout->parse(subview);
      layout->_direction = ioc._direction;
      i += subview.numTokens(); // advance layout
      auto try_semicolon = view.token(i)->text;
      if( try_semicolon == ";") {
        layout->_standaloneLayout = true;
        _interfacelayouts.push_back(layout);
      }
      else{
          assert(ioc._pendinglayout==nullptr);
          ioc._pendinglayout = layout;
      }
      continue;
    }
    
    //////////////////////////////////
    // check for ioattr decorators
    //////////////////////////////////

    if(_container->isIoAttrDecorator(dt_tok->text)){
      decos.insert(dt_tok->text);
      i++; // advance decorator
      continue;
    }

    //////////////////////////////////
    // typename
    //////////////////////////////////

    printf("  parseOutputs DtTok<%s>\n", dt_tok->text.c_str());
    bool typeisvalid = _container->validateTypeName(dt_tok->text);
    auto io = new InterfaceIoNode(_container);
    ioc._nodes.push_back(io);
    io->_typeName   = dt_tok->text;
    io->_decorators = decos;
    decos.clear();
    if (ioc._pendinglayout) {
      io->_layout        = ioc._pendinglayout;
      ioc._pendinglayout = nullptr;
    }

    i++; // advance typename
    
    ///////////////////////////////////////////
    // inline struct type ?
    ///////////////////////////////////////////

    auto try_struct = view.token(i)->text;
    
    if (try_struct == "{") {
      io->_inlineStruct = new InterfaceInlineStructNode(_container);
      io->_inlineStruct->_tokens.push_back(view.token(i));
      i += 1; // advance {
      // struct io
      int closer = -1;
      for (int s = 0; (s < 100) and (closer == -1); s++) {
        const Token* test_tok = view.token(i++);
        if (test_tok->text == "}") {
          closer = s;
        }
        io->_inlineStruct->_tokens.push_back(test_tok);
      }
      assert(closer > 0);
    }

    ///////////////////////////////////////////
    // var name
    ///////////////////////////////////////////

    const Token* nam_tok = view.token(i);
    std::string named = nam_tok ? nam_tok->text : "";
    printf("  parseOutputs named<%s>\n", named.c_str());
    auto it = ioc._dupecheck.find(named);
    assert(it == ioc._dupecheck.end()); // make sure there are no duplicate attrs
    ioc._dupecheck.insert(named);
    io->_name              = named;

    i++; // advance name

    ///////////////////////////////////////////
    // array ?
    ///////////////////////////////////////////

    auto try_array = view.token(i)->text;

    if (try_array == "[") { // array ?
        if (view.token(i + 1)->text == "]") {
          // unsized array
          io->_arraySize = -1;
          i += 2; // advance []
        } else {
          assert(view.token(i + 2)->text == "]");
          io->_arraySize = atoi(view.token(i + 1)->text.c_str());
          i += 3; // advance [n]
        }
    }

    ///////////////////////////////////////////
    // Semantic ?
    ///////////////////////////////////////////

    auto try_semantic = view.token(i)->text;

    if (try_semantic == ":") { // semantic ?
      io->_semantic = view.token(i + 1)->text;
      i += 2; // advance : semantic
    }

    ///////////////////////////////////////////
    // ;
    ///////////////////////////////////////////

    auto try_semicolon = view.token(i)->text;

    assert (try_semicolon == ";");
    i ++; // advance ;

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
      parseIos(inputsview,_inputs);
    } else if (vt_tok->text == "outputs") {
      assert(view.token(i + 1)->text == "{");
      ScannerView outputsview(view,i+1);
      i += outputsview.numTokens() + 1;
      parseIos(outputsview,_outputs);
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

  ////////////////////////
  // interface scoped layouts
  ////////////////////////

  psi->mPreamble.push_back("/*begin SIF preamble*/");
  std::string line;
  for( auto iflayout : _interfacelayouts ){
    for (auto item : iflayout->_tokens ) {
      const auto& tok = item->text;
      line += tok;
      if( tok == ")" ) {
        line += " " + iflayout->_direction;
        line += ";";
        psi->mPreamble.push_back(line);
        line = "";
      }
      
      // GS primtype
      int primsize = 0;
      if( tok == "points" )
        primsize = 1;
      else if( tok == "lines" )
        primsize = 2;
      else if( tok == "triangles" )
        primsize = 3;
      else if( tok == "triangle_strip" )
        primsize = 4;

      if(primsize!=0){
        if(iflayout->_direction=="in"){
            psi->_gspriminpsize = primsize;
        }
        else {
            psi->_gsprimoutsize = primsize;
        }
      }
    }
  }
  psi->mPreamble.push_back("/*end SIF preamble*/");

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
  // attribute scoped layouts
  ////////////////////////
  
  for (auto input : _inputs._nodes) {
    int iloc                       = int(psi->_inputAttributes.size());
    Attribute* pattr               = new Attribute(input->_name);
    pattr->mTypeName               = input->_typeName;
    pattr->mDirection              = "in";
    pattr->mSemantic               = input->_semantic;
    psi->_inputAttributes[input->_name] = pattr;
    pattr->mLocation               = int(psi->_inputAttributes.size());
    pattr->mArraySize = input->_arraySize;
    if( input->_layout ){
      for (auto item : input->_layout->_tokens ) {
        const auto& tok = item->text;
        pattr->mLayout += tok;
      }
    }
  }
  //
  for (auto output : _outputs._nodes) {
    
    int iloc                        = int(psi->_outputAttributes.size());
    Attribute* pattr                = new Attribute(output->_name);
    pattr->mTypeName                = output->_typeName;
    pattr->mDirection               = "out";
    pattr->mLocation                = iloc;
    psi->_outputAttributes[output->_name] = pattr;
    pattr->_decorators              = output->_decorators;

    int arysiz = output->_arraySize;
    arysiz  = (arysiz==0)
            ? psi->_gsprimoutsize
            : arysiz;

    pattr->mArraySize = arysiz;

    for( auto deco : output->_decorators )
        pattr->mDecorators += deco + " ";
    
    if( output->_inlineStruct ) {
      for (auto tok : output->_inlineStruct->_tokens )
        pattr->mInlineStruct += tok->text + " ";
    }

    if( output->_layout ){
      for (auto item : output->_layout->_tokens )
        pattr->mLayout+=item->text;
    }
  }

  ////////////////////////
  // sort attributes for performance
  //  (see
  //  http://stackoverflow.com/questions/16415037/opengl-core-profile-incredible-slowdown-on-os-x)
  ////////////////////////

  std::multimap<int, Attribute*> attr_sort_map;
  for (const auto& it : psi->_inputAttributes) {
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
