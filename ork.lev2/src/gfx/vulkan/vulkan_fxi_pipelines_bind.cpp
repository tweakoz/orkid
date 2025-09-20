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

    // printf( "SETVP<%p> x<%f> y<%f> w<%f> h<%f>\n", pipeline.get(), vkvp.x, vkvp.y, vkvp.width, vkvp.height);
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
    // printf( "SETSC<%p> x<%d> y<%d> w<%d> h<%d>\n", pipeline.get(), vksc.offset.x, vksc.offset.y, vksc.extent.width,
    // vksc.extent.height);
    vkCmdSetScissor(
        cmdbuf, // command buffer
        0,      // first scissor
        1,      // scissor count
        &vksc); // scissor data
  }

  ////////////////////////////////////////
  // upload descriptor sets and push constants
  ////////////////////////////////////////

  _uploadPipelineData(cmdbuf, pipeline);
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_uploadPipelineData(VkCommandBuffer CB, vkpipeline_obj_ptr_t pipeline) {
  auto prog = _currentVKPASS->_vk_program;

  // Apply dynamic UBO updates for this draw
  // This allocates per-draw memory and copies shadow buffers
  static uint32_t frame_index = 0; // TODO: Get actual frame index from swapchain
  pipeline->applyPendingUboUpdates(CB, frame_index);

  // Flush uniform blocks BEFORE fetching descriptor set
  // This ensures the GPU buffers have the correct data when bound
  // Note: With dynamic UBOs, this may become unnecessary
  _flushDirtyUniformBlocks();

  if (prog->_tek_name == "FWD_DEPTHPREPASS_RI_NI_MO") {
    // OrkBreak();
  }
  auto desc_set = pipeline->_descriptorSetCache->fetchDescriptorSetForProgram(prog);
  if (desc_set) {
    // Bind descriptor set with dynamic offsets from applyPendingUboUpdates
    if (!pipeline->_dynamic_offsets.empty()) {
      vkCmdBindDescriptorSets(
          CB,
          VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline->_pipelineLayout,
          0, // first set
          1, // set count
          &desc_set->_vkdescset,
          pipeline->_dynamic_offsets.size(),
          pipeline->_dynamic_offsets.data());
    } else {
      // Fallback to static binding if no dynamic offsets
      _bindGfxDescriptorSetOnSlot(CB, desc_set, 0);
    }
  }

  pipeline->applyPendingPushConstants(CB);
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_flushRenderPassScopedState() {
  for (int slot = 0; slot < 4; slot++) {
    _active_vbs[slot]                = nullptr;
    _active_gfx_descriptorSets[slot] = nullptr;
  }
  _currentPipeline = nullptr;
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

  OrkAssert(_vk_program->_pushConstantBlock != nullptr);
  size_t num_params = _vk_program->_pending_params.size();

  auto data_layout = _vk_program->_pushConstantBlock->_data_layout;
  auto& ranges     = _vk_program->_pushConstantBlock->_ranges;
  size_t blocksize = _vk_program->_pushConstantBlock->_blockSize;

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

vkdescriptorset_ptr_t VulkanDescriptorSetCache::fetchDescriptorSetForProgram(vkfxsprg_ptr_t program) {

  /////////////////////////////////
  // todo: this is the slow path
  //    for the sake of efficiency,
  //    we will (over time) expose descriptor sets to higher level systems
  /////////////////////////////////

  // Check if program has any merged resource bindings
  if (program->_merged_resource_bindings.empty()) {
    logchan_vkpipb->log("Program<%s> has no merged resource bindings - returning null descriptor set", program->_tek_name.c_str());
    return nullptr; // No descriptor sets needed for push constants only
  }

  boost::Crc64 crc64;
  crc64.init();

  // Include merged resource bindings in hash calculation
  for (auto it : program->_merged_resource_bindings) {
    auto param                = it.first;
    auto [set_id, binding_id] = it.second;

    crc64.accumulateItem(set_id);
    crc64.accumulateItem(binding_id);

    // Check if this is a texture binding
    auto tex_it = program->_textures_by_orkparam.find(param);
    if (tex_it != program->_textures_by_orkparam.end()) {
      auto vk_tex  = tex_it->second;
      auto img_obj = vk_tex->_imgobj;
      crc64.accumulateItem(vk_tex.get());
      crc64.accumulateItem(img_obj.get());
      crc64.accumulateItem(vk_tex->_image_params_hash);
      crc64.accumulateItem(vk_tex->_vkdescriptor_info.imageView);
    } else {
      // For UBOs, just use the param pointer as part of the hash
      crc64.accumulateItem(param);
    }
  }

  crc64.finish();
  uint64_t descset_bits = crc64.result();
  // printf( "dscache<%p> descset_bits<%016llx>\n", this, descset_bits );
  auto it                           = _vkDescriptorSetByHash.find(descset_bits);
  vkdescriptorset_ptr_t descset_ptr = nullptr;
  if (it != _vkDescriptorSetByHash.end()) {
    descset_ptr = it->second;
  } else {
    // make new descriptor set
    static int descset_count             = 0;
    descset_ptr                          = std::make_shared<VulkanDescriptorSet>();
    _vkDescriptorSetByHash[descset_bits] = descset_ptr;

    VkDescriptorSetAllocateInfo DSAI;
    initializeVkStruct(DSAI, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
    DSAI.descriptorPool     = _ctxVK->_vkDescriptorPool;
    DSAI.descriptorSetCount = 1;

    // Use merged resource layouts if available, otherwise fall back to legacy
    VkDescriptorSetLayout layout_to_use = VK_NULL_HANDLE;

    // Check if we have a current pipeline with merged resource layouts
    if (_ctxVK->_fxi->_currentPipeline && !_ctxVK->_fxi->_currentPipeline->_dset_layouts.empty()) {
      // Use the first merged resource layout (assuming single descriptor set for now)
      layout_to_use = _ctxVK->_fxi->_currentPipeline->_dset_layouts[0];
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
  }
  // Update descriptor set with merged resource bindings
  std::vector<VkWriteDescriptorSet> descriptor_writes;
  std::vector<VkDescriptorBufferInfo> buffer_infos; // Keep alive during vkUpdateDescriptorSets

  // Reserve space to prevent reallocation
  size_t estimated_buffer_count = 128; // Estimate max UBOs we might have
  buffer_infos.reserve(estimated_buffer_count);

  // First, handle textures/samplers - ensure ALL samplers from merged resources are bound
  // Build a map of what's already bound (only for texture params)
  std::map<int, vktexobj_ptr_t> bound_textures;
  for (auto it : program->_merged_resource_bindings) {
    auto param                = it.first;
    auto [set_id, binding_id] = it.second;
    // Only process textures here, skip UBOs
    auto tex_it = program->_textures_by_orkparam.find(param);
    if (tex_it != program->_textures_by_orkparam.end()) {
      auto vk_tex                = tex_it->second;
      bound_textures[binding_id] = vk_tex;
    }
  }

  // Now iterate through ALL sampler bindings from merged resources
  if (_ctxVK->_fxi->_currentVKPASS && _ctxVK->_fxi->_currentVKPASS->_merged_resources) {
    auto merged_resources = _ctxVK->_fxi->_currentVKPASS->_merged_resources;

    for (const auto& [set_id, sources] : merged_resources->descriptor_sets) {
      for (const auto& source : sources) {
        for (const auto& binding : source->bindings) {
          if (binding->type == VkMergedResourceBinding::Type::Sampler) {
            vktexobj_ptr_t vk_tex;

            // Check if this binding is already bound
            auto bound_it = bound_textures.find(binding->binding_id);
            if (bound_it != bound_textures.end()) {
              vk_tex = bound_it->second;
              logchan_vkpipb->log(
                  "update descset (merged): set<%d> bidx<%d> tex<%p> name<%s>",
                  set_id,
                  binding->binding_id,
                  (void*)vk_tex.get(),
                  binding->name.c_str());
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
              logchan_vkpipb->log(
                  "update descset (default): set<%d> bidx<%d> tex<%p> name<%s> type<%s>",
                  set_id,
                  binding->binding_id,
                  (void*)vk_tex.get(),
                  binding->name.c_str(),
                  binding->datatype.c_str());
            }

            // Create descriptor write
            auto& desc_info = vk_tex->_vkdescriptor_info;
            OrkAssert(desc_info.imageView != VK_NULL_HANDLE);

            VkWriteDescriptorSet DWRITE = {};
            initializeVkStruct(DWRITE, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
            DWRITE.dstSet          = descset_ptr->_vkdescset;
            DWRITE.dstBinding      = binding->binding_id;
            DWRITE.descriptorCount = 1;
            DWRITE.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            DWRITE.pImageInfo      = &desc_info;

            descriptor_writes.push_back(DWRITE);
          }
        }
      }
    }
  }

  // Now handle UBOs from merged resources
  if (1)
    logchan_vkpipb->log("UBO_DESC_CHECK: _currentVKPASS<%p>", (void*)_ctxVK->_fxi->_currentVKPASS.get());
  if (_ctxVK->_fxi->_currentVKPASS) {
    if (1)
      logchan_vkpipb->log("UBO_DESC_CHECK: _merged_resources<%p>", (void*)_ctxVK->_fxi->_currentVKPASS->_merged_resources.get());
  }
  if (_ctxVK->_fxi->_currentVKPASS && _ctxVK->_fxi->_currentVKPASS->_merged_resources) {
    auto merged_resources = _ctxVK->_fxi->_currentVKPASS->_merged_resources;
    auto vk_program       = _ctxVK->_fxi->_currentVKPASS->_vk_program;
    if (1)
      logchan_vkpipb->log(
          "UBO_DESC_CHECK: Found merged_resources with %zu descriptor sets", merged_resources->descriptor_sets.size());

    for (const auto& [set_id, sources] : merged_resources->descriptor_sets) {
      for (const auto& source : sources) {
        for (const auto& binding : source->bindings) {
          if (1)
            logchan_vkpipb->log(
                "UBO_DESC_CHECK: Binding<%s> type<%d> UniformBlock=%d",
                binding->name.c_str(),
                (int)binding->type,
                (int)VkMergedResourceBinding::Type::UniformBlock);
          if (binding->type == VkMergedResourceBinding::Type::UniformBlock) {
            // Find the corresponding VkFxShaderUniformBlk
            VkFxShaderUniformBlk* ubo_block = nullptr;

            // Search in the program's uniform blocks
            if (1)
              logchan_vkpipb->log("UBO_DESC_CHECK: Looking for UBO<%s> in program's _vk_uniformblks", binding->name.c_str());
            auto it = vk_program->_vk_uniformblks.find(binding->name);
            if (it != vk_program->_vk_uniformblks.end()) {
              ubo_block = it->second.get();
              if (1)
                logchan_vkpipb->log("UBO_DESC_CHECK: Found UBO<%s> ptr<%p>", binding->name.c_str(), (void*)ubo_block);
            } else {
              if (1)
                logchan_vkpipb->log("UBO_DESC_CHECK: UBO<%s> NOT FOUND in _vk_uniformblks", binding->name.c_str());
            }

            if (ubo_block) {
              if (1)
                logchan_vkpipb->log("UBO_DESC_CHECK: UBO<%s> _buffer_size<%zu>", binding->name.c_str(), ubo_block->_buffer_size);
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

              logchan_vkpipb->log(
                  "UBO_DESC_UPDATE: ubo<%s> binding<%d> global_buffer<%p> size<%zu> block_ptr<%p>",
                  binding->name.c_str(),
                  binding->binding_id,
                  (void*)global_buffer->_vkbuffer,
                  ubo_block->_buffer_size,
                  (void*)ubo_block);

              descriptor_writes.push_back(DWRITE);
            } else if (ubo_block) {
              if (0)
                logchan_vkpipb->log("UBO_DESC_CHECK: SKIPPING UBO<%s> - zero size", binding->name.c_str());
            }
          }
        }
      }
    }
  }

  // Update all descriptors at once
  if (!descriptor_writes.empty()) {
    vkUpdateDescriptorSets(_ctxVK->_vkdevice, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);

    // Append descriptor set update info to pipeline report if report filename is stored
    if (0 and _ctxVK->_fxi->_currentPipeline && !_ctxVK->_fxi->_currentPipeline->_report_filename.empty()) {
      // Append update info to report file
      FILE* fp = fopen(_ctxVK->_fxi->_currentPipeline->_report_filename.c_str(), "a");
      if (fp) {
        static int update_count = 0;
        fprintf(fp, "\n## Descriptor Set Update %d (%p)\n\n", update_count++, (void*)descset_ptr->_vkdescset);
        fprintf(fp, "**Update contains %zu writes**\n\n", descriptor_writes.size());
        fprintf(fp, "```\n");
        fprintf(fp, "Bind | Type    | Resource\n");
        fprintf(fp, "-----|---------|--------------------------------\n");

        // Sort writes by binding ID for comparison with layout
        std::vector<std::tuple<int, std::string, std::string>> updates;

        for (const auto& write : descriptor_writes) {
          std::string type_str;
          std::string resource_str;

          if (write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            type_str = "Sampler";
            // Find the texture param name
            for (auto it : program->_merged_resource_bindings) {
              auto [set_id, binding_id] = it.second;
              if (binding_id == write.dstBinding) {
                resource_str = it.first->_name;
                break;
              }
            }
          } else if (write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            type_str = "UBO";
            // Find UBO name from merged resources
            if (_ctxVK->_fxi->_currentVKPASS && _ctxVK->_fxi->_currentVKPASS->_merged_resources) {
              auto merged_resources = _ctxVK->_fxi->_currentVKPASS->_merged_resources;
              for (const auto& [set_id, sources] : merged_resources->descriptor_sets) {
                for (const auto& source : sources) {
                  for (const auto& binding : source->bindings) {
                    if (binding->binding_id == write.dstBinding && binding->type == VkMergedResourceBinding::Type::UniformBlock) {
                      resource_str = binding->name;
                      break;
                    }
                  }
                  if (!resource_str.empty())
                    break;
                }
                if (!resource_str.empty())
                  break;
              }
            }
          }

          updates.push_back(std::make_tuple(write.dstBinding, type_str, resource_str));
        }

        std::sort(updates.begin(), updates.end(), [](const auto& a, const auto& b) { return std::get<0>(a) < std::get<0>(b); });

        for (const auto& [bind_id, type, resource] : updates) {
          fprintf(fp, "%4d | %-7s | %s\n", bind_id, type.c_str(), resource.c_str());
        }
        fprintf(fp, "```\n\n");

        // Compare with expected layout
        fprintf(fp, "### Binding Verification\n\n");
        fprintf(fp, "Comparing descriptor set updates with layout creation to identify mismatches.\n\n");

        fclose(fp);
        logchan_vkpipb->log("Appended descriptor set update to pipeline report");
      }
    }
  }

  return descset_ptr;
}

///////////////////////////////////////////////////////////////////////////////

VkFxShaderProgram::VkFxShaderProgram(VkFxShaderFile* file)
    : _shader_file(file) {
  _pushdatabuffer.reserve(1024); // todo : grow as needed
}

///////////////////////////////////////////////////////////////////////////////

void VkFxShaderProgram::bindDescriptorTexture(fxparam_constptr_t param, const Texture* pTex) {
  if (pTex) {
    vktexobj_ptr_t vk_tex;
    if (auto as_to = pTex->_impl.tryAsShared<VulkanTextureObject>()) {
      vk_tex = as_to.value();
    } else {
      // printf("No Texture impl tex<%p:%s>\n", pTex, pTex->_debugName.c_str());
      return;
    }

    // Store the texture object for merged resource binding
    _textures_by_orkparam[param] = vk_tex;

    // If this is a texture array, ensure the descriptor info is set up correctly
    if (pTex->_texType == ETEXTYPE_2D_ARRAY) {
      // The image view should already be configured as VK_IMAGE_VIEW_TYPE_2D_ARRAY
      // from initTextureArray2DFromData
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
