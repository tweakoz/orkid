////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>
#include <ork/util/logger.h>
#include <ork/util/hexdump.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_ubo = logger()->configureChannel("VKUBO", fvec3(1,1,.4), true);

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

void VkFxInterface::_flushDirtyUniformBlocks() {
  // This function is now obsolete - dynamic UBO system handles updates
  // The actual data copying happens in VkPipelineObject::applyPendingUboUpdates
  // which allocates from the dynamic system and copies shadow buffer data
  
  if (!_currentVKPASS) return;
  
  // Just clear the dirty ranges since the data has already been handled
  for (auto& block : _currentVKPASS->_dirty_uniform_blocks) {
    block->_dirty_ranges.clear();
  }
  
  // Don't clear _dirty_uniform_blocks here - let applyPendingUboUpdates handle it
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////