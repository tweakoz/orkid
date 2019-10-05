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

///////////////////////////////////////////////////////////////////////////////
// FX Interface
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
void GfxTargetGL::FxInit() {
  static bool binit = true;
  if (true == binit) {
    binit = false;
    FxShader::RegisterLoaders("shaders/glfx/", "glfx");
  }
}
}

///////////////////////////////////////////////////////////////////////////////
// implementation
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2::glslfx {

///////////////////////////////////////////////////////////////////////////////

Interface::Interface(GfxTargetGL& glctx)
    : mTarget(glctx) {}

///////////////////////////////////////////////////////////////////////////////

void Interface::DoBeginFrame() { mLastPass = 0; }

///////////////////////////////////////////////////////////////////////////////

bool Interface::LoadFxShader(const AssetPath& pth, FxShader* pfxshader) {
  // printf( "GLSLFXI LoadShader<%s>\n", pth.c_str() );
  GL_ERRORCHECK();
  bool bok = false;
  pfxshader->SetInternalHandle(0);

  Container* pcontainer = LoadFxFromFile(pth);
  OrkAssert(pcontainer != nullptr);
  pfxshader->SetInternalHandle((void*)pcontainer);
  bok = pcontainer->IsValid();

  pcontainer->mFxShader = pfxshader;

  if (bok) {
    BindContainerToAbstract(pcontainer, pfxshader);
  }
  GL_ERRORCHECK();

  return bok;
}

///////////////////////////////////////////////////////////////////////////////

void Interface::BindContainerToAbstract(Container* pcont, FxShader* fxh) {
  for (const auto& ittek : pcont->_techniqueMap) {
    Technique* ptek            = ittek.second;
    FxShaderTechnique* ork_tek = new FxShaderTechnique((void*)ptek);
    ork_tek->mTechniqueName    = ittek.first;
    // pabstek->mPasses = ittek->first;
    ork_tek->mbValidated = true;
    fxh->addTechnique(ork_tek);
  }
  for (const auto& itp : pcont->_uniforms) {
    Uniform* puni                = itp.second;
    FxShaderParam* ork_parm      = new FxShaderParam;
    ork_parm->_name     = itp.first;
    ork_parm->mParameterSemantic = puni->_semantic;
    ork_parm->mInternalHandle    = (void*)puni;
    fxh->addParameter(ork_parm);
  }
}


///////////////////////////////////////////////////////////////////////////////

int Interface::BeginBlock(FxShader* hfx, const RenderContextInstData& data) {
  mTarget.SetRenderContextInstData(&data);
  Container* container = static_cast<Container*>(hfx->GetInternalHandle());
  mpActiveEffect       = container;
  mpActiveFxShader     = hfx;
  if (nullptr == container->mActiveTechnique)
    return 0;
  return container->mActiveTechnique->mPasses.size();
}

///////////////////////////////////////////////////////////////////////////////

void Interface::EndBlock(FxShader* hfx) { mpActiveFxShader = 0; }

///////////////////////////////////////////////////////////////////////////////

void Interface::CommitParams(void) {
  if (mpActiveEffect && mpActiveEffect->mActivePass && mpActiveEffect->mActivePass->_stateBlock) {
    const auto& items = mpActiveEffect->mActivePass->_stateBlock->mApplicators;

    for (const auto& item : items) {
      item(&mTarget);
    }
    // const SRasterState& rstate =
    // mpActiveEffect->mActivePass->_stateBlock->mState;
    // mTarget.RSI()->BindRasterState(rstate);
  }
  // if( (mpActiveEffect->mActivePass != mLastPass) ||
  // (mTarget.GetCurMaterial()!=mpLastFxMaterial) )
  {
    // orkprintf( "CgFxInterface::CommitParams() activepass<%p>\n",
    // mpActiveEffect->mActivePass ); cgSetPassState( mpActiveEffect->mActivePass
    // ); mpLastFxMaterial = mTarget.GetCurMaterial(); mLastPass =
    // mpActiveEffect->mActivePass;
  }
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderTechnique* Interface::technique(FxShader* hfx, const std::string& name) {
  // orkprintf( "Get cgtek<%s> hfx<%x>\n", name.c_str(), hfx );
  OrkAssert(hfx != 0);
  Container* container = static_cast<Container*>(hfx->GetInternalHandle());
  OrkAssert(container != 0);
  /////////////////////////////////////////////////////////////

  const auto& tekmap            = hfx->techniques();
  const auto& it                = tekmap.find(name);
  const FxShaderTechnique* htek = (it != tekmap.end()) ? it->second : 0;

  return htek;
}

///////////////////////////////////////////////////////////////////////////////

bool Interface::BindTechnique(FxShader* hfx, const FxShaderTechnique* htek) {
  if (nullptr == hfx)
    return false;
  if (nullptr == htek)
    return false;

  Container* container        = static_cast<Container*>(hfx->GetInternalHandle());
  const Technique* ptekcont   = static_cast<const Technique*>(htek->GetPlatformHandle());
  container->mActiveTechnique = ptekcont;
  container->mActivePass      = 0;

  // orkprintf( "binding cgtek<%s:%x>\n", ptekcont->mName.c_str(), ptekcont );

  return (ptekcont->mPasses.size() > 0);
}

///////////////////////////////////////////////////////////////////////////////

bool Interface::BindPass(FxShader* hfx, int ipass) {
  Container* container = static_cast<Container*>(hfx->GetInternalHandle());
  if (container->mShaderCompileFailed)
    return false;

  assert(container->mActiveTechnique != nullptr);

  container->mActivePass = container->mActiveTechnique->mPasses[ipass];
  GL_ERRORCHECK();
  if (0 == container->mActivePass->_programObjectId){
    bool complinkok = compileAndLink(container);
    hfx->SetFailedCompile(false==complinkok);
  }
  GL_ERRORCHECK();
  glUseProgram(container->mActivePass->_programObjectId);
  GL_ERRORCHECK();

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void Interface::EndPass(FxShader* hfx) {
  auto container = static_cast<Container*>(hfx->GetInternalHandle());
  GL_ERRORCHECK();
  glUseProgram(0);
  GL_ERRORCHECK();
  // cgResetPassState( container->mActivePass );
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

const FxShaderParamBlock* Interface::parameterBlock(FxShader* hfx, const std::string& name) {
  OrkAssert(0 != hfx);
  auto& parammap        = hfx->_parameterBlockByName;
  auto it              = parammap.find(name);
  auto fxsblock = (FxShaderParamBlock*) ((it != parammap.end()) ? it->second : nullptr);
  auto container = static_cast<Container*>(hfx->GetInternalHandle());

  auto ublock = container->uniformBlock(name);
  if( ublock != nullptr and fxsblock == nullptr){
    fxsblock = new FxShaderParamBlock;
     fxsblock->_fxi = this;
     fxsblock->_impl.Set<UniformBlock*>(ublock);
     parammap[name]=fxsblock;
    for( auto u : ublock->_subuniforms ){
      auto p = new FxShaderParam;
      p->mInternalHandle = (void*) u.second;
      p->_name = u.first;
     fxsblock->_subparams[p->_name]=p;
    }

  }
  return fxsblock;
}

paramblockmappingptr_t Interface::mapParamBlock(const FxShaderParamBlock*b,size_t base, size_t length) {
    auto mapping = std::make_shared<FxShaderParamBlockMapping>(base,length);
    mapping->_fxi = this;
   	return mapping;
}


///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_SHADER_STORAGE)

const FxShaderStorageBlock* Interface::storageBlock(FxShader* hfx, const std::string& name) {
  OrkAssert(0 != hfx);
  const auto& storagemap        = hfx->namedStorageBlocks();
  const auto& it              = storagemap.find(name);
  auto fxsblock = (FxShaderStorageBlock*) (it != storagemap.end()) ? it->second : nullptr;

  auto container = static_cast<Container*>(hfx->GetInternalHandle());
  auto ublk = container->storageBlock(name);

  return fxsblock;
}

#endif



} // namespace ork::lev2::glslfx
