////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <chrono>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_vkcomp = logger()->configureChannel("VKCOMP", fvec3(0.2, 1, 0.8), false);

///////////////////////////////////////////////////////////////////////////////
// VkComputePipelineState implementation
///////////////////////////////////////////////////////////////////////////////

VkComputePipelineState::VkComputePipelineState(vkcontext_rawptr_t ctx)
    : _contextVK(ctx) {
}

///////////////////////////////////////////////////////////////////////////////

VkComputePipelineState::~VkComputePipelineState() {
  if (_contextVK)
    _contextVK->destroyComputePipelineState(_pipeline, _pipelineLayout, _descriptorSetLayout, _setPools);
}

///////////////////////////////////////////////////////////////////////////////

void VkComputePipelineState::bindStorageBuffer(uint32_t binding_index, VkBuffer buffer, VkDeviceSize size) {
  StorageBufferBinding binding;
  binding.buffer = buffer;
  binding.offset = 0;
  binding.size = size;
  _ssbo_bindings[binding_index] = binding;
  _descriptors_dirty = true;
}

///////////////////////////////////////////////////////////////////////////////

void VkComputePipelineState::bindSampler(uint32_t binding_index, VkDescriptorImageInfo desc_info) {
  _sampler_bindings[binding_index] = desc_info;
  _descriptors_dirty = true;
}

///////////////////////////////////////////////////////////////////////////////

// kSetsPerPool — descriptor sets allocated per pool chunk in the growable ring.
static constexpr uint32_t kSetsPerPool = 64;

void VkComputePipelineState::_growSetPool() {
  // append one more descriptor pool holding kSetsPerPool sets for this pipeline's layout.
  std::vector<VkDescriptorPoolSize> poolSizes;
  if (_ssboCount > 0)
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _ssboCount * kSetsPerPool});
  if (_uboCount > 0)
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _uboCount * kSetsPerPool});
  if (_samplerCount > 0)
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _samplerCount * kSetsPerPool});

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes    = poolSizes.data();
  poolInfo.maxSets       = kSetsPerPool;

  VkDescriptorPool pool = VK_NULL_HANDLE;
  VkResult result       = vkCreateDescriptorPool(_contextVK->_vkdevice, &poolInfo, nullptr, &pool);
  OrkAssert(result == VK_SUCCESS);
  _setPools.push_back(pool);
  _poolFreeSlots = kSetsPerPool;
}

VkDescriptorSet VkComputePipelineState::acquireDescriptorSet(uint64_t generation) {
  // a new dispatch phase (generation) recycles the ring: prior-phase sets are free
  // because endDispatchPhase submitted+waited before this generation began.
  if (generation != _setGeneration) {
    _setGeneration = generation;
    _setCursor     = 0;
  }
  if (_setCursor < _setRing.size()) {
    return _setRing[_setCursor++]; // reuse an already-allocated set
  }
  // need a brand new set (first time this many dispatches occur in one generation)
  if (_poolFreeSlots == 0) {
    _growSetPool();
  }
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool     = _setPools.back();
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts        = &_descriptorSetLayout;
  VkDescriptorSet set          = VK_NULL_HANDLE;
  VkResult result              = vkAllocateDescriptorSets(_contextVK->_vkdevice, &allocInfo, &set);
  OrkAssert(result == VK_SUCCESS);
  _poolFreeSlots--;
  _setRing.push_back(set);
  _setCursor++;
  return set;
}

void VkComputePipelineState::writeDescriptorSet(VkDescriptorSet set) {
  // populate `set` from the currently-recorded bindings. Always writes (each dispatch
  // gets a fresh/recycled set), so there is no dirty-skip. Buffer/image infos are
  // reserved to exact size so .back() pointers stay valid across the loop.
  std::vector<VkWriteDescriptorSet> writes;
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(_ssbo_bindings.size());

  for (auto& [binding_id, binding] : _ssbo_bindings) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = binding.buffer;
    bufferInfo.offset = binding.offset;
    bufferInfo.range  = binding.size;
    bufferInfos.push_back(bufferInfo);

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = set;
    write.dstBinding      = binding_id;
    write.dstArrayElement = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo     = &bufferInfos.back();
    writes.push_back(write);
  }

  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.reserve(_sampler_bindings.size());

  for (auto& [binding_id, img_info] : _sampler_bindings) {
    imageInfos.push_back(img_info);

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = set;
    write.dstBinding      = binding_id;
    write.dstArrayElement = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo      = &imageInfos.back();
    writes.push_back(write);
  }

  if (!writes.empty())
    vkUpdateDescriptorSets(_contextVK->_vkdevice, writes.size(), writes.data(), 0, nullptr);
}

