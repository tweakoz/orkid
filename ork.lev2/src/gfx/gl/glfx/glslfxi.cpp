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

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
// FX Interface
///////////////////////////////////////////////////////////////////////////////

void GfxTargetGL::FxInit() {
  static bool binit = true;

  if (true == binit) {
    binit = false;
    FxShader::RegisterLoaders("shaders/glfx/", "glfx");
  }
}

}

namespace ork::lev2::glslfx {

Container* GenPlat2SolidFx(const AssetPath& pth);
Container* GenPlat2UiFx(const AssetPath& pth);

///////////////////////////////////////////////////////////////////////////////

void Interface::DoBeginFrame() { mLastPass = 0; }

///////////////////////////////////////////////////////////////////////////////

bool Pass::HasUniformInstance(UniformInstance* puni) const {
  Uniform* pun                                               = puni->mpUniform;
  std::map<std::string, UniformInstance*>::const_iterator it = mUniformInstances.find(pun->mName);
  return it != mUniformInstances.end();
}

///////////////////////////////////////////////////////////////////////////////

const UniformInstance* Pass::GetUniformInstance(Uniform* puni) const {
  std::map<std::string, UniformInstance*>::const_iterator it = mUniformInstances.find(puni->mName);
  return (it != mUniformInstances.end()) ? it->second : nullptr;
}

///////////////////////////////////////////////////////////////////////////////

Interface::Interface(GfxTargetGL& glctx)
    : mTarget(glctx) {}

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
  for (const auto& ittek : pcont->mTechniqueMap) {
    Technique* ptek            = ittek.second;
    FxShaderTechnique* ork_tek = new FxShaderTechnique((void*)ptek);
    ork_tek->mTechniqueName    = ittek.first;
    // pabstek->mPasses = ittek->first;
    ork_tek->mbValidated = true;
    fxh->AddTechnique(ork_tek);
  }
  for (const auto& itp : pcont->mUniforms) {
    Uniform* puni                = itp.second;
    FxShaderParam* ork_parm      = new FxShaderParam;
    ork_parm->mParameterName     = itp.first;
    ork_parm->mParameterSemantic = puni->mSemantic;
    ork_parm->mInternalHandle    = (void*)puni;
    fxh->AddParameter(ork_parm);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Container::AddConfig(Config* pcfg) { mConfigs[pcfg->mName] = pcfg; }
void Container::addUniformSet(UniformSet* pif) { _uniformSets[pif->_name] = pif; }
void Container::AddVertexInterface(StreamInterface* pif) { mVertexInterfaces[pif->mName] = pif; }
void Container::AddTessCtrlInterface(StreamInterface* pif) { mTessCtrlInterfaces[pif->mName] = pif; }
void Container::AddTessEvalInterface(StreamInterface* pif) { mTessEvalInterfaces[pif->mName] = pif; }
void Container::AddGeometryInterface(StreamInterface* pif) { mGeometryInterfaces[pif->mName] = pif; }
void Container::AddFragmentInterface(StreamInterface* pif) { mFragmentInterfaces[pif->mName] = pif; }
void Container::AddStateBlock(StateBlock* psb) { mStateBlocks[psb->mName] = psb; }
void Container::AddLibBlock(LibBlock* plb) { mLibBlocks[plb->mName] = plb; }
void Container::AddTechnique(Technique* ptek) { mTechniqueMap[ptek->mName] = ptek; }
void Container::AddVertexProgram(ShaderVtx* psha) { mVertexPrograms[psha->mName] = psha; }
void Container::AddTessCtrlProgram(ShaderTsC* psha) { mTessCtrlPrograms[psha->mName] = psha; }
void Container::AddTessEvalProgram(ShaderTsE* psha) { mTessEvalPrograms[psha->mName] = psha; }
void Container::AddGeometryProgram(ShaderGeo* psha) { mGeometryPrograms[psha->mName] = psha; }
void Container::AddFragmentProgram(ShaderFrg* psha) { mFragmentPrograms[psha->mName] = psha; }

///////////////////////////////////////////////////////////////////////////////

StateBlock* Container::GetStateBlock(const std::string& name) const {
  const auto& it = mStateBlocks.find(name);
  return (it == mStateBlocks.end()) ? nullptr : it->second;
}
ShaderVtx* Container::GetVertexProgram(const std::string& name) const {
  const auto& it = mVertexPrograms.find(name);
  return (it == mVertexPrograms.end()) ? nullptr : it->second;
}
ShaderTsC* Container::GetTessCtrlProgram(const std::string& name) const {
  const auto& it = mTessCtrlPrograms.find(name);
  return (it == mTessCtrlPrograms.end()) ? nullptr : it->second;
}
ShaderTsE* Container::GetTessEvalProgram(const std::string& name) const {
  const auto& it = mTessEvalPrograms.find(name);
  return (it == mTessEvalPrograms.end()) ? nullptr : it->second;
}
ShaderGeo* Container::GetGeometryProgram(const std::string& name) const {
  const auto& it = mGeometryPrograms.find(name);
  return (it == mGeometryPrograms.end()) ? nullptr : it->second;
}
ShaderFrg* Container::GetFragmentProgram(const std::string& name) const {
  const auto& it = mFragmentPrograms.find(name);
  return (it == mFragmentPrograms.end()) ? nullptr : it->second;
}
UniformSet* Container::uniformSet(const std::string& name) const {
  const auto& it = _uniformSets.find(name);
  return (it == _uniformSets.end()) ? nullptr : it->second;
}
StreamInterface* Container::GetVertexInterface(const std::string& name) const {
  const auto& it = mVertexInterfaces.find(name);
  return (it == mVertexInterfaces.end()) ? nullptr : it->second;
}
StreamInterface* Container::GetTessCtrlInterface(const std::string& name) const {
  const auto& it = mTessCtrlInterfaces.find(name);
  return (it == mTessCtrlInterfaces.end()) ? nullptr : it->second;
}
StreamInterface* Container::GetTessEvalInterface(const std::string& name) const {
  const auto& it = mTessEvalInterfaces.find(name);
  return (it == mTessEvalInterfaces.end()) ? nullptr : it->second;
}
StreamInterface* Container::GetGeometryInterface(const std::string& name) const {
  const auto& it = mGeometryInterfaces.find(name);
  return (it == mGeometryInterfaces.end()) ? nullptr : it->second;
}
StreamInterface* Container::GetFragmentInterface(const std::string& name) const {
  const auto& it = mFragmentInterfaces.find(name);
  return (it == mFragmentInterfaces.end()) ? nullptr : it->second;
}
Uniform* Container::GetUniform(const std::string& name) const {
  const auto& it = mUniforms.find(name);
  return (it == mUniforms.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

Uniform* Container::MergeUniform(const std::string& name) {
  Uniform* pret  = nullptr;
  const auto& it = mUniforms.find(name);
  if (it == mUniforms.end()) {
    pret            = new Uniform(name);
    mUniforms[name] = pret;
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
    , mActivePass(nullptr)
    , mActiveNumPasses(0)
    , mShaderCompileFailed(false) {
  StateBlock* pdefsb = new StateBlock;
  pdefsb->mName      = "default";
  this->AddStateBlock(pdefsb);
}

///////////////////////////////////////////////////////////////////////////////

void Container::Destroy(void) {}

///////////////////////////////////////////////////////////////////////////////

bool Container::IsValid(void) { return true; }

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
  if (mpActiveEffect && mpActiveEffect->mActivePass && mpActiveEffect->mActivePass->mStateBlock) {
    const auto& items = mpActiveEffect->mActivePass->mStateBlock->mApplicators;

    for (const auto& item : items) {
      item(&mTarget);
    }
    // const SRasterState& rstate =
    // mpActiveEffect->mActivePass->mStateBlock->mState;
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

const FxShaderTechnique* Interface::GetTechnique(FxShader* hfx, const std::string& name) {
  // orkprintf( "Get cgtek<%s> hfx<%x>\n", name.c_str(), hfx );
  OrkAssert(hfx != 0);
  Container* container = static_cast<Container*>(hfx->GetInternalHandle());
  OrkAssert(container != 0);
  /////////////////////////////////////////////////////////////

  const auto& tekmap            = hfx->GetTechniques();
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

bool Shader::Compile() {
  GL_NF_ERRORCHECK();
  mShaderObjectId = glCreateShader(mShaderType);

  std::string shadertext = "";

  shadertext += mShaderText;

  shadertext += "void main() { ";
  shadertext += mName;
  shadertext += "(); }\n";
  const char* c_str = shadertext.c_str();

  // printf( "Shader<%s>\n/////////////\n%s\n///////////////////\n",
  // mName.c_str(), c_str );

  GL_NF_ERRORCHECK();
  glShaderSource(mShaderObjectId, 1, &c_str, NULL);
  GL_NF_ERRORCHECK();
  glCompileShader(mShaderObjectId);
  GL_NF_ERRORCHECK();

  GLint compiledOk = 0;
  glGetShaderiv(mShaderObjectId, GL_COMPILE_STATUS, &compiledOk);
  if (GL_FALSE == compiledOk) {
    char infoLog[1 << 16];
    glGetShaderInfoLog(mShaderObjectId, sizeof(infoLog), NULL, infoLog);
    printf("//////////////////////////////////\n");
    printf("%s\n", c_str);
    printf("//////////////////////////////////\n");
    printf("Effect<%s>\n", mpContainer->mEffectName.c_str());
    printf("ShaderType<0x%x>\n", mShaderType);
    printf("Shader<%s> InfoLog<%s>\n", mName.c_str(), infoLog);
    printf("//////////////////////////////////\n");

    if (mpContainer->mFxShader->GetAllowCompileFailure() == false)
      OrkAssert(false);

    mbError = true;
    return false;
  }
  mbCompiled = true;
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Shader::IsCompiled() const { return mbCompiled; }

///////////////////////////////////////////////////////////////////////////////

bool Interface::BindPass(FxShader* hfx, int ipass) {
  Container* container = static_cast<Container*>(hfx->GetInternalHandle());
  if (container->mShaderCompileFailed)
    return false;

  assert(container->mActiveTechnique != nullptr);

  container->mActivePass = container->mActiveTechnique->mPasses[ipass];
  Pass* ppass            = const_cast<Pass*>(container->mActivePass);

  GL_ERRORCHECK();
  if (0 == container->mActivePass->mProgramObjectId) {
    Shader* pvtxshader = container->mActivePass->mVertexProgram;
    Shader* ptecshader = container->mActivePass->mTessCtrlProgram;
    Shader* pteeshader = container->mActivePass->mTessEvalProgram;
    Shader* pgeoshader = container->mActivePass->mGeometryProgram;
    Shader* pfrgshader = container->mActivePass->mFragmentProgram;

    OrkAssert(pvtxshader != nullptr);
    OrkAssert(pfrgshader != nullptr);

    auto l_compile = [&](Shader* psh) {
      bool compile_ok = true;
      if (psh && psh->IsCompiled() == false)
        compile_ok = psh->Compile();

      if (false == compile_ok) {
        container->mShaderCompileFailed = true;
        hfx->SetFailedCompile(true);
      }
    };

    l_compile(pvtxshader);
    l_compile(ptecshader);
    l_compile(pteeshader);
    l_compile(pgeoshader);
    l_compile(pfrgshader);

    if (container->mShaderCompileFailed)
      return false;

    if (pvtxshader->IsCompiled() && pfrgshader->IsCompiled()) {
      GL_ERRORCHECK();
      GLuint prgo             = glCreateProgram();
      ppass->mProgramObjectId = prgo;

      //////////////
      // attach shaders
      //////////////

      glAttachShader(prgo, pvtxshader->mShaderObjectId);
      GL_ERRORCHECK();

      if (ptecshader && ptecshader->IsCompiled()) {
        glAttachShader(prgo, ptecshader->mShaderObjectId);
        GL_ERRORCHECK();
      }

      if (pteeshader && pteeshader->IsCompiled()) {
        glAttachShader(prgo, pteeshader->mShaderObjectId);
        GL_ERRORCHECK();
      }

      if (pgeoshader && pgeoshader->IsCompiled()) {
        glAttachShader(prgo, pgeoshader->mShaderObjectId);
        GL_ERRORCHECK();
      }

      glAttachShader(prgo, pfrgshader->mShaderObjectId);
      GL_ERRORCHECK();

      //////////////
      // link
      //////////////

      StreamInterface* vtx_iface = pvtxshader->mpInterface;
      StreamInterface* frg_iface = pfrgshader->mpInterface;

      /*printf( "//////////////////////////////////\n");

      printf( "Linking pass<%p> {\n", ppass );

      if( pvtxshader )
              printf( "	vtxshader<%s:%p> compiled<%d>\n",
      pvtxshader->mName.c_str(), pvtxshader, pvtxshader->IsCompiled() ); if(
      ptecshader ) printf( "	tecshader<%s:%p> compiled<%d>\n",
      ptecshader->mName.c_str(), ptecshader, ptecshader->IsCompiled() ); if(
      pteeshader ) printf( "	teeshader<%s:%p> compiled<%d>\n",
      pteeshader->mName.c_str(), pteeshader, pteeshader->IsCompiled() ); if(
      pgeoshader ) printf( "	geoshader<%s:%p> compiled<%d>\n",
      pgeoshader->mName.c_str(), pgeoshader, pgeoshader->IsCompiled() ); if(
      pfrgshader ) printf( "	frgshader<%s:%p> compiled<%d>\n",
      pfrgshader->mName.c_str(), pfrgshader, pfrgshader->IsCompiled() );
*/

      if (nullptr == vtx_iface) {
        printf("	vtxshader<%s> has no interface!\n", pvtxshader->mName.c_str());
        OrkAssert(false);
      }
      if (nullptr == frg_iface) {
        printf("	frgshader<%s> has no interface!\n", pfrgshader->mName.c_str());
        OrkAssert(false);
      }

      // printf( "	binding vertex attributes count<%d>\n",
      // int(vtx_iface->mAttributes.size()) );

      //////////////////////////
      // Bind Vertex Attributes
      //////////////////////////

      for (const auto& itp : vtx_iface->mAttributes) {
        Attribute* pattr = itp.second;
        int iloc         = pattr->mLocation;
        // printf( "	vtxattr<%s> loc<%d> dir<%s> sem<%s>\n",
        // pattr->mName.c_str(), iloc, pattr->mDirection.c_str(),
        // pattr->mSemantic.c_str() );
        glBindAttribLocation(prgo, iloc, pattr->mName.c_str());
        GL_ERRORCHECK();
        ppass->mVtxAttributeById[iloc]                    = pattr;
        ppass->mVtxAttributesBySemantic[pattr->mSemantic] = pattr;
      }

      //////////////////////////
      // ensure vtx_iface exports what frg_iface imports
      //////////////////////////

      /*if( nullptr == pgeoshader )
      {
              for( const auto& itp : frg_iface->mAttributes )
              {	const Attribute* pfrgattr = itp.second;
                      if( pfrgattr->mDirection=="in" )
                      {
                              int iloc = pfrgattr->mLocation;
                              const std::string& name = pfrgattr->mName;
                              //printf( "frgattr<%s> loc<%d> dir<%s>\n",
      pfrgattr->mName.c_str(), iloc, pfrgattr->mDirection.c_str() ); const auto&
      itf=vtx_iface->mAttributes.find(name); const Attribute* pvtxattr =
      (itf!=vtx_iface->mAttributes.end()) ? itf->second : nullptr; OrkAssert(
      pfrgattr != nullptr ); OrkAssert( pvtxattr != nullptr ); OrkAssert(
      pvtxattr->mTypeName == pfrgattr->mTypeName );
                      }
              }
      }*/

      //////////////////////////

      GL_ERRORCHECK();
      glLinkProgram(prgo);
      GL_ERRORCHECK();
      GLint linkstat = 0;
      glGetProgramiv(prgo, GL_LINK_STATUS, &linkstat);
      if (linkstat != GL_TRUE) {
        char infoLog[1 << 16];
        glGetProgramInfoLog(prgo, sizeof(infoLog), NULL, infoLog);
        printf("\n\n//////////////////////////////////\n");
        printf("program InfoLog<%s>\n", infoLog);
        printf("//////////////////////////////////\n\n");
        OrkAssert(false);
      }
      OrkAssert(linkstat == GL_TRUE);

      // printf( "} // linking complete..\n" );

      // printf( "//////////////////////////////////\n");

      //////////////////////////
      // query attr
      //////////////////////////

      GLint numattr = 0;
      glGetProgramiv(prgo, GL_ACTIVE_ATTRIBUTES, &numattr);
      GL_ERRORCHECK();

      for (int i = 0; i < numattr; i++) {
        GLchar nambuf[256];
        GLsizei namlen = 0;
        GLint atrsiz   = 0;
        GLenum atrtyp  = GL_ZERO;
        GL_ERRORCHECK();
        glGetActiveAttrib(prgo, i, sizeof(nambuf), &namlen, &atrsiz, &atrtyp, nambuf);
        OrkAssert(namlen < sizeof(nambuf));
        GL_ERRORCHECK();
        const auto& it = vtx_iface->mAttributes.find(nambuf);
        OrkAssert(it != vtx_iface->mAttributes.end());
        Attribute* pattr = it->second;
        // printf( "qattr<%d> loc<%d> name<%s>\n", i, pattr->mLocation, nambuf
        // );
        pattr->meType = atrtyp;
        // pattr->mLocation = i;
      }

      //////////////////////////
      // query unis
      //////////////////////////

      GLint numunis = 0;
      GL_ERRORCHECK();
      glGetProgramiv(prgo, GL_ACTIVE_UNIFORMS, &numunis);
      GL_ERRORCHECK();

      ppass->mSamplerCount = 0;

      for (int i = 0; i < numunis; i++) {
        GLsizei namlen = 0;
        GLint unisiz   = 0;
        GLenum unityp  = GL_ZERO;
        std::string str_name;

        {
          GLchar nambuf[256];
          glGetActiveUniform(prgo, i, sizeof(nambuf), &namlen, &unisiz, &unityp, nambuf);
          OrkAssert(namlen < sizeof(nambuf));
          // printf( "find uni<%s>\n", nambuf );
          GL_ERRORCHECK();

          str_name = nambuf;
          auto its = str_name.find('[');
          if (its != str_name.npos) {
            str_name = str_name.substr(0, its);
            // printf( "nnam<%s>\n", str_name.c_str() );
          }
        }
        const auto& it = container->mUniforms.find(str_name);
        OrkAssert(it != container->mUniforms.end());
        Uniform* puni = it->second;

        puni->meType = unityp;

        UniformInstance* pinst = new UniformInstance;
        pinst->mpUniform       = puni;

        GLint uniloc     = glGetUniformLocation(prgo, str_name.c_str());
        pinst->mLocation = uniloc;

        if (puni->mTypeName == "sampler2D") {
          pinst->mSubItemIndex = ppass->mSamplerCount;
          ppass->mSamplerCount++;
          pinst->mPrivData.Set<GLenum>(GL_TEXTURE_2D);
        } else if (puni->mTypeName == "sampler3D") {
          pinst->mSubItemIndex = ppass->mSamplerCount;
          ppass->mSamplerCount++;
          pinst->mPrivData.Set<GLenum>(GL_TEXTURE_3D);
        } else if (puni->mTypeName == "sampler2DShadow") {
          pinst->mSubItemIndex = ppass->mSamplerCount;
          ppass->mSamplerCount++;
          pinst->mPrivData.Set<GLenum>(GL_TEXTURE_2D);
        }

        const char* fshnam = pfrgshader->mName.c_str();

        // printf("fshnam<%s> uninam<%s> loc<%d>\n", fshnam, str_name.c_str(),
        // (int) uniloc );

        const_cast<Pass*>(container->mActivePass)->mUniformInstances[puni->mName] = pinst;
      }
    }
  }

  GL_ERRORCHECK();
  glUseProgram(container->mActivePass->mProgramObjectId);
  GL_ERRORCHECK();

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void Interface::EndPass(FxShader* hfx) {
  Container* container = static_cast<Container*>(hfx->GetInternalHandle());
  GL_ERRORCHECK();
  glUseProgram(0);
  GL_ERRORCHECK();
  // cgResetPassState( container->mActivePass );
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderParam* Interface::GetParameterH(FxShader* hfx, const std::string& name) {
  OrkAssert(0 != hfx);
  const auto& parammap        = hfx->GetParametersByName();
  const auto& it              = parammap.find(name);
  const FxShaderParam* hparam = (it != parammap.end()) ? it->second : 0;
  return hparam;
}

///////////////////////////////////////////////////////////////////////////////

void Interface::BindParamBool(FxShader* hfx, const FxShaderParam* hpar, const bool bv) {}

///////////////////////////////////////////////////////////////////////////////

void Interface::BindParamInt(FxShader* hfx, const FxShaderParam* hpar, const int iv) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->GetUniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_INT);

      GL_ERRORCHECK();
      glUniform1i(iloc, iv);
      GL_ERRORCHECK();
    } else {
      assert(false);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void Interface::BindParamVect2(FxShader* hfx, const FxShaderParam* hpar, const fvec2& Vec) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->GetUniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT_VEC2);

      GL_ERRORCHECK();
      glUniform2fv(iloc, 1, Vec.GetArray());
      GL_ERRORCHECK();
    }
  }
}

void Interface::BindParamVect3(FxShader* hfx, const FxShaderParam* hpar, const fvec3& Vec) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->GetUniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT_VEC3);

      GL_ERRORCHECK();
      glUniform3fv(iloc, 1, Vec.GetArray());
      GL_ERRORCHECK();
    }
  }
}

void Interface::BindParamVect4(FxShader* hfx, const FxShaderParam* hpar, const fvec4& Vec) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->GetUniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT_VEC4);

