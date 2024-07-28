////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include <ork/file/file.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/datacache.h>

constexpr bool _DEBUG_SHADER_COMPILE = false;

namespace ork::lev2::glslfx {
///////////////////////////////////////////////////////////////////////////////

void Shader::dumpFinalText() const {
  const char* c_str = _finalText.c_str();
  printf("//////////////////////////////////\n");
  printf("%s\n", c_str);
  printf("//////////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////

bool Shader::Compile() {
  GL_ERRORCHECK();

  Timer ctimer;
  ctimer.Start();

  mShaderObjectId = glCreateShader(mShaderType);

#if defined(ENABLE_COMPUTE_SHADERS)
  if (mShaderType == GL_COMPUTE_SHADER) {
    //printf("yo\n");
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
    printf("ShaderType<0x%x>\n", mShaderType);
    printf("Shader<%s> InfoLog<%s>\n", mName.c_str(), infoLog);
    printf("//////////////////////////////////\n");

    if (_rootcontainer->mFxShader->GetAllowCompileFailure() == false) {
      OrkAssert(false);
    }

    mbError = true;
    return false;
  }
  mbCompiled = true;

  if(_DEBUG_SHADER_COMPILE){
    double compile_time = ctimer.SecsSinceStart();
    printf( "SHADER<%s> COMPILE TIME<%f>\n", mName.c_str(), compile_time );
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////

bool Interface::compilePipelineVTG(rootcontainer_ptr_t container) {

  bool OK = true;
  GL_ERRORCHECK();
  GLuint prgo = glCreateProgram();

  auto pass = const_cast<Pass*>(container->_activePass);

  auto& pipeVTG = pass->_primpipe.get<PrimPipelineVTG>();

  Shader* pvtxshader = pipeVTG._vertexShader;
  Shader* ptecshader = pipeVTG._tessCtrlShader;
  Shader* pteeshader = pipeVTG._tessEvalShader;
  Shader* pgeoshader = pipeVTG._geometryShader;
  Shader* pfrgshader = pipeVTG._fragmentShader;
  StreamInterface* vtx_iface = nullptr;
  StreamInterface* frg_iface = nullptr;

  OrkAssert(pvtxshader != nullptr);
  OrkAssert(pfrgshader != nullptr);

  /////////////////////////////////////////
  // check if precompiled
  /////////////////////////////////////////
  auto pipeline_hasher = DataBlock::createHasher();
  pipeline_hasher->accumulateString("glfx_pipeline"); // identifier
  pipeline_hasher->accumulateItem<float>(1.01);         // version code

  ////////////////////////////////////////////////////////////////////////
  // generate HASH for pipeline
  ////////////////////////////////////////////////////////////////////////

  if(pvtxshader){
    vtx_iface = pvtxshader->_inputInterface;
    pipeline_hasher->accumulateString("vtx");
    //printf( "pvtxshader->mShaderText<%s>\n", pvtxshader->mShaderText.c_str() );
    pipeline_hasher->accumulateString(pvtxshader->mShaderText);
    for (const auto& itp : vtx_iface->_inputAttributes) {
      Attribute* pattr = itp.second;
      int iloc         = pattr->mLocation;
      pipeline_hasher->accumulateItem<int>(iloc);
      pipeline_hasher->accumulateString(pattr->mName);
      pipeline_hasher->accumulateString(pattr->mSemantic);
    }
  }
  if(ptecshader){
    pipeline_hasher->accumulateString("tec");
    pipeline_hasher->accumulateString(ptecshader->mShaderText);
  }
  if(pteeshader){
    pipeline_hasher->accumulateString("tee");
    pipeline_hasher->accumulateString(pteeshader->mShaderText);
  }
  if(pgeoshader){
    pipeline_hasher->accumulateString("geo");
    pipeline_hasher->accumulateString(pgeoshader->mShaderText);
  }
  if(pfrgshader){
    frg_iface = pfrgshader->_inputInterface;
    pipeline_hasher->accumulateString("frg");
    pipeline_hasher->accumulateString(pfrgshader->mShaderText);
  }
  
  pipeline_hasher->finish();
  uint64_t pipeline_hash = pipeline_hasher->result();

  auto pipeline_datablock = DataBlockCache::findDataBlock(pipeline_hash);
  if(true) { //_debugDrawCall){
    pipeline_datablock = nullptr;
  }
  ////////////////////////////////////////////////////////////
  // cached?
  ////////////////////////////////////////////////////////////
  if (pipeline_datablock) { 
    Timer load_timer;
    load_timer.Start();
    chunkfile::DefaultLoadAllocator load_alloc;
    chunkfile::Reader chunkreader(pipeline_datablock, load_alloc);
    auto header_input_stream   = chunkreader.GetStream("header");
    auto shader_input_stream   = chunkreader.GetStream("shaders");
    OrkAssert(header_input_stream != nullptr);
    OrkAssert(shader_input_stream != nullptr);
    auto str_attrs = header_input_stream->ReadIndexedString(chunkreader);
    OrkAssert(str_attrs == "begin-attributes");
    size_t num_attrs    = header_input_stream->ReadItem<size_t>();
    for( size_t iattr=0; iattr<num_attrs; iattr++ ){
      int iloc = header_input_stream->ReadItem<int>();
      auto attr_name = header_input_stream->ReadIndexedString(chunkreader);
      auto attr_sem  = header_input_stream->ReadIndexedString(chunkreader);
      auto it = vtx_iface->_inputAttributes.find(attr_name);
      OrkAssert(it != vtx_iface->_inputAttributes.end());
      Attribute* pattr = it->second;
      pattr->mLocation = iloc;
      pass->_vtxAttributeById[iloc]                    = pattr;
      pass->_vtxAttributesBySemantic[pattr->mSemantic] = pattr;
    }
    auto str_attrs_done = header_input_stream->ReadIndexedString(chunkreader);
    OrkAssert(str_attrs_done == "end-attributes");
    GLenum binary_format = header_input_stream->ReadItem<GLenum>();
    size_t binary_length = header_input_stream->ReadItem<size_t>();
    auto binary_data = shader_input_stream->GetDataAt(0);
    glProgramBinary(prgo, binary_format, binary_data, binary_length);
    GL_ERRORCHECK();
    double precompiled_load_time = load_timer.SecsSinceStart();
    //printf( "SHADERPROGRAM TOTAL PRECOMPILED LOADTIME<%f>\n", precompiled_load_time );
  }
  ////////////////////////////////////////////////////////////
  // not cached..
  ////////////////////////////////////////////////////////////
  else{ 
    /////////////////////////////////////////
    chunkfile::Writer chunkwriter("xfx-gl");
    auto header_stream   = chunkwriter.AddStream("header");
    auto shader_stream   = chunkwriter.AddStream("shaders");
    /////////////////////////////////////////
    Timer ctimer;
    ctimer.Start();

    auto l_compile = [&](Shader* psh) -> bool {
      bool compile_ok = true;
      if (psh && psh->IsCompiled() == false)
        compile_ok = psh->Compile();

      if (false == compile_ok) {
        container->mShaderCompileFailed = true;
        OrkAssert(false);
      }
      return compile_ok;
    };

    OK &= l_compile(pvtxshader);
    OK &= l_compile(ptecshader);
    OK &= l_compile(pteeshader);
    OK &= l_compile(pgeoshader);
    OK &= l_compile(pfrgshader);

    container->mShaderCompileFailed = (false == OK);

    if (OK) {

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

      if (pgeoshader) {
        OrkAssert(pgeoshader->IsCompiled());
        glAttachShader(prgo, pgeoshader->mShaderObjectId);
        GL_ERRORCHECK();
      }

      glAttachShader(prgo, pfrgshader->mShaderObjectId);
      GL_ERRORCHECK();

      //////////////
      // link
      //////////////


      if (_DEBUG_SHADER_COMPILE or _debugDrawCall) {
        auto tek = pass->_technique;

        printf( "link tek<%s> pass<%s> prgo<%d>\n",
                tek->_name.c_str(),
                pass->_name.c_str(),
                int(prgo));

        if (pvtxshader) {
          printf(
              "link tek<%s> pass<%s> vtxshader<%s> interface<%p> shobj<%d>\n",
              tek->_name.c_str(),
              pass->_name.c_str(),
              pvtxshader->mName.c_str(),
              (void*)vtx_iface,
              int(pvtxshader->mShaderObjectId));
        }
        if (pgeoshader) {
          StreamInterface* geo_iface = pgeoshader->_inputInterface;
          printf(
              "link tek<%s> pass<%s> geoshader<%s> interface<%p> shobj<%d>\n",
              tek->_name.c_str(),
              pass->_name.c_str(),
              pgeoshader->mName.c_str(),
              (void*)geo_iface,
              int(pgeoshader->mShaderObjectId));
        }
        if (pfrgshader) {

          printf(
              "link tek<%s> pass<%s> frgshader<%s> interface<%p> shobj<%d>\n",
              tek->_name.c_str(),
              pass->_name.c_str(),
              pfrgshader->mName.c_str(),
              (void*)frg_iface,
              int(pfrgshader->mShaderObjectId));
        }
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

      static int counter = 0;

      header_stream->AddIndexedString("begin-attributes",chunkwriter);
      header_stream->AddItem<size_t>(vtx_iface->_inputAttributes.size());

      for (const auto& itp : vtx_iface->_inputAttributes) {
        Attribute* pattr = itp.second;
        int iloc         = pattr->mLocation;
        header_stream->AddItem<int>(iloc);
        //printf( "	vtxattr<%s> loc<%d> dir<%s> sem<%s>\n",
        //pattr->mName.c_str(), iloc, pattr->mDirection.c_str(),
        //pattr->mSemantic.c_str() );
        glBindAttribLocation(prgo, iloc, pattr->mName.c_str());
        GL_ERRORCHECK();
        pass->_vtxAttributeById[iloc]                    = pattr;
        pass->_vtxAttributesBySemantic[pattr->mSemantic] = pattr;
        counter++;
        header_stream->AddIndexedString(pattr->mName,chunkwriter);
        header_stream->AddIndexedString(pattr->mSemantic,chunkwriter);
      }
      header_stream->AddIndexedString("end-attributes",chunkwriter);

      //////////////////////////

      bool dump_and_exit = false;
      //if(pass->_technique->_name=="tflatparticle_streaks_stereo")
        //dump_and_exit = true;

      double link_start_time = ctimer.SecsSinceStart();

      GL_ERRORCHECK();
      glLinkProgram(prgo);
      GL_ERRORCHECK();
      GLint linkstat = 0;
      glGetProgramiv(prgo, GL_LINK_STATUS, &linkstat);
      if (linkstat != GL_TRUE or dump_and_exit or _debugDrawCall) {
        if (pvtxshader)
          pvtxshader->dumpFinalText();
        if (ptecshader)
          ptecshader->dumpFinalText();
        if (pteeshader)
          pteeshader->dumpFinalText();
        if (pgeoshader)
          pgeoshader->dumpFinalText();
        if (pfrgshader)
          pfrgshader->dumpFinalText();
        char infoLog[1 << 16];
        glGetProgramInfoLog(prgo, sizeof(infoLog), NULL, infoLog);
        printf("\n\n//////////////////////////////////\n");
        printf("program VTG InfoLog<%s>\n", infoLog);
        printf("//////////////////////////////////\n\n");
        OrkAssert(not dump_and_exit);
      }
      OrkAssert(linkstat == GL_TRUE);
      double link_time = ctimer.SecsSinceStart()-link_start_time;
      //printf( "SHADER LINK TIME<%f>\n", link_time );

      // printf( "} // linking complete..\n" );


      // printf( "//////////////////////////////////\n");

      ///////////////////////////////////
      // fetch shader binary
      ///////////////////////////////////

      if(mTarget._SUPPORTS_BINARY_PIPELINE){
        GLint binaryLength = 0;
        GL_ERRORCHECK();
        glGetProgramiv(prgo, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
        GL_ERRORCHECK();
        std::vector<GLubyte> binary_bytes;
        binary_bytes.resize(binaryLength);
        GLenum binaryFormat;
        glGetProgramBinary(prgo, binaryLength, NULL, &binaryFormat, binary_bytes.data());
        header_stream->AddItem<GLenum>(binaryFormat);
        header_stream->AddItem<size_t>(binary_bytes.size());
        shader_stream->AddData(binary_bytes.data(),binary_bytes.size());
        GL_ERRORCHECK();

        ///////////////////////////////////
        // write to datablock cache
        ///////////////////////////////////

        printf( "WRITING SHADER hash<%016llx> TO CACHE\n", pipeline_hash );

        pipeline_datablock = std::make_shared<DataBlock>();
        chunkwriter.writeToDataBlock(pipeline_datablock);
        DataBlockCache::setDataBlock(pipeline_hash, pipeline_datablock);
      }

    }
    double compile_time = ctimer.SecsSinceStart();
    //printf( "SHADERPROGRAM TOTAL COMPILE/LINK TIME<%f>\n", compile_time );
  
  } // not cached

  if(OK){
    pass->_programObjectId = prgo;
  }

  ///////////////////////////////////////////////////////////////
  // post shader-compile/link (or post-load) operations
  //   query attr
  ///////////////////////////////////////////////////////////////
  Timer gatimer;
  gatimer.Start();

  GLint numattr = 0;
  GL_ERRORCHECK();
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
    auto it = vtx_iface->_inputAttributes.find(nambuf);
    if (it != vtx_iface->_inputAttributes.end()) {
      Attribute* pattr = it->second;
      pattr->meType = atrtyp;
    } else {
      // check for builtin attr
      bool ok = nambuf[0] == 'g';
      ok &= nambuf[1] == 'l';
      ok &= nambuf[2] == '_';
      OrkAssert(ok);
    }
  }
  double getattr_time = gatimer.SecsSinceStart();
  //printf( "getattr_time<%f>\n", getattr_time );

  //////////////////////////
  pass->postProc(container);
  //////////////////////////

  ///////////////////////////////////////////////////////////////
  return OK;
}

////////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_NVMESH_SHADERS)
bool Interface::compilePipelineNVTM(rootcontainer_ptr_t container) {
  auto pass           = const_cast<Pass*>(container->_activePass);
  auto& pipeNVTM      = pass->_primpipe.get<PrimPipelineNVTM>();
  Shader* ptaskshader = pipeNVTM._nvTaskShader;
  Shader* pmeshhader  = pipeNVTM._nvMeshShader;
  Shader* pfragshader = pipeNVTM._fragmentShader;
  OrkAssert(pmeshhader != nullptr);
  OrkAssert(pfragshader != nullptr);
  auto l_compile = [&](Shader* psh) -> bool {
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

bool Interface::compileAndLink(rootcontainer_ptr_t container) {
  bool OK   = false;
  auto pass = const_cast<Pass*>(container->_activePass);
  if (pass->_primpipe.isA<PrimPipelineVTG>())
    OK = compilePipelineVTG(container);
#if defined(ENABLE_NVMESH_SHADERS)
  else if (pass->_primpipe.isA<PrimPipelineNVTM>())
    OK = compilePipelineNVTM(container);
#endif
  return OK;
}

} // namespace ork::lev2::glslfx