///////////////////////////////////////////////////////////////////////////////

bool VkComputePipelineState::createPipeline(vkfxsstage_ptr_t computeShader) {
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
      binding.binding = ssbo->_binding_id;  // real SPIR-V binding (matches the generated GLSL)
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
  //printf("createPipeline<%s>: _smpset_refs=%p\n", _name.c_str(), computeShader->_smpset_refs.get());
  if (computeShader->_smpset_refs) {
    for (const auto& [name, smpset] : computeShader->_smpset_refs->_smpsets) {
      for (const auto& [samp_name, sampler] : smpset->_samplers_by_name) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = sampler->_binding_id;  // real SPIR-V binding (matches GLSL; bound AFTER the SSBOs)
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
  // Create descriptor set layout. Sort bindings ascending — they're gathered
  // from name-keyed maps (arbitrary order), and MoltenVK's SPIR-V->MSL resource
  // mapping indexes by binding and asserts on unsorted/sparse input.
  //////////////////////////////////////////////////////////
  std::sort(layoutBindings.begin(), layoutBindings.end(),
            [](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b) {
              return a.binding < b.binding;
            });
  // diagnostic: dump the resolved bindings; a duplicate or sparse set here is the usual cause of a
  // compute-pipeline failure (compute-only storage interfaces get fallback binding ids that can
  // collide across shaders that share an interface).
  {
    std::string dump;
    int prev = -1;
    bool dup = false, sparse = false;
    for (const auto& b : layoutBindings) {
      dump += " " + std::to_string(b.binding);
      if (int(b.binding) == prev) dup = true;
      if (int(b.binding) != prev + 1 && prev != -1) sparse = true; // post-sort gap
      prev = int(b.binding);
    }
    if(0)printf("computePipeline<%s>: %zu bindings ->%s%s%s\n", _name.c_str(), layoutBindings.size(),
           dump.c_str(), dup ? "  [DUPLICATE!]" : "", sparse ? "  [SPARSE!]" : "");
  }
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
  layoutInfo.pBindings = layoutBindings.data();

  VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &_descriptorSetLayout);
  if (result != VK_SUCCESS) {
    printf("createPipeline<%s>: FAILED vkCreateDescriptorSetLayout result<%d>\n", _name.c_str(), int(result));
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
    printf("createPipeline<%s>: FAILED vkCreatePipelineLayout result<%d>\n", _name.c_str(), int(result));
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
    printf("createPipeline<%s>: FAILED vkCreateComputePipelines result<%d>\n", _name.c_str(), int(result));
    return false;
  }

  //////////////////////////////////////////////////////////
  // Per-set descriptor type counts (used to size the growable per-dispatch pools
  // in _growSetPool). The sets themselves are allocated lazily at dispatch time
  // (acquireDescriptorSet), one per dispatch, so multiple dispatches of this
  // pipeline can coexist in a single command buffer with distinct bindings.
  //////////////////////////////////////////////////////////
  _ssboCount = _uboCount = _samplerCount = 0;
  for (const auto& binding : layoutBindings) {
    if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
      _ssboCount++;
    } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
      _uboCount++;
    } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
      _samplerCount++;
    }
  }
  _hasDescriptors = !layoutBindings.empty();

  logchan_vkcomp->log("createPipeline: successfully created compute pipeline<%s>", _name.c_str());
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// VkComputeInterface implementation
///////////////////////////////////////////////////////////////////////////////

VkComputeInterface::VkComputeInterface(vkcontext_rawptr_t ctx)
    : ComputeInterface()
    , _contextVK(ctx) {
  _nonblocking = (getenv("ORK_HM_NB_SUBMIT") != nullptr); // C.5 P3b flag (blocking = the soak-default)
}

