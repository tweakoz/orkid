////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>
#include <ork/util/logger.h>
#include <cstdlib>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_vkbuf = logger()->configureChannel("VKBUF", fvec3(0.6, 0.8, 0.4), true);

FxUniformBuffer* VkFxInterface::createUniformBuffer(size_t length) {
  auto ub = new FxUniformBuffer;
  ub->_length         = length;
  auto uniblk_buf = ub->_impl.makeShared<VulkanBuffer>(_contextVK, length, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  return ub;
}

///////////////////////////////////////////////////////////////////////////////

fxuniformbuffermapping_ptr_t VkFxInterface::mapUniformBuffer(FxUniformBuffer* b, //
                                                             size_t base, //
                                                             size_t length) { //

  auto bufimpl = b->_impl.getShared<VulkanBuffer>();
  auto mapping = std::make_shared<FxUniformBufferMapping>();
  mapping->_buffer = b;
  mapping->_offset   = base;
  mapping->_fxi    = this;  // Fix: Set the FxInterface pointer
  if(length==0){
    mapping->_length = bufimpl->_length;
  }
  else{
    mapping->_length   = length;
  }
  mapping->_mappedaddr = bufimpl->map(base, mapping->_length,0);
  return mapping;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::unmapUniformBuffer(FxUniformBufferMapping* mapping) {
  auto bufimpl = mapping->_buffer->_impl.getShared<VulkanBuffer>();
  bufimpl->unmap();
  mapping->_impl.make<void*>(nullptr);
  mapping->_mappedaddr = nullptr; // Clear the mapped address
}

///////////////////////////////////////////////////////////////////////////////

FxShaderStorageBuffer* VkFxInterface::createStorageBuffer(size_t length,
                                                          StorageBufferUsage usage,
                                                          BufferResidency residency) {
  auto ssbo     = new FxShaderStorageBuffer;
  ssbo->_length = length;
  // TRANSFER_SRC|DST always (staging + buffer-to-buffer copies — free on linear buffers).
  VkBufferUsageFlags vku = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  if (usage == StorageBufferUsage::DEFAULT) {  // historical full set (compute SSBO + GPU-driven draw inputs)
    vku |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  } else {                                     // precise roles
    if (usage & StorageBufferUsage::STORAGE)  vku |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (usage & StorageBufferUsage::INDEX)    vku |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage & StorageBufferUsage::INDIRECT) vku |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (usage & StorageBufferUsage::VERTEX)   vku |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  VkMemoryPropertyFlags memprops;
  switch (residency) {
    case BufferResidency::DEVICE: // GPU-only VRAM; map()/copy staged
      memprops = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      break;
    case BufferResidency::BAR: // CPU-writable VRAM window; degrades to HOST via _findMemoryType /
                               // alloc-retry when absent or full (see vulkan_ctx_bufmem.cpp)
      memprops = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      break;
    case BufferResidency::HOST:
    default:
      memprops = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      break;
  }
  ssbo->_impl.makeShared<VulkanBuffer>(_contextVK, length, vku, std::string(""), memprops);
  return ssbo;
}

///////////////////////////////////////////////////////////////////////////////

// env-gated (ORKID_FXI_MAP_TRACE=1) profiling of the storage-buffer map path —
// accumulates where the time goes and prints a summary every 5s.
struct MapTraceStats {
  std::atomic<uint64_t> _syncNs{0}, _mapNs{0}, _unmapNs{0}, _stageNs{0};
  std::atomic<uint64_t> _maps{0}, _unmaps{0};
  double _windowStart = 0.0;
  static MapTraceStats& instance() {
    static MapTraceStats s;
    return s;
  }
  static bool enabled() {
    static bool e = (getenv("ORKID_FXI_MAP_TRACE") != nullptr);
    return e;
  }
  void report() {
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (_windowStart == 0.0) {
      _windowStart = now;
      return;
    }
    if ((now - _windowStart) < 5.0)
      return;
    printf(
        "[FXIMAPTRACE] 5s: maps<%zu> unmaps<%zu> syncWait<%.2fms> vkMap<%.2fms> vkUnmap<%.2fms> staging<%.2fms>\n",
        size_t(_maps.exchange(0)),
        size_t(_unmaps.exchange(0)),
        1e-6 * double(_syncNs.exchange(0)),
        1e-6 * double(_mapNs.exchange(0)),
        1e-6 * double(_unmapNs.exchange(0)),
        1e-6 * double(_stageNs.exchange(0)));
    _windowStart = now;
  }
};
static inline uint64_t _nsNow() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

storagebuffermappingptr_t VkFxInterface::mapStorageBuffer(FxShaderStorageBuffer* b, //
                                                          size_t base, //
                                                          size_t length, //
                                                          BufferMapAccess access) { //
  bool trace = MapTraceStats::enabled();
  // C.5: a host map is a HAZARD POINT for a pending non-blocking dispatch phase (the GPU may
  // still read what we are about to rewrite, or still be writing what we are about to read) —
  // wait it out first. No-op in blocking mode / when nothing pends.
  if (_contextVK->_ci) {
    uint64_t t0 = trace ? _nsNow() : 0;
    _contextVK->_ci->syncPendingDispatch();
    if (trace)
      MapTraceStats::instance()._syncNs += (_nsNow() - t0);
  }
  auto bufimpl = b->_impl.getShared<VulkanBuffer>();
  auto mapping = std::make_shared<FxShaderStorageBufferMapping>();
  mapping->_buffer = b;
  mapping->_fxi = this;
  mapping->_offset = base;
  mapping->_access = access;
  mapping->_length = (length == 0) ? bufimpl->_length : length;
  uint64_t t1 = trace ? _nsNow() : 0;
  if (not bufimpl->_hostVisible) {
    // device-local: back the mapping with a host temp; pre-fill on read, flush on unmap (write).
    void* temp = std::malloc(mapping->_length);
    bool reads = (access == BufferMapAccess::READ_ONLY) || (access == BufferMapAccess::READ_WRITE);
    if (reads)
      bufimpl->copyToHost(temp, mapping->_length, base);
    mapping->_mappedaddr = temp;
    if (trace)
      MapTraceStats::instance()._stageNs += (_nsNow() - t1);
  } else {
    mapping->_mappedaddr = bufimpl->map(base, mapping->_length, 0);
    if (trace)
      MapTraceStats::instance()._mapNs += (_nsNow() - t1);
  }
  if (trace) {
    MapTraceStats::instance()._maps++;
    MapTraceStats::instance().report();
  }
  return mapping;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::unmapStorageBuffer(FxShaderStorageBufferMapping* mapping) {
  bool trace  = MapTraceStats::enabled();
  uint64_t t0 = trace ? _nsNow() : 0;
  auto bufimpl = mapping->_buffer->_impl.getShared<VulkanBuffer>();
  if (not bufimpl->_hostVisible) {
    bool writes = (mapping->_access == BufferMapAccess::WRITE_ONLY) || (mapping->_access == BufferMapAccess::READ_WRITE);
    // MUST stay synchronous: a deferred-update optimization here (stash small WRITE_ONLY
    // writes, replay as vkCmdUpdateBuffer at the next dispatch phase) shipped a correctness
    // bug (warm-cook-cached static hypermesh scenes rendered nothing) and was removed —
    // read ork.dox/vk_deferred_updates_postmortem.md before reattempting.
    if (writes)
      bufimpl->copyFromHost(mapping->_mappedaddr, mapping->_length, mapping->_offset); // flush staging -> device
    std::free(mapping->_mappedaddr);
    if (trace)
      MapTraceStats::instance()._stageNs += (_nsNow() - t0);
  } else {
    bufimpl->unmap();
    if (trace)
      MapTraceStats::instance()._unmapNs += (_nsNow() - t0);
  }
  if (trace)
    MapTraceStats::instance()._unmaps++;
  mapping->_impl.make<void*>(nullptr);
  mapping->_mappedaddr = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

// GPU-idle contract (fxi.h): the caller guarantees no in-flight CB references the
// buffer, so skip the deferred queue — bulk bake-teardown frees (hundreds of 64MB
// planes) would otherwise trickle out at the delayed-destroy drain budget.
void VkFxInterface::destroyStorageBuffer(FxShaderStorageBuffer* buffer) {
  if (nullptr == buffer)
    return;
  if (auto impl = buffer->_impl.getShared<VulkanBuffer>())
    impl->_deferredDestroy = false;
  delete buffer;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::copyBufferIntoStorageBuffer(FxShaderStorageBuffer* ssbo,
                                                std::vector<uint8_t> buffer,
                                                size_t dest_offset) {
  auto bufimpl = ssbo->_impl.getShared<VulkanBuffer>();
  OrkAssert((dest_offset + buffer.size()) <= ssbo->_length);
  bufimpl->copyFromHost(buffer.data(), buffer.size(), dest_offset);  // host-or-staged per residency
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindStorageBuffer(const FxShaderStorageBlock* block,
                                      FxShaderStorageBuffer* buffer,
                                      size_t byte_offset) {

 if(0) printf("bindStorageBuffer: block<%s> buffer<%p> offset<%zu>\n", block ? block->_name.c_str() : "null", buffer, byte_offset);

  if (!block || !buffer) {
    return;
  }

  // Get Vulkan-specific implementations
  auto& vk_block_ptr = block->_impl.getShared<VkFxShaderStorageBlock*>();
  if (!vk_block_ptr) {
    return;
  }
  auto vk_block = *vk_block_ptr;

  auto& vk_buffer = buffer->_impl.getShared<VulkanBuffer>();
  if (!vk_buffer) {
    return;
  }

  // Store binding per-context so concurrent VkContexts don't collide
  auto* block_state = storageStateForBlock(vk_block);
  if (!block_state) {
    logchan_vkbuf->log("bindStorageBuffer: no storage state for block<%s>", block->_name.c_str());
    return;
  }

  // Store binding for later use when creating descriptor sets. The OFFSET is part of the binding
  // identity: the same buffer at a new offset needs a fresh descriptor set (sub-range LOD draws bind
  // ONE OUT_M at per-tier offsets within a frame), so invalidate the per-pass hash on either change.
  if (block_state->_bound_buffer != vk_buffer || block_state->_bound_offset != VkDeviceSize(byte_offset)) {
    block_state->_bound_buffer = vk_buffer;
    block_state->_bound_offset = VkDeviceSize(byte_offset);
    if (_current_shader_pass_state) {
      _current_shader_pass_state->_samplers_hash = 0;
    }
  }
  block_state->_bound_ssbo   = buffer;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
