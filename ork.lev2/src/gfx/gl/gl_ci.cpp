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
#include <ork/util/logger.h>

#if defined(ENABLE_PYTORCH) and defined(ENABLE_CUDA)

#undef ThreadLocal // conflicts with c10

#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
#include <torch/extension.h> // for PyTorch C++ extension

#endif

namespace ork::lev2::glslfx {

  static logchannel_ptr_t logchan_ci = logger()->configureChannel("GLCI", fvec3(0.8, 0.8, 0.3));

  ///////////////////////////////////////////////////////////////////////////////

ComputeInterface::ComputeInterface(ContextGL& glctx)
    : _targetGL(glctx) {
  _fxi = dynamic_cast<Interface*>(glctx.FXI());

  _stats_timer.Start();
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::dispatchCompute(
    const FxComputeShader* shader,
    uint32_t numgroups_x,
    uint32_t numgroups_y,
    uint32_t numgroups_z) {

  #if defined(ENABLE_COMPUTE_SHADERS)
  auto csh = shader->_impl.get<ComputeShader*>();
  assert(csh);
  bindComputeShader(csh);
  GL_ERRORCHECK();
  glDispatchCompute(numgroups_x, numgroups_y, numgroups_z);
  GL_ERRORCHECK();
  bindComputeShader(nullptr);
  #endif
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::dispatchComputeIndirect(const FxComputeShader* shader, int32_t* indirect) {
  #if defined(ENABLE_COMPUTE_SHADERS)
  auto csh = shader->_impl.get<ComputeShader*>();
  assert(csh);
  bindComputeShader(csh);
  GL_ERRORCHECK();
  glDispatchComputeIndirect((GLintptr)indirect);
  GL_ERRORCHECK();
  bindComputeShader(nullptr);
  #endif
}

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_SSBO)
void ComputeInterface::bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) {
  #if defined(ENABLE_COMPUTE_SHADERS)
  auto csh = shader->_impl.get<ComputeShader*>();
  assert(csh);
  bindComputeShader(csh);
  assert(buffer != nullptr);
  auto bufferimpl = buffer->_impl.get<ShaderStorageBuffer*>();
  assert(bufferimpl != nullptr);
  GL_ERRORCHECK();
  GLuint unit = 0;
  glShaderStorageBlockBinding(csh->_computePipe->_programObjectId, unit, binding_index);

  GLuint binding_point_index = 80;
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_index, bufferimpl->_glbufid);

  GL_ERRORCHECK();
  #endif
}
#endif
///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) {
  #if defined(ENABLE_COMPUTE_SHADERS)
  auto csh = shader->_impl.get<ComputeShader*>();
  assert(csh);
  bindComputeShader(csh);
  auto texobj = tex->_impl.get<gltexobj_ptr_t>();
  glActiveTexture(GL_TEXTURE0 + binding_index);
  glBindTexture(GL_TEXTURE_2D, texobj->_textureObject);
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
      texobj->_textureObject,
      0,         // miplevel
      GL_FALSE,  // layered ?
      0,         // layerid
      glaccess,  // access
      GL_R32UI); // format
  GL_ERRORCHECK();
  #endif
}

///////////////////////////////////////////////////////////////////////////////

PipelineCompute* ComputeInterface::createComputePipe(ComputeShader* csh) {
  Timer citimer;
  citimer.Start();

  auto pipe = new PipelineCompute;
  GL_ERRORCHECK();
  GLuint prgo            = glCreateProgram();
  csh->_computePipe = pipe;
  #if defined(ENABLE_COMPUTE_SHADERS)

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
    GLenum binary_format = header_input_stream->ReadItem<GLenum>();
    size_t binary_length = header_input_stream->ReadItem<size_t>();
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
  #endif
  double citime      = citimer.SecsSinceStart();
  printf("ComputeInterface::createComputePipe<%p> took<%g>\n", (void*)csh, citime);
  return pipe;
}

///////////////////////////////////////////////////////////////////////////////

