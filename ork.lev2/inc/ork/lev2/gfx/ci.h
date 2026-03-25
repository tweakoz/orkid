#pragma once

namespace ork::lev2 {

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
  #if defined(ENABLE_PYTORCH)
  virtual void copyTensorIntoStorageBuffer(FxShaderStorageBuffer* ssbo, torchtensor_ptr_t tensor, size_t dest_offset) { }
  virtual FxShaderStorageBuffer* storageBufferFromTensor(torchtensor_ptr_t tensor) { return nullptr; }
  #endif


  virtual void bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) {}

  virtual void bindSampler(const FxComputeShader* shader, uint32_t binding_index, Texture* tex) {}

};

} //namespace ork::lev2 {
