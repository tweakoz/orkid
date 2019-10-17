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

ComputeInterface::ComputeInterface(GfxTargetGL& glctx)
  : _targetGL(glctx)
  {
  _fxi = dynamic_cast<Interface*>(glctx.FXI());

}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::dispatchCompute(const FxComputeShader* shader,
                     uint32_t numgroups_x,
                     uint32_t numgroups_y,
                     uint32_t numgroups_z ){
    assert(_currentComputePipeline!=nullptr);
  
    GL_ERRORCHECK();
    glDispatchCompute(numgroups_x, numgroups_y, numgroups_z);
    GL_ERRORCHECK();

}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::dispatchComputeIndirect(const FxComputeShader* shader,int32_t* indirect) {
    assert(_currentComputePipeline!=nullptr);

    GL_ERRORCHECK();
    glDispatchComputeIndirect((GLintptr)indirect);
    GL_ERRORCHECK();

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

storagebuffermappingptr_t ComputeInterface::mapStorageBuffer(FxShaderStorageBuffer*b,size_t base, size_t length) {
  auto mapping = std::make_shared<FxShaderStorageBufferMapping>();
  auto ssb      = b->_impl.Get<ShaderStorageBuffer*>();
  if (length == 0) {
    assert(base == 0);
    length = b->_length;
  }

  mapping->_offset = base;
  mapping->_length = length;
  mapping->_ci = this;
  mapping->_buffer = b;
  mapping->_impl.Make<StorageBufferMapping>();
  GL_ERRORCHECK();
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb->_glbufid);
  // mapping->_mappedaddr = malloc(length);
  // glMapBuffer(GL_SHADER_STORAGE_BUFFER,
  //                                      GL_WRITE_ONLY);
  mapping->_mappedaddr = glMapBufferRange(
      GL_SHADER_STORAGE_BUFFER, base, length,
      GL_MAP_WRITE_BIT |
      GL_MAP_INVALIDATE_RANGE_BIT |
      //GL_MAP_FLUSH_EXPLICIT_BIT |
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
  //glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER,mapping->_offset,mapping->_length);
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  GL_ERRORCHECK();
  mapping->_impl.Make<void*>(nullptr);
  mapping->_mappedaddr = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::bindStorageBuffer(const FxShaderStorageBlock* block, FxShaderStorageBuffer* buffer) {
  //auto uniblock  = block->_impl.Get<UniformBlock*>();
  //auto unibuffer = buffer->_impl.Get<StorageBuffer*>();
  //assert(uniblock != nullptr);
  //assert(unibuffer != nullptr);
  //auto pass = mpActiveEffect->_activePass;
  //assert(pass != nullptr);
  //pass->bindUniformBlockBuffer(uniblock, unibuffer);
  assert(false);
}

///////////////////////////////////////////////////////////////////////////////

#endif

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