///////////////////////////////////////////////////////////////////////////////
// C.5: wait out a submitted-but-unwaited dispatch phase. Called at every hazard point: the next
// beginDispatchPhase (cmdbuf reset / descriptor-ring recycle / pool reuse) and ANY host
// storage-buffer map (param rewrites, readbacks). No-op in blocking mode or when nothing pends.
///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::enqueueDeferredBufferUpdate(
    std::shared_ptr<VulkanBuffer> buf, //
    size_t offset,
    const void* data,
    size_t length) {
  std::lock_guard<std::mutex> lk(_pendingBufferUpdatesMutex);
  // exact-key dedupe (same buffer+offset+size): last write wins, and avoids a
  // transfer-transfer hazard between two vkCmdUpdateBuffers on the same range.
  for (auto& u : _pendingBufferUpdates) {
    if (u._buffer == buf and u._offset == offset and u._data.size() == length) {
      std::memcpy(u._data.data(), data, length);
      return;
    }
  }
  PendingBufferUpdate u;
  u._buffer = buf;
  u._offset = offset;
  u._data.assign((const uint8_t*)data, (const uint8_t*)data + length);
  _pendingBufferUpdates.push_back(std::move(u));
}

// host read-your-writes: a host READ of a buffer with pending deferred updates
// must see them — apply those entries synchronously (staged copy) and drop them.
void VkComputeInterface::applyPendingUpdatesFor(const std::shared_ptr<VulkanBuffer>& buf) {
  std::vector<PendingBufferUpdate> to_apply;
  {
    std::lock_guard<std::mutex> lk(_pendingBufferUpdatesMutex);
    for (auto it = _pendingBufferUpdates.begin(); it != _pendingBufferUpdates.end();) {
      if (it->_buffer == buf) {
        to_apply.push_back(std::move(*it));
        it = _pendingBufferUpdates.erase(it);
      } else {
        ++it;
      }
    }
  }
  for (auto& u : to_apply)
    u._buffer->copyFromHost(u._data.data(), u._data.size(), u._offset);
}

void VkComputeInterface::_flushDeferredBufferUpdates() {
  std::vector<PendingBufferUpdate> updates;
  {
    std::lock_guard<std::mutex> lk(_pendingBufferUpdatesMutex);
    updates.swap(_pendingBufferUpdates);
  }
  if (updates.empty())
    return;
  for (auto& u : updates)
    vkCmdUpdateBuffer(_computeCmdBuf, u._buffer->_vkbuffer, u._offset, u._data.size(), u._data.data());
  // transfer writes -> visible to this phase's dispatches
  VkMemoryBarrier bar{};
  bar.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  vkCmdPipelineBarrier(
      _computeCmdBuf,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      0,
      1, &bar,
      0, nullptr,
      0, nullptr);
}

