////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include <ork/file/file.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/string/string.h>

constexpr bool _DEBUG_SHADER_COMPILE = false;

namespace ork::lev2::orksl {
///////////////////////////////////////////////////////////////////////////////

void Shader::dumpFinalText() const {
  const char* c_str = _finalText.c_str();
  printf("//////////////////////////////////\n");
  printf("%s\n", c_str);
  printf("//////////////////////////////////\n");
}

bool Shader::Compile() {
  GL_NF_ERRORCHECK();

  GLenum glshadertype = GL_ZERO;

  switch(_shadertype){
    case EShaderType::Vertex:
      glshadertype = GL_VERTEX_SHADER;
      break;
    case EShaderType::Fragment:
      glshadertype = GL_FRAGMENT_SHADER;
      break;
    case EShaderType::Geometry:
      glshadertype = GL_GEOMETRY_SHADER;
      break;
    case EShaderType::TessCtrl:
      glshadertype = GL_TESS_CONTROL_SHADER;
      break;
    case EShaderType::TessEval:
      glshadertype = GL_TESS_EVALUATION_SHADER;
      break;
#if defined(ENABLE_COMPUTE_SHADERS)
    case EShaderType::Compute:
      glshadertype = GL_COMPUTE_SHADER;
      break;
#endif
#if defined(ENABLE_NVMESH_SHADERS)
    case EShaderType::NvTask:
      glshadertype = GL_TASK_SHADER_NV;
      break;
    case EShaderType::NvMesh:
      glshadertype = GL_MESH_SHADER_NV;
      break;
#endif
    default:
      OrkAssert(false);
      break;
  }

  mShaderObjectId = glCreateShader(glshadertype);

#if defined(ENABLE_COMPUTE_SHADERS)
  if (glshadertype == GL_COMPUTE_SHADER) {
    printf("yo\n");
  }
#endif

  std::string shadertext = "";

  shadertext += mShaderText;

  shadertext += "void main() { ";
  shadertext += mName;
  shadertext += "(); }\n";

  _finalText        = shadertext;
  const char* c_str = shadertext.c_str();

  if (_DEBUG_SHADER_COMPILE) {
    printf("Shader<%s>\n/////////////\n%s\n///////////////////\n", mName.c_str(), c_str);
  }

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
    dumpFinalText();
    printf("Effect<%s>\n", _rootcontainer->mEffectName.c_str());
    printf("ShaderType<0x%x>\n", _shadertype);
    printf("Shader<%s> InfoLog<%s>\n", mName.c_str(), infoLog);
    printf("//////////////////////////////////\n");

    if (_rootcontainer->mFxShader->GetAllowCompileFailure() == false){
      OrkAssert(false);
    }

    mbError = true;
    return false;
  }
  mbCompiled = true;
  return true;
}

} //namespace ork::lev2::orksl {

