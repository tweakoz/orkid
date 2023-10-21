///////////////////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
///////////////////////////////////////////////////////////////////////////////

#include "gl.h"
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/pch.h>
#include <ork/kernel/datacache.h>

namespace ork::lev2::glslfx {
///////////////////////////////////////////////////////////////////////////////

ComputeShader::ComputeShader(const std::string& nam)
#if defined(ENABLE_COMPUTE_SHADERSS)
  : Shader(nam, GL_COMPUTE_SHADER)
#else
  : Shader(nam, 0)
#endif
{
}

///////////////////////////////////////////////////////////////////////////////

ComputeInterface::ComputeInterface(ContextGL& glctx)
    : _targetGL(glctx) {
  _fxi = dynamic_cast<Interface*>(glctx.FXI());
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::dispatchCompute(
    const FxComputeShader* shader,
    uint32_t numgroups_x,
    uint32_t numgroups_y,
    uint32_t numgroups_z) {

  auto csh = shader->_impl.get<ComputeShader*>();
  assert(csh);
  bindComputeShader(csh);
  GL_ERRORCHECK();
#if defined(ENABLE_COMPUTE_SHADERSS)
  glDispatchCompute(numgroups_x, numgroups_y, numgroups_z);
#endif
  GL_ERRORCHECK();
  bindComputeShader(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::dispatchComputeIndirect(const FxComputeShader* shader, int32_t* indirect) {
  auto csh = shader->_impl.get<ComputeShader*>();
  assert(csh);
  bindComputeShader(csh);
  GL_ERRORCHECK();
#if defined(ENABLE_COMPUTE_SHADERSS)
  glDispatchComputeIndirect((GLintptr)indirect);
#endif
  GL_ERRORCHECK();
  bindComputeShader(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) {
  auto csh = shader->_impl.get<ComputeShader*>();
  assert(csh);
  bindComputeShader(csh);
  assert(buffer != nullptr);
  auto bufferimpl = buffer->_impl.get<ShaderStorageBuffer*>();
  assert(bufferimpl != nullptr);
  GL_ERRORCHECK();
  GLuint unit = 0;
#if defined(ENABLE_COMPUTE_SHADERS)
  glShaderStorageBlockBinding(csh->_computePipe->_programObjectId, unit, binding_index);

  GLuint binding_point_index = 80;
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_index, bufferimpl->_glbufid);
#endif
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) {
  auto csh = shader->_impl.get<ComputeShader*>();
  assert(csh);
  bindComputeShader(csh);
  auto texobj = tex->_impl.get<gltexobj_ptr_t>();
  glActiveTexture(GL_TEXTURE0 + binding_index);
  glBindTexture(GL_TEXTURE_2D, texobj->mObject);
  GL_ERRORCHECK();
  GLenum glaccess;
  switch (access) {
    case EIBA_READ_ONLY:
      glaccess = GL_READ_ONLY;
      break;
    case EIBA_WRITE_ONLY:
      glaccess = GL_WRITE_ONLY;
      break;
    case EIBA_READ_WRITE:
      glaccess = GL_READ_WRITE;
      break;
  }
#if defined(ENABLE_COMPUTE_SHADERS)
  glBindImageTexture(
      binding_index,
      texobj->mObject,
      0,         // miplevel
      GL_FALSE,  // layered ?
      0,         // layerid
      glaccess,  // access
      GL_R32UI); // format
#endif
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

PipelineCompute* ComputeInterface::createComputePipe(ComputeShader* csh) {
  Timer citimer;
  citimer.Start();

  auto pipe = new PipelineCompute;
  GL_ERRORCHECK();
  GLuint prgo            = glCreateProgram();
  csh->_computePipe = pipe;

  /////////////////////////////////////////
  // check if precompiled
  /////////////////////////////////////////
  auto pipeline_hasher = DataBlock::createHasher();
  pipeline_hasher->accumulateString("glfx_compute_pipeline"); // identifier
  pipeline_hasher->accumulateItem<float>(0.01);               // version code
  printf( "csh->mShaderText<%s>\n", csh->mShaderText.c_str());
  pipeline_hasher->accumulateString(csh->mShaderText);
  pipeline_hasher->finish();
  uint64_t pipeline_hash = pipeline_hasher->result();
  auto pipeline_datablock = DataBlockCache::findDataBlock(pipeline_hash);
  ////////////////////////////////////////////////////////////
  // cached?
  ////////////////////////////////////////////////////////////
  if (pipeline_datablock) { 
    chunkfile::DefaultLoadAllocator load_alloc;
    chunkfile::Reader chunkreader(pipeline_datablock, load_alloc);
    auto header_input_stream   = chunkreader.GetStream("header");
    auto shader_input_stream   = chunkreader.GetStream("shaders");
    OrkAssert(header_input_stream != nullptr);
    OrkAssert(shader_input_stream != nullptr);
    GLenum binary_format = header_input_stream->readItem<GLenum>();
    size_t binary_length = header_input_stream->readItem<size_t>();
    auto binary_data = shader_input_stream->GetDataAt(0);
    glProgramBinary(prgo, binary_format, binary_data, binary_length);
    GL_ERRORCHECK();
  }
  else{
    bool compileok = csh->Compile();
    assert(compileok);
    glAttachShader(prgo, csh->mShaderObjectId);
    GL_ERRORCHECK();
    glLinkProgram(prgo);
    GL_ERRORCHECK();
    GLint linkstat = 0;
    glGetProgramiv(prgo, GL_LINK_STATUS, &linkstat);
    if (linkstat != GL_TRUE) {
      if (csh)
        csh->dumpFinalText();
      char infoLog[1 << 16];
      glGetProgramInfoLog(prgo, sizeof(infoLog), NULL, infoLog);
      printf("\n\n//////////////////////////////////\n");
      printf("program COMPUTE InfoLog<%s>\n", infoLog);
      printf("//////////////////////////////////\n\n");
      OrkAssert(false);
    }
    ///////////////////////////////////
    // fetch shader binary
    ///////////////////////////////////

    #if !defined(__APPLE__)

    chunkfile::Writer chunkwriter("xfx-glci");
    auto header_stream   = chunkwriter.AddStream("header");
    auto shader_stream   = chunkwriter.AddStream("shaders");

    GLint binaryLength = 0;
    glGetProgramiv(prgo, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
    std::vector<GLubyte> binary_bytes;
    binary_bytes.resize(binaryLength);
    GLenum binaryFormat;
    glGetProgramBinary(prgo, binaryLength, NULL, &binaryFormat, binary_bytes.data());
    header_stream->AddItem<GLenum>(binaryFormat);
    header_stream->AddItem<size_t>(binary_bytes.size());
    shader_stream->AddData(binary_bytes.data(),binary_bytes.size());

    ///////////////////////////////////
    // write to datablock cache
    ///////////////////////////////////

    printf( "WRITING COMPUTE SHADER hash<%016zx> TO CACHE\n", pipeline_hash );

    pipeline_datablock = std::make_shared<DataBlock>();
    chunkwriter.writeToDataBlock(pipeline_datablock);
    DataBlockCache::setDataBlock(pipeline_hash, pipeline_datablock);

    #endif
  }
  pipe->_programObjectId = prgo;
  double citime      = citimer.SecsSinceStart();
  printf("ComputeInterface::createComputePipe<%p> took<%g>\n", (void*)csh, citime);
  return pipe;
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::bindComputeShader(ComputeShader* csh) {
  if (nullptr == csh) {
    glUseProgram(0);
    _currentComputePipeline = nullptr;
    return;
  }
  if (nullptr == csh->_computePipe) {
    csh->_computePipe = createComputePipe(csh);
  }
  assert(csh->_computePipe != nullptr);
  GL_ERRORCHECK();
  glUseProgram(csh->_computePipe->_programObjectId);
  GL_ERRORCHECK();
  _currentComputePipeline = csh->_computePipe;
}

///////////////////////////////////////////////////////////////////////////////

FxShaderStorageBuffer* ComputeInterface::createStorageBuffer(size_t length) {
  auto ssb    = new ShaderStorageBuffer;
  ssb->_fxssb = new FxShaderStorageBuffer;
  ssb->_fxssb->_impl.set<ShaderStorageBuffer*>(ssb);
  ssb->_length         = length;
  ssb->_fxssb->_length = length;
  GL_ERRORCHECK();
  glGenBuffers(1, &ssb->_glbufid);
  printf("Create SSBO<%p> glid<%d>\n", (void*)ssb, ssb->_glbufid);
#if defined(ENABLE_COMPUTE_SHADERS)
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb->_glbufid);
  auto mem = new char[length];
  for (int i = 0; i < length; i++)
    mem[i] = 0;
  glBufferData(GL_SHADER_STORAGE_BUFFER, length, mem, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  delete[] mem;
#endif
  GL_ERRORCHECK();
  return ssb->_fxssb;
}

///////////////////////////////////////////////////////////////////////////////

struct StorageBufferMapping {};

///////////////////////////////////////////////////////////////////////////////

storagebuffermappingptr_t ComputeInterface::mapStorageBuffer(FxShaderStorageBuffer* b, size_t base, size_t length) {
  auto mapping = std::make_shared<FxShaderStorageBufferMapping>();
  auto ssb     = b->_impl.get<ShaderStorageBuffer*>();
  if (length == 0) {
    assert(base == 0);
    length = b->_length;
  }

  mapping->_offset = base;
  mapping->_length = length;
  mapping->_ci     = this;
  mapping->_buffer = b;
  mapping->_impl.make<StorageBufferMapping>();
  GL_ERRORCHECK();
#if defined(ENABLE_COMPUTE_SHADERS)
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb->_glbufid);
  // mapping->_mappedaddr = malloc(length);
  // glMapBuffer(GL_SHADER_STORAGE_BUFFER,
  //                                      GL_WRITE_ONLY);
  mapping->_mappedaddr = glMapBufferRange(
      GL_SHADER_STORAGE_BUFFER,
      base,
      length,
      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT |
          // GL_MAP_FLUSH_EXPLICIT_BIT |
          0);
  assert(mapping->_mappedaddr != nullptr);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#endif
  GL_ERRORCHECK();
  return mapping;
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::unmapStorageBuffer(FxShaderStorageBufferMapping* mapping) {
  assert(mapping->_impl.isA<StorageBufferMapping>());
  auto ssb = mapping->_buffer->_impl.get<ShaderStorageBuffer*>();
  GL_ERRORCHECK();
#if defined(ENABLE_COMPUTE_SHADERS)
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb->_glbufid);
  // glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER,mapping->_offset,mapping->_length);
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#endif
  GL_ERRORCHECK();
  mapping->_impl.make<void*>(nullptr);
  mapping->_mappedaddr = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
