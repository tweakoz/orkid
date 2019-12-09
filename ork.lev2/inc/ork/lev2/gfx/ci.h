#pragma once

#if defined (ENABLE_COMPUTE_SHADERS)

struct FxComputeShader;
struct FxShaderStorageBlock;
struct FxShaderStorageBuffer;
struct FxShaderStorageBufferMapping;
typedef std::shared_ptr<FxShaderStorageBufferMapping> storagebuffermappingptr_t;

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
  
  virtual FxShaderStorageBuffer* createStorageBuffer(size_t length) { return nullptr; }
  virtual storagebuffermappingptr_t mapStorageBuffer(FxShaderStorageBuffer*b,size_t base=0, size_t length=0) { return nullptr; }
  virtual void unmapStorageBuffer(FxShaderStorageBufferMapping* mapping) {}
  virtual void bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) {}
  virtual void bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) {}

};
#endif