////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkComputeInterface::VkComputeInterface(vkcontext_rawptr_t ctx)
    : ComputeInterface()
    , _contextVK(ctx) {

}

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::dispatchCompute(
    const FxComputeShader* shader,
    uint32_t numgroups_x,
    uint32_t numgroups_y,
    uint32_t numgroups_z) {
      OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::dispatchComputeIndirect(const FxComputeShader* shader, int32_t* indirect) {
}

///////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_SSBO)
FxShaderStorageBuffer* VkComputeInterface::createStorageBuffer(size_t length) {
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

storagebuffermappingptr_t VkComputeInterface::mapStorageBuffer(FxShaderStorageBuffer* b, size_t base, size_t length) {
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::unmapStorageBuffer(FxShaderStorageBufferMapping* mapping) {
}

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) {
}

void VkComputeInterface::copyBufferIntoStorageBuffer(FxShaderStorageBuffer* ssbo, std::vector<uint8_t> data, size_t dest_offset) { 
  OrkAssert(false);
}

#if defined(ENABLE_PYTORCH)

FxShaderStorageBuffer* VkComputeInterface::storageBufferFromTensor(torchtensor_ptr_t tensor) {
  OrkAssert(false);
  return nullptr;
}
void VkComputeInterface::copyTensorIntoStorageBuffer(FxShaderStorageBuffer* ssbo, torchtensor_ptr_t tensor, size_t dest_offset) {
  OrkAssert(false);

}

#endif

#endif

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) {
}

///////////////////////////////////////////////////////////////////////////////

//PipelineCompute* VkComputeInterface::createComputePipe(ComputeShader* csh) {
  //return nullptr;
//}

///////////////////////////////////////////////////////////////////////////////

//void VkComputeInterface::bindComputeShader(ComputeShader* csh) {
//}




///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