      GL_ERRORCHECK();
      glUniform4fv(iloc, 1, Vec.GetArray());
      GL_ERRORCHECK();
    }
  }
}

void Interface::BindParamVect4Array(FxShader* hfx, const FxShaderParam* hpar, const fvec4* Vec, const int icount) {
  /*Container* container = static_cast<Container*>(
  hfx->GetInternalHandle() ); Uniform* puni = static_cast<Uniform*>(
  hpar->GetPlatformHandle() ); int iloc = puni->mLocation; if( iloc>=0 )
  {
          const char* psem = puni->mSemantic.c_str();
          const char* pnam = puni->mName.c_str();
          GLenum etyp = puni->meType;
          OrkAssert( etyp == 	GL_FLOAT_VEC4 );

          glUniform4fv( iloc, icount, (float*) Vec );
          GL_ERRORCHECK();
  }*/
}

void Interface::BindParamFloat(FxShader* hfx, const FxShaderParam* hpar, float fA) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->GetUniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT);

      GL_ERRORCHECK();
      glUniform1f(iloc, fA);
      GL_ERRORCHECK();
    }
  }
}
void Interface::BindParamFloatArray(FxShader* hfx, const FxShaderParam* hpar, const float* pfa, const int icount) {
  /*Container* container = static_cast<Container*>(
  hfx->GetInternalHandle() ); Uniform* puni = static_cast<Uniform*>(
  hpar->GetPlatformHandle() ); int iloc = puni->mLocation; if( iloc>=0 )
  {
          const char* psem = puni->mSemantic.c_str();
          const char* pnam = puni->mName.c_str();
          GLenum etyp = puni->meType;
          OrkAssert( etyp == GL_FLOAT );

          glUniform1fv( iloc, icount, pfa );
          GL_ERRORCHECK();
  }*/
}

