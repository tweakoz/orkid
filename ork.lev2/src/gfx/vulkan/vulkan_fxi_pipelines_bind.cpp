////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include "vulkan_ub_layout.inl"
#include "vulkan_ubo_dynamic.h"
#include <ork/lev2/gfx/shadman.h>
#include <ork/util/hexdump.inl>
#include <ctime>
#include <boost/filesystem.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_vkpipb = logger()->configureChannel("VKPIPB", fvec3(1, 1, .2), false);

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_bindPipeline(VkCommandBuffer cmdbuf, vkpipeline_obj_ptr_t pipeline) {

  auto fbi    = _contextVK->_fbi;
  auto fbi_vp = fbi->_viewportTracker;
  auto fbi_sc = fbi->_scissorTracker;

  ////////////////////////////////////////
  // bind pipeline (if not already bound)
  ////////////////////////////////////////

  if (_currentPipeline != pipeline) {
    vkCmdBindPipeline(
        cmdbuf,                          // command buffer
        VK_PIPELINE_BIND_POINT_GRAPHICS, // pipeline type
        pipeline->_pipeline);            // pipeline
    _currentPipeline            = pipeline;
    _currentPipeline->_viewport = nullptr;
    _currentPipeline->_scissor  = nullptr;
  }

  ////////////////////////////////////////
  // set dynamic viewport (if changed)
  ////////////////////////////////////////

  if (pipeline->_viewport != fbi_vp) {
    pipeline->_viewport = fbi_vp;
    VkViewport vkvp     = {};
    vkvp.x              = fbi_vp->_x;
    vkvp.width          = fbi_vp->_width;

    vkvp.minDepth = 0.0f;
    vkvp.maxDepth = 1.0f;

    if (not FLIP_Y_LIKE_OPENGL) {
      vkvp.y      = (fbi_vp->_y + fbi_vp->_height);
      vkvp.height = -fbi_vp->_height;
    } else {
      vkvp.y      = fbi_vp->_y;
      vkvp.height = fbi_vp->_height;
    }

    if(0)printf( "SETVP<%p> x<%f> y<%f> w<%f> h<%f>\n", pipeline.get(), vkvp.x, vkvp.y, vkvp.width, vkvp.height);
    vkCmdSetViewport(
        cmdbuf, // command buffer
        0,      // first viewport
        1,      // viewport count
        &vkvp); // viewport data
  }

  ////////////////////////////////////////
  // set dynamic scissor (if changed)
  ////////////////////////////////////////

  if (pipeline->_scissor != fbi_sc) {
    pipeline->_scissor = fbi_sc;
    VkRect2D vksc      = {};
    vksc.offset.x      = fbi_sc->_x;
    vksc.offset.y      = fbi_sc->_y;
    vksc.extent.width  = fbi_sc->_width;
    vksc.extent.height = fbi_sc->_height;
    if(0)printf( "SETSC<%p> x<%d> y<%d> w<%d> h<%d>\n", pipeline.get(), vksc.offset.x, vksc.offset.y, vksc.extent.width, vksc.extent.height);
    vkCmdSetScissor(
        cmdbuf, // command buffer
        0,      // first scissor
        1,      // scissor count
        &vksc); // scissor data
  }

  ////////////////////////////////////////
  // set dynamic blend constants (always)
  ////////////////////////////////////////

  auto rasterstate = _rasterstate_stack.resolve();
  fvec4 current_blend_constants = rasterstate ? rasterstate->_blendConstant : fvec4(0.0f, 0.0f, 0.0f, 0.0f);
  float bc[4] = {
    current_blend_constants.x,
    current_blend_constants.y,
    current_blend_constants.z,
    current_blend_constants.w
  };
  vkCmdSetBlendConstants(cmdbuf, bc);

  ////////////////////////////////////////
  // upload ubo data and push constants
  ////////////////////////////////////////

  _uploadPipelineData(cmdbuf, pipeline);

  ////////////////////////////////////////
  // bind descriptor set (if changed)
  ////////////////////////////////////////
  static int counter = 0;
    counter++;
    if(counter==3){
        printf("yo\n");
    }
  auto prog = _currentVKPASS->_vk_program;
  auto desc_set = pipeline->_descriptorSetCache->fetchDescriptorSetForProgram(prog);
  if (desc_set) {
    // Bind descriptor set with dynamic offsets from applyPendingUboUpdates
    if (!pipeline->_dynamic_offsets.empty()) {
      vkCmdBindDescriptorSets(
          cmdbuf,
          VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline->_pipelineLayout,
          0, // first set
          1, // set count
          &desc_set->_vkdescset,
          pipeline->_dynamic_offsets.size(),
          pipeline->_dynamic_offsets.data());
    } else {
      // Fallback to static binding if no dynamic offsets
      _bindGfxDescriptorSetOnSlot(cmdbuf, desc_set, 0);
    }
  }

}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_uploadPipelineData(VkCommandBuffer CB, vkpipeline_obj_ptr_t pipeline) {

  // Apply dynamic UBO updates for this draw
  // This allocates per-draw memory and copies shadow buffers
  static uint32_t frame_index = 0; // TODO: Get actual frame index from swapchain
  pipeline->applyPendingUboUpdates(CB, frame_index);

  // Flush uniform blocks BEFORE fetching descriptor set
  // This ensures the GPU buffers have the correct data when bound
  // Note: With dynamic UBOs, this may become unnecessary
  _flushDirtyUniformBlocks();

  pipeline->applyPendingPushConstants(CB);
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_bindVertexBufferOnSlot(VkCommandBuffer cmdbuf, vkvtxbuf_ptr_t vb, size_t slot) {
  if (true) { //_active_vbs[slot] != vb) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(
        cmdbuf,                    // command buffer
        slot,                      // slot to bind to
        1,                         // binding count
        &vb->_vkbuffer->_vkbuffer, // buffers
        &offset);                  // offsets
    _active_vbs[slot] = vb;
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkPipelineObject::applyPendingPushConstants(VkCommandBuffer cmdbuf) { //
  if(not _vk_program->_pushConstantBlock){
    return;
  }

  auto data_layout = _vk_program->_pushConstantBlock->_data_layout;
  auto& ranges     = _vk_program->_pushConstantBlock->_ranges;

  if(ranges.size()==0) return;
  
  size_t blocksize = _vk_program->_pushConstantBlock->_blockSize;

  size_t num_params = _vk_program->_pending_params.size();


  auto data = _vk_program->_pushdatabuffer.data();

  for (auto item : _vk_program->_pending_params) {
    auto dst_offset = data_layout->offsetForParam(item._ork_param);
    if (dst_offset != -1) {
      auto parm_name   = item._ork_param->_name;
      auto parm_type   = item._vk_param->_datatype;
      size_t parm_size = item._value.size();
      if (0) {
        // Find the correct range for this parameter
        size_t range_idx = item._vk_param->_range_index;
        int range_offset = (range_idx < ranges.size()) ? ranges[range_idx].offset : -1;
        printf(
            "parm<%s:%s:%zu> range_idx<%zu> range_offset<%d> dst_offset<%zu> ", //
            parm_type.c_str(),
            parm_name.c_str(),
            parm_size,
            range_idx,
            range_offset,
            dst_offset);
        printf("\n");
      }
      // dst_offset is already the absolute offset in the combined push constant block
      // We don't need to add range offset - that's for the shader's view, not CPU layout
      OrkAssert((dst_offset + parm_size) <= blocksize);
      memcpy(data + dst_offset, item._value.data(), parm_size);
    }
  }
  // hexdumpbytes(data,blocksize);

  // Push each range separately so shaders see their data at offset 0
  for (const auto& range : ranges) {
    // Each range gets pushed to offset 0 for its shader stage
    // The shader sees its uniform_set starting at offset 0
    if(range.size==0) continue;
    vkCmdPushConstants(
        cmdbuf,
        _pipelineLayout,
        range.stageFlags,   // Only the stages that use this range
        0,                  // Shader sees it at offset 0
        range.size,         // Size of this range
        data + range.offset // Source data at the range's offset in our buffer
    );
  }
  _vk_program->_pending_params.clear();
}

///////////////////////////////////////////////////////////////////////////////

VulkanDescriptorSetCache::VulkanDescriptorSetCache(vkcontext_rawptr_t ctx)
    : _ctxVK(ctx) {
}

///////////////////////////////////////////////////////////////////////////////

void VkPipelineObject::applyPendingUboUpdates(VkCommandBuffer cmdbuf, uint32_t frame_index) {
  // Ensure global dynamic UBO system is initialized
  extern VkDynamicUBOSystem* g_dynamic_ubo_system;
  if (!g_dynamic_ubo_system) {
    // Dynamic UBO system not initialized yet
    return;
  }

  _dynamic_offsets.clear();

  // Process all UBOs in binding order (already sorted)
  for (auto* ubo : _uniform_blocks) {
    // Allocate dynamic memory for this draw
    auto allocation = g_dynamic_ubo_system->allocate(ubo->_shadow_buffer.size(), frame_index);

    // Copy shadow buffer to dynamic allocation
    memcpy(allocation.cpu_ptr, ubo->_shadow_buffer.data(), ubo->_shadow_buffer.size());

    // Track offset for descriptor binding
    _dynamic_offsets.push_back(allocation.dynamic_offset);
  }

  // Note: The actual descriptor set binding with dynamic offsets will happen
  // in the draw call when descriptor sets are bound
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_bindGfxDescriptorSetOnSlot(
    VkCommandBuffer cmdbuf,         //
    vkdescriptorset_ptr_t desc_set, //
    size_t slot) {                  //
  OrkAssert(desc_set);
  vkCmdBindDescriptorSets(
      cmdbuf,
      VK_PIPELINE_BIND_POINT_GRAPHICS,   // pipeline bind point
      _currentPipeline->_pipelineLayout, // pipeline layout
      slot,                              // index into descriptor sets slots
      1,                                 // bind 1 descriptor set
      &desc_set->_vkdescset,             // bind 1 descriptor set
      0,                                 // dynamic offset count
      nullptr);                          // dynamic offsets
  _active_gfx_descriptorSets[slot] = desc_set;
}

///////////////////////////////////////////////////////////////////////////////

vkdescriptorset_ptr_t VulkanDescriptorSetCache::_createNewDescriptorSetForProgram(vkfxsprg_ptr_t program){
  auto current_pass = _ctxVK->_fxi->_currentVKPASS;
  auto cur_pipeline = _ctxVK->_fxi->_currentPipeline;
  OrkAssert(current_pass != nullptr);
  OrkAssert(cur_pipeline != nullptr);
  auto merged_resources = current_pass->_merged_resources;
  ////////////////////////
  // make new descriptor set
  ////////////////////////
  
  static int descset_count             = 0;
  auto descset_ptr                          = std::make_shared<VulkanDescriptorSet>();

  VkDescriptorSetAllocateInfo DSAI;
  initializeVkStruct(DSAI, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
  DSAI.descriptorPool     = _ctxVK->_vkDescriptorPool;
  DSAI.descriptorSetCount = 1;

  // Use merged resource layouts if available, otherwise fall back to legacy
  VkDescriptorSetLayout layout_to_use = VK_NULL_HANDLE;

  // Check if we have a current pipeline with merged resource layouts
  if (not cur_pipeline->_dset_layouts.empty()) {
    // Use the first merged resource layout (assuming single descriptor set for now)
    layout_to_use = cur_pipeline->_dset_layouts[0];
    logchan_vkpipb->log("Using merged resource descriptor set layout: %p", (void*)layout_to_use);
  } else {
    OrkAssert(false); // No valid descriptor set layout found - merged resources should always be available
  }

  DSAI.pSetLayouts = &layout_to_use;

  // printf("ALLOC DESC SET<%d:%p>\n", descset_count, descset_ptr.get());
  VkResult OK = vkAllocateDescriptorSets(
      _ctxVK->_vkdevice, //
      &DSAI,             //
      &descset_ptr->_vkdescset);

  descset_count++;
  switch (OK) {
    case VK_SUCCESS:
      break;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
      printf("VK_ERROR_OUT_OF_HOST_MEMORY\n");
      break;
    case VK_ERROR_OUT_OF_POOL_MEMORY:
      printf("VK_ERROR_OUT_OF_POOL_MEMORY\n");
      break;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      printf("VK_ERROR_OUT_OF_DEVICE_MEMORY\n");
      break;
    case VK_ERROR_FRAGMENTED_POOL:
      printf("VK_ERROR_FRAGMENTED_POOL\n");
      break;
    case VK_ERROR_TOO_MANY_OBJECTS:
      printf("VK_ERROR_TOO_MANY_OBJECTS\n");
      break;
    default:
      printf("VK_ERROR_UNKNOWN\n");
      break;
  }
  OrkAssert(VK_SUCCESS == OK);
  return descset_ptr;
}

///////////////////////////////////////////////////////////////////////////////

vkdescriptorset_ptr_t VulkanDescriptorSetCache::fetchDescriptorSetForProgram(vkfxsprg_ptr_t vk_program) {

  auto current_pass = _ctxVK->_fxi->_currentVKPASS;
  auto cur_pipeline = _ctxVK->_fxi->_currentPipeline;
  OrkAssert(current_pass != nullptr);
  OrkAssert(cur_pipeline != nullptr);
  auto merged_resources = current_pass->_merged_resources;
  auto shfile = vk_program->_shader_file;
  auto shname = shfile->_shader_name;
  /////////////////////
  // early exits
  /////////////////////

  if (not merged_resources) {
    return nullptr;
  }
  if (vk_program->_merged_resource_bindings.empty()) {
    return nullptr; 
  }

  /////////////////////////////////
  // todo: this is the slow path
  //    for the sake of efficiency,
  //    we will (over time) expose descriptor sets to higher level systems
  /////////////////////////////////

  uint64_t descset_bits = vk_program->samplersHash();
  auto it               = _vkDescriptorSetByHash.find(descset_bits);
  vkdescriptorset_ptr_t descset_ptr = nullptr;
  if (it != _vkDescriptorSetByHash.end()) {
    descset_ptr = it->second;
  } else {
    descset_ptr = _createNewDescriptorSetForProgram(vk_program);  
    _vkDescriptorSetByHash[descset_bits] = descset_ptr;

    ////////////////////////
    // Update descriptor set with merged resource bindings
    ////////////////////////

    static std::vector<VkDescriptorBufferInfo> buffer_infos; // Keep alive during vkUpdateDescriptorSets
    buffer_infos.clear();

    // Reserve space to prevent reallocation
    size_t estimated_buffer_count = 128; // Estimate max UBOs we might have
    buffer_infos.reserve(estimated_buffer_count);

    // First, handle textures/samplers - ensure ALL samplers from merged resources are bound
    // Build a map of what's already bound (only for texture params)
    static std::unordered_map<int, vktexobj_ptr_t> bound_textures;
    bound_textures.clear();

    for (auto it : vk_program->_merged_resource_bindings) {
      auto param                = it.first;
      auto [set_id, binding_id] = it.second;
      // Only process textures here, skip UBOs
      auto tex_it = vk_program->_textures_by_orkparam.find(param);
      if (tex_it != vk_program->_textures_by_orkparam.end()) {
        auto vk_tex                = tex_it->second;
        bound_textures[binding_id] = vk_tex;
      }
    }

    // Now iterate through ALL sampler bindings and UBO's from merged resources
    static std::vector<VkWriteDescriptorSet> descriptor_writes;
    descriptor_writes.clear();
    for (const auto& [set_id, sources] : merged_resources->descriptor_sets) {
      for (const auto& source : sources) {
        for (const auto& binding : source->bindings) {

          switch (binding->type) {
            case VkMergedResourceBinding::Type::Sampler: {
              vktexobj_ptr_t vk_tex;

              // Check if this binding is already bound
              auto bound_it = bound_textures.find(binding->binding_id);
              if (bound_it != bound_textures.end()) {
                vk_tex = bound_it->second;
              } else {
                // Use default texture for unbound samplers
                // Determine texture type from datatype string if possible
                if (binding->datatype.find("Cube") != std::string::npos) {
                  vk_tex = _ctxVK->_defaultTexImplCube;
                } else if (
                    binding->datatype.find("Array") != std::string::npos || binding->datatype.find("2DA") != std::string::npos) {
                  vk_tex = _ctxVK->_defaultTexImpl2DArray;
                } else if (binding->datatype.find("3D") != std::string::npos) {
                  vk_tex = _ctxVK->_defaultTexImpl3D;
                } else {
                  vk_tex = _ctxVK->_defaultTexImpl2D; // Default to 2D
                }
              }

              // Create descriptor write using active sampling descriptor
              auto desc_info = vk_tex->_descset_sampling;
              OrkAssert(desc_info);
              OrkAssert(desc_info->imageView != VK_NULL_HANDLE);
              OrkAssert(desc_info->sampler != VK_NULL_HANDLE);

              VkWriteDescriptorSet DWRITE = {};
              initializeVkStruct(DWRITE, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
              DWRITE.dstSet          = descset_ptr->_vkdescset;
              DWRITE.dstBinding      = binding->binding_id;
              DWRITE.descriptorCount = 1;
              DWRITE.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
              DWRITE.pImageInfo      = desc_info.get();

              descriptor_writes.push_back(DWRITE);
              break;
            } // case VkMergedResourceBinding::Type::Sampler: {
            case VkMergedResourceBinding::Type::UniformBlock: {
              // Find the corresponding VkFxShaderUniformBlk
              VkFxShaderUniformBlk* ubo_block = nullptr;

              auto it = vk_program->_vk_uniformblks.find(binding->name);
              if (it != vk_program->_vk_uniformblks.end()) {
                ubo_block = it->second.get();
              }

              if (ubo_block && ubo_block->_buffer_size > 0) {
                // Use global dynamic UBO buffer
                extern VkDynamicUBOSystem* g_dynamic_ubo_system;
                OrkAssert(g_dynamic_ubo_system != nullptr);
                auto global_buffer = g_dynamic_ubo_system->get_buffer();
                OrkAssert(global_buffer != nullptr);

                VkDescriptorBufferInfo buffer_info = {};
                buffer_info.buffer                 = global_buffer->_vkbuffer;
                buffer_info.offset                 = 0; // Dynamic offset will be provided at bind time
                buffer_info.range                  = ubo_block->_buffer_size;
                buffer_infos.push_back(buffer_info);

                VkWriteDescriptorSet DWRITE = {};
                initializeVkStruct(DWRITE, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
                DWRITE.dstSet          = descset_ptr->_vkdescset;
                DWRITE.dstBinding      = binding->binding_id;
                DWRITE.descriptorCount = 1;
                DWRITE.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                DWRITE.pBufferInfo     = &buffer_infos.back();

                descriptor_writes.push_back(DWRITE);
              }
              break;
            } // case VkMergedResourceBinding::Type::UniformBlock: {
            case VkMergedResourceBinding::Type::StorageBuffer: {
              // Find the corresponding VkFxShaderStorageBlock
              VkFxShaderStorageBlock* ssbo_block = nullptr;

              auto it = vk_program->_vk_ssbo_blocks.find(binding->name);
              if (it != vk_program->_vk_ssbo_blocks.end()) {
                ssbo_block = it->second.get();
              }

              if (ssbo_block && ssbo_block->_buffer_size > 0) {
                // Check if a buffer is bound
                VkBuffer vk_buffer = VK_NULL_HANDLE;
                VkDeviceSize buffer_size = ssbo_block->_buffer_size;

                if (ssbo_block->_bound_buffer) {
                  vk_buffer = ssbo_block->_bound_buffer->_vkbuffer;
                  buffer_size = ssbo_block->_bound_buffer->_length;
                } else {
                  // Create a default buffer if none is bound
                  // This is just a placeholder - real app should bind proper buffer
                  if(1)printf("WARNING: No SSBO bound for block '%p:%s', skipping descriptor update. tek<%s> sh<%s>\n", //
                              (void*) ssbo_block,
                              binding->name.c_str(), //
                              vk_program->_tek_name.c_str(), //
                              shname.c_str());  //
                  break;
                }

                VkDescriptorBufferInfo buffer_info = {};
                buffer_info.buffer = vk_buffer;
                buffer_info.offset = 0;
                buffer_info.range = buffer_size;
                buffer_infos.push_back(buffer_info);

                VkWriteDescriptorSet DWRITE = {};
                initializeVkStruct(DWRITE, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
                DWRITE.dstSet = descset_ptr->_vkdescset;
                DWRITE.dstBinding = binding->binding_id;
                DWRITE.descriptorCount = 1;
                DWRITE.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                DWRITE.pBufferInfo = &buffer_infos.back();

                descriptor_writes.push_back(DWRITE);
              }
              break;
            } // case VkMergedResourceBinding::Type::StorageBuffer: {
            default:
              // Ignore other types for now
              break;
          } // switch (binding->type) {
        } // for (const auto& binding : source->bindings) {
      } // for (const auto& source : sources) {
    } // for (const auto& [set_id, sources] : merged_resources->descriptor_sets) {

    // Update all descriptors at once
    if (!descriptor_writes.empty()) {
      vkUpdateDescriptorSets(_ctxVK->_vkdevice, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }
  }

  return descset_ptr;
}

///////////////////////////////////////////////////////////////////////////////

VkFxShaderProgram::VkFxShaderProgram(VkFxShaderFile* file)
    : _shader_file(file) {
  _pushdatabuffer.reserve(1024); // todo : grow as needed
  _incr_crc64.init();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
