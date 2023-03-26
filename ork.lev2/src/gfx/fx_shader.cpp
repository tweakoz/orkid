////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadman.h>

namespace ork { namespace lev2 {

extern bool gearlyhack;

FxShader::FxShader() {
}
///////////////////////////////////////////////////////////////////////////////

FxParamRec::FxParamRec()
    : _name("")
    , mParameterSemantic("")
    , meParameterType(ork::EPROPTYPE_END)
    , mParameterHandle(0)
    , meBindingScope(ESCOPE_PEROBJECT)
    , mTargetHash(0xffffffff) {
}

///////////////////////////////////////////////////////////////////////////////

FxShaderPass::FxShaderPass(void* ih)
    : mInternalHandle(ih)
    , mbRestorePass(false) {
}

FxShaderTechnique::FxShaderTechnique(void* ih)
    : mInternalHandle(ih)
    , mbValidated(false) {
}

FxShaderParam::FxShaderParam(void* ih)
    : meParamType(EPROPTYPE_END)
    , mInternalHandle(ih)
    , mBindable(true)
    , mChildParam(0) {
}

///////////////////////////////////////////////////////////////////////////////

FxShaderParam* FxShaderParamBlock::param(const std::string& name) const {
  auto it = _subparams.find(name);
  return (it != _subparams.end()) ? it->second : nullptr;
}

FxShaderParamBufferMapping::FxShaderParamBufferMapping() {
}
FxShaderParamBufferMapping::~FxShaderParamBufferMapping() {
  assert(_mappedaddr == nullptr);
}
void FxShaderParamBufferMapping::unmap() {
  _fxi->unmapParamBuffer(this);
}

///////////////////////////////////////////////////////////////////////////////

void FxShader::addTechnique(const FxShaderTechnique* tek) {
  _techniques[tek->mTechniqueName] = tek;
}

void FxShader::addParameter(const FxShaderParam* param) {
  _parameterByName[param->_name] = param;
}
void FxShader::addParameterBlock(const FxShaderParamBlock* block) {
  _parameterBlockByName[block->_name] = block;
}
#if defined(ENABLE_COMPUTE_SHADERS)
void FxShader::addComputeShader(const FxComputeShader* csh) {
  _computeShaderByName[csh->_name] = csh;
}
FxComputeShader* FxShader::findComputeShader(const std::string& named) {
  auto it = _computeShaderByName.find(named);
  return const_cast<FxComputeShader*>((it != _computeShaderByName.end()) ? it->second : nullptr);
}
#endif
#if defined(ENABLE_SHADER_STORAGE)
void FxShader::addStorageBlock(const FxShaderStorageBlock* block) {
  _storageBlockByName[block->_name] = block;
}
FxShaderStorageBlock* FxShader::storageBlockByName(const std::string& named) {
  auto it = _storageBlockByName.find(named);
  return const_cast<FxShaderStorageBlock*>((it != _storageBlockByName.end()) ? it->second : nullptr);
}
FxShaderStorageBufferMapping::FxShaderStorageBufferMapping() {
}
FxShaderStorageBufferMapping::~FxShaderStorageBufferMapping() {
  assert(_mappedaddr == nullptr);
}
void FxShaderStorageBufferMapping::unmap() {
  _ci->unmapStorageBuffer(this);
}

#endif

///////////////////////////////////////////////////////////////////////////////

void FxShader::RegisterLoaders(const file::Path& base, const std::string& ext) {
  static auto gShaderFileContext1 = std::make_shared<FileDevContext>();
  auto lev2ctx                    = FileEnv::contextForUriProto("lev2://");
  auto shaderpath                 = lev2ctx->getFilesystemBaseAbs() / base;
  auto shaderfilectx              = FileEnv::createContextForUriBase("orkshader://", shaderpath);
  shaderfilectx->SetFilesystemBaseEnable(true);
  // OrkAssert(false);
  if (0)
    printf(
        "FxShader::RegisterLoaders ext<%s> l2<%s> base<%s> shaderpath<%s>\n", //
        ext.c_str(),
        lev2ctx->getFilesystemBaseAbs().c_str(),
        base.c_str(),
        shaderpath.c_str());
  gearlyhack = false;
}

///////////////////////////////////////////////////////////////////////////////

void FxShader::OnReset() {
  auto target = lev2::contextForCurrentThread();

  for (const auto& it : _parameterByName ) {
    const FxShaderParam* param = it.second;
    const std::string& type    = param->mParameterType;
    if (param->mParameterType == "sampler" || param->mParameterType == "texture") {
      target->FXI()->BindParamCTex(param, 0);
    }
  }
  //_techniques.clear();
  //_parameterByName.clear();
  // mParameterBySemantic.clear();
}

///////////////////////////////////////////////////////////////////////////////

FxShaderParam* FxShader::FindParamByName(const std::string& named) {
  orkmap<std::string, const FxShaderParam*>::iterator it = _parameterByName.find(named);
  return const_cast<FxShaderParam*>((it != _parameterByName.end()) ? it->second : 0);
}

///////////////////////////////////////////////////////////////////////////////

FxShaderTechnique* FxShader::FindTechniqueByName(const std::string& named) {
  orkmap<std::string, const FxShaderTechnique*>::iterator it = _techniques.find(named);
  return const_cast<FxShaderTechnique*>((it != _techniques.end()) ? it->second : 0);
}

void FxShader::SetName(const char* name) {
  mName = name;
}

const char* FxShader::GetName() {
  return mName.c_str();
}

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_COMPUTE_SHADER)
FxComputeShader* FxShader::findComputeShader(const std::string& named) {
  FxComputeShader* rval = nullptr;
  assert(false);
  return rval;
}
#endif

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