void VkComputeInterface::syncPendingDispatch() {
  if (not _phasePending)
    return;
  auto w0 = std::chrono::steady_clock::now();
  vkWaitForFences(_contextVK->_vkdevice, 1, &_phaseFence, VK_TRUE, UINT64_MAX);
  _gpuWaitAccum += std::chrono::duration<double>(std::chrono::steady_clock::now() - w0).count();
  vkResetFences(_contextVK->_vkdevice, 1, &_phaseFence);
  _phasePending = false;
}

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::beginDispatchPhase() {
  if (_phaseDepth++ > 0) {
    return; // nested begin — the outer phase is already open (reentrant: a per-view fan-out
            // batches every drawable's cull into this one phase / one submit)
  }

  // C.5: a prior non-blocking phase must complete before we reset its command buffer (and before
  // the descriptor-set rings recycle — the generation bump below assumes the prior phase is DONE).
  syncPendingDispatch();

  // Allocate compute command buffer if needed
  if (_computeCmdBuf == VK_NULL_HANDLE) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _contextVK->_vkcmdpool_graphics;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(_contextVK->_vkdevice, &allocInfo, &_computeCmdBuf);
  }

  // Reset and begin the compute command buffer
  vkResetCommandBuffer(_computeCmdBuf, 0);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(_computeCmdBuf, &beginInfo);

  // Insert memory barrier: ensure host writes and any prior compute shader writes
  // are complete and visible before this compute pass reads or transfers.
  VkMemoryBarrier memoryBarrier{};
  memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;

  vkCmdPipelineBarrier(
      _computeCmdBuf,
      VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
      0,
      1, &memoryBarrier,
      0, nullptr,
      0, nullptr
  );

  // deferred small WRITE_ONLY updates (param/header writes) — recorded here as
  // vkCmdUpdateBuffer instead of a synchronous submit+fence per write.
  _flushDeferredBufferUpdates();

  _dispatchCount = 0;
  _inDispatchPhase = true;
  // new generation: per-dispatch descriptor-set rings recycle from cursor 0 (the
  // prior phase's command buffer has completed, so its sets are free to rewrite).
  _dispatchGeneration++;
}

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::storageBarrier() {
  OrkAssert(_inDispatchPhase && "storageBarrier must be called within a dispatch phase");
  OrkAssert(_computeCmdBuf != VK_NULL_HANDLE);

  VkMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

  vkCmdPipelineBarrier(
      _computeCmdBuf,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      0, 1, &barrier, 0, nullptr, 0, nullptr);
}

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::copyBufferRegion(
    FxShaderStorageBuffer* src, size_t src_offset,
    FxShaderStorageBuffer* dst, size_t dst_offset,
    size_t size) {
  OrkAssert(_inDispatchPhase && "copyBufferRegion must be called within a dispatch phase");
  OrkAssert(_computeCmdBuf != VK_NULL_HANDLE);
  OrkAssert(src && dst);

  auto vk_src = src->_impl.getShared<VulkanBuffer>();
  auto vk_dst = dst->_impl.getShared<VulkanBuffer>();
  OrkAssert(vk_src && vk_dst);

  VkBufferCopy region{};
  region.srcOffset = src_offset;
  region.dstOffset = dst_offset;
  region.size = size;
  vkCmdCopyBuffer(_computeCmdBuf, vk_src->_vkbuffer, vk_dst->_vkbuffer, 1, &region);
}

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::copySSBOToVertexBuffer(
    FxShaderStorageBuffer* src, size_t src_offset,
    VertexBufferBase* dst_vb, size_t dst_offset,
    size_t size) {
  OrkAssert(_inDispatchPhase && "copySSBOToVertexBuffer must be called within a dispatch phase");
  OrkAssert(_computeCmdBuf != VK_NULL_HANDLE);
  OrkAssert(src && dst_vb);

  auto vk_src = src->_impl.getShared<VulkanBuffer>();
  auto vk_vb = dst_vb->_impl.getShared<VulkanVertexBuffer>();
  OrkAssert(vk_src && vk_vb && vk_vb->_vkbuffer);

  VkBufferCopy region{};
  region.srcOffset = src_offset;
  region.dstOffset = dst_offset;
  region.size = size;
  vkCmdCopyBuffer(_computeCmdBuf, vk_src->_vkbuffer, vk_vb->_vkbuffer->_vkbuffer, 1, &region);
}

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::endDispatchPhase() {
  if (_phaseDepth == 0) {
    return; // unbalanced end (no open phase)
  }
  if (--_phaseDepth > 0) {
    return; // nested end — defer the submit+barrier to the OUTERMOST end (all culls in one submit)
  }
  if (!_inDispatchPhase) {
    return; // Not in dispatch phase
  }

  // Insert memory barrier: ensure compute writes and transfer writes are complete before vertex
  // shader / vertex input / INDIRECT-command reads (the DrawIndexedIndirect args + index buffers
  // and the dispatchComputeIndirect args are all compute-written).
  VkMemoryBarrier memoryBarrier{};
  memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
  memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                                VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

  vkCmdPipelineBarrier(
      _computeCmdBuf,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
          VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
      0, 1, &memoryBarrier, 0, nullptr, 0, nullptr
  );

  // End the compute command buffer
  vkEndCommandBuffer(_computeCmdBuf);

  // Only submit if we actually dispatched something
  if (_dispatchCount > 0) {
    if (_phaseFence == VK_NULL_HANDLE) {           // persistent fence (created once; reset per use)
      VkFenceCreateInfo fenceInfo{};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      vkCreateFence(_contextVK->_vkdevice, &fenceInfo, nullptr, &_phaseFence);
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_computeCmdBuf;

    _contextVK->_gfxqueue->queueSubmit(&submitInfo, _phaseFence);

    if (_nonblocking) {
      // C.5 P3b: RETURN without waiting — the CPU overlaps this phase's GPU work. The wait
      // happens at the next hazard point (syncPendingDispatch: next begin / any host map).
      // GPU->GPU ordering vs the render is free: same queue, submission order + the barrier above.
      _phasePending = true;
    } else {
      auto w0 = std::chrono::steady_clock::now();
      vkWaitForFences(_contextVK->_vkdevice, 1, &_phaseFence, VK_TRUE, UINT64_MAX);
      _gpuWaitAccum += std::chrono::duration<double>(std::chrono::steady_clock::now() - w0).count();
      vkResetFences(_contextVK->_vkdevice, 1, &_phaseFence);
      logchan_vkcomp->log("endDispatchPhase: submitted and completed %u dispatches", _dispatchCount);
    }
  }

  _inDispatchPhase = false;
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
    OrkAssert(false && "Compute shader has no VkComputePipelineState");
    return;
  }

  auto pipeline = vk_compute_pipeline.value();
  if (!pipeline || pipeline->_pipeline == VK_NULL_HANDLE) {
    logchan_vkcomp->log("dispatchCompute: invalid pipeline");
    OrkAssert(false && "Invalid compute pipeline");
    return;
  }

  // Use dedicated compute command buffer
  OrkAssert(_inDispatchPhase && "Must call beginDispatchPhase before dispatch");
  OrkAssert(_computeCmdBuf != VK_NULL_HANDLE);

  // Bind compute pipeline
  vkCmdBindPipeline(_computeCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->_pipeline);

  // Acquire a FRESH descriptor set for THIS dispatch and populate it with the
  // currently-recorded bindings, then bind it. A fresh set per dispatch is what lets
  // many dispatches of the same pipeline live in one command buffer (one submit) with
  // distinct bindings (e.g. a ping-pong relaxation) — previously a single in-place set
  // forced a submit+wait between every dispatch.
  if (pipeline->_hasDescriptors) {
    VkDescriptorSet dset = pipeline->acquireDescriptorSet(_dispatchGeneration);
    pipeline->writeDescriptorSet(dset);
    vkCmdBindDescriptorSets(
        _computeCmdBuf,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->_pipelineLayout,
        0,  // first set
        1,  // set count
        &dset,
        0,      // dynamic offset count
        nullptr // dynamic offsets
    );
  }

  // Dispatch compute work
  vkCmdDispatch(_computeCmdBuf, numgroups_x, numgroups_y, numgroups_z);
  _dispatchCount++;

  logchan_vkcomp->log("dispatchCompute: dispatched %u x %u x %u work groups", numgroups_x, numgroups_y, numgroups_z);
}

