#pragma once

namespace ork::lev2 {

class VertexBufferBase;

enum ImageBindAccess {
  EIBA_READ_ONLY = 0,
  EIBA_WRITE_ONLY = 1,
  EIBA_READ_WRITE = 2
};

struct ComputeInterface {

  ComputeInterface() {}
  virtual ~ComputeInterface() {}

  // Dispatch phase management - suspends render pass if active, resumes on end
  virtual void beginDispatchPhase() {}
  virtual void endDispatchPhase() {}

  virtual void dispatchCompute( const FxComputeShader* shader,
                                uint32_t numgroups_x,
                                uint32_t numgroups_y,
                                uint32_t numgroups_z ) {}

                               
  virtual void dispatchComputeIndirect(const FxComputeShader* shader, int32_t* indirect) {}
  
  virtual void bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) {}

  // Insert a shader storage (SSBO) memory barrier within an active dispatch phase.
  // Ensures all SSBO writes from prior dispatches are visible to subsequent dispatches
  // in the same command buffer. Must be called between dependent compute passes.
  virtual void storageBarrier() {}

  // GPU buffer-to-buffer copy within a dispatch phase.
  // Copies `size` bytes from src SSBO at `src_offset` to dst SSBO at `dst_offset`.
  virtual void copyBufferRegion(
      FxShaderStorageBuffer* src, size_t src_offset,
      FxShaderStorageBuffer* dst, size_t dst_offset,
      size_t size) {}

  // GPU copy from SSBO to vertex buffer within a dispatch phase.
  virtual void copySSBOToVertexBuffer(
      FxShaderStorageBuffer* src, size_t src_offset,
      VertexBufferBase* dst_vb, size_t dst_offset,
      size_t size) {}
  #if defined(ENABLE_PYTORCH)
  virtual void copyTensorIntoStorageBuffer(FxShaderStorageBuffer* ssbo, torchtensor_ptr_t tensor, size_t dest_offset) { }
  virtual FxShaderStorageBuffer* storageBufferFromTensor(torchtensor_ptr_t tensor) { return nullptr; }
  #endif


  virtual void bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) {}

  virtual void bindSampler(const FxComputeShader* shader, uint32_t binding_index, Texture* tex) {}

};

} //namespace ork::lev2 {
