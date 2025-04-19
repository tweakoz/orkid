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
  
  virtual void dispatchCompute( const FxComputeShader* shader,
                                uint32_t numgroups_x,
                                uint32_t numgroups_y,
                                uint32_t numgroups_z ) {}

                               
  virtual void dispatchComputeIndirect(const FxComputeShader* shader, int32_t* indirect) {}
  
  #if defined(ENABLE_SSBO)
  virtual FxShaderStorageBuffer* createStorageBuffer(size_t length) { return nullptr; }
  virtual void copyBufferIntoStorageBuffer(FxShaderStorageBuffer* ssbo, std::vector<uint8_t> buffer, size_t dest_offset) { }
  virtual storagebuffermappingptr_t mapStorageBuffer(FxShaderStorageBuffer*b,size_t base=0, size_t length=0) { return nullptr; }
  virtual void unmapStorageBuffer(FxShaderStorageBufferMapping* mapping) {}
  virtual void bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) {}
  #if defined(ENABLE_PYTORCH)
  virtual void copyTensorIntoStorageBuffer(FxShaderStorageBuffer* ssbo, torchtensor_ptr_t tensor, size_t dest_offset) { }
  virtual FxShaderStorageBuffer* storageBufferFromTensor(torchtensor_ptr_t tensor) { return nullptr; }
  #endif
  #endif


  virtual void bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) {}

};

} //namespace ork::lev2 {
