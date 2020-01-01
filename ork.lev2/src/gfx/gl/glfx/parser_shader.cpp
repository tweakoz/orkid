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

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
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
///////////////////////////////////////////////////////////
void ShaderNode::parse(const ScannerView& view) {
  DecoBlockNode::parse(view);
  _body.parse(view);
}
void ShaderNode::pregen(shaderbuilder::BackEnd& backend) { DecoBlockNode::_pregen(backend); }
///////////////////////////////////////////////////////////
void ShaderNode::_generateCommon(shaderbuilder::BackEnd& backend) const {
  auto pshader  = backend._statemap["curshader"].Get<Shader*>();
  auto& codegen = backend._codegen;
  codegen.flush();

////////////////////////////////////////////////////
#if defined(__APPLE__)
  codegen.formatLine("#version 410 core");
#else
  codegen.formatLine("#version 460 core");
#endif
  ////////////////////////////////////////////////////

  pshader->mName = _name;
  // LibBlock* plibblock = nullptr;
  Container* c = pshader->mpContainer;

  bool is_vertex_shader   = pshader->mShaderType == GL_VERTEX_SHADER;
  bool is_tessctrl_shader = pshader->mShaderType == GL_TESS_CONTROL_SHADER;
  bool is_tesseval_shader = pshader->mShaderType == GL_TESS_EVALUATION_SHADER;
  bool is_geometry_shader = pshader->mShaderType == GL_GEOMETRY_SHADER;
  bool is_fragment_shader = pshader->mShaderType == GL_FRAGMENT_SHADER;
#if defined(ENABLE_NVMESH_SHADERS)
  bool is_nvtask_shader = pshader->mShaderType == GL_TASK_SHADER_NV;
  bool is_nvmesh_shader = pshader->mShaderType == GL_MESH_SHADER_NV;
#endif
#if defined(ENABLE_COMPUTE_SHADERS)
  bool is_compute_shader = pshader->mShaderType == GL_COMPUTE_SHADER;
#endif

  //////////////////////////////////////////////
  // visit lib blocks
  //////////////////////////////////////////////

  for (auto as_lib : _libraryBlocks) {
    for (auto tok_deco : as_lib->_decorators) {
      auto lib_deco = tok_deco->text;
      auto it_usetl = c->_uniformSets.find(lib_deco);
      auto it_ublkl = c->_uniformBlocks.find(lib_deco);
      if (it_ublkl != (c->_uniformBlocks.end())) {
        auto ublk = it_ublkl->second;
        pshader->addUniformBlock(ublk);
      }
      if (it_usetl != (c->_uniformSets.end())) {
        auto uset = it_usetl->second;
        pshader->addUniformSet(uset);
      }
    }
  }

  //////////////////////////////////////////////
  // visit interfaces
  //////////////////////////////////////////////

  for (auto ifnode : _interfaceNodes) {
    const auto& named = ifnode->_name;
    //////////////////////////////////////////////////////////////////
    if (auto as_vxif = dynamic_cast<VertexInterfaceNode*>(ifnode)) {
      assert(is_vertex_shader);
      auto it_vi = c->_vertexInterfaces.find(named);
      assert(it_vi != c->_vertexInterfaces.end());
      pshader->setInputInterface(it_vi->second);
    }
    //////////////////////////////////////////////////////////////////
    if (auto as_tcif = dynamic_cast<TessCtrlInterfaceNode*>(ifnode)) {
      assert(is_tessctrl_shader);
      auto it_tcif = c->_tessCtrlInterfaces.find(named);
      assert(it_tcif != c->_tessCtrlInterfaces.end());
      pshader->setInputInterface(it_tcif->second);
    }
    //////////////////////////////////////////////////////////////////
    if (auto as_teif = dynamic_cast<TessEvalInterfaceNode*>(ifnode)) {
      assert(is_tesseval_shader);
      auto it_teif = c->_tessEvalInterfaces.find(named);
      assert(it_teif != c->_tessEvalInterfaces.end());
      pshader->setInputInterface(it_teif->second);
    }
    //////////////////////////////////////////////////////////////////
    if (auto as_geif = dynamic_cast<GeometryInterfaceNode*>(ifnode)) {
      assert(is_geometry_shader);
      auto it_geif = c->_geometryInterfaces.find(named);
      assert(it_geif != c->_geometryInterfaces.end());
      pshader->setInputInterface(it_geif->second);
    }
    //////////////////////////////////////////////////////////////////
    if (auto as_frif = dynamic_cast<FragmentInterfaceNode*>(ifnode)) {
      assert(is_fragment_shader);
      auto it_frif = c->_fragmentInterfaces.find(named);
      assert(it_frif != c->_fragmentInterfaces.end());
      pshader->setInputInterface(it_frif->second);
    }
    //////////////////////////////////////////////////////////////////
#if defined(ENABLE_NVMESH_SHADERS)
    //////////////////////////////////////////////////////////////////
    if (auto as_ntif = dynamic_cast<NvTaskInterfaceNode*>(ifnode)) {
      assert(is_nvtask_shader);
      auto it_ntif = c->_nvTaskInterfaces.find(named);
      assert(it_ntif != c->_nvTaskInterfaces.end());
      pshader->setInputInterface(it_ntif->second);
    }
    //////////////////////////////////////////////////////////////////
    if (auto as_nmif = dynamic_cast<NvMeshInterfaceNode*>(ifnode)) {
      assert(is_nvmesh_shader);
      auto it_nmif = c->_nvMeshInterfaces.find(named);
      assert(it_nmif != c->_nvMeshInterfaces.end());
      pshader->setInputInterface(it_nmif->second);
    }
#endif
    //////////////////////////////////////////////////////////////////
#if defined(ENABLE_COMPUTE_SHADERS)
    //////////////////////////////////////////////////////////////////
    if (auto as_cxif = dynamic_cast<ComputeInterfaceNode*>(ifnode)) {
      assert(is_compute_shader);
      auto it_cxif = c->_computeInterfaces.find(named);
      assert(it_cxif != c->_computeInterfaces.end());
      pshader->setInputInterface(it_cxif->second);
    }
#endif
    //////////////////////////////////////////////////////////////////
  }

  //////////////////////////////////////////////
  // visit direct uniforms
  //////////////////////////////////////////////

  for (auto usetnode : _uniformSets) {
    auto it_uset = c->_uniformSets.find(usetnode->_name);
    auto uset    = it_uset->second;
    pshader->addUniformSet(uset);
  }

  //////////////////////////////////////////////
  // visit direct uniform blocks
  //////////////////////////////////////////////

  for (auto ublknode : _uniformBlocks) {
    auto it_ublk = c->_uniformBlocks.find(ublknode->_name);
    auto ublk    = it_ublk->second;
    pshader->addUniformBlock(ublk);
  }

  //////////////////////////////////////////////
  //
  //////////////////////////////////////////////

  auto iface = pshader->_inputInterface;
  assert(iface != nullptr);

  ////////////////////////////////////////////////////////////////////////////
  // declare required extensions
  ////////////////////////////////////////////////////////////////////////////

  for (auto extnode : _requiredExtensions)
    extnode->emit(backend);

  ////////////////////////////////////////////////////////////////////////////
  // this will (recursively) emit
  //  library blocks
  //  interface nodes
  //  uniformSets
  //  uniformBlocks
  ////////////////////////////////////////////////////////////////////////////

  DecoBlockNode::emit(backend);

  ///////////////////////
  // code
  ///////////////////////

  codegen.formatLine("///////////////////////////////////////////////////////////////////");

  codegen.formatLine("void %s(){", _name.c_str());
  codegen.incIndent();
  _body.emit(backend);
  codegen.decIndent();
  codegen.formatLine("}");

  ///////////////////////////////////

  pshader->mName = _name;

  pshader->mShaderText = codegen.flush();

  ///////////////////////////////////
  //printf("shaderbody\n");
 // printf("///////////////////////////////\n");
  //printf("%s", pshader->mShaderText.c_str());
  //printf("///////////////////////////////\n");
}

