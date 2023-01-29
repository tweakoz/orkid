////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
#include <ork/lev2/glfw/ctx_glfw.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
///////////////////////////////////////////////////////////
void ShaderNode::parse(GlSlFxParser* parser, const ScannerView& view) {
  DecoBlockNode::parse(parser,view);
  _body.parse(parser,view);
}
void ShaderNode::pregen(shaderbuilder::BackEnd& backend) const {
  auto& codegen = backend._codegen;
  codegen.formatLine("/// shaderdep<%p:%s> pregen",this, _name.c_str() );
  DecoBlockNode::_pregen(backend);
}
///////////////////////////////////////////////////////////
void ShaderNode::_generate2Common(shaderbuilder::BackEnd& backend) const {
  auto pshader  = backend._statemap["curshader"].get<Shader*>();
  auto& codegen = backend._codegen;
  codegen.flush();

////////////////////////////////////////////////////

  auto global_ctxbase = CtxGLFW::globalOffscreenContext();

  int minor_api_version = global_ctxbase->_vars->typedValueForKey<int>("GL_API_MINOR_VERSION").value();

  switch(minor_api_version){
    case 6:
      codegen.formatLine("#version 460 core");
      break;
    case 5:
      codegen.formatLine("#version 450 core");
      break;
    case 3:
      codegen.formatLine("#version 430 core");
      break;
    case 1:
    case 0:
      codegen.formatLine("#version 400 core");
      OrkAssert(false);
      break;
    default:
      OrkAssert(false);
      break;
  }

  ////////////////////////////////////////////////////

  pshader->mName = _name;
  // LibBlock* plibblock = nullptr;
  auto c = pshader->_rootcontainer;

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

  auto it_deco_children = backend._decochildrenmap.find(this);
  OrkAssert(it_deco_children!=backend._decochildrenmap.end());
  auto deco_children = it_deco_children->second;

  for (auto as_lib : deco_children->_libraryBlocks) {
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

  for (auto ifnode : deco_children->_interfaceNodes) {
    const auto& named = ifnode->_name;
    //////////////////////////////////////////////////////////////////
    if (auto as_vxif = std::dynamic_pointer_cast<VertexInterfaceNode>(ifnode)) {
      assert(is_vertex_shader);
      auto it_vi = c->_vertexInterfaces.find(named);
      assert(it_vi != c->_vertexInterfaces.end());
      pshader->setInputInterface(it_vi->second);
    }
    //////////////////////////////////////////////////////////////////
    else if (auto as_tcif = std::dynamic_pointer_cast<TessCtrlInterfaceNode>(ifnode)) {
      assert(is_tessctrl_shader);
      auto it_tcif = c->_tessCtrlInterfaces.find(named);
      assert(it_tcif != c->_tessCtrlInterfaces.end());
      pshader->setInputInterface(it_tcif->second);
    }
    //////////////////////////////////////////////////////////////////
    else if (auto as_teif = std::dynamic_pointer_cast<TessEvalInterfaceNode>(ifnode)) {
      assert(is_tesseval_shader);
      auto it_teif = c->_tessEvalInterfaces.find(named);
      assert(it_teif != c->_tessEvalInterfaces.end());
      pshader->setInputInterface(it_teif->second);
    }
    //////////////////////////////////////////////////////////////////
    else if (auto as_geif = std::dynamic_pointer_cast<GeometryInterfaceNode>(ifnode)) {
      assert(is_geometry_shader);
      auto it_geif = c->_geometryInterfaces.find(named);
      assert(it_geif != c->_geometryInterfaces.end());
      pshader->setInputInterface(it_geif->second);
    }
    //////////////////////////////////////////////////////////////////
    else if (auto as_frif = std::dynamic_pointer_cast<FragmentInterfaceNode>(ifnode)) {
      assert(is_fragment_shader);
      auto it_frif = c->_fragmentInterfaces.find(named);
      assert(it_frif != c->_fragmentInterfaces.end());
      pshader->setInputInterface(it_frif->second);
    }
    //////////////////////////////////////////////////////////////////
#if defined(ENABLE_NVMESH_SHADERS)
    //////////////////////////////////////////////////////////////////
    else if (auto as_ntif = std::dynamic_pointer_cast<NvTaskInterfaceNode>(ifnode)) {
      assert(is_nvtask_shader);
      auto it_ntif = c->_nvTaskInterfaces.find(named);
      assert(it_ntif != c->_nvTaskInterfaces.end());
      pshader->setInputInterface(it_ntif->second);
    }
    //////////////////////////////////////////////////////////////////
    else if (auto as_nmif = std::dynamic_pointer_cast<NvMeshInterfaceNode>(ifnode)) {
      assert(is_nvmesh_shader);
      auto it_nmif = c->_nvMeshInterfaces.find(named);
      assert(it_nmif != c->_nvMeshInterfaces.end());
      pshader->setInputInterface(it_nmif->second);
    }
#endif
    //////////////////////////////////////////////////////////////////
#if defined(ENABLE_COMPUTE_SHADERS)
    //////////////////////////////////////////////////////////////////
    else if (auto as_cxif = std::dynamic_pointer_cast<ComputeInterfaceNode>(ifnode)) {
      assert(is_compute_shader);
      auto it_cxif = c->_computeInterfaces.find(named);
      assert(it_cxif != c->_computeInterfaces.end());
      pshader->setInputInterface(it_cxif->second);
    }
#endif
    //////////////////////////////////////////////////////////////////
    else{
      OrkAssert(false);
    }
  }

  //////////////////////////////////////////////
  // visit direct uniforms
  //////////////////////////////////////////////

  for (auto usetnode : deco_children->_uniformSets) {
    auto it_uset = c->_uniformSets.find(usetnode->_name);
    auto uset    = it_uset->second;
    pshader->addUniformSet(uset);
  }

  //////////////////////////////////////////////
  // visit direct uniform blocks
  //////////////////////////////////////////////

  for (auto ublknode : deco_children->_uniformBlocks) {
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

  this->emitShaderDeps(backend);

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
   if( 0 ){
      printf("shaderbody\n");
      printf("///////////////////////////////\n");
      printf("%s", pshader->mShaderText.c_str());
      printf("///////////////////////////////\n");
  }
}
///////////////////////////////////////////////////////////
template <typename T>
struct unique_collection{
  using ptr_t = typename T::element_type*;
  using vec_t = std::vector<ptr_t>;
  using que_t = std::queue<decoblocknode_rawconstptr_t>;
  using set_t = std::set<decoblocknode_rawconstptr_t>;

  //using pop_t = std::function<void(decoblocknode_rawconstptr_t node,que_t&)>;
  
  unique_collection(shaderbuilder::BackEnd& backend) :_backend(backend) {

  }

  void visit(decoblocknode_rawconstptr_t node){
  
    auto it_deco_children = _backend._decochildrenmap.find(node);
    OrkAssert(it_deco_children!=_backend._decochildrenmap.end());
    auto deco_children = it_deco_children->second;

    for (auto ch : deco_children->_uniformBlocks) visit(ch.get());
    for (auto ch : deco_children->_uniformSets) visit(ch.get());
    for (auto ch : deco_children->_interfaceNodes) visit(ch.get());
    for (auto ch : deco_children->_libraryBlocks) visit(ch.get());

    _recursion_queue.push(node);
 
    _collect_type_t();

  }

  void _collect_type_t(){
  
    auto& codegen = _backend._codegen;

    while(not _recursion_queue.empty()){
      auto raw_test_node = _recursion_queue.front();
      _recursion_queue.pop();

      auto as_t_node = dynamic_cast<ptr_t>(raw_test_node);
      if( as_t_node ){
        auto it = _unique_names.find(raw_test_node);
        if(it==_unique_names.end()){
          size_t index = _collection.size();
          codegen.formatLine("/// shaderdep col<%p> adding child<%zu,%p,%s>", this, index, as_t_node, as_t_node->_name.c_str() );
          _collection.push_back(as_t_node);
         _unique_names.insert(raw_test_node);
        }else{
          codegen.formatLine("/// WTF - visted twice<%s>", raw_test_node->_name.c_str() );
          //OrkAssert(false);
        }
      }
    }

  }

  set_t _unique_names;
  vec_t _collection;
  que_t _recursion_queue;
  shaderbuilder::BackEnd& _backend;
};
///////////////////////////////////////////////////////////
void ShaderNode::emitShaderDeps(shaderbuilder::BackEnd& backend) const{

  //printf("shaderdep<%p:%s> emit\n", this, _name.c_str() );
  auto& codegen = backend._codegen;
  codegen.formatLine("/// shaderdep<%p:%s> emit",this, _name.c_str() );

  /////////////////////////////////////////////////
  // recursively collect all deps
  /////////////////////////////////////////////////

  unique_collection<libblock_constptr_t> libblocks(backend);
  unique_collection<uniformblocknode_constptr_t> uniblocks(backend);
  unique_collection<uniformsetnode_constptr_t> unisets(backend);
  unique_collection<interfacenode_constptr_t> interfaces(backend);
  
  libblocks.visit(this);

  for (auto library : libblocks._collection){
    uniblocks.visit(library);
    unisets.visit(library);
    interfaces.visit(library);
  }

  uniblocks.visit(this);
  unisets.visit(this);
  interfaces.visit(this);

  /////////////////////////////////////////////////
  // emit deps
  /////////////////////////////////////////////////

  codegen.formatLine("/// begin shaderdeps<%p>",this);

  codegen.formatLine("/// shaderdeps<%p> uniblocks",this);
  for (auto node : uniblocks._collection)
    node->emit(backend);
  codegen.formatLine("/// shaderdeps<%p> unisets",this);
  for (auto node : unisets._collection)
    node->emit(backend);
  codegen.formatLine("/// shaderdeps<%p> interfaces",this);
  for (auto node : interfaces._collection)
    node->emitInterface(backend);
  codegen.formatLine("/// shaderdeps<%p> libblocks",this);
  for (auto node : libblocks._collection)
    node->emitLibrary(backend);

  codegen.formatLine("/// end shaderdeps<%p>",this);
}
///////////////////////////////////////////////////////////
void VertexShaderNode::_generate2(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderVtx();
  backend._statemap["curshader"].set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->_rootcontainer = c;
  _generate2Common(backend);
  c->addVertexShader(pshader);
}
///////////////////////////////////////////////////////////
void TessCtrlShaderNode::_generate2(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderTsC();
  backend._statemap["curshader"].set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->_rootcontainer = c;
  _generate2Common(backend);
  c->addTessCtrlShader(pshader);
}
///////////////////////////////////////////////////////////
void TessEvalShaderNode::_generate2(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderTsE();
  backend._statemap["curshader"].set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->_rootcontainer = c;
  _generate2Common(backend);
  c->addTessEvalShader(pshader);
}
///////////////////////////////////////////////////////////
void GeometryShaderNode::_generate2(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderGeo();
  backend._statemap["curshader"].set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->_rootcontainer = backend._container;
  _generate2Common(backend);
  c->addGeometryShader(pshader);
}
///////////////////////////////////////////////////////////
void FragmentShaderNode::_generate2(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderFrg();
  backend._statemap["curshader"].set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->_rootcontainer = backend._container;
  _generate2Common(backend);
  backend._container->addFragmentShader(pshader);
}
#if defined(ENABLE_NVMESH_SHADERS)
void NvTaskShaderNode::_generate2(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderNvTask();
  backend._statemap["curshader"].set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->_rootcontainer = backend._container;
  _generate2Common(backend);
  backend._container->addNvTaskShader(pshader);
}
void NvMeshShaderNode::_generate2(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ShaderNvMesh();
  backend._statemap["curshader"].set<Shader*>(pshader);
  auto c               = backend._container;
  pshader->_rootcontainer = backend._container;
  _generate2Common(backend);
  backend._container->addNvMeshShader(pshader);
}
#endif

#if defined(ENABLE_COMPUTE_SHADERS)
void ComputeShaderNode::_generate2(shaderbuilder::BackEnd& backend) const {
  auto pshader = new ComputeShader();
  backend._statemap["curshader"].set<Shader*>(pshader);
  pshader->_rootcontainer = backend._container;
  _generate2Common(backend);
  backend._container->addComputeShader(pshader);
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
