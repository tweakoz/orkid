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

///////////////////////////////////////////////////////////////////////////////
// FX Interface
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {


void ContextGL::FxInit() {
}
} // namespace ork::lev2

///////////////////////////////////////////////////////////////////////////////
// implementation
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2::glslfx {

rootcontainer_ptr_t LoadFxFromText(const std::string& name, const std::string& shadertext);

///////////////////////////////////////////////////////////////////////////////

Interface::Interface(ContextGL& glctx)
    : mTarget(glctx) {
}

///////////////////////////////////////////////////////////////////////////////

void Interface::_doBeginFrame() {
  mLastPass = 0;
}

///////////////////////////////////////////////////////////////////////////////

bool Interface::LoadFxShader(const AssetPath& pth, FxShader* pfxshader) {
  // printf( "GLSLFXI LoadShader<%s>\n", pth.c_str() );
  GL_ERRORCHECK();
  bool bok = false;

  auto container = LoadFxFromFile(pth);
  OrkAssert(container != nullptr);
  pfxshader->_internalHandle.set<rootcontainer_ptr_t>(container);
  bok = container->IsValid();

  container->mFxShader = pfxshader;

  if (bok) {
    BindContainerToAbstract(container, pfxshader);
  }
  GL_ERRORCHECK();

  return bok;
}

///////////////////////////////////////////////////////////////////////////////

FxShader* Interface::shaderFromShaderText(const std::string& name, const std::string& shadertext) {
  FxShader* pfxshader = new FxShader;
  auto  container = LoadFxFromText(name, shadertext);
  OrkAssert(container != nullptr);
  pfxshader->_internalHandle.set<rootcontainer_ptr_t>(container);
  bool bok = container->IsValid();

  container->mFxShader = pfxshader;

  if (bok) {
    BindContainerToAbstract(container, pfxshader);
  }
  GL_ERRORCHECK();

  return pfxshader;
}

///////////////////////////////////////////////////////////////////////////////

void Interface::BindContainerToAbstract(rootcontainer_ptr_t container, FxShader* fxh) {
  for (const auto& ittek : container->_techniqueMap) {
    Technique* ptek         = ittek.second;
    auto ork_tek            = new FxShaderTechnique((void*)ptek);
    ork_tek->_shader        = fxh;
    ork_tek->mTechniqueName = ittek.first;
    // pabstek->mPasses = ittek->first;
    ork_tek->mbValidated = fxh != nullptr;
    fxh->addTechnique(ork_tek);
  }
  for (const auto& itp : container->_uniforms) {
    Uniform* puni                = itp.second;
    FxShaderParam* ork_parm      = new FxShaderParam;
    ork_parm->_name              = itp.first;
    ork_parm->mParameterSemantic = puni->_semantic;
    ork_parm->mParameterType     = puni->_typeName;
    ork_parm->mInternalHandle    = (void*)puni;
    fxh->addParameter(ork_parm);
  }
#if defined(ENABLE_COMPUTE_SHADERS)
  for (const auto& itp : container->_computeShaders) {
    ComputeShader* csh = itp.second;
    auto fxcsh         = new FxComputeShader;
    fxcsh->_name       = itp.first;
    fxcsh->_impl.set<ComputeShader*>(csh);
    fxh->addComputeShader(fxcsh);
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderTechnique* Interface::technique(FxShader* hfx, const std::string& name) {
  // orkprintf( "Get cgtek<%s> hfx<%x>\n", name.c_str(), hfx );
  OrkAssert(hfx != 0);
  auto container = hfx->_internalHandle.get<rootcontainer_ptr_t>();
  OrkAssert(container != 0);
  /////////////////////////////////////////////////////////////

  const auto& tekmap            = hfx->techniques();
  const auto& it                = tekmap.find(name);
  const FxShaderTechnique* htek = (it != tekmap.end()) ? it->second : 0;

  return htek;
}

///////////////////////////////////////////////////////////////////////////////

int Interface::BeginBlock(const FxShaderTechnique* tek, const RenderContextInstData& data) {

  if (nullptr == tek)
    return 0;
  auto tek_cont = static_cast<const Technique*>(tek->GetPlatformHandle());
  OrkAssert(tek_cont != nullptr);
  _activeTechnique = tek;
  _activeShader    = tek->_shader;
  auto container = _activeShader->_internalHandle.get<rootcontainer_ptr_t>();
  OrkAssert(container);
  _active_effect              = container;
  container->mActiveTechnique = tek_cont;
  container->_activePass      = 0;

  mTarget.SetRenderContextInstData(&data);

  return tek_cont->mPasses.size();
}

///////////////////////////////////////////////////////////////////////////////

void Interface::EndBlock() {
  _activeShader    = nullptr;
  _activeTechnique = nullptr;
  glUseProgram(0);
}

///////////////////////////////////////////////////////////////////////////////

bool Interface::BindPass(int ipass) {
  if (_active_effect->mShaderCompileFailed)
    return false;

  assert(_active_effect->mActiveTechnique != nullptr);

  _active_effect->_activePass = _active_effect->mActiveTechnique->mPasses[ipass];
  GL_ERRORCHECK();
  if (0 == _active_effect->_activePass->_programObjectId) {
    bool complinkok = compileAndLink(_active_effect);
    auto fx         = const_cast<FxShader*>(_active_effect->mFxShader);
    fx->SetFailedCompile(false == complinkok);
  }
  auto pass = _active_effect->_activePass;
  auto tek  = pass->_technique;
  // printf("binding pass<%p:%s> tek<%s>\n", pass, pass->_name.c_str(), tek->_name.c_str());

  GL_ERRORCHECK();
  glUseProgram(_active_effect->_activePass->_programObjectId);
  GL_ERRORCHECK();

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void Interface::EndPass() {
}

///////////////////////////////////////////////////////////////////////////////

void Interface::CommitParams(void) {
  if (_active_effect && _active_effect->_activePass && _active_effect->_activePass->_stateBlock) {
    const auto& items = _active_effect->_activePass->_stateBlock->mApplicators;

    for (const auto& item : items) {
      item(&mTarget);
    }
    // const SRasterState& rstate =
    // _active_effect->_activePass->_stateBlock->mState;
    // mTarget.RSI()->BindRasterState(rstate);
  }
  // if( (_active_effect->_activePass != mLastPass) ||
  // (mTarget.currentMaterial()!=mpLastFxMaterial) )
  {
    // orkprintf( "CgFxInterface::CommitParams() activepass<%p>\n",
    // _active_effect->_activePass ); cgSetPassState( _active_effect->_activePass
    // ); mpLastFxMaterial = mTarget.currentMaterial(); mLastPass =
    // _active_effect->_activePass;
  }
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderParam* Interface::parameter(FxShader* hfx, const std::string& name) {
  OrkAssert(0 != hfx);
  const auto& parammap        = hfx->namedParams();
  const auto& it              = parammap.find(name);
  const FxShaderParam* hparam = (it != parammap.end()) ? it->second : 0;
  return hparam;
}

///////////////////////////////////////////////////////////////////////////////
// UBO mgmt
///////////////////////////////////////////////////////////////////////////////

FxShaderParamBuffer* Interface::createParamBuffer(size_t length) {
  assert(length <= 65536);
  auto ub    = new UniformBuffer;
  ub->_fxspb = new FxShaderParamBuffer;
  ub->_fxspb->_impl.set<UniformBuffer*>(ub);
  ub->_length         = length;
  ub->_fxspb->_length = length;
  GL_ERRORCHECK();
  glGenBuffers(1, &ub->_glbufid);
  //printf("Create UBO<%p> glid<%d>\n", ub, ub->_glbufid);
  glBindBuffer(GL_UNIFORM_BUFFER, ub->_glbufid);
  auto mem = new char[length];
  for (int i = 0; i < length; i++)
    mem[i] = 0;
  glBufferData(GL_UNIFORM_BUFFER, length, mem, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  delete[] mem;
  GL_ERRORCHECK();
  return ub->_fxspb;
}

///////////////////////////////////////////////////////////////////////////////

struct UniformBufferMapping {};

///////////////////////////////////////////////////////////////////////////////

parambuffermappingptr_t Interface::mapParamBuffer(FxShaderParamBuffer* b, size_t base, size_t length) {
  auto mapping = std::make_shared<FxShaderParamBufferMapping>();
  auto ub      = b->_impl.get<UniformBuffer*>();
  if (length == 0) {
    assert(base == 0);
    length = b->_length;
  }

  mapping->_offset = base;
  mapping->_length = length;
  mapping->_fxi    = this;
  mapping->_buffer = b;
  mapping->_impl.make<UniformBufferMapping>();
  GL_ERRORCHECK();
  glBindBuffer(GL_UNIFORM_BUFFER, ub->_glbufid);
  // mapping->_mappedaddr = malloc(length);
  // glMapBuffer(GL_UNIFORM_BUFFER,
  //                                      GL_WRITE_ONLY);
  mapping->_mappedaddr = glMapBufferRange(
      GL_UNIFORM_BUFFER,
      base,
      length,
      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT |
          // GL_MAP_FLUSH_EXPLICIT_BIT |
          0);
  assert(mapping->_mappedaddr != nullptr);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  GL_ERRORCHECK();
  return mapping;
}

///////////////////////////////////////////////////////////////////////////////

void Interface::unmapParamBuffer(FxShaderParamBufferMapping* mapping) {
  assert(mapping->_impl.isA<UniformBufferMapping>());
  auto ub = mapping->_buffer->_impl.get<UniformBuffer*>();
  GL_ERRORCHECK();
  glBindBuffer(GL_UNIFORM_BUFFER, ub->_glbufid);
  // glFlushMappedBufferRange(GL_UNIFORM_BUFFER,mapping->_offset,mapping->_length);
  glUnmapBuffer(GL_UNIFORM_BUFFER);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  GL_ERRORCHECK();
  mapping->_impl.make<void*>(nullptr);
  mapping->_mappedaddr = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamBlockBuffer(const FxShaderParamBlock* block, FxShaderParamBuffer* buffer) {
  auto uniblock  = block->_impl.get<UniformBlock*>();
  auto unibuffer = buffer->_impl.get<UniformBuffer*>();
  assert(uniblock != nullptr);
  assert(unibuffer != nullptr);
  auto pass = _active_effect->_activePass;
  assert(pass != nullptr);
  pass->bindUniformBlockBuffer(uniblock, unibuffer);
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderParamBlock* Interface::parameterBlock(FxShader* hfx, const std::string& name) {
  OrkAssert(0 != hfx);
  auto& parammap = hfx->_parameterBlockByName;
  auto it        = parammap.find(name);
  auto fxsblock  = (FxShaderParamBlock*)((it != parammap.end()) ? it->second : nullptr);
  auto container = hfx->_internalHandle.get<rootcontainer_ptr_t>();

  auto ublock = container->uniformBlock(name);
  if (ublock != nullptr and fxsblock == nullptr) {
    fxsblock       = new FxShaderParamBlock;
    fxsblock->_fxi = this;
    fxsblock->_impl.set<UniformBlock*>(ublock);
    parammap[name] = fxsblock;
    for (auto u : ublock->_subuniforms) {
      auto p                         = new FxShaderParam;
      p->_blockinfo                  = new FxShaderParamInBlockInfo;
      p->_blockinfo->_parent         = fxsblock;
      p->mInternalHandle             = (void*)u;
      p->_name                       = u->_name;
      fxsblock->_subparams[p->_name] = p;
    }
  }
  return fxsblock;
}

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_SHADER_STORAGE)

const FxShaderStorageBlock* Interface::storageBlock(FxShader* hfx, const std::string& name) {
  OrkAssert(0 != hfx);
  const auto& storagemap = hfx->namedStorageBlocks();
  const auto& it         = storagemap.find(name);
  auto fxsblock          = (FxShaderStorageBlock*)(it != storagemap.end()) ? it->second : nullptr;

  assert(false); // not implmented yet
  auto container = hfx->_internalHandle.get<rootcontainer_ptr_t>();
  // auto ublk      = container->storageBlock(name);

  return fxsblock;
}

#endif

#if defined(ENABLE_COMPUTE_SHADERS)

const FxComputeShader* Interface::computeShader(FxShader* hfx, const std::string& name) {
  OrkAssert(0 != hfx);
  const auto& cshmap = hfx->namedComputeShaders();
  const auto& it     = cshmap.find(name);
  auto csh           = (FxComputeShader*)(it != cshmap.end()) ? it->second : nullptr;

  // auto container = static_cast<RootContainer*>(hfx->GetInternalHandle());
  // auto ublk      = container->storageBlock(name);

  return csh;
}

#endif

} // namespace ork::lev2::glslfx