///////////////////////////////////////////////////////////
void VertexShaderNode::generate(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderVtx();
  backend._statemap["curshader"].Set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->mpContainer = c;
  _generateCommon(backend);
  c->addVertexShader(pshader);
}
///////////////////////////////////////////////////////////
void TessCtrlShaderNode::generate(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderTsC();
  backend._statemap["curshader"].Set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->mpContainer = c;
  _generateCommon(backend);
  c->addTessCtrlShader(pshader);
}
///////////////////////////////////////////////////////////
void TessEvalShaderNode::generate(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderTsE();
  backend._statemap["curshader"].Set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->mpContainer = c;
  _generateCommon(backend);
  c->addTessEvalShader(pshader);
}
///////////////////////////////////////////////////////////
void GeometryShaderNode::generate(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderGeo();
  backend._statemap["curshader"].Set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->mpContainer = backend._container;
  _generateCommon(backend);
  c->addGeometryShader(pshader);
}
///////////////////////////////////////////////////////////
void FragmentShaderNode::generate(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderFrg();
  backend._statemap["curshader"].Set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->mpContainer = backend._container;
  _generateCommon(backend);
  backend._container->addFragmentShader(pshader);
}
#if defined(ENABLE_NVMESH_SHADERS)
void NvTaskShaderNode::generate(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderNvTask();
  backend._statemap["curshader"].Set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->mpContainer = backend._container;
  _generateCommon(backend);
  backend._container->addNvTaskShader(pshader);
}
void NvMeshShaderNode::generate(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderNvMesh();
  backend._statemap["curshader"].Set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->mpContainer = backend._container;
  _generateCommon(backend);
  backend._container->addNvMeshShader(pshader);
}
#endif

#if defined(ENABLE_COMPUTE_SHADERS)
void ComputeShaderNode::generate(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ComputeShader();
  backend._statemap["curshader"].Set<Shader*>(pshader);
  pshader->mpContainer = backend._container;
  _generateCommon(backend);
  backend._container->addComputeShader(pshader);
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