///////////////////////////////////////////////////////////////////////////////

// GPU-driven dispatch (C.3): group counts come from a VkDispatchIndirectCommand (x,y,z uint32) at
// `args_offset` inside a GPU-written SSBO — the compute analogue of DrawIndexedIndirectEML. Same
// pipeline/descriptor handling as dispatchCompute; only the final command differs.
void VkComputeInterface::dispatchComputeIndirect(const FxComputeShader* shader, FxShaderStorageBuffer* args, size_t args_offset) {
  if (!shader) {
    logchan_vkcomp->log("dispatchComputeIndirect: null shader");
    return;
  }
  auto vk_compute_pipeline = shader->_impl.tryAs<vkcompute_pipeline_ptr_t>();
  if (!vk_compute_pipeline) {
    logchan_vkcomp->log("dispatchComputeIndirect: shader has no compute pipeline");
    OrkAssert(false && "Compute shader has no VkComputePipelineState");
    return;
  }
  auto pipeline = vk_compute_pipeline.value();
  if (!pipeline || pipeline->_pipeline == VK_NULL_HANDLE) {
    logchan_vkcomp->log("dispatchComputeIndirect: invalid pipeline");
    OrkAssert(false && "Invalid compute pipeline");
    return;
  }
  OrkAssert(args != nullptr);
  OrkAssert((args_offset & 3) == 0 && "indirect args offset must be 4-byte aligned");
  auto vk_args = args->_impl.getShared<VulkanBuffer>();
  OrkAssert(vk_args);

  OrkAssert(_inDispatchPhase && "Must call beginDispatchPhase before dispatch");
  OrkAssert(_computeCmdBuf != VK_NULL_HANDLE);

  vkCmdBindPipeline(_computeCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->_pipeline);

  // fresh descriptor set per dispatch (same rationale as dispatchCompute above)
  if (pipeline->_hasDescriptors) {
    VkDescriptorSet dset = pipeline->acquireDescriptorSet(_dispatchGeneration);
    pipeline->writeDescriptorSet(dset);
    vkCmdBindDescriptorSets(
        _computeCmdBuf,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->_pipelineLayout,
        0,  // first set
        1,  // set count
        &dset,
        0,      // dynamic offset count
        nullptr // dynamic offsets
    );
  }

  vkCmdDispatchIndirect(_computeCmdBuf, vk_args->_vkbuffer, VkDeviceSize(args_offset));
  _dispatchCount++;

  logchan_vkcomp->log("dispatchComputeIndirect: dispatched from GPU args (offset %zu)", args_offset);
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
// Auto-resolving bind: resolve `block`'s reflected SPIR-V binding within THIS compute
// shader (by name) — the same _binding_id createPipeline built the descriptor layout from
// (vulkan_compute.cpp:175). Frees callers from hardcoding an index that must match the
// merged binding id (non-obvious when the shader shares a storage block with a graphics
// technique, e.g. FWD_SSBO_CUSTOM where sif_ptex_vtx sits past the lighting storage).
///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::bindStorageBufferOnBlock(
    const FxComputeShader* shader,
    FxShaderStorageBuffer* buffer,
    const FxShaderStorageBlock* block) {

  if (!shader || !block || !buffer) {
    logchan_vkcomp->log("bindStorageBufferOnBlock: null shader/block/buffer");
    return;
  }
  auto vk_compute_pipeline = shader->_impl.tryAs<vkcompute_pipeline_ptr_t>();
  if (!vk_compute_pipeline) {
    logchan_vkcomp->log("bindStorageBufferOnBlock: shader has no compute pipeline");
    return;
  }
  auto pipeline = vk_compute_pipeline.value();
  auto cs       = pipeline->_computeShader;
  if (!cs || !cs->_ssbo_refs) {
    logchan_vkcomp->log("bindStorageBufferOnBlock: compute shader has no ssbo refs");
    return;
  }
  auto it = cs->_ssbo_refs->_ssbo_blocks.find(block->_name);
  if (it == cs->_ssbo_refs->_ssbo_blocks.end()) {
    logchan_vkcomp->log(
        "bindStorageBufferOnBlock: block<%s> not referenced by compute shader<%s>",
        block->_name.c_str(), cs->_name.c_str());
    return;
  }
  bindStorageBuffer(shader, uint32_t(it->second->_binding_id), buffer);
}

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