void ComputeInterface::bindComputeShader(ComputeShader* csh) {
  #if defined(ENABLE_COMPUTE_SHADERS)
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
  #endif
}

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_SSBO)
FxShaderStorageBuffer* ComputeInterface::createStorageBuffer(size_t length) {
  auto ssb    = new ShaderStorageBuffer;
  ssb->_fxssb = new FxShaderStorageBuffer;
  ssb->_fxssb->_impl.set<ShaderStorageBuffer*>(ssb);
  ssb->_length         = length;
  ssb->_fxssb->_length = length;
  #if defined(ENABLE_COMPUTE_SHADERS)
  GL_ERRORCHECK();
  glGenBuffers(1, &ssb->_glbufid);
  printf("Create SSBO<%p> glid<%d>\n", (void*)ssb, ssb->_glbufid);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb->_glbufid);
  auto mem = new char[length];
  for (int i = 0; i < length; i++)
    mem[i] = 0;
  glBufferData(GL_SHADER_STORAGE_BUFFER, length, mem, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  delete[] mem;
  GL_ERRORCHECK();
  #endif
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
  GL_ERRORCHECK();
  #endif
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
  GL_ERRORCHECK();
  #endif
  mapping->_impl.make<void*>(nullptr);
  mapping->_mappedaddr = nullptr;
}
#endif
#if defined(ENABLE_PYTORCH) and defined(ENABLE_SSBO)
FxShaderStorageBuffer* ComputeInterface::storageBufferFromTensor(torchtensor_ptr_t l2tensor) {
  return nullptr;
}
#endif
#if defined(ENABLE_PYTORCH) and defined(ENABLE_SSBO) and ! defined(ENABLE_CUDA)
void ComputeInterface::copyTensorIntoStorageBuffer(
  FxShaderStorageBuffer* ssbo, 
  torchtensor_ptr_t l2tensor,
  size_t dest_offset ) {
}
#endif
///////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_PYTORCH) and defined(ENABLE_CUDA)