void Interface::BindParamFloat2(FxShader* hfx, const FxShaderParam* hpar, float fA, float fB) {
  /*Container* container = static_cast<Container*>(
  hfx->GetInternalHandle() ); Uniform* puni = static_cast<Uniform*>(
  hpar->GetPlatformHandle() ); int iloc = puni->mLocation; if( iloc>=0 )
  {
          const char* psem = puni->mSemantic.c_str();
          const char* pnam = puni->mName.c_str();
          GLenum etyp = puni->meType;
          OrkAssert( etyp == 	GL_FLOAT_VEC2 );

          fvec2 v2( fA, fB );

          glUniform2fv( iloc, 1, v2.GetArray() );
          GL_ERRORCHECK();
  }*/
}

void Interface::BindParamFloat3(FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC) {
  /*Container* container = static_cast<Container*>(
  hfx->GetInternalHandle() ); Uniform* puni = static_cast<Uniform*>(
  hpar->GetPlatformHandle() ); int iloc = puni->mLocation; if( iloc>=0 )
  {
          const char* psem = puni->mSemantic.c_str();
          const char* pnam = puni->mName.c_str();
          GLenum etyp = puni->meType;
          OrkAssert( etyp == 	GL_FLOAT_VEC3 );

          fvec3 v3( fA, fB, fC );

          glUniform3fv( iloc, 1, v3.GetArray() );
          GL_ERRORCHECK();
  }*/
}

