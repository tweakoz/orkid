////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include "ork/orkstd.h"
#include "vulkan_ubo_dynamic.h"
#include "vulkan_ub_layout.inl"
#include "vulkan_desclife.h"
#include <cstddef>
#include <ork/lev2/gfx/shadman.h>
#include <ork/util/hexdump.inl>
#include <ctime>
#include <cmath>
#include <boost/filesystem.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_vkpipb = logger()->configureChannel("VKPIPB", fvec3(1, 1, .2), false);

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_bindPipeline(VkCommandBuffer cmdbuf, vkpipelinestate_rawptr_t pipeline) {

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

    if(0)printf( "SETVP<%p> x<%f> y<%f> w<%f> h<%f>\n", pipeline, vkvp.x, vkvp.y, vkvp.width, vkvp.height);
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
    if(0)printf( "SETSC<%p> x<%d> y<%d> w<%d> h<%d>\n", pipeline, vksc.offset.x, vksc.offset.y, vksc.extent.width, vksc.extent.height);
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

  // Dynamic cull mode is set in VkFxInterface::applyRasterState, which
  // runs after per-draw state lambdas have mutated the material rasterstate.

  ////////////////////////////////////////
  // set dynamic depth-write (only for pipelines that DECLARED it dynamic)
  //
  // The material's writemaskZ is only half the answer: an RTG whose depth
  // attachment was flipped to DEPTH_READ_ONLY_OPTIMAL (FBI::transitionDepth-
  // ForSampling, so the color pass can SAMPLE the prepass depth) forbids depth
  // writes for the whole pass — VUID-vkCmdDraw-None-06886. Every opaque
  // material bakes writemaskZ=true, so the mask has to be ANDed with the pass
  // here rather than branched per-material. Outside a read-only pass this
  // reproduces exactly what the pipeline baked (same effective rasterstate
  // resolution _fetchPipeline* used).
  //
  // The GATE is not an optimization: issuing vkCmdSetDepthWriteEnable while a
  // pipeline that bakes depth-write statically is bound is itself invalid
  // (VUID-vkCmd*-None-08608, which this call reported on every classic, indexed,
  // indirect and mesh draw before the gate existed). Today only the mesh path
  // declares the state; the other paths keep their baked value.
  ////////////////////////////////////////

  if (pipeline->_dynamicDepthWrite) {
    auto rtg_impl        = fbi->_active_rtgroup->_impl.getShared<VkRtGroupImpl>();
    auto eff_rasterstate = _effectiveRasterState();
    bool depth_writemask = eff_rasterstate ? eff_rasterstate->_writemaskZ : false;
    VkBool32 depth_write = (depth_writemask and not rtg_impl->_depthReadOnlyMode) ? VK_TRUE : VK_FALSE;
    _contextVK->_vkCmdSetDepthWriteEnableEXT(cmdbuf, depth_write);
  }

  ////////////////////////////////////////
  // upload ubo data and push constants
  ////////////////////////////////////////

  _uploadPipelineData(cmdbuf, pipeline);

  ////////////////////////////////////////
  // bind descriptor set (if changed)
  ////////////////////////////////////////
  auto prog = _currentVKPASS;
  auto desc_set = pipeline->_descriptorSetCache->fetchDescriptorSetForProgram(prog);

  if (desc_set) {
    // Bind descriptor set with dynamic offsets from applyPendingUboUpdates
    if (!_dynamic_offsets.empty()) {
      vkCmdBindDescriptorSets(
          cmdbuf,
          VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline->_pipelineLayout,
          0, // first set
          1, // set count
          &desc_set->_vkdescset,
          _dynamic_offsets.size(),
          _dynamic_offsets.data());
    } else {
      // Fallback to static binding if no dynamic offsets
      _bindGfxDescriptorSetOnSlot(cmdbuf, desc_set, 0);
    }
  }

}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_uploadPipelineData(VkCommandBuffer CB, vkpipelinestate_rawptr_t pipeline) {
  
  // Apply dynamic UBO updates for this draw
  // This allocates per-draw memory and copies shadow buffers
  static uint32_t frame_index = 0; // TODO: Get actual frame index from swapchain
  pipeline->applyPendingUboUpdates(CB, frame_index);

  pipeline->applyPendingPushConstants(CB);
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_bindVertexBufferOnSlot(VkCommandBuffer cmdbuf, vkvtxbuf_ptr_t vb, size_t slot) {
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(
      cmdbuf,                    // command buffer
      slot,                      // slot to bind to
      1,                         // binding count
      &vb->_vkbuffer->_vkbuffer, // buffers
      &offset);                  // offsets
  _active_vbs[slot] = vb;
}

///////////////////////////////////////////////////////////////////////////////

void VkPipelineState::applyPendingPushConstants(VkCommandBuffer cmdbuf) {
  auto& pending_params = _shader_state->_pending_params;
  auto& pushdatabuffer = _shader_state->_pushdatabuffer;

  auto* shader = _shader_state->_shader;
  if(not shader->_pushConstantBlock){
    return;
  }

  auto& data_layout = shader->_pushConstantBlock->_data_layout;
  auto& ranges      = shader->_pushConstantBlock->_ranges;

  if(ranges.size()==0) return;

  size_t blocksize = shader->_pushConstantBlock->_blockSize;

  size_t num_params = pending_params.size();

  if(pushdatabuffer.size() < blocksize)
    pushdatabuffer.resize(blocksize, 0);

  auto* data = pushdatabuffer.data();

  for (auto item : pending_params) {
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
  pending_params.clear();
}

///////////////////////////////////////////////////////////////////////////////

VulkanDescriptorSetCacheState::VulkanDescriptorSetCacheState(vkcontext_rawptr_t ctx)
    : _ctxVK(ctx) {
}

///////////////////////////////////////////////////////////////////////////////

void VkPipelineState::applyPendingUboUpdates(VkCommandBuffer cmdbuf, uint32_t frame_index) {
  // Ensure global dynamic UBO system is initialized
  extern VkDynamicUBOSystem* g_dynamic_ubo_system;
  if (!g_dynamic_ubo_system) {
    // Dynamic UBO system not initialized yet
    return;
  }

  _descriptorSetCache->_ctxVK->_fxi->_dynamic_offsets.clear();

  static int log_count = 0;
  bool do_log = (log_count++ < 100);

  // Process all UBOs in binding order (already sorted)
  for (auto* ubo_state : _shader_state->_ordered_uniform_states) {
    // Allocate dynamic memory for this draw
    auto allocation = g_dynamic_ubo_system->allocate(ubo_state->_shadow_buffer.size(), frame_index);

    // Debug: check shadow buffer before copy
    if (do_log && ubo_state->_shadow_buffer.size() >= 16) {
      float* fdata = (float*)ubo_state->_shadow_buffer.data();
      bool has_nan = std::isnan(fdata[0]) || std::isnan(fdata[1]) || std::isnan(fdata[2]) || std::isnan(fdata[3]);
      if (has_nan) {
        auto* ubo = ubo_state->_shader_uniform_block;
        logchan_vkpipb->log("WARN: UBO<%s> shadow_buffer has NaN! [%g %g %g %g] size=%zu",
                            ubo->_orkparamblock ? ubo->_orkparamblock->_name.c_str() : "?",
                            fdata[0], fdata[1], fdata[2], fdata[3],
                            ubo_state->_shadow_buffer.size());
      }
    }

    // Copy shadow buffer to dynamic allocation
    memcpy(allocation.cpu_ptr, ubo_state->_shadow_buffer.data(), ubo_state->_shadow_buffer.size());

    // Track offset for descriptor binding
    _descriptorSetCache->_ctxVK->_fxi->_dynamic_offsets.push_back(allocation.dynamic_offset);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_bindGfxDescriptorSetOnSlot(
    VkCommandBuffer cmdbuf,
    vkdescriptorsetstate_ptr_t desc_set,
    size_t slot) {
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

vkdescriptorsetstate_ptr_t VulkanDescriptorSetCacheState::_createNewDescriptorSetForProgram(vkfxshaderpass_rawptr_t program){
  auto current_pass = _ctxVK->_fxi->_currentVKPASS;
  auto cur_pipeline = _ctxVK->_fxi->_currentPipeline;
  OrkAssertI(current_pass != nullptr, "current pass is null");
  OrkAssertI(cur_pipeline != nullptr, "current pipeline is null");
  auto merged_resources = current_pass->_merged_resources;
  ////////////////////////
  // make new descriptor set
  ////////////////////////
  
  static int descset_count = 0;
  auto descset_ptr         = std::make_shared<VulkanDescriptorSetState>();

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

vkdescriptorsetstate_ptr_t VulkanDescriptorSetCacheState::fetchDescriptorSetForProgram(vkfxshaderpass_rawptr_t vk_program) {

  auto current_pass = _ctxVK->_fxi->_currentVKPASS;
  auto cur_pipeline = _ctxVK->_fxi->_currentPipeline;
  OrkAssertI(current_pass != nullptr, "current pass is null");
  OrkAssertI(cur_pipeline != nullptr, "current pipeline is null");
  auto merged_resources = current_pass->_merged_resources;
  auto shfile = vk_program->_shader_file;
  auto shname = shfile->_shader_name;
  /////////////////////
  // early exits
  /////////////////////

  if (not merged_resources) {
    return nullptr;
  }
  // Need descriptor set if we have param bindings OR SSBO resources
  if (vk_program->_merged_resource_bindings.empty() && !vk_program->_has_ssbo_resources) {
    return nullptr;
  }

  /////////////////////////////////
  // todo: this is the slow path
  //    for the sake of efficiency,
  //    we will (over time) expose descriptor sets to higher level systems
  /////////////////////////////////

  auto shader_state = _ctxVK->_fxi->_current_shader_pass_state;
  OrkAssertI(shader_state != nullptr, "shader state is null");

  uint64_t descset_bits = _ctxVK->_fxi->_current_shader_pass_state->samplersHash();
  // BOUND STORAGE BUFFERS ARE PART OF THE DESCRIPTOR SET, so they must be
  // part of its cache key. Keyed on samplers alone, N renderer instances
  // binding their own vertex SSBOs against one SHARED material all hit the
  // FIRST instance's cached set — every particle trail drew slot 1's buffer
  // (the "only one fireball" bug). Buffers are stable per instance, so this
  // stays fully cached (one set per distinct buffer combination).
  for (auto& [ssbo_name, ssbo_blk] : vk_program->_vk_ssbo_blocks) {
    auto* ssbo_state = _ctxVK->_fxi->storageStateForBlock(ssbo_blk.get());
    uint64_t bufbits = (ssbo_state and ssbo_state->_bound_buffer) //
                           ? uint64_t(ssbo_state->_bound_buffer->_vkbuffer)
                           : 0ull;
    descset_bits = descset_bits * 0x9E3779B97F4A7C15ull + bufbits; // hash-combine
    // OFFSET is part of the descriptor identity (sub-range bind): same buffer, distinct offset ->
    // distinct set, else the second tier draw reuses the first tier's offset-0 descriptor.
    uint64_t offbits = ssbo_state ? uint64_t(ssbo_state->_bound_offset) : 0ull;
    descset_bits = descset_bits * 0x9E3779B97F4A7C15ull + offbits;
  }
  auto it               = _vkDescriptorSetByHash.find(descset_bits);
  vkdescriptorsetstate_ptr_t descset_ptr = nullptr;
  if (it != _vkDescriptorSetByHash.end()) {
    descset_ptr = it->second;
    ////////////////////////////////////////////////////////////
    // DESCLIFE FETCH — the discriminator. Reports what the PASS STATE holds
    // for the env-specular samplers right now, against the set the cache is
    // about to hand back. Pass state holding the dead view => the defect is
    // upstream of the cache (stale texture served to the binder). Pass state
    // holding a LIVE view while the cached set was written with a dead one
    // => the defect is the cache entry itself.
    // Change-gated: this is the per-draw path, and only transitions matter —
    // the last line logged for a (set,param) IS its current value.
    ////////////////////////////////////////////////////////////
    if (desclifeEnabled()) {
      for (const auto& [param, descb] : vk_program->_merged_resource_bindings) {
        const auto& pname = param->_name;
        if (pname.find("MapSpecularEnv") == std::string::npos)
          continue;
        VulkanTextureObject* vktex = nullptr;
        auto tex_it                = shader_state->_textures_by_orkparam.find(param);
        if (tex_it != shader_state->_textures_by_orkparam.end())
          vktex = tex_it->second.get();
        auto sample_img       = vktex ? vktex->samplingImage() : nullptr;
        VkImageView held_view = sample_img ? sample_img->_vkimageview : VK_NULL_HANDLE;
        size_t held_sn        = sample_img ? sample_img->_serial_number : 0;
        static std::map<std::pair<void*, std::string>, std::pair<void*, size_t>> last_seen;
        auto key = std::make_pair((void*)descset_ptr->_vkdescset, pname);
        auto now = std::make_pair((void*)held_view, held_sn);
        auto le  = last_seen.find(key);
        if (le == last_seen.end() or le->second != now) {
          last_seen[key] = now;
          printf(
              "[DESCLIFE] FETCH-HIT set<%p> key<0x%016llx> prog<%p> b<%u> param<%s> heldview<%p> heldsn<%zu> frame<%zu>\n",
              (void*)descset_ptr->_vkdescset,
              (unsigned long long)descset_bits,
              (void*)vk_program,
              descb._binding_id,
              pname.c_str(),
              (void*)held_view,
              held_sn,
              desclifeFrame(_ctxVK));
        }
      }
    }
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

    // Patched image infos: when an RTG depth texture is sampled while simultaneously
    // bound as a read-only depth attachment, its actual layout is DEPTH_READ_ONLY_OPTIMAL,
    // not SHADER_READ_ONLY_OPTIMAL. Vulkan requires VkDescriptorImageInfo::imageLayout to
    // match the actual layout (VUID-VkDescriptorImageInfo-imageLayout-00344).
    static std::vector<VkDescriptorImageInfo> image_infos;
    image_infos.clear();
    image_infos.reserve(256);

    // First, handle textures/samplers - ensure ALL samplers from merged resources are bound
    // Build a map of what's already bound (only for texture params)
    static std::unordered_map<int, VulkanTextureObject*> bound_textures;
    bound_textures.clear();

    for (auto& [param, vktex] : shader_state->_textures_by_orkparam) {
      auto binding_it = vk_program->_merged_resource_bindings.find(param);
      if (binding_it != vk_program->_merged_resource_bindings.end()) {
        auto [set_id, binding_id] = binding_it->second;
        if (!vktex) continue;
        bound_textures[binding_id] = vktex.get();
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
              VulkanTextureObject* vk_tex = nullptr;

              // Check if this binding is already bound
              auto bound_it = bound_textures.find(binding->binding_id);
              if (bound_it != bound_textures.end()) {
                vk_tex = bound_it->second;
              } else {
                // Use default texture for unbound samplers
                // Determine texture type from datatype string if possible
                if (binding->datatype.find("Cube") != std::string::npos) {
                  vk_tex = _ctxVK->_defaultTexImplCube.get();
                } else if (
                    binding->datatype.find("Array") != std::string::npos || binding->datatype.find("2DA") != std::string::npos) {
                  vk_tex = _ctxVK->_defaultTexImpl2DArray.get();
                } else if (binding->datatype.find("3D") != std::string::npos) {
                  vk_tex = _ctxVK->_defaultTexImpl3D.get();
                } else {
                  vk_tex = _ctxVK->_defaultTexImpl2D.get(); // Default to 2D
                }
              }

              // Create descriptor write using active sampling descriptor
              auto desc_info = vk_tex->_descset_sampling;
              OrkAssert(desc_info);
              OrkAssert(desc_info->imageView != VK_NULL_HANDLE);
              OrkAssert(desc_info->sampler != VK_NULL_HANDLE);

              // Copy the image info and patch imageLayout to match the actual
              // current layout of the underlying image. This is required when
              // an RTG depth texture is simultaneously bound as a read-only
              // depth attachment — actual layout is DEPTH_READ_ONLY_OPTIMAL,
              // not SHADER_READ_ONLY_OPTIMAL.
              OrkAssert(image_infos.size() < image_infos.capacity());
              image_infos.push_back(*desc_info);
              VkDescriptorImageInfo& patched = image_infos.back();
              auto sample_img = vk_tex->samplingImage();
              if (sample_img) {
                VkImageLayout actual = sample_img->_currentLayout;
                if (actual == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL) {
                  patched.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
                }
              }

              VkWriteDescriptorSet DWRITE = {};
              initializeVkStruct(DWRITE, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
              DWRITE.dstSet          = descset_ptr->_vkdescset;
              DWRITE.dstBinding      = binding->binding_id;
              DWRITE.descriptorCount = 1;
              DWRITE.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
              DWRITE.pImageInfo      = &patched;

              // DESCLIFE WRITE — the moment this set CAPTURES a view. Cache
              // miss only, so one line per sampler per distinct key.
              if (desclifeEnabled()) {
                printf(
                    "[DESCLIFE] WRITE set<%p> key<0x%016llx> prog<%p> b<%u> param<%s> view<%p> imgsn<%zu> tex<%p> frame<%zu>\n",
                    (void*)descset_ptr->_vkdescset,
                    (unsigned long long)descset_bits,
                    (void*)vk_program,
                    binding->binding_id,
                    binding->name.c_str(),
                    (void*)patched.imageView,
                    sample_img ? sample_img->_serial_number : 0,
                    (void*)vk_tex,
                    desclifeFrame(_ctxVK));
              }

              descriptor_writes.push_back(DWRITE);
              break;
            } // case VkMergedResourceBinding::Type::Sampler: {
            case VkMergedResourceBinding::Type::UniformBlock: {
              // Find the corresponding VkFxShaderUniformBlk
              VkFxShaderUniformBlock* ubo_block = nullptr;

              auto it = vk_program->_vk_uniformblks.find(binding->name);
              if (it != vk_program->_vk_uniformblks.end()) {
                ubo_block = it->second.get();
              }

              if (ubo_block && ubo_block->_buffer_size > 0) {

                const bool nondynamic = isNonDynamicUniformBlock(binding->name);

                VkBuffer vk_ubo_buffer = VK_NULL_HANDLE;

                if (nondynamic) {
                  ///////////////////////////////////////////////////////////////
                  // N = 1. THIS IS A SINGLE-BUFFER BIND WITH NO FRAME SLOTS.
                  //
                  // It is only legal because the frame model fully synchronizes
                  // CPU and GPU once per frame on BOTH output paths (the present
                  // path waits its frame fence, the offscreen path waits inside
                  // submit), so the frame that writes this buffer cannot be in
                  // flight while the next frame overwrites it.
                  //
                  // IF THAT EVER CHANGES — if frames become genuinely overlapped
                  // — this block needs N=2 (per-frame-slot buffers) AND a frame-
                  // slot bit folded into the descriptor-set cache key, or draws
                  // will read the wrong frame's sun. Precedent for the key work:
                  // the SSBO buffer/offset hash-combine in
                  // fetchDescriptorSetForProgram just below.
                  ///////////////////////////////////////////////////////////////
                  auto it_buf = _ctxVK->_fxi->_nondynamic_ubo_buffers.find(binding->name);
                  if (it_buf != _ctxVK->_fxi->_nondynamic_ubo_buffers.end() and it_buf->second) {
                    vk_ubo_buffer = it_buf->second->_vkbuffer;
                  } else {
                    // DECLARED BUT NEVER BOUND. Real case: the cloud-deck material
                    // (cloudlayermtl, drawn from the sun-cookie prologue) inherits
                    // lib_fwd and so declares ublk_sun, but no lambda binds the sun
                    // buffer for it. Under the dynamic path such a program read its
                    // own all-zero shadow buffer every draw — has_sun==0, the shader's
                    // no-op branch. Handing it the LIVE sun buffer instead would
                    // change what it renders, so it keeps reading zeros: one shared
                    // zero-filled buffer, allocated on demand per block size.
                    vk_ubo_buffer = _ctxVK->_fxi->_zeroUniformBuffer(ubo_block->_buffer_size)->_vkbuffer;
                  }
                } else {
                  // Use global dynamic UBO buffer
                  extern VkDynamicUBOSystem* g_dynamic_ubo_system;
                  OrkAssert(g_dynamic_ubo_system != nullptr);
                  auto global_buffer = g_dynamic_ubo_system->get_buffer();
                  OrkAssert(global_buffer != nullptr);
                  vk_ubo_buffer = global_buffer->_vkbuffer;
                }

                VkDescriptorBufferInfo buffer_info = {};
                buffer_info.buffer                 = vk_ubo_buffer;
                buffer_info.offset                 = 0; // dynamic blocks get their offset at bind time
                buffer_info.range                  = ubo_block->_buffer_size;
                buffer_infos.push_back(buffer_info);

                VkWriteDescriptorSet DWRITE = {};
                initializeVkStruct(DWRITE, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
                DWRITE.dstSet          = descset_ptr->_vkdescset;
                DWRITE.dstBinding      = binding->binding_id;
                DWRITE.descriptorCount = 1;
                DWRITE.descriptorType  = nondynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER //
                                                    : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
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

              // NB: do NOT gate on _buffer_size>0 — a SIZE-LESS runtime array (`T x[];`) has 0 static
              // bytes, but is bound dynamically; its range comes from the bound buffer's length below.
              if (ssbo_block) {
                // Check if a buffer is bound
                VkBuffer vk_buffer = VK_NULL_HANDLE;
                VkDeviceSize buffer_size = ssbo_block->_buffer_size;

                VkDeviceSize bind_offset = 0;
                auto* ssbo_state = _ctxVK->_fxi->storageStateForBlock(ssbo_block);
                if (ssbo_state && ssbo_state->_bound_buffer) {
                  vk_buffer = ssbo_state->_bound_buffer->_vkbuffer;
                  buffer_size = ssbo_state->_bound_buffer->_length;
                  bind_offset = ssbo_state->_bound_offset; // sub-range bind (0 = whole)
                }

                // No SSBO bound — skip descriptor update
                if (vk_buffer == VK_NULL_HANDLE) {
                  break;
                }

                VkDescriptorBufferInfo buffer_info = {};
                buffer_info.buffer = vk_buffer;
                // sub-range: descriptor reads [bind_offset, end). offset must satisfy
                // minStorageBufferOffsetAlignment (the caller pads strides; we just forward it).
                buffer_info.offset = bind_offset;
                buffer_info.range  = (bind_offset < buffer_size) ? (buffer_size - bind_offset) : buffer_size;
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

VkFxShaderPass::VkFxShaderPass(VkFxShaderFile* file)
    : _shader_file(file) {
  _incr_crc64.init();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
