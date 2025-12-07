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

static logchannel_ptr_t logchan_vkcomp = logger()->configureChannel("VKCOMP", fvec3(0.2, 1, 0.8), false);

///////////////////////////////////////////////////////////////////////////////
// VkComputePipelineObject implementation
///////////////////////////////////////////////////////////////////////////////

VkComputePipelineObject::VkComputePipelineObject(vkcontext_rawptr_t ctx)
    : _contextVK(ctx) {
}

///////////////////////////////////////////////////////////////////////////////

VkComputePipelineObject::~VkComputePipelineObject() {
  if (_contextVK && _contextVK->_vkdevice) {
    if (_pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(_contextVK->_vkdevice, _pipeline, nullptr);
    }
    if (_pipelineLayout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(_contextVK->_vkdevice, _pipelineLayout, nullptr);
    }
    if (_descriptorSetLayout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(_contextVK->_vkdevice, _descriptorSetLayout, nullptr);
    }
    if (_descriptorPool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(_contextVK->_vkdevice, _descriptorPool, nullptr);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkComputePipelineObject::bindStorageBuffer(uint32_t binding_index, VkBuffer buffer, VkDeviceSize size) {
  StorageBufferBinding binding;
  binding.buffer = buffer;
  binding.offset = 0;
  binding.size = size;
  _ssbo_bindings[binding_index] = binding;
  _descriptors_dirty = true;
}

///////////////////////////////////////////////////////////////////////////////

void VkComputePipelineObject::updateDescriptorSet() {
  if (!_descriptors_dirty || _ssbo_bindings.empty()) {
    return;
  }

  std::vector<VkWriteDescriptorSet> writes;
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(_ssbo_bindings.size());

  for (auto& [binding_id, binding] : _ssbo_bindings) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = binding.buffer;
    bufferInfo.offset = binding.offset;
    bufferInfo.range = binding.size;
    bufferInfos.push_back(bufferInfo);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = _descriptorSet;
    write.dstBinding = binding_id;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfos.back();
    writes.push_back(write);
  }

  vkUpdateDescriptorSets(_contextVK->_vkdevice, writes.size(), writes.data(), 0, nullptr);
  _descriptors_dirty = false;
}

///////////////////////////////////////////////////////////////////////////////

bool VkComputePipelineObject::createPipeline(vkfxsobj_ptr_t computeShader) {
  if (!computeShader) {
    logchan_vkcomp->log("createPipeline: null compute shader");
    return false;
  }

  _computeShader = computeShader;
  _name = computeShader->_name;

  VkDevice device = _contextVK->_vkdevice;

  //////////////////////////////////////////////////////////
  // Collect SSBO bindings from the compute shader
  //////////////////////////////////////////////////////////
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings;

  logchan_vkcomp->log("createPipeline: _ssbo_refs=%p", computeShader->_ssbo_refs.get());
  if (computeShader->_ssbo_refs) {
    logchan_vkcomp->log("createPipeline: _ssbo_refs has %zu blocks", computeShader->_ssbo_refs->_ssbo_blocks.size());
    for (const auto& [name, ssbo] : computeShader->_ssbo_refs->_ssbo_blocks) {
      VkDescriptorSetLayoutBinding binding{};
      binding.binding = ssbo->_descriptor_set_id;  // Use descriptor_set_id as binding index
      binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      binding.descriptorCount = 1;
      binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      binding.pImmutableSamplers = nullptr;
      layoutBindings.push_back(binding);

      logchan_vkcomp->log("createPipeline: SSBO<%s> at binding %u", name.c_str(), binding.binding);
    }
  }

  // Add UBO bindings if present
  if (computeShader->_uniblk_refs) {
    for (const auto& [name, ubo] : computeShader->_uniblk_refs->_uniblks) {
      VkDescriptorSetLayoutBinding binding{};
      binding.binding = ubo->_descriptor_set_id;  // Use descriptor_set_id as binding index
      binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      binding.descriptorCount = 1;
      binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      binding.pImmutableSamplers = nullptr;
      layoutBindings.push_back(binding);

      logchan_vkcomp->log("createPipeline: UBO<%s> at binding %u", name.c_str(), binding.binding);
    }
  }

  // Add sampler bindings if present
  if (computeShader->_smpset_refs) {
    for (const auto& [name, smpset] : computeShader->_smpset_refs->_smpsets) {
      for (const auto& [samp_name, sampler] : smpset->_samplers_by_name) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = smpset->_descriptor_set_id;  // Use descriptor_set_id as binding index
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        binding.pImmutableSamplers = nullptr;
        layoutBindings.push_back(binding);

        logchan_vkcomp->log("createPipeline: Sampler<%s> at binding %u", samp_name.c_str(), binding.binding);
      }
    }
  }

  //////////////////////////////////////////////////////////
  // Create descriptor set layout
  //////////////////////////////////////////////////////////
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
  layoutInfo.pBindings = layoutBindings.data();

  VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &_descriptorSetLayout);
  if (result != VK_SUCCESS) {
    logchan_vkcomp->log("createPipeline: failed to create descriptor set layout");
    return false;
  }

  //////////////////////////////////////////////////////////
  // Create pipeline layout
  //////////////////////////////////////////////////////////
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &_descriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;

  // TODO: Add push constant support for compute if needed

  result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &_pipelineLayout);
  if (result != VK_SUCCESS) {
    logchan_vkcomp->log("createPipeline: failed to create pipeline layout");
    return false;
  }

  //////////////////////////////////////////////////////////
  // Create compute pipeline
  //////////////////////////////////////////////////////////
  VkComputePipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.stage = computeShader->_shaderstageinfo;
  pipelineInfo.layout = _pipelineLayout;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
  pipelineInfo.basePipelineIndex = -1;

  result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline);
  if (result != VK_SUCCESS) {
    logchan_vkcomp->log("createPipeline: failed to create compute pipeline");
    return false;
  }

  //////////////////////////////////////////////////////////
  // Create descriptor pool
  //////////////////////////////////////////////////////////
  if (!layoutBindings.empty()) {
    // Count descriptor types
    uint32_t ssboCount = 0, uboCount = 0, samplerCount = 0;
    for (const auto& binding : layoutBindings) {
      if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
        ssboCount++;
      } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        uboCount++;
      } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        samplerCount++;
      }
    }

    std::vector<VkDescriptorPoolSize> poolSizes;
    if (ssboCount > 0) {
      poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ssboCount});
    }
    if (uboCount > 0) {
      poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uboCount});
    }
    if (samplerCount > 0) {
      poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, samplerCount});
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &_descriptorPool);
    if (result != VK_SUCCESS) {
      logchan_vkcomp->log("createPipeline: failed to create descriptor pool");
      return false;
    }

    //////////////////////////////////////////////////////////
    // Allocate descriptor set
    //////////////////////////////////////////////////////////
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &_descriptorSetLayout;

    result = vkAllocateDescriptorSets(device, &allocInfo, &_descriptorSet);
    if (result != VK_SUCCESS) {
      logchan_vkcomp->log("createPipeline: failed to allocate descriptor set");
      return false;
    }
  }

  logchan_vkcomp->log("createPipeline: successfully created compute pipeline<%s>", _name.c_str());
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// VkComputeInterface implementation
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

  if (!shader) {
    logchan_vkcomp->log("dispatchCompute: null shader");
    return;
  }

  // Get compute pipeline from shader
  auto vk_compute_pipeline = shader->_impl.tryAs<vkcompute_pipeline_ptr_t>();
  if (!vk_compute_pipeline) {
    logchan_vkcomp->log("dispatchCompute: shader has no compute pipeline");
    OrkAssert(false && "Compute shader has no VkComputePipelineObject");
    return;
  }

  auto pipeline = vk_compute_pipeline.value();
  if (!pipeline || pipeline->_pipeline == VK_NULL_HANDLE) {
    logchan_vkcomp->log("dispatchCompute: invalid pipeline");
    OrkAssert(false && "Invalid compute pipeline");
    return;
  }

  // Get command buffer
  auto& cmdbuf = _contextVK->primary_cb()->_vkcmdbuf;

  // Update descriptor set if dirty
  pipeline->updateDescriptorSet();

  // Bind compute pipeline
  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->_pipeline);

  // Bind descriptor sets
  if (pipeline->_descriptorSet != VK_NULL_HANDLE) {
    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->_pipelineLayout,
        0,  // first set
        1,  // set count
        &pipeline->_descriptorSet,
        0,      // dynamic offset count
        nullptr // dynamic offsets
    );
  }

  // Dispatch compute work
  vkCmdDispatch(cmdbuf, numgroups_x, numgroups_y, numgroups_z);

  // Insert memory barrier: compute shader write -> vertex shader read (or other)
  VkMemoryBarrier memoryBarrier{};
  memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

  vkCmdPipelineBarrier(
      cmdbuf,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
      0,
      1, &memoryBarrier,
      0, nullptr,
      0, nullptr
  );

  logchan_vkcomp->log("dispatchCompute: dispatched %u x %u x %u work groups", numgroups_x, numgroups_y, numgroups_z);
}

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::dispatchComputeIndirect(const FxComputeShader* shader, int32_t* indirect) {
  // TODO: Implement indirect dispatch
  logchan_vkcomp->log("dispatchComputeIndirect: not implemented");
}

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::bindStorageBuffer(
    const FxComputeShader* shader,
    uint32_t binding_index,
    FxShaderStorageBuffer* buffer) {

  if (!shader || !buffer) {
    logchan_vkcomp->log("bindStorageBuffer: null shader or buffer");
    return;
  }

  auto vk_compute_pipeline = shader->_impl.tryAs<vkcompute_pipeline_ptr_t>();
  if (!vk_compute_pipeline) {
    logchan_vkcomp->log("bindStorageBuffer: shader has no compute pipeline");
    return;
  }

  auto pipeline = vk_compute_pipeline.value();

  // Get VkBuffer from FxShaderStorageBuffer
  auto vkbuf = buffer->_impl.getShared<VulkanBuffer>();
  if (!vkbuf) {
    logchan_vkcomp->log("bindStorageBuffer: buffer has no VulkanBuffer impl");
    return;
  }

  pipeline->bindStorageBuffer(binding_index, vkbuf->_vkbuffer, buffer->_length);

  logchan_vkcomp->log("bindStorageBuffer: bound buffer at binding %u, size %zu", binding_index, buffer->_length);
}

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_PYTORCH)

FxShaderStorageBuffer* VkComputeInterface::storageBufferFromTensor(torchtensor_ptr_t tensor) {
  // TODO: Implement tensor-to-SSBO
  logchan_vkcomp->log("storageBufferFromTensor: not implemented");
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::copyTensorIntoStorageBuffer(
    FxShaderStorageBuffer* ssbo,
    torchtensor_ptr_t tensor,
    size_t dest_offset) {
  // TODO: Implement tensor copy to SSBO
  logchan_vkcomp->log("copyTensorIntoStorageBuffer: not implemented");
  OrkAssert(false);
}

#endif

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::bindImage(
    const FxComputeShader* shader,
    uint32_t binding_index,
    Texture* tex,
    ImageBindAccess access) {
  // TODO: Implement image binding for compute
  logchan_vkcomp->log("bindImage: not implemented");
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
