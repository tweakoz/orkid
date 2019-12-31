///////////////////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
///////////////////////////////////////////////////////////////////////////////

#include "gl.h"
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/pch.h>

namespace ork::lev2::glslfx {
///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_COMPUTE_SHADERS)

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

  auto csh = shader->_impl.Get<ComputeShader*>();
  assert(csh);
  bindComputeShader(csh);
  GL_ERRORCHECK();
  glDispatchCompute(numgroups_x, numgroups_y, numgroups_z);
  GL_ERRORCHECK();
  bindComputeShader(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::dispatchComputeIndirect(const FxComputeShader* shader, int32_t* indirect) {
  auto csh = shader->_impl.Get<ComputeShader*>();
  assert(csh);
  bindComputeShader(csh);
  GL_ERRORCHECK();
  glDispatchComputeIndirect((GLintptr)indirect);
  GL_ERRORCHECK();
  bindComputeShader(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) {
  auto csh = shader->_impl.Get<ComputeShader*>();
  assert(csh);
  bindComputeShader(csh);
  assert(buffer != nullptr);
  auto bufferimpl = buffer->_impl.Get<ShaderStorageBuffer*>();
  assert(bufferimpl != nullptr);
  GL_ERRORCHECK();
  GLuint unit = 0;
  glShaderStorageBlockBinding(csh->_computePipe->_programObjectId, unit, binding_index);

  GLuint binding_point_index = 80;
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_index, bufferimpl->_glbufid);

  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) {
  auto csh = shader->_impl.Get<ComputeShader*>();
  assert(csh);
  bindComputeShader(csh);
  auto texobj = (GLTextureObject*)tex->GetTexIH();
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
  glBindImageTexture(
      binding_index,
      texobj->mObject,
      0,         // miplevel
      GL_FALSE,  // layered ?
      0,         // layerid
      glaccess,  // access
      GL_R32UI); // format
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

PipelineCompute* ComputeInterface::createComputePipe(ComputeShader* csh) {
  auto pipe = new PipelineCompute;
  GL_ERRORCHECK();
  bool compileok = csh->Compile();
  assert(compileok);
  GLuint prgo            = glCreateProgram();
  pipe->_programObjectId = prgo;
  glAttachShader(prgo, csh->mShaderObjectId);
  GL_ERRORCHECK();
  glLinkProgram(prgo);
  GL_ERRORCHECK();
  csh->_computePipe = pipe;
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
  ssb->_fxssb->_impl.Set<ShaderStorageBuffer*>(ssb);
  ssb->_length         = length;
  ssb->_fxssb->_length = length;
  GL_ERRORCHECK();
  glGenBuffers(1, &ssb->_glbufid);
  printf("Create SSBO<%p> glid<%d>\n", ssb, ssb->_glbufid);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb->_glbufid);
  auto mem = new char[length];
  for (int i = 0; i < length; i++)
    mem[i] = 0;
  glBufferData(GL_SHADER_STORAGE_BUFFER, length, mem, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  delete[] mem;
  GL_ERRORCHECK();
  return ssb->_fxssb;
}

///////////////////////////////////////////////////////////////////////////////

struct StorageBufferMapping {};

///////////////////////////////////////////////////////////////////////////////

storagebuffermappingptr_t ComputeInterface::mapStorageBuffer(FxShaderStorageBuffer* b, size_t base, size_t length) {
  auto mapping = std::make_shared<FxShaderStorageBufferMapping>();
  auto ssb     = b->_impl.Get<ShaderStorageBuffer*>();
  if (length == 0) {
    assert(base == 0);
    length = b->_length;
  }

  mapping->_offset = base;
  mapping->_length = length;
  mapping->_ci     = this;
  mapping->_buffer = b;
  mapping->_impl.Make<StorageBufferMapping>();
  GL_ERRORCHECK();
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
  GL_ERRORCHECK();
  return mapping;
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::unmapStorageBuffer(FxShaderStorageBufferMapping* mapping) {
  assert(mapping->_impl.IsA<StorageBufferMapping>());
  auto ssb = mapping->_buffer->_impl.Get<ShaderStorageBuffer*>();
  GL_ERRORCHECK();
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb->_glbufid);
  // glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER,mapping->_offset,mapping->_length);
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  GL_ERRORCHECK();
  mapping->_impl.Make<void*>(nullptr);
  mapping->_mappedaddr = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

#endif

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