void Interface::BindParamFloat4(FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD) {
  /*Container* container = static_cast<Container*>(
  hfx->GetInternalHandle() ); Uniform* puni = static_cast<Uniform*>(
  hpar->GetPlatformHandle() ); int iloc = puni->mLocation; if( iloc>=0 )
  {
          const char* psem = puni->mSemantic.c_str();
          const char* pnam = puni->mName.c_str();
          GLenum etyp = puni->meType;
          OrkAssert( etyp == 	GL_FLOAT_VEC4 );

          fvec4 v4( fA, fB, fC, fD );

          glUniform4fv( iloc, 1, v4.GetArray() );
          GL_ERRORCHECK();
  }*/
}

void Interface::BindParamU32(FxShader* hfx, const FxShaderParam* hpar, U32 uval) {
  /*
          CgFxContainer* container = static_cast<CgFxContainer*>(
     hfx->GetInternalHandle() ); CGeffect cgeffect = container->mCgEffect;
          CGparameter cgparam =
     reinterpret_cast<CGparameter>(hpar->GetPlatformHandle()); GL_ERRORCHECK();
  */
}

void Interface::BindParamMatrix(FxShader* hfx, const FxShaderParam* hpar, const fmtx4& Mat) {
  Container* container = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni        = static_cast<Uniform*>(hpar->GetPlatformHandle());
  assert(container->mActivePass != nullptr);
  const UniformInstance* pinst = container->mActivePass->GetUniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT_MAT4);

      GL_ERRORCHECK();
      glUniformMatrix4fv(iloc, 1, GL_FALSE, Mat.GetArray());
      GL_ERRORCHECK();
    }
  }
}