void VkComputeInterface::bindSampler(
    const FxComputeShader* shader,
    uint32_t binding_index,
    Texture* tex) {

  if (!shader || !tex) {
    logchan_vkcomp->log("bindSampler: null shader or texture");
    return;
  }

  auto vk_compute_pipeline = shader->_impl.tryAs<vkcompute_pipeline_ptr_t>();
  if (!vk_compute_pipeline) {
    printf("bindSampler: shader has no compute pipeline!\n");
    return;
  }

  auto pipeline = vk_compute_pipeline.value();

  // Get VulkanTextureObject from texture
  auto vktex = tex->_impl.getShared<VulkanTextureObject>();
  if (!vktex) {
    printf("bindSampler: texture has no VulkanTextureObject!\n");
    return;
  }
  if (!vktex->_descset_sampling) {
    printf("bindSampler: texture has no _descset_sampling! img_sampling=%p\n", vktex->_img_sampling.get());
    return;
  }

  // Use the active sampling descriptor
  VkDescriptorImageInfo desc_info = *vktex->_descset_sampling;
  if(0)printf("bindSampler: binding=%u imageView=%p sampler=%p layout=%d\n",
         binding_index, (void*)desc_info.imageView, (void*)desc_info.sampler, (int)desc_info.imageLayout);
  pipeline->bindSampler(binding_index, desc_info);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
