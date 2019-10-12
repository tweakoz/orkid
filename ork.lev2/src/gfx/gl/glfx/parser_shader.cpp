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

/*void Shader::addLibBlock(LibBlock* lib) {
  _libblocks.push_back(lib);
  for (auto uset : lib->_uniformSets)
    addUniformSet(uset);
  for (auto ublk : lib->_uniformBlocks)
    addUniformBlock(ublk);
}*/
/////////////////////////////////////////////////////////////////////////////////////////////////
void Shader::setInputInterface(StreamInterface* iface) {
  _inputInterface = iface;
  for (auto uset : iface->_uniformSets)
    addUniformSet(uset);
  for (auto ublk : iface->_uniformBlocks)
    addUniformBlock(ublk);
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void Shader::addUniformSet(UniformSet* uset) { _unisets.push_back(uset); }
/////////////////////////////////////////////////////////////////////////////////////////////////
void Shader::addUniformBlock(UniformBlock* ublk) { _uniblocks.push_back(ublk); }

/////////////////////////////////////////////////////////////////////////////////////////////////

void ShaderBody::parse(const ScannerView& view) {
  ShaderLine* out_line = nullptr;
  int ist              = view._start + 1;
  int ien              = view._end - 1;
  bool bnewline        = true;
  int indent           = 1;
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
}
///////////////////////////////////////////////////////////
void ShaderNode::parse(const ScannerView& view) {
  DecoBlockNode::parse(view);
  _body.parse(view);
}
///////////////////////////////////////////////////////////
void ShaderNode::_generateCommon(Shader* pshader) {
  pshader->mName      = _name;
  //LibBlock* plibblock = nullptr;
  Container* c        = pshader->mpContainer;

  bool is_vertex_shader   = pshader->mShaderType == GL_VERTEX_SHADER;
  bool is_tessctrl_shader = pshader->mShaderType == GL_TESS_CONTROL_SHADER;
  bool is_tesseval_shader = pshader->mShaderType == GL_TESS_EVALUATION_SHADER;
  bool is_geometry_shader = pshader->mShaderType == GL_GEOMETRY_SHADER;
  bool is_fragment_shader = pshader->mShaderType == GL_FRAGMENT_SHADER;
#if defined(ENABLE_NVMESH_SHADERS)
  bool is_nvtask_shader = pshader->mShaderType == GL_TASK_SHADER_NV;
  bool is_nvmesh_shader = pshader->mShaderType == GL_MESH_SHADER_NV;
#endif

  for( auto ext : _requiredExtensions ) {
      pshader->requireExtension(ext);
  }

  //////////////////////////////////////////////
  // enumerate lib blocks / interfaces
  //////////////////////////////////////////////

  size_t inumdecos = _decorators.size();

  for (size_t i = 0; i < inumdecos; i++) {

    auto deco = _decorators[i]->text;
    auto it_nodedeco  = _container->_blockNodes.find(deco);
    DecoBlockNode* blocknode = (it_nodedeco!=_container->_blockNodes.end())
                             ? it_nodedeco->second
                             : nullptr;
    auto it_uset = c->_uniformSets.find(deco);
    auto it_ublk = c->_uniformBlocks.find(deco);
    auto it_vi   = c->_vertexInterfaces.find(deco);
    auto it_tc   = c->_tessCtrlInterfaces.find(deco);
    auto it_te   = c->_tessEvalInterfaces.find(deco);
    auto it_gi   = c->_geometryInterfaces.find(deco);
    auto it_fi   = c->_fragmentInterfaces.find(deco);

#if defined(ENABLE_NVMESH_SHADERS)
    auto it_nvt = c->_nvTaskInterfaces.find(deco);
    auto it_nvm = c->_nvMeshInterfaces.find(deco);
#endif


    if (auto as_lib = dynamic_cast<LibraryBlockNode*>(blocknode)) {
      for( auto tok_deco : as_lib->_decorators ){
        auto lib_deco = tok_deco->text;
        auto it_usetl = c->_uniformSets.find(lib_deco);
         auto it_ublkl = c->_uniformBlocks.find(lib_deco);
          if( it_ublkl != (c->_uniformBlocks.end())) {
           auto ublk = it_ublkl->second;
           pshader->addUniformBlock(ublk);
         }
          if( it_usetl != (c->_uniformSets.end())) {
           auto uset = it_usetl->second;
           pshader->addUniformSet(uset);
         }
      }
      //assert(false);
      //pshader->addLibBlock(plibblock);
    } else if (it_ublk != (c->_uniformBlocks.end())) {
      auto ublk = it_ublk->second;
      pshader->addUniformBlock(ublk);
    } else if (it_uset != (c->_uniformSets.end())) {
      auto uset = it_uset->second;
      pshader->addUniformSet(uset);
    } else if (is_vertex_shader && it_vi != (c->_vertexInterfaces.end())) {
      pshader->setInputInterface(it_vi->second);
    } else if (is_tessctrl_shader && (it_tc != c->_tessCtrlInterfaces.end())) {
      pshader->setInputInterface(it_tc->second);
    } else if (is_tesseval_shader && (it_te != c->_tessEvalInterfaces.end())) {
      pshader->setInputInterface(it_te->second);
    } else if (is_geometry_shader && (it_gi != c->_geometryInterfaces.end())) {
      pshader->setInputInterface(it_gi->second);
    } else if (is_fragment_shader && (it_fi != c->_fragmentInterfaces.end())) {
      pshader->setInputInterface(it_fi->second);
#if defined(ENABLE_NVMESH_SHADERS)
    } else if (is_nvtask_shader && (it_nvt != c->_nvTaskInterfaces.end())) {
      pshader->setInputInterface(it_nvt->second);
    } else if (is_nvmesh_shader && (it_nvm != c->_nvMeshInterfaces.end())) {
      pshader->setInputInterface(it_nvm->second);
#endif
    } else {
      printf("bad shader interface decorator!\n");
      printf("shader<%s>\n", _name.c_str());
      printf("deco<%s>\n", deco.c_str());
      printf("is_vtx<%d> is_geo<%d> is_frg<%d>\n", int(is_vertex_shader), int(is_geometry_shader), int(is_fragment_shader));
      assert(false);
    }
  }
  //////////////////////////////////////////////
  //
  //////////////////////////////////////////////
  auto iface = pshader->_inputInterface;
  assert(iface != nullptr);
  //////////////////////////////////////////////

  struct ShaderLine {
      ShaderLine(std::string l) : _text(l) {}
      std::string _text;
  };
  struct ShaderLines {
    void add(std::string l) { _lines.push_back(new ShaderLine(l)); }
    std::vector<ShaderLine*> _lines;
  };
  ShaderLines lines;

  lines.add("#version 410 core");

  ////////////////////////////////////////////////////////////////////////////
  // declare required extensions
  ////////////////////////////////////////////////////////////////////////////

  for (auto extension : pshader->_requiredExtensions) {
    lines.add(FormatString("#extension %s : enable", extension.c_str()));
  }

  ////////////////////////////////////////////////////////////////////////////

  for (const auto& preamble_line : iface->mPreamble) {
    lines.add(preamble_line);
  }

  ///////////////////////
  // UNIFORM Set
  ///////////////////////

  for (const auto& ub : pshader->_unisets) {
    for (auto itu : ub->_uniforms) {
      std::string l = "uniform ";
      l += itu.second->genshaderbody();
      l += ";";
      lines.add(l);
    }
  }

  ///////////////////////
  // UNIFORM Block
  ///////////////////////

  for (const auto& ub : pshader->_uniblocks) {
    lines.add(FormatString("layout(std140) uniform %s {", ub->_name.c_str()));
    for (auto sub : ub->_subuniforms) {
      lines.add(sub->genshaderbody()+";");
    }
    lines.add("};");
  }

  ///////////////////////
  // ATTRIBUTES
  ///////////////////////
  for (StreamInterface::AttrMap::const_iterator ita = iface->mAttributes.begin(); ita != iface->mAttributes.end(); ita++) {
    Attribute* pa = ita->second;

    std::string l;
  
    if( pa->mLayout.length() )
      l += pa->mLayout + " ";
  
    l += pa->mDirection + " ";
    l += pa->mTypeName + " ";

    if (pa->mArraySize) {
      ork::FixedString<128> fxs;
      // fxs.format("%s[%d]", pa->mName.c_str(), pa->mArraySize );
      fxs.format("%s[]", pa->mName.c_str());
      l += fxs.c_str();
    } else
      l += pa->mName;

    l += ";";

    if (pa->mComment.length()) {
      l += pa->mComment;
    }
    lines.add(l);
  }

  ///////////////////////
  // code
  ///////////////////////

  for (auto block : _container->_orderedBlockNodes) {
    auto libblock = dynamic_cast<LibraryBlockNode*>(block);
    if (libblock) {
      bool present = false;
      for( auto b : _decorators ){
          if(b->text == libblock->_name ){
            lines.add("// libblock<" + libblock->_name + "> ///////////////////////////////////");

            for (auto l : libblock->_body._lines) {
              std::string ol;
              for (int in = 0; in < l->_indent; in++)
                ol += "\t";
              for (auto t : l->_tokens) {
                ol += t->text;
                ol += " ";
              }
              lines.add(ol);
            }
          }
      }
    }
  }

  lines.add("///////////////////////////////////////////////////////////////////");

  lines.add("void " + _name + "(){");

  for (auto l : _body._lines) {
    std::string ol;
    for (int in = 0; in < l->_indent; in++)
      ol += "\t";
    for (auto t : l->_tokens) {
      ol += t->text;
      ol += " ";
    }
    lines.add(ol);
  }

  lines.add("}");

  ///////////////////////////////////

  pshader->mName       = _name;

  std::string shaderbody;
  int iline = 0;
  for( auto l : lines._lines ){
    shaderbody += FormatString("/*%03d*/ ",iline++);
    shaderbody += l->_text;
    shaderbody += "\n";
  }
  pshader->mShaderText = shaderbody;

  ///////////////////////////////////
  printf( "shaderbody\n" );
  printf( "///////////////////////////////\n" );
  printf( "%s", shaderbody.c_str() );
  printf( "///////////////////////////////\n" );
}

///////////////////////////////////////////////////////////
ShaderVtx* VertexShaderNode::generate(Container* c) {
  auto pshader         = new ShaderVtx();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  c->addVertexShader(pshader);
  return pshader;
}
///////////////////////////////////////////////////////////
ShaderTsC* TessCtrlShaderNode::generate(Container* c) {
  auto pshader         = new ShaderTsC();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  c->addTessCtrlShader(pshader);
  return pshader;
}
///////////////////////////////////////////////////////////
ShaderTsE* TessEvalShaderNode::generate(Container* c) {
  auto pshader         = new ShaderTsE();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  c->addTessEvalShader(pshader);
  return pshader;
}
///////////////////////////////////////////////////////////
ShaderGeo* GeometryShaderNode::generate(Container* c) {
  auto pshader         = new ShaderGeo();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  c->addGeometryShader(pshader);
  return pshader;
}
///////////////////////////////////////////////////////////
ShaderFrg* FragmentShaderNode::generate(Container* c) {
  auto pshader         = new ShaderFrg();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  c->addFragmentShader(pshader);
  return pshader;
}
#if defined(ENABLE_NVMESH_SHADERS)
ShaderNvTask* NvTaskShaderNode::generate(Container* c) {
  auto pshader         = new ShaderNvTask();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  c->addNvTaskShader(pshader);
  return pshader;
}
ShaderNvMesh* NvMeshShaderNode::generate(Container* c) {
  auto pshader         = new ShaderNvMesh();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  c->addNvMeshShader(pshader);
  return pshader;
}
#endif

///////////////////////////////////////////////////////////
void LibraryBlockNode::parse(const ScannerView& view) {
  DecoBlockNode::parse(view);
  _body.parse(view);
}

//////////////////////////////////////////////////////////////////////////////////
/*
LibBlock::LibBlock(const Scanner& s)
    : mFilter(nullptr)
    , mView(nullptr) {
  mFilter = new ScanViewFilter();
  mView   = new ScannerView(s, *mFilter);
}*/

//////////////////////////////////////////////////////////////////////////////////

void LibraryBlockNode::generate(Container* c) const {

  for (auto l : _body._lines) {
    for (auto t : l->_tokens) {
    }
  }


  /*int cachetokidx = itokidx;
  ////////////////////////////////////////////
  // scan library block code
  ////////////////////////////////////////////
  //auto libblock = new LibBlock(scanner);
  libblock->mView->scanBlock(itokidx);
  libblock->mName = scanner.tokens[itokidx + 1].text;
  itokidx         = libblock->mView->blockEnd() + 1;
  ////////////////////////////////////////////
  // scan decorators
  ////////////////////////////////////////////
  ScanViewRegex r("(\n)", true);
  ScannerView v(scanner, r);
  v.scanBlock(cachetokidx);
  size_t inumdecos = v.numBlockDecorators();
  for (size_t ideco = 0; ideco < inumdecos; ideco++) {
    auto ptok          = v.blockDecorator(ideco);
    auto it_uniformset = mpContainer->_uniformSets.find(ptok->text);
    auto it_uniformblk = mpContainer->_uniformBlocks.find(ptok->text);
    if (it_uniformset != mpContainer->_uniformSets.end()) {
      libblock->_uniformSets.push_back(it_uniformset->second);
    } else if (it_uniformblk != mpContainer->_uniformBlocks.end()) {
      libblock->_uniformBlocks.push_back(it_uniformblk->second);
    } else {
      assert(false);
    }
  }
  ////////////////////////////////////////////
  return libblock;
   */
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