void ComputeInterface::copyTensorIntoStorageBuffer(
  FxShaderStorageBuffer* ssbo, 
  torchtensor_ptr_t l2tensor,
  size_t dest_offset ) {

  
  /////////////////////////////////////
  // use CUDA to copy tensor into SSBO
  /////////////////////////////////////

  auto as_tt = l2tensor->_impl.get<torch::Tensor>();
  auto ssb   = ssbo->_impl.get<ShaderStorageBuffer*>();
  size_t ssb_length = ssb->_length;
  size_t length = as_tt.numel() * as_tt.element_size();

  size_t required_length = length + dest_offset;
  //OrkAssert((dest_offset + length) <= dst_size);

  /////////////////////////////////////
  // 1. Check that 'tensor' is on CUDA
  /////////////////////////////////////

  TORCH_CHECK(as_tt.is_cuda(), "Tensor must be a CUDA tensor.");

  /////////////////////////////////////
  // check if we need to resize the SSBO
  /////////////////////////////////////

  bool buffer_needs_realloc = not ssb->_cudaimpl.isSet();
  buffer_needs_realloc |= (ssb_length < required_length);

  if (buffer_needs_realloc) {


    /////////////////////////////////////
    // resize the SSBO
    /////////////////////////////////////

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb->_glbufid);
    glBufferData(GL_SHADER_STORAGE_BUFFER, required_length, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    ssb->_length = required_length;

    /////////////////////////////////////
    // create CUDA resource
    //  and associate it with the SSBO
    /////////////////////////////////////

    cudaGraphicsResource* new_cuda_resource = nullptr;
    cudaGraphicsGLRegisterBuffer(&new_cuda_resource, ssb->_glbufid, cudaGraphicsRegisterFlagsNone);

    /////////////////////////////////////
    // store the CUDA resource in the SSBO impl
    /////////////////////////////////////

    ssb->_cudaimpl.set<cudaGraphicsResource*>(new_cuda_resource);
    printf("SSBO reallocated to required_length<%zu>\n", required_length);
    buffer_needs_realloc = false;

  }

  auto cudaResource = ssb->_cudaimpl.get<cudaGraphicsResource*>();

  /////////////////////////////////////
  // Map resources so we can obtain a CUDA device pointer to the SSBO
  //  do NOT render from GL whilst mapped
  /////////////////////////////////////

  cudaGraphicsMapResources( 1,             // count
                            &cudaResource, // resources (array)
                            0);            // synchronization stream
  void* dst_base = nullptr;
  size_t dst_size = 0;
  cudaGraphicsResourceGetMappedPointer(&dst_base, &dst_size, cudaResource);

  auto dst_ptr = ((char*) dst_base) + dest_offset;

  /////////////////////////////////////
  // At this point, dst_pointer is a CUDA-accessible pointer associated with the OpenGL SSBO
  // We can cudaMemcpy from the PyTorch pointer (ptr) into dst_pointer
  /////////////////////////////////////

  void* src_ptr = as_tt.data_ptr();      // pointer to tensor in CUDA memory (device)

  cudaMemcpy( dst_ptr,                   // destination pointer
              src_ptr,                   // source pointer
              length,                    // number of bytes to copy
              cudaMemcpyDeviceToDevice); // copy flags
  
  /////////////////////////////////////
  // unmap resource (release from CUDA)
  /////////////////////////////////////

  cudaGraphicsUnmapResources( 1,             // count
                              &cudaResource, // resources (array)
                              0);            // synchronization stream

  /////////////////////////////////////
  // track performance
  /////////////////////////////////////

  _ssbo_copy_byte_counter += length;
  _ssbo_copy_counter++;
  double elapsed = _stats_timer.SecsSinceStart();
  if (elapsed > 5.0) {
    double byte_rate = double(_ssbo_copy_byte_counter) / elapsed;
    double byte_rate_mbps = byte_rate / double(1<<20);
    double count = double(_ssbo_copy_counter) / elapsed;
    logchan_ci->log("SSBO copy count<%g> rate<%g MB/s>", count, byte_rate_mbps);
    _ssbo_copy_byte_counter = 0;
    _ssbo_copy_counter = 0;
    _stats_timer.Start();
  }
}
#endif // #if defined(ENABLE_PYTORCH) and defined(ENABLE_CUDA)
#if defined(ENABLE_SSBO)
void ComputeInterface::copyBufferIntoStorageBuffer(FxShaderStorageBuffer* ssbo, 
                                                   std::vector<uint8_t> data, 
                                                   size_t dest_offset) { 
  auto ssb   = ssbo->_impl.get<ShaderStorageBuffer*>();
  size_t ssb_length = ssb->_length;
  size_t xfer_length = data.size();
  size_t required_length = xfer_length + dest_offset;

  bool buffer_needs_realloc = required_length > ssb_length;

  if (buffer_needs_realloc) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb->_glbufid);
    glBufferData(GL_SHADER_STORAGE_BUFFER, required_length, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    ssb->_length = required_length;
    printf("SSBO reallocated to required_length<%zu>\n", required_length);
    buffer_needs_realloc = false;
  }
  
  auto mapping = mapStorageBuffer(ssbo, dest_offset, xfer_length);
  memcpy(mapping->_mappedaddr, data.data(), xfer_length);
  unmapStorageBuffer(mapping.get());

  /////////////////////////////////////
  // track performance
  /////////////////////////////////////

  _ssbo_copy_byte_counter += xfer_length;
  _ssbo_copy_counter++;
  double elapsed = _stats_timer.SecsSinceStart();
  if (elapsed > 5.0) {
    double byte_rate = double(_ssbo_copy_byte_counter) / elapsed;
    double byte_rate_mbps = byte_rate / double(1<<20);
    double count = double(_ssbo_copy_counter) / elapsed;
    logchan_ci->log("SSBO copy count<%g> rate<%g MB/s>", count, byte_rate_mbps);
    _ssbo_copy_byte_counter = 0;
    _ssbo_copy_counter = 0;
    _stats_timer.Start();
  }
}

#endif

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
