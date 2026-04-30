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
// VkFxShaderUniformBlkState implementation
///////////////////////////////////////////////////////////////////////////////

void VkFxShaderUniformBlockState::addDirtyRange(size_t offset, size_t size) {
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
      //coalesceRanges();
      return;
    }
  }

  // No overlap, add new range
  _dirty_ranges.push_back(new_range);
}

///////////////////////////////////////////////////////////////////////////////

void VkFxShaderUniformBlockState::coalesceRanges() {
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

std::vector<alignedrange_ptr_t> VkFxShaderUniformBlockState::getAlignedRanges(VkDeviceSize atom_size) const {
  std::vector<alignedrange_ptr_t> aligned;
  VkDeviceSize buffer_size = _shader_uniform_block->_buffer_size;
  for (auto& dirty : _dirty_ranges) {
    aligned.push_back(AlignedRange::fromDirtyRange(dirty, atom_size, buffer_size));
  }
  return aligned;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxShaderState::initForProgram(vkfxsprg_rawptr_t prog) {
  if (_shader == prog) return;
  _shader = prog;

  if (_uniform_states.empty()) {
    // Originally it made a buffer for every param in the file even if the shader didn't use them.
    auto& blks = prog->_shader_file->_vk_uniformblks;

    // If you make buffers only for what is in use by going off of prog->_vk_uniformblks:
    //    auto& blks = prog->_vk_uniformblks;
    // Then you end up with less buffers and params per shader state.
    // But then you must guard against some being params having null state in vulkan_fxi_bindparam:
    //   auto* block_state = _current_shader_state->uniformStateForBlock(as_uniblk_item.value()->_parent_block);
    //   if (!block_state) return;
    // It is probably better to go off of prog->_vk_uniformblks and only end up with what is used.
    // However there needs to be a change higher up so that it doesn't redundantly call bindParam____
    // On a bunch of params which don't have any state, so you don't need to null guard each block_state.

    _uniform_states.reserve(blks.size());
    for (auto& [name, blk] : blks) {
      auto& state = _uniform_states[blk.get()];
      state._shader_uniform_block = blk.get();
      state._shadow_buffer.resize(blk->_buffer_size, 0);
    }
  }

  if (_storage_states.empty()) {
    _storage_states.reserve(prog->_vk_ssbo_blocks.size());
    for (auto& [name, blk] : prog->_vk_ssbo_blocks) {
      auto& state = _storage_states[blk.get()];
      state._shader_storage_block = blk.get();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

VkFxShaderUniformBlockState* VkFxShaderState::uniformStateForBlock(VkFxShaderUniformBlock* blk) {
  auto it = _uniform_states.find(blk);
  return it != _uniform_states.end() ? &it->second : nullptr;
}

///////////////////////////////////////////////////////////////////////////////

VkFxShaderStorageBlockState* VkFxShaderState::storageStateForBlock(VkFxShaderStorageBlock* block) {
  auto it = _storage_states.find(block);
  return it != _storage_states.end() ? &it->second : nullptr;
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// VkFxInterface implementation
///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_flushDirtyUniformBlocks() {
  // This function is now obsolete - dynamic UBO system handles updates
  // The actual data copying happens in VkPipelineObject::applyPendingUboUpdates
  // which allocates from the dynamic system and copies shadow buffer data

  if (!_current_shader_state) return;

  for (auto block : _current_shader_state->_dirty_uniform_blocks) {
    block->_dirty_ranges.clear();
  }

  // Don't clear _dirty_uniform_blocks here - let applyPendingUboUpdates handle it -
  // -- ApplyPendingUpdates never cleared it!?
  _current_shader_state->_dirty_uniform_blocks.clear();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////