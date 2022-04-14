////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include <ork/file/file.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/string/string.h>

namespace ork::lev2::glslfx {

///////////////////////////////////////////////////////////////////////////////

void RootContainer::addConfig(Config* pcfg) {
  //printf( "RootContainer<%p> addConfig<%p:%s>\n", this, pcfg, pcfg->mName.c_str() );
  auto it = mConfigs.find(pcfg->mName);
  assert(it==mConfigs.end());
  mConfigs[pcfg->mName] = pcfg;
}
void RootContainer::addUniformBlock(UniformBlock* unis) {
  //printf( "RootContainer<%p> addUniformBlock<%p:%s>\n", this, unis, unis->_name.c_str() );
  auto it = _uniformBlocks.find(unis->_name);
  assert(it==_uniformBlocks.end());
  _uniformBlocks[unis->_name] = unis;
}
void RootContainer::addUniformSet(UniformSet* unis) {
  //printf( "RootContainer<%p> addUniformSet<%p:%s>\n", this, unis, unis->_name.c_str() );
  auto it = _uniformSets.find(unis->_name);
  assert(it==_uniformSets.end());
  _uniformSets[unis->_name] = unis;
}
void RootContainer::addVertexInterface(StreamInterface* pif) {
  //printf( "RootContainer<%p> addVertexInterface<%p:%s>\n", this, pif, pif->mName.c_str() );
  auto it = _vertexInterfaces.find(pif->mName);
  assert(it==_vertexInterfaces.end());
  _vertexInterfaces[pif->mName] = pif;
}
void RootContainer::addTessCtrlInterface(StreamInterface* pif) {
  //printf( "RootContainer<%p> addTessCtrlInterface<%p:%s>\n", this, pif, pif->mName.c_str() );
  auto it = _tessCtrlInterfaces.find(pif->mName);
  assert(it==_tessCtrlInterfaces.end());
  _tessCtrlInterfaces[pif->mName] = pif;
}
void RootContainer::addTessEvalInterface(StreamInterface* pif) {
  //printf( "RootContainer<%p> addTessEvalInterface<%p:%s>\n", this, pif, pif->mName.c_str() );
  auto it = _tessEvalInterfaces.find(pif->mName);
  assert(it==_tessEvalInterfaces.end());
  _tessEvalInterfaces[pif->mName] = pif;
}
void RootContainer::addGeometryInterface(StreamInterface* pif) {
  //printf( "RootContainer<%p> addGeometryInterface<%p:%s>\n", this, pif, pif->mName.c_str() );
  auto it = _geometryInterfaces.find(pif->mName);
  assert(it==_geometryInterfaces.end());
  _geometryInterfaces[pif->mName] = pif;
}
void RootContainer::addFragmentInterface(StreamInterface* pif) {
  //printf( "RootContainer<%p> addFragmentInterface<%p:%s>\n", this, pif, pif->mName.c_str() );
  auto it = _fragmentInterfaces.find(pif->mName);
  assert(it==_fragmentInterfaces.end());
  _fragmentInterfaces[pif->mName] = pif;
}
void RootContainer::addStateBlock(StateBlock* psb) {
  //printf( "RootContainer<%p> addStateBlock<%p:%s>\n", this, psb, psb->mName.c_str() );
  auto it = _stateBlocks.find(psb->mName);
  assert(it==_stateBlocks.end());
  _stateBlocks[psb->mName] = psb;
}
void RootContainer::addTechnique(Technique* ptek) {
  //printf( "RootContainer<%p> addTechnique<%p:%s>\n", this, ptek, ptek->_name.c_str() );
  auto it = _techniqueMap.find(ptek->_name);
  assert(it==_techniqueMap.end());
  _techniqueMap[ptek->_name] = ptek;
}
void RootContainer::addVertexShader(ShaderVtx* psha) {
  //printf( "RootContainer<%p> addVertexShader<%p:%s>\n", this, psha, psha->mName.c_str() );
  auto it = _vertexShaders.find(psha->mName);
  assert(it==_vertexShaders.end());
  _vertexShaders[psha->mName] = psha;
}
void RootContainer::addTessCtrlShader(ShaderTsC* psha) {
  //printf( "RootContainer<%p> addTessCtrlShader<%p:%s>\n", this, psha, psha->mName.c_str() );
  auto it = _tessCtrlShaders.find(psha->mName);
  assert(it==_tessCtrlShaders.end());
  _tessCtrlShaders[psha->mName] = psha;
}
void RootContainer::addTessEvalShader(ShaderTsE* psha) {
  //printf( "RootContainer<%p> addTessEvalShader<%p:%s>\n", this, psha, psha->mName.c_str() );
  auto it = _tessEvalShaders.find(psha->mName);
  assert(it==_tessEvalShaders.end());
  _tessEvalShaders[psha->mName] = psha;
}
void RootContainer::addGeometryShader(ShaderGeo* psha) {
  //printf( "RootContainer<%p> addGeometryShader<%p:%s>\n", this, psha, psha->mName.c_str() );
  auto it = _geometryShaders.find(psha->mName);
  assert(it==_geometryShaders.end());
  _geometryShaders[psha->mName] = psha;
}
void RootContainer::addFragmentShader(ShaderFrg* psha) {
  //printf( "RootContainer<%p> addFragmentShader<%p:%s>\n", this, psha, psha->mName.c_str() );
  auto it = _fragmentShaders.find(psha->mName);
  assert(it==_fragmentShaders.end());
  _fragmentShaders[psha->mName] = psha;
}

///////////////////////////////////////////////////////////////////////////////

StateBlock* RootContainer::GetStateBlock(const std::string& name) const {
  const auto& it = _stateBlocks.find(name);
  return (it == _stateBlocks.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

ShaderVtx* RootContainer::vertexShader(const std::string& name) const {
  const auto& it = _vertexShaders.find(name);
  return (it == _vertexShaders.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

ShaderTsC* RootContainer::tessCtrlShader(const std::string& name) const {
  const auto& it = _tessCtrlShaders.find(name);
  return (it == _tessCtrlShaders.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

ShaderTsE* RootContainer::tessEvalShader(const std::string& name) const {
  const auto& it = _tessEvalShaders.find(name);
  return (it == _tessEvalShaders.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

ShaderGeo* RootContainer::geometryShader(const std::string& name) const {
  const auto& it = _geometryShaders.find(name);
  return (it == _geometryShaders.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

ShaderFrg* RootContainer::fragmentShader(const std::string& name) const {
  const auto& it = _fragmentShaders.find(name);
  return (it == _fragmentShaders.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

UniformBlock* RootContainer::uniformBlock(const std::string& name) const {
  const auto& it = _uniformBlocks.find(name);
  return (it == _uniformBlocks.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

UniformSet* RootContainer::uniformset(const std::string& name) const {
  const auto& it = _uniformSets.find(name);
  return (it == _uniformSets.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

StreamInterface* RootContainer::vertexInterface(const std::string& name) const {
  const auto& it = _vertexInterfaces.find(name);
  return (it == _vertexInterfaces.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

StreamInterface* RootContainer::tessCtrlInterface(const std::string& name) const {
  const auto& it = _tessCtrlInterfaces.find(name);
  return (it == _tessCtrlInterfaces.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

StreamInterface* RootContainer::tessEvalInterface(const std::string& name) const {
  const auto& it = _tessEvalInterfaces.find(name);
  return (it == _tessEvalInterfaces.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

StreamInterface* RootContainer::geometryInterface(const std::string& name) const {
  const auto& it = _geometryInterfaces.find(name);
  return (it == _geometryInterfaces.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

StreamInterface* RootContainer::fragmentInterface(const std::string& name) const {
  const auto& it = _fragmentInterfaces.find(name);
  return (it == _fragmentInterfaces.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

Uniform* RootContainer::GetUniform(const std::string& name) const {
  const auto& it = _uniforms.find(name);
  return (it == _uniforms.end()) ? nullptr : it->second;
}

#if defined(ENABLE_NVMESH_SHADERS)
void RootContainer::addNvTaskInterface(StreamInterface* pif) {
  _nvTaskInterfaces[pif->mName] = pif;
}
void RootContainer::addNvMeshInterface(StreamInterface* pif) {
  _nvMeshInterfaces[pif->mName] = pif;
}
void RootContainer::addNvTaskShader(ShaderNvTask* psha) {
  _nvTaskShaders[psha->mName] = psha;
}
void RootContainer::addNvMeshShader(ShaderNvMesh* psha) {
  _nvMeshShaders[psha->mName] = psha;
}
ShaderNvTask* RootContainer::nvTaskShader(const std::string& name) const {
  const auto& it = _nvTaskShaders.find(name);
  return (it == _nvTaskShaders.end()) ? nullptr : it->second;
}
ShaderNvMesh* RootContainer::nvMeshShader(const std::string& name) const {
  const auto& it = _nvMeshShaders.find(name);
  return (it == _nvMeshShaders.end()) ? nullptr : it->second;
}
StreamInterface* RootContainer::nvTaskInterface(const std::string& name) const {
  const auto& it = _nvTaskInterfaces.find(name);
  return (it == _nvTaskInterfaces.end()) ? nullptr : it->second;
}
StreamInterface* RootContainer::nvMeshInterface(const std::string& name) const {
  const auto& it = _nvMeshInterfaces.find(name);
  return (it == _nvMeshInterfaces.end()) ? nullptr : it->second;
}
#endif

#if defined(ENABLE_COMPUTE_SHADERS)
void RootContainer::addComputeInterface(StreamInterface* pif) {
  _computeInterfaces[pif->mName] = pif;
}
StreamInterface* RootContainer::computeInterface(const std::string& name) const {
  const auto& it = _computeInterfaces.find(name);
  return (it == _computeInterfaces.end()) ? nullptr : it->second;
}
void RootContainer::addComputeShader(ComputeShader* psha) {
  _computeShaders[psha->mName] = psha;
}
ComputeShader* RootContainer::computeShader(const std::string& name) const {
  const auto& it = _computeShaders.find(name);
  return (it == _computeShaders.end()) ? nullptr : it->second;
}
#endif

///////////////////////////////////////////////////////////////////////////////

Uniform* RootContainer::MergeUniform(const std::string& name) {
  Uniform* pret  = nullptr;
  const auto& it = _uniforms.find(name);
  if (it == _uniforms.end()) {
    pret            = new Uniform(name);
    _uniforms[name] = pret;
  } else {
    pret = it->second;
  }
  // printf( "MergedUniform<%s><%p>\n", name.c_str(), pret );
  return pret;
}

///////////////////////////////////////////////////////////////////////////////

RootContainer::RootContainer(const std::string& nam)
    : mEffectName(nam)
    , mActiveTechnique(nullptr)
    , _activePass(nullptr)
    , mActiveNumPasses(0)
    , mShaderCompileFailed(false) {

  //printf("new RootContainer<%p:%s>\n", this, nam.c_str() );

  StateBlock* pdefsb = new StateBlock;
  pdefsb->mName      = "default";
  this->addStateBlock(pdefsb);
}

///////////////////////////////////////////////////////////////////////////////

void RootContainer::Destroy(void) {
}

///////////////////////////////////////////////////////////////////////////////

bool RootContainer::IsValid(void) {
  return true;
}

std::unordered_map<std::string, Uniform*> RootContainer::flatUniMap() const {
  std::unordered_map<std::string, Uniform*> flatunimap;
  for (auto u : this->_uniforms) {
    flatunimap[u.first] = u.second;
  }
  for (auto b : this->_uniformBlocks) {
    UniformBlock* block = b.second;
    for (auto s : block->_subuniforms) {
      auto it = flatunimap.find(s->_name);
      assert(it == flatunimap.end());
      flatunimap[s->_name] = s;
    }
  }
  return flatunimap;
}

} // namespace ork::lev2::glslfx
