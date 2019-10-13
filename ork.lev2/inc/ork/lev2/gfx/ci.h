#pragma once

#if defined (ENABLE_COMPUTE_SHADERS)

struct FxComputeShader;

struct ComputeInterface {

  ComputeInterface() {}
  virtual ~ComputeInterface() {}
  
  virtual void dispatchCompute( FxComputeShader* shader,
                                uint32_t numgroups_x,
                                uint32_t numgroups_y,
                                uint32_t numgroups_z ) {}

                               
  virtual void dispatchComputeIndirect(FxComputeShader* shader, int32_t* indirect) {}
  
};
#endif