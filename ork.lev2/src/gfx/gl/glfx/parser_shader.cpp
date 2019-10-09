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

void Shader::addLibBlock(LibBlock* lib) {
  _libblocks.push_back(lib);
  for (auto uset : lib->_uniformSets)
    addUniformSet(uset);
  for (auto ublk : lib->_uniformBlocks)
    addUniformBlock(ublk);
}
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
  int ist = view._start + 1;
  int ien = view._end - 1;
  bool bnewline = true;
  int indent    = 1;
  for (size_t i = ist; i <= ien; i++) {
    ////////////////////////
    // create a new line if its a new line...
    ////////////////////////
    if (bnewline) {
      out_line = new ShaderLine;
      out_line->_indent = indent;
      _lines.push_back(out_line);
    }
    bnewline = false;
    ////////////////////////
    auto ptok = view.token(i);
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
void ShaderNode::parse(const ScannerView& view) { _body.parse(view); }
///////////////////////////////////////////////////////////
void ShaderNode::_generateCommon(Shader* pshader) {
  LibBlock* plibblock = nullptr;
  Container* c   = pshader->mpContainer;

  bool is_vertex_shader   = pshader->mShaderType == GL_VERTEX_SHADER;
  bool is_tessctrl_shader = pshader->mShaderType == GL_TESS_CONTROL_SHADER;
  bool is_tesseval_shader = pshader->mShaderType == GL_TESS_EVALUATION_SHADER;
  bool is_geometry_shader = pshader->mShaderType == GL_GEOMETRY_SHADER;
  bool is_fragment_shader = pshader->mShaderType == GL_FRAGMENT_SHADER;
#if defined(ENABLE_NVMESH_SHADERS)
  bool is_nvtask_shader = pshader->mShaderType == GL_TASK_SHADER_NV;
  bool is_nvmesh_shader = pshader->mShaderType == GL_MESH_SHADER_NV;
#endif

  //////////////////////////////////////////////
  // enumerate lib blocks / interfaces
  //////////////////////////////////////////////

  size_t inumdecos = _decorators.size();

  for (size_t i = 0; i < inumdecos; i++) {

    auto deco = _decorators[i]->text;

    auto it_uset = c->_uniformSets.find(deco);
    auto it_ublk = c->_uniformBlocks.find(deco);
    auto it_lib  = c->_libBlocks.find(deco);
    auto it_vi   = c->_vertexInterfaces.find(deco);
    auto it_tc   = c->_tessCtrlInterfaces.find(deco);
    auto it_te   = c->_tessEvalInterfaces.find(deco);
    auto it_gi   = c->_geometryInterfaces.find(deco);
    auto it_fi   = c->_fragmentInterfaces.find(deco);

#if defined(ENABLE_NVMESH_SHADERS)
    auto it_nvt = c->_nvTaskInterfaces.find(deco);
    auto it_nvm = c->_nvMeshInterfaces.find(deco);
#endif

    if (it_lib != c->_libBlocks.end()) {
      auto plibblock = it_lib->second;
      pshader->addLibBlock(plibblock);
    } else if (it_ublk != (c->_uniformBlocks.end())) {
      auto ublk = it_ublk->second;
      pshader->addUniformBlock(ublk);
    } else if (it_uset != (c->_uniformSets.end())) {
      auto uset = it_uset->second;
      pshader->addUniformSet(uset);
    } else if (deco == "extension") {
      auto lparen = _decorators[i + 1];
      auto rparen = _decorators[i + 3];
      assert(lparen->text == "(");
      assert(rparen->text == ")");
      auto extid = _decorators[i + 2];
      pshader->requireExtension(extid->text);
      i += 3; // eat parens and extensionid
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

   std::string shaderbody;

  size_t iline = 1;
  FixedString<64> fxstr;
  auto prline = [&]() {
    fxstr.format("/*%03d*/", int(iline));
    shaderbody += fxstr.c_str();
    iline++;
  };

  prline();

  shaderbody += "#version 410 core\n";

    ////////////////////////////////////////////////////////////////////////////
  // declare required extensions
  ////////////////////////////////////////////////////////////////////////////

  for (auto extension : pshader->_requiredExtensions) {
    prline();
    shaderbody += FormatString("#extension %s : enable\n", extension.c_str());
  }

    ////////////////////////////////////////////////////////////////////////////

  for (const auto& preamble_line : iface->mPreamble) {
    prline();
    shaderbody += preamble_line;
  }

  ///////////////////////
  // UNIFORM Set
  ///////////////////////

  for (const auto& ub : pshader->_unisets) {
    for (auto itu : ub->_uniforms) {
      prline();
      shaderbody += "uniform ";
      shaderbody += itu.second->genshaderbody();
      shaderbody += ";\n";
    }
  }

  ///////////////////////
  // UNIFORM Block
  ///////////////////////

  for (const auto& ub : pshader->_uniblocks) {
    prline();
    shaderbody += FormatString("layout(std140) uniform %s {\n", ub->_name.c_str());
    for (auto itsub : ub->_subuniforms) {
      prline();
      shaderbody += itsub.second->genshaderbody();
      shaderbody += ";\n";
    }
    prline();
    shaderbody += "};\n";
  }

  ///////////////////////
  // ATTRIBUTES
  ///////////////////////
  for (StreamInterface::AttrMap::const_iterator ita = iface->mAttributes.begin(); ita != iface->mAttributes.end(); ita++) {
    prline();
    Attribute* pa = ita->second;

    shaderbody += pa->mDirection + " ";
    shaderbody += pa->mTypeName + " ";

    if (pa->mArraySize) {
      ork::FixedString<128> fxs;
      // fxs.format("%s[%d]", pa->mName.c_str(), pa->mArraySize );
      fxs.format("%s[]", pa->mName.c_str());
      shaderbody += fxs.c_str();
    } else
      shaderbody += pa->mName;

    shaderbody += ";";

    if (pa->mComment.length()) {
      shaderbody += pa->mComment;
    }

    shaderbody += "\n";
  }
  
  ///////////////////////
  // code
  ///////////////////////

  for( auto blockitem : _container->_blockNodes ) {
    auto block = blockitem.second;
    auto libblock = dynamic_cast<LibraryBlockNode*>(block);
    if( libblock ) {
      prline();
      shaderbody += "// libblock<" + libblock->_name + "> ///////////////////////////////////\n";

      for (auto l : libblock->_body._lines) {
        prline();
        for (int in = 0; in < l->_indent; in++)
          shaderbody += "\t";
        for (auto t : l->_tokens) {
          shaderbody += t->text;
          shaderbody += " ";
        }
        shaderbody += "\n";
      }
    }
  }
  
  shaderbody += "///////////////////////////////////////////////////////////////////\n";

  for( auto l : _body._lines ){
    prline();
    for (int in = 0; in < l->_indent; in++)
      shaderbody += "\t";
    for( auto t : l->_tokens ){
      shaderbody += t->text;
      shaderbody += " ";
    }
    shaderbody += "\n";
  }

  ///////////////////////////////////

  prline();
  shaderbody += "void " + _name + "()\n{";
  shaderbody += "}\n";

  ///////////////////////////////////

  pshader->mName       = _name;
  pshader->mShaderText = shaderbody;

  ///////////////////////////////////
  // printf( "shaderbody\n" );
  // printf( "///////////////////////////////\n" );
  // printf( "%s", shaderbody.c_str() );
  // printf( "///////////////////////////////\n" );

}

///////////////////////////////////////////////////////////
ShaderVtx* VertexShaderNode::generate(Container* c) {
  auto pshader         = new ShaderVtx();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  return pshader;
}
///////////////////////////////////////////////////////////
ShaderTsC* TessCtrlShaderNode::generate(Container* c) {
  auto pshader         = new ShaderTsC();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  return pshader;
}
///////////////////////////////////////////////////////////
ShaderTsE* TessEvalShaderNode::generate(Container* c) {
  auto pshader         = new ShaderTsE();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  return pshader;
}
///////////////////////////////////////////////////////////
ShaderGeo* GeometryShaderNode::generate(Container* c) {
  auto pshader         = new ShaderGeo();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  return pshader;
}
///////////////////////////////////////////////////////////
ShaderFrg* FragmentShaderNode::generate(Container* c) {
  auto pshader         = new ShaderFrg();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  return pshader;
}
#if defined(ENABLE_NVMESH_SHADERS)
ShaderNvTask* NvTaskShaderNode::generate(Container* c) {
  auto pshader = new ShaderNvTask();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  return pshader;
}
ShaderNvMesh* NvMeshShaderNode::generate(Container* c) {
  auto pshader = new ShaderNvMesh();
  pshader->mpContainer = c;
  _generateCommon(pshader);
  return pshader;
}
#endif


///////////////////////////////////////////////////////////
void LibraryBlockNode::parse(const ScannerView& view) {
  _body.parse(view);
}

//////////////////////////////////////////////////////////////////////////////////

LibBlock::LibBlock(const Scanner& s)
    : mFilter(nullptr)
    , mView(nullptr) {
  mFilter = new ScanViewFilter();
  mView   = new ScannerView(s, *mFilter);
}

//////////////////////////////////////////////////////////////////////////////////

LibBlock* LibraryBlockNode::generate(Container* c) const {
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
  return libblock;*/
  return nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
