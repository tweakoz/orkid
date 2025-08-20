////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

// AlignedRange implementation
alignedrange_ptr_t AlignedRange::fromDirtyRange(
    dirtyrange_ptr_t dirty, 
    VkDeviceSize atom_size, 
    VkDeviceSize buffer_size) {
  auto result = std::make_shared<AlignedRange>();
  result->offset = (dirty->offset / atom_size) * atom_size;
  VkDeviceSize end = dirty->offset + dirty->size;
  VkDeviceSize aligned_end = ((end + atom_size - 1) / atom_size) * atom_size;
  result->size = std::min(aligned_end - result->offset, buffer_size - result->offset);
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// VkFxShaderUniformBlk implementation
///////////////////////////////////////////////////////////////////////////////

void VkFxShaderUniformBlk::addDirtyRange(size_t offset, size_t size) {
  auto new_range = std::make_shared<DirtyRange>();
  new_range->offset = offset;
  new_range->size = size;
  
  // Try to merge with existing ranges
  for (auto& range : _dirty_ranges) {
    // Check if overlapping or adjacent
    if (offset <= range->offset + range->size && 
        offset + size >= range->offset) {
      // Merge ranges
      size_t new_start = std::min(offset, range->offset);
      size_t new_end = std::max(offset + size, range->offset + range->size);
      range->offset = new_start;
      range->size = new_end - new_start;
      coalesceRanges();
      return;
    }
  }
  
  // No overlap, add new range
  _dirty_ranges.push_back(new_range);
}

///////////////////////////////////////////////////////////////////////////////

void VkFxShaderUniformBlk::coalesceRanges() {
  if (_dirty_ranges.size() < 2) return;
  
  // Sort by offset
  std::sort(_dirty_ranges.begin(), _dirty_ranges.end(),
    [](dirtyrange_ptr_t a, dirtyrange_ptr_t b) {
      return a->offset < b->offset;
    });
  
  std::vector<dirtyrange_ptr_t> merged;
  merged.push_back(_dirty_ranges[0]);
  
  const size_t merge_threshold = 64; // Cache line size
  
  for (size_t i = 1; i < _dirty_ranges.size(); i++) {
    auto& last = merged.back();
    auto& curr = _dirty_ranges[i];
    
    // Merge if overlapping or within threshold
    if (curr->offset <= last->offset + last->size + merge_threshold) {
      last->size = std::max(last->offset + last->size, curr->offset + curr->size) - last->offset;
    } else {
      merged.push_back(curr);
    }
  }
  
  _dirty_ranges = std::move(merged);
}

///////////////////////////////////////////////////////////////////////////////

std::vector<alignedrange_ptr_t> VkFxShaderUniformBlk::getAlignedRanges(VkDeviceSize atom_size) const {
  std::vector<alignedrange_ptr_t> aligned;
  for (auto& dirty : _dirty_ranges) {
    aligned.push_back(AlignedRange::fromDirtyRange(dirty, atom_size, _buffer_size));
  }
  return aligned;
}

///////////////////////////////////////////////////////////////////////////////
// VkFxInterface implementation
///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::flushDirtyUniformBlocks() {
  if (!_currentVKPASS) return;
  
  for (auto& block : _currentVKPASS->_dirty_uniform_blocks) {
    if (block->_dirty_ranges.empty()) continue;
    
    // Debug: Log UBO flush
    //printf("UBO_FLUSH: block<%s> dset<%zu> ranges<%zu>\n", 
    //       block->_orkparamblock ? block->_orkparamblock->_name.c_str() : "unknown", 
    //       block->_descriptor_set_id,
    //       block->_dirty_ranges.size());
    
    // If using coherent memory, just copy
    if (!block->_needs_flush) {                                                              
      OrkAssert(block->_mapped_ptr != nullptr);
      for (auto& range : block->_dirty_ranges) {

        //printf("Flushing coherent dirty range: offset=%zu, size=%zu\n", range->offset, range->size);

        memcpy(
          static_cast<uint8_t*>(block->_mapped_ptr) + range->offset,
          block->_shadow_buffer.data() + range->offset,
          range->size
        );
      }
      block->_dirty_ranges.clear();
      continue;
    }
    
    // Non-coherent memory needs flush
    if (block->_mapped_ptr) {
      // Copy shadow data to mapped memory
      for (auto& range : block->_dirty_ranges) {
        //printf("Flushing non-coherent dirty range: offset=%zu, size=%zu\n", range->offset, range->size);
        memcpy(
          static_cast<uint8_t*>(block->_mapped_ptr) + range->offset,
          block->_shadow_buffer.data() + range->offset,
          range->size
        );
      }
      
      // Get aligned ranges for flush
      auto aligned_ranges = block->getAlignedRanges(
        _contextVK->_vkdeviceinfo->_devprops.limits.nonCoherentAtomSize
      );
      
      // Build flush descriptors
      std::vector<VkMappedMemoryRange> flush_ranges;
      for (auto& range : aligned_ranges) {
        VkMappedMemoryRange flush_range = {};
        flush_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        flush_range.memory = block->_gpu_memory;
        flush_range.offset = range->offset;
        flush_range.size = range->size;
        flush_ranges.push_back(flush_range);
      }
      
      // Flush to GPU
      vkFlushMappedMemoryRanges(_contextVK->_vkdevice, 
                                 flush_ranges.size(), 
                                 flush_ranges.data());
    }
    
    block->_dirty_ranges.clear();
  }
  
  _currentVKPASS->_dirty_uniform_blocks.clear();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////