void Interface::BindParamMatrix(FxShader* hfx, const FxShaderParam* hpar, const fmtx3& Mat) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->GetUniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT_MAT3);

      GL_ERRORCHECK();
      glUniformMatrix3fv(iloc, 1, GL_FALSE, Mat.GetArray());
      GL_ERRORCHECK();
    }
  }
}

void Interface::BindParamMatrixArray(FxShader* hfx, const FxShaderParam* hpar, const fmtx4* Mat, int iCount) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->GetUniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT_MAT4);

      // printf( "pnam<%s>\n", pnam );
      GL_ERRORCHECK();
      glUniformMatrix4fv(iloc, iCount, GL_FALSE, (const float*)Mat);
      GL_ERRORCHECK();
    }
  }

  /*Container* container = static_cast<Container*>(
  hfx->GetInternalHandle() ); Uniform* puni = static_cast<Uniform*>(
  hpar->GetPlatformHandle() ); int iloc = puni->mLocation; if( iloc>=0 )
  {
          const char* psem = puni->mSemantic.c_str();
          const char* pnam = puni->mName.c_str();
          GLenum etyp = puni->meType;
          OrkAssert( etyp == GL_FLOAT_MAT4 );

          glUniformMatrix4fv( iloc, iCount, GL_FALSE, (float*) Mat );
          GL_ERRORCHECK();
  }*/
}