namespace ork::lev2::glslfx {

////////////////////////////////////////////////////////////////////////////////

bool Interface::compilePipelineVTG(orksl::rootcontainer_ptr_t container) {

  auto pass = const_cast<orksl::Pass*>(container->_activePass);

  auto& pipeVTG = pass->_primpipe.Get<orksl::PrimPipelineVTG>();

  orksl::Shader* pvtxshader = pipeVTG._vertexShader;
  orksl::Shader* ptecshader = pipeVTG._tessCtrlShader;
  orksl::Shader* pteeshader = pipeVTG._tessEvalShader;
  orksl::Shader* pgeoshader = pipeVTG._geometryShader;
  orksl::Shader* pfrgshader = pipeVTG._fragmentShader;

  OrkAssert(pvtxshader != nullptr);
  OrkAssert(pfrgshader != nullptr);

  auto l_compile = [&](orksl::Shader* psh) -> bool {
    bool compile_ok = true;
    if (psh && psh->IsCompiled() == false)
      compile_ok = psh->Compile();

    if (false == compile_ok) {
      container->mShaderCompileFailed = true;
    }
    return compile_ok;
  };

  bool OK = true;
  OK &= l_compile(pvtxshader);
  OK &= l_compile(ptecshader);
  OK &= l_compile(pteeshader);
  OK &= l_compile(pgeoshader);
  OK &= l_compile(pfrgshader);

  container->mShaderCompileFailed = (false == OK);

  if (OK) {
    GL_ERRORCHECK();
    GLuint prgo            = glCreateProgram();
    pass->_programObjectId = prgo;

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

    orksl::StreamInterface* vtx_iface = pvtxshader->_inputInterface;
    orksl::StreamInterface* frg_iface = pfrgshader->_inputInterface;
    if (_DEBUG_SHADER_COMPILE) {
      auto tek = pass->_technique;
      printf(
          "	link tek<%s> pass<%s> vtxshader<%s> interface<%p>\n",
          tek->_name.c_str(),
          pass->_name.c_str(),
          pvtxshader->mName.c_str(),
          vtx_iface);
      printf(
          "	link tek<%s> pass<%s>frgshader<%s> interface<%p>\n",
          tek->_name.c_str(),
          pass->_name.c_str(),
          pfrgshader->mName.c_str(),
          frg_iface);
    }
    if (nullptr == vtx_iface) {
      printf("	vtxshader<%s> has no interface!\n", pvtxshader->mName.c_str());
      OrkAssert(false);
    }
    if (nullptr == frg_iface) {
      printf("	frgshader<%s> has no interface!\n", pfrgshader->mName.c_str());
      OrkAssert(false);
    }

    //////////////////////////
    // Bind Vertex Attributes
    //////////////////////////

    // printf( "	binding vertex attributes count<%d>\n",
    // int(vtx_iface->mAttributes.size()) );

    for (const auto& itp : vtx_iface->_inputAttributes) {
      orksl::Attribute* pattr = itp.second;
      int iloc         = pattr->_location;
      // printf( "	vtxattr<%s> loc<%d> dir<%s> sem<%s>\n",
      // pattr->mName.c_str(), iloc, pattr->mDirection.c_str(),
      // pattr->mSemantic.c_str() );
      glBindAttribLocation(prgo, iloc, pattr->mName.c_str());
      GL_ERRORCHECK();
      pass->_vtxAttributeById[iloc]                    = pattr;
      pass->_vtxAttributesBySemantic[pattr->mSemantic] = pattr;
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
      if (pvtxshader)
        pvtxshader->dumpFinalText();
      if (pfrgshader)
        pfrgshader->dumpFinalText();
      char infoLog[1 << 16];
      glGetProgramInfoLog(prgo, sizeof(infoLog), NULL, infoLog);
      printf("\n\n//////////////////////////////////\n");
      printf("program VTG InfoLog<%s>\n", infoLog);
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
      GLenum atr_apitype  = GL_ZERO;
      GL_ERRORCHECK();
      glGetActiveAttrib(prgo, i, sizeof(nambuf), &namlen, &atrsiz, &atr_apitype, nambuf);
      OrkAssert(namlen < sizeof(nambuf));
      GL_ERRORCHECK();
      auto it = vtx_iface->_inputAttributes.find(nambuf);
      if (it != vtx_iface->_inputAttributes.end()) {
        orksl::Attribute* pattr = it->second;
        // printf( "qattr<%d> loc<%d> name<%s>\n", i, pattr->mLocation, nambuf
        // );
        pattr->_apitype.Set<GLenum>(atr_apitype);
        switch(atr_apitype){
          case GL_FLOAT:
            pattr->_type = orksl::EAttributeType::Float;
            break;
          case GL_FLOAT_VEC2:
            pattr->_type = orksl::EAttributeType::Vec2f;
            break;
          case GL_FLOAT_VEC3:
            pattr->_type = orksl::EAttributeType::Vec3f;
            break;
          case GL_FLOAT_VEC4:
            pattr->_type = orksl::EAttributeType::Vec4f;
            break;
          case GL_INT_VEC2:
            pattr->_type = orksl::EAttributeType::Vec2i;
            break;
          case GL_INT_VEC3:
            pattr->_type = orksl::EAttributeType::Vec3i;
            break;
          case GL_INT_VEC4:
            pattr->_type = orksl::EAttributeType::Vec4i;
            break;
          case GL_UNSIGNED_INT_VEC2:
            pattr->_type = orksl::EAttributeType::Vec2u;
            break;
          case GL_UNSIGNED_INT_VEC3:
            pattr->_type = orksl::EAttributeType::Vec3u;
            break;
          case GL_UNSIGNED_INT_VEC4:
            pattr->_type = orksl::EAttributeType::Vec4u;
            break;
          default:
            OrkAssert(false);
        }
        // pattr->mLocation = i;
      } else {
        // check for builtin attr
        bool ok = nambuf[0] == 'g';
        ok &= nambuf[1] == 'l';
        ok &= nambuf[2] == '_';
        OrkAssert(ok);
      }
    }

    //////////////////////////
    pass->postProc(container);
    //////////////////////////
  }
  return OK;
}

////////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_NVMESH_SHADERS)
bool Interface::compilePipelineNVTM(orksl::rootcontainer_ptr_t container) {
  auto pass           = const_cast<orksl::Pass*>(container->_activePass);
  auto& pipeNVTM      = pass->_primpipe.Get<orksl::PrimPipelineNVTM>();
  orksl::Shader* ptaskshader = pipeNVTM._nvTaskShader;
  orksl::Shader* pmeshhader  = pipeNVTM._nvMeshShader;
  orksl::Shader* pfragshader = pipeNVTM._fragmentShader;
  OrkAssert(pmeshhader != nullptr);
  OrkAssert(pfragshader != nullptr);
  auto l_compile = [&](orksl::Shader* psh) -> bool {
    bool compile_ok = true;
    if (psh && psh->IsCompiled() == false)
      compile_ok = psh->Compile();
    if (false == compile_ok) {
      container->mShaderCompileFailed = true;
    }
    return compile_ok;
  };
  bool OK = true;
  OK &= l_compile(ptaskshader);
  OK &= l_compile(pmeshhader);
  OK &= l_compile(pfragshader);
  container->mShaderCompileFailed = (false == OK);
  if (OK) {
    GL_ERRORCHECK();
    GLuint prgo            = glCreateProgram();
    pass->_programObjectId = prgo;
    //////////////
    // attach shaders
    //////////////
    if (ptaskshader && ptaskshader->IsCompiled()) {
      glAttachShader(prgo, ptaskshader->mShaderObjectId);
      GL_ERRORCHECK();
    }
    glAttachShader(prgo, pmeshhader->mShaderObjectId);
    GL_ERRORCHECK();
    glAttachShader(prgo, pfragshader->mShaderObjectId);
    GL_ERRORCHECK();
    //////////////////////////
    // link
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
      printf("program NVTM InfoLog<%s>\n", infoLog);
      printf("//////////////////////////////////\n\n");
      OrkAssert(false);
    }
    OrkAssert(linkstat == GL_TRUE);
    //////////////////////////
    // post process pass
    //////////////////////////
    pass->postProc(container);
    //////////////////////////
  }
  return OK;
}
#endif
///////////////////////////////////////////////////////////////////////////////

bool Interface::compileAndLink(orksl::rootcontainer_ptr_t container) {
  bool OK   = false;
  auto pass = const_cast<orksl::Pass*>(container->_activePass);
  if (pass->_primpipe.IsA<orksl::PrimPipelineVTG>())
    OK = compilePipelineVTG(container);
#if defined(ENABLE_NVMESH_SHADERS)
  else if (pass->_primpipe.IsA<orksl::PrimPipelineNVTM>())
    OK = compilePipelineNVTM(container);
#endif
  return OK;
}

} // namespace ork::lev2::glslfx
