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

VkFxShaderUniformBlockState* VkFxInterface::uniformStateForBlock(VkFxShaderUniformBlock* block) {
  auto it = _uniform_block_states.find(block);
  return it != _uniform_block_states.end() ? &it->second : nullptr;
}

VkFxShaderStorageBlockState* VkFxInterface::storageStateForBlock(VkFxShaderStorageBlock* block) {
  auto it = _storage_block_states.find(block);
  return it != _storage_block_states.end() ? &it->second : nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_ensureBlockStates(vkfxshaderpass_rawptr_t pass) {
  for (auto& [name, blk] : pass->_vk_uniformblks) {
    if (_uniform_block_states.find(blk.get()) == _uniform_block_states.end()) {
      auto& state = _uniform_block_states[blk.get()];
      state._shader_uniform_block = blk.get();
      state._shadow_buffer.resize(blk->_buffer_size, 0);
    }
  }

  for (auto& [name, blk] : pass->_vk_ssbo_blocks) {
    if (_storage_block_states.find(blk.get()) == _storage_block_states.end()) {
      _storage_block_states[blk.get()]._shader_storage_block = blk.get();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////