///////////////////////////////////////////////////////////////////////////////

void Interface::BindParamCTex(FxShader* hfx, const FxShaderParam* hpar, const Texture* pTex) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->GetUniformInstance(puni);
  // printf( "Bind1 Tex<%p> par<%s> pinst<%p>\n",
  // pTex,hpar->mParameterName.c_str(), pinst );
  if (pinst) {
    int iloc = pinst->mLocation;

    const char* teknam = container->mActiveTechnique->mName.c_str();

    // printf( "Bind2 Tex<%p> par<%s> iloc<%d> teknam<%s>\n",
    // pTex,hpar->mParameterName.c_str(), iloc, teknam );
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      // OrkAssert( etyp == GL_FLOAT_MAT4 );

      if (pTex != 0) {
        const GLTextureObject* pTEXOBJ = (GLTextureObject*)pTex->GetTexIH();
        GLuint texID                   = pTEXOBJ ? pTEXOBJ->mObject : 0;
        int itexunit                   = pinst->mSubItemIndex;

        GLenum textgt = pinst->mPrivData.Get<GLenum>();

        // printf( "Bind3 ISDEPTH<%d> tex<%p> texobj<%d> itexunit<%d>
        // textgt<%d>\n", int(pTex->_isDepthTexture), pTex, texID, itexunit,
        // int(textgt) );

        GL_ERRORCHECK();
        glActiveTexture(GL_TEXTURE0 + itexunit);
        GL_ERRORCHECK();
        glBindTexture(textgt, texID);
        GL_ERRORCHECK();
        // glEnable( GL_TEXTURE_2D );
        // GL_ERRORCHECK();
        glUniform1i(iloc, itexunit);
        GL_ERRORCHECK();
      }
    }
  }
  /*
          if( 0 == hpar ) return;
          CgFxContainer* container = static_cast<CgFxContainer*>(
     hfx->GetInternalHandle() ); CGeffect cgeffect = container->mCgEffect;
          CGparameter cgparam =
     reinterpret_cast<CGparameter>(hpar->GetPlatformHandle()); if( (pTex!=0) &&
     (cgparam!=0) )
          {
                  const GLTextureObject* pTEXOBJ = (GLTextureObject*)
     pTex->GetTexIH();
                  //orkprintf( "BINDTEX param<%p:%s> pTEX<%p> pTEXOBJ<%p>
     obj<%d>\n", hpar, hpar->mParameterName.c_str(), pTex, pTEXOBJ,
     pTEXOBJ->mObject ); cgGLSetTextureParameter( cgparam, pTEXOBJ ?
     pTEXOBJ->mObject : 0 );
          }
          else
          {
                  cgGLSetTextureParameter( cgparam, 0 );
          }
          GL_ERRORCHECK();
  */
}

} // namespace ork::lev2::glslfx
