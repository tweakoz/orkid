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

bool Shader::Compile() {
  GL_NF_ERRORCHECK();
  mShaderObjectId = glCreateShader(mShaderType);

  std::string shadertext = "";

  shadertext += mShaderText;

  shadertext += "void main() { ";
  shadertext += mName;
  shadertext += "(); }\n";
  const char *c_str = shadertext.c_str();

  printf( "Shader<%s>\n/////////////\n%s\n///////////////////\n",
  mName.c_str(), c_str );

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

bool Interface::compileAndLink(Container* container) {
  auto pass = const_cast<Pass*>(container->mActivePass);

  auto &pipeVTG = pass->_primpipe.Get<PrimPipelineVTG>();

  Shader *pvtxshader = pipeVTG._vertexShader;
  Shader *ptecshader = pipeVTG._tessCtrlShader;
  Shader *pteeshader = pipeVTG._tessEvalShader;
  Shader *pgeoshader = pipeVTG._geometryShader;
  Shader *pfrgshader = pipeVTG._fragmentShader;

  OrkAssert(pvtxshader != nullptr);
  OrkAssert(pfrgshader != nullptr);

  auto l_compile = [&](Shader *psh) ->bool {
    bool compile_ok = true;
    if (psh && psh->IsCompiled() == false)
      compile_ok = psh->Compile();

    if (false == compile_ok) {
      container->mShaderCompileFailed = true;
    }
    return compile_ok;
  };

  bool vok = l_compile(pvtxshader);
  bool tcok = l_compile(ptecshader);
  bool teok = l_compile(pteeshader);
  bool gok = l_compile(pgeoshader);
  bool fok = l_compile(pfrgshader);

 if( false == vok )
  printf( "vsh<%s> compile failed\n", pvtxshader->mName.c_str() );
 if( false == tcok )
  printf( "tcsh<%s> compile failed\n", ptecshader->mName.c_str() );
 if( false == teok )
  printf( "tesh<%s> compile failed\n", pteeshader->mName.c_str() );
 if( false == gok )
  printf( "gsh<%s> compile failed\n", pgeoshader->mName.c_str() );
 if( false == fok )
  printf( "fsh<%s> compile failed\n", pfrgshader->mName.c_str() );


  if (container->mShaderCompileFailed)
    return false;

  if (pvtxshader->IsCompiled() && pfrgshader->IsCompiled()) {
    GL_ERRORCHECK();
    GLuint prgo = glCreateProgram();
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

    StreamInterface *vtx_iface = pvtxshader->mpInterface;
    StreamInterface *frg_iface = pfrgshader->mpInterface;

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

    for (const auto &itp : vtx_iface->mAttributes) {
      Attribute *pattr = itp.second;
      int iloc = pattr->mLocation;
      // printf( "	vtxattr<%s> loc<%d> dir<%s> sem<%s>\n",
      // pattr->mName.c_str(), iloc, pattr->mDirection.c_str(),
      // pattr->mSemantic.c_str() );
      glBindAttribLocation(prgo, iloc, pattr->mName.c_str());
      GL_ERRORCHECK();
      pass->_vtxAttributeById[iloc] = pattr;
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
      GLint atrsiz = 0;
      GLenum atrtyp = GL_ZERO;
      GL_ERRORCHECK();
      glGetActiveAttrib(prgo, i, sizeof(nambuf), &namlen, &atrsiz, &atrtyp,
                        nambuf);
      OrkAssert(namlen < sizeof(nambuf));
      GL_ERRORCHECK();
      const auto &it = vtx_iface->mAttributes.find(nambuf);
      OrkAssert(it != vtx_iface->mAttributes.end());
      Attribute *pattr = it->second;
      // printf( "qattr<%d> loc<%d> name<%s>\n", i, pattr->mLocation, nambuf
      // );
      pattr->meType = atrtyp;
      // pattr->mLocation = i;
    }

    //////////////////////////

    std::map<std::string,Uniform*> flatunimap;

    for( auto u : container->_uniforms ){
      flatunimap[u.first] = u.second;
    }
    for( auto b : container->_uniformBlocks ){
      UniformBlock* block = b.second;
      for( auto s : block->_subuniforms ){
        auto it = flatunimap.find(s.first);
        assert(it==flatunimap.end());
        flatunimap[s.first]=s.second;
      }
      auto binding = pass->uniformBlockBinding(b.first);
    }
    if( pass->_uboBindingMap.size() ){
    }
    //////////////////////////
    // query unis
    //////////////////////////

    GLint numunis = 0;
    GL_ERRORCHECK();
    glGetProgramiv(prgo, GL_ACTIVE_UNIFORMS, &numunis);
    GL_ERRORCHECK();

    pass->_samplerCount = 0;

    for (int i = 0; i < numunis; i++) {
      GLsizei namlen = 0;
      GLint unisiz = 0;
      GLenum unityp = GL_ZERO;
      std::string str_name;

      {
        GLchar nambuf[256];
        glGetActiveUniform(prgo, i, sizeof(nambuf), &namlen, &unisiz, &unityp,
                           nambuf);
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
      auto it = container->_uniforms.find(str_name);
      if( it != container->_uniforms.end() ){

          Uniform *puni = it->second;

          puni->_type = unityp;

          UniformInstance *pinst = new UniformInstance;
          pinst->mpUniform = puni;

          GLint uniloc = glGetUniformLocation(prgo, str_name.c_str());
          pinst->mLocation = uniloc;

          if (puni->_typeName == "sampler2D") {
            pinst->mSubItemIndex = pass->_samplerCount;
            pass->_samplerCount++;
            pinst->mPrivData.Set<GLenum>(GL_TEXTURE_2D);
          } else if (puni->_typeName == "sampler3D") {
            pinst->mSubItemIndex = pass->_samplerCount;
            pass->_samplerCount++;
            pinst->mPrivData.Set<GLenum>(GL_TEXTURE_3D);
          } else if (puni->_typeName == "sampler2DShadow") {
            pinst->mSubItemIndex = pass->_samplerCount;
            pass->_samplerCount++;
            pinst->mPrivData.Set<GLenum>(GL_TEXTURE_2D);
          }

          const char *fshnam = pfrgshader->mName.c_str();

          // printf("fshnam<%s> uninam<%s> loc<%d>\n", fshnam, str_name.c_str(),
          // (int) uniloc );

          pass->_uniformInstances[puni->_name] = pinst;
      }
      else {
        it = flatunimap.find(str_name);
        assert(it!=flatunimap.end());
        // prob a UBO uni
      }
    }
  }
  return true;
}

} // namespace ork::lev2 {
