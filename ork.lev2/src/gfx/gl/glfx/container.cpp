////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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

void Container::addConfig(Config* pcfg) { mConfigs[pcfg->mName] = pcfg; }
void Container::addUniformBlock(UniformBlock* pif) { _uniformBlocks[pif->_name] = pif; }
void Container::addUniformSet(UniformSet* pif) { _uniformSets[pif->_name] = pif; }
void Container::addVertexInterface(StreamInterface* pif) { _vertexInterfaces[pif->mName] = pif; }
void Container::addTessCtrlInterface(StreamInterface* pif) { _tessCtrlInterfaces[pif->mName] = pif; }
void Container::addTessEvalInterface(StreamInterface* pif) { _tessEvalInterfaces[pif->mName] = pif; }
void Container::addGeometryInterface(StreamInterface* pif) { _geometryInterfaces[pif->mName] = pif; }
void Container::addFragmentInterface(StreamInterface* pif) { _fragmentInterfaces[pif->mName] = pif; }
void Container::addStateBlock(StateBlock* psb) { _stateBlocks[psb->mName] = psb; }
// void Container::addLibBlock(LibBlock* plb) { _libBlocks[plb->mName] = plb; }
void Container::addTechnique(Technique* ptek) { _techniqueMap[ptek->mName] = ptek; }
void Container::addVertexShader(ShaderVtx* psha) { _vertexShaders[psha->mName] = psha; }
void Container::addTessCtrlShader(ShaderTsC* psha) { _tessCtrlShaders[psha->mName] = psha; }
void Container::addTessEvalShader(ShaderTsE* psha) { _tessEvalShaders[psha->mName] = psha; }
void Container::addGeometryShader(ShaderGeo* psha) { _geometryShaders[psha->mName] = psha; }
void Container::addFragmentShader(ShaderFrg* psha) { _fragmentShaders[psha->mName] = psha; }

///////////////////////////////////////////////////////////////////////////////

StateBlock* Container::GetStateBlock(const std::string& name) const {
  const auto& it = _stateBlocks.find(name);
  return (it == _stateBlocks.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

ShaderVtx* Container::vertexShader(const std::string& name) const {
  const auto& it = _vertexShaders.find(name);
  return (it == _vertexShaders.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

ShaderTsC* Container::tessCtrlShader(const std::string& name) const {
  const auto& it = _tessCtrlShaders.find(name);
  return (it == _tessCtrlShaders.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

ShaderTsE* Container::tessEvalShader(const std::string& name) const {
  const auto& it = _tessEvalShaders.find(name);
  return (it == _tessEvalShaders.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

ShaderGeo* Container::geometryShader(const std::string& name) const {
  const auto& it = _geometryShaders.find(name);
  return (it == _geometryShaders.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

ShaderFrg* Container::fragmentShader(const std::string& name) const {
  const auto& it = _fragmentShaders.find(name);
  return (it == _fragmentShaders.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

UniformBlock* Container::uniformBlock(const std::string& name) const {
  const auto& it = _uniformBlocks.find(name);
  return (it == _uniformBlocks.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

UniformSet* Container::uniformSet(const std::string& name) const {
  const auto& it = _uniformSets.find(name);
  return (it == _uniformSets.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

StreamInterface* Container::vertexInterface(const std::string& name) const {
  const auto& it = _vertexInterfaces.find(name);
  return (it == _vertexInterfaces.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

StreamInterface* Container::tessCtrlInterface(const std::string& name) const {
  const auto& it = _tessCtrlInterfaces.find(name);
  return (it == _tessCtrlInterfaces.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

StreamInterface* Container::tessEvalInterface(const std::string& name) const {
  const auto& it = _tessEvalInterfaces.find(name);
  return (it == _tessEvalInterfaces.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

StreamInterface* Container::geometryInterface(const std::string& name) const {
  const auto& it = _geometryInterfaces.find(name);
  return (it == _geometryInterfaces.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

StreamInterface* Container::fragmentInterface(const std::string& name) const {
  const auto& it = _fragmentInterfaces.find(name);
  return (it == _fragmentInterfaces.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

Uniform* Container::GetUniform(const std::string& name) const {
  const auto& it = _uniforms.find(name);
  return (it == _uniforms.end()) ? nullptr : it->second;
}

#if defined(ENABLE_NVMESH_SHADERS)
void Container::addNvTaskInterface(StreamInterface* pif) { _nvTaskInterfaces[pif->mName] = pif; }
void Container::addNvMeshInterface(StreamInterface* pif) { _nvMeshInterfaces[pif->mName] = pif; }
void Container::addNvTaskShader(ShaderNvTask* psha) { _nvTaskShaders[psha->mName] = psha; }
void Container::addNvMeshShader(ShaderNvMesh* psha) { _nvMeshShaders[psha->mName] = psha; }
ShaderNvTask* Container::nvTaskShader(const std::string& name) const {
  const auto& it = _nvTaskShaders.find(name);
  return (it == _nvTaskShaders.end()) ? nullptr : it->second;
}
ShaderNvMesh* Container::nvMeshShader(const std::string& name) const {
  const auto& it = _nvMeshShaders.find(name);
  return (it == _nvMeshShaders.end()) ? nullptr : it->second;
}
StreamInterface* Container::nvTaskInterface(const std::string& name) const {
  const auto& it = _nvTaskInterfaces.find(name);
  return (it == _nvTaskInterfaces.end()) ? nullptr : it->second;
}
StreamInterface* Container::nvMeshInterface(const std::string& name) const {
  const auto& it = _nvMeshInterfaces.find(name);
  return (it == _nvMeshInterfaces.end()) ? nullptr : it->second;
}
#endif

#if defined(ENABLE_COMPUTE_SHADERS)
void Container::addComputeInterface(StreamInterface* pif) { _computeInterfaces[pif->mName] = pif; }
StreamInterface* Container::computeInterface(const std::string& name) const {
  const auto& it = _computeInterfaces.find(name);
  return (it == _computeInterfaces.end()) ? nullptr : it->second;
}
void Container::addComputeShader(ComputeShader* psha) { _computeShaders[psha->mName] = psha; }
ComputeShader* Container::computeShader(const std::string& name) const {
  const auto& it = _computeShaders.find(name);
  return (it == _computeShaders.end()) ? nullptr : it->second;
}
#endif

///////////////////////////////////////////////////////////////////////////////

Uniform* Container::MergeUniform(const std::string& name) {
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

Container::Container(const std::string& nam)
    : mEffectName(nam)
    , mActiveTechnique(nullptr)
    , _activePass(nullptr)
    , mActiveNumPasses(0)
    , mShaderCompileFailed(false) {
  StateBlock* pdefsb = new StateBlock;
  pdefsb->mName      = "default";
  this->addStateBlock(pdefsb);
}

///////////////////////////////////////////////////////////////////////////////

void Container::Destroy(void) {}

///////////////////////////////////////////////////////////////////////////////

bool Container::IsValid(void) { return true; }

std::unordered_map<std::string, Uniform*> Container::flatUniMap() const {
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
