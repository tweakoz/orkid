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
#include <cstdlib>
#include <set>
#include <boost/filesystem.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_vkpipc = logger()->configureChannel("VKPIPC", fvec3(1, 1, .2), false);
///////////////////////////////////////////////////////////////////////////////

vkpipelinestate_ptr_t VkFxInterface::_createPipeline(
    vkvtxbuf_ptr_t vb,              //
    vkprimclass_ptr_t primclass,    //
    vkrasterstate_ptr_t vkrstate) { //

  OrkAssert(_currentVKPASS != nullptr);
  vkpipelinestate_ptr_t pipeline = std::make_shared<VkPipelineState>(_contextVK);
  auto shprog                   = _currentVKPASS;
  pipeline->_shader_state       = &_shader_pass_states[shprog];
  pipeline->_rasterstate        = vkrstate;
  auto fbi                      = _contextVK->_fbi;
  auto gbi                      = _contextVK->_gbi;
  auto rtg                      = fbi->_active_rtgroup;
  auto rtg_impl                 = rtg->_impl.getShared<VkRtGroupImpl>();

  ////////////////////////////////////////////////////
  // create pipeline info
  ////////////////////////////////////////////////////

  auto& PIPE_CREATE_INFO = pipeline->_VKGFXPCI;
  initializeVkStruct(PIPE_CREATE_INFO, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);

  PIPE_CREATE_INFO.flags      = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
  PIPE_CREATE_INFO.renderPass = VK_NULL_HANDLE;
  PIPE_CREATE_INFO.subpass    = 0;

  ////////////////////////////////////////////////////
  // Dynamic rendering info
  ////////////////////////////////////////////////////

  rtg_impl->_prinfo_retain = std::make_shared<VulkanPipelineRenderInfo>(rtg);

  OrkAssert(rtg_impl->_prinfo_retain);
  PIPE_CREATE_INFO.pNext = &rtg_impl->_prinfo_retain->_createInfo; // Set the dynamic rendering info

  ////////////////////////////////////////////////////
  // count/assign shader stages
  ////////////////////////////////////////////////////

  std::vector<VkPipelineShaderStageCreateInfo> stages;
  if (shprog->_vtxshader)
    stages.push_back(shprog->_vtxshader->_shaderstageinfo);
  if (shprog->_geoshader)
    stages.push_back(shprog->_geoshader->_shaderstageinfo);
  if (shprog->_frgshader)
    stages.push_back(shprog->_frgshader->_shaderstageinfo);

  auto VIF = shprog->_vertexinterface;
  auto vtx_state = gbi->vertexInputState(vb, VIF);
  OrkAssert(vtx_state);

  PIPE_CREATE_INFO.stageCount          = stages.size();
  PIPE_CREATE_INFO.pStages             = stages.data();
  PIPE_CREATE_INFO.pVertexInputState   = &vtx_state->_vertex_input_state;
  PIPE_CREATE_INFO.pInputAssemblyState = &primclass->_input_assembly_state;

  ////////////////////////////////////////////////////
  // dynamic states (viewport, scissor, blend constants, cull mode, depth write)
  //  depth write MUST be dynamic: the same material pipeline is submitted into
  //  both the depth prepass (read/write depth) and the color pass, which runs
  //  with the depth attachment in DEPTH_READ_ONLY_OPTIMAL. A baked
  //  depthWriteEnable=TRUE there violates VUID-vkCmdDraw-None-06886.
  //  _bindPipeline sets it per draw from (baked value AND not read-only pass).
  ////////////////////////////////////////////////////

  std::vector<VkDynamicState> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_BLEND_CONSTANTS,
      VK_DYNAMIC_STATE_CULL_MODE_EXT};
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  initializeVkStruct(dynamicState, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
  dynamicState.dynamicStateCount = dynamic_states.size();
  dynamicState.pDynamicStates    = dynamic_states.data();

  PIPE_CREATE_INFO.pDynamicState = &dynamicState;

  VkPipelineViewportStateCreateInfo VPSTATE = {};
  VPSTATE.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  VPSTATE.viewportCount                     = 1;       // You can adjust this based on your needs
  VPSTATE.pViewports                        = nullptr; // Since you're setting this dynamically
  VPSTATE.scissorCount                      = 1;       // This should match viewportCount
  VPSTATE.pScissors                         = nullptr; // Assuming you're also setting scissor dynamically

  PIPE_CREATE_INFO.pViewportState = &VPSTATE;

  ////////////////////////////////////////////////////
  // MSAA state
  ////////////////////////////////////////////////////

  VkPipelineMultisampleStateCreateInfo MSAA = {};
  initializeVkStruct(MSAA, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  // rasterizationSamples MUST match the target RtGroup's color/depth attachment sample count
  // (the multisample images created in _vkCreateImageForBuffer). Pipelines are cached per RtGroup,
  // so the MSAA forward RTG gets MSAA pipelines and other RTGs stay 1x.
  MSAA.sampleShadingEnable   = VK_FALSE;
  MSAA.rasterizationSamples  = (VkSampleCountFlagBits)msaaEnumToInt(rtg->_msaa_samples);
  MSAA.minSampleShading      = 1.0f;                  // Minimum fraction for sample shading; closer to 1 is smoother
  MSAA.pSampleMask           = nullptr;               // Optional
  MSAA.alphaToCoverageEnable =                        // A2C from the material raster state (order-independent
      (pipeline->_rasterstate and pipeline->_rasterstate->_alphaToCoverage) ? VK_TRUE : VK_FALSE; // foliage)
  MSAA.alphaToOneEnable      = VK_FALSE;              // Enable/Disable alpha to one

  PIPE_CREATE_INFO.pMultisampleState = &MSAA;

  ////////////////////////////////////////////////////
  // raster states
  ////////////////////////////////////////////////////

  PIPE_CREATE_INFO.pRasterizationState = &pipeline->_rasterstate->_VKRSCI;
  PIPE_CREATE_INFO.pDepthStencilState  = &pipeline->_rasterstate->_VKDSSCI;
  PIPE_CREATE_INFO.pColorBlendState    = &pipeline->_rasterstate->_VKCBSI;

  ///////////////////////////////////////////////////
  // create pipeline layout
  // (descriptor sets and push constants)
  ////////////////////////////////////////////////////

  auto PLCI = _createPipelineLayoutData(pipeline);

  VkResult OK = vkCreatePipelineLayout(
      _contextVK->_vkdevice,       // device
      &PLCI,                       // pipeline layout create info
      nullptr,                     // allocator
      &pipeline->_pipelineLayout); // pipeline layout
  OrkAssert(VK_SUCCESS == OK);

  PIPE_CREATE_INFO.layout = pipeline->_pipelineLayout;

  ///////////////////////////////////////////////////
  // create the graphics pipeline
  ///////////////////////////////////////////////////

  OK = vkCreateGraphicsPipelines(
      _contextVK->_vkdevice,        // device
      _contextVK->_vkPipelineCache, // persisted pipeline cache (WS3)
      1,                     // count
      &PIPE_CREATE_INFO,     // create info
      nullptr,               // allocator
      &pipeline->_pipeline);

  if (OK != VK_SUCCESS) {
    printf("vkCreateGraphicsPipelines FAILED for TEK<%s>: VkResult=%d\n", shprog->_tek_name.c_str(), (int)OK);
  }
  OrkAssert(VK_SUCCESS == OK);

  ///////////////////////////////////////////////////
  // pipeline report (generates report and stores filename in pipeline)
  ///////////////////////////////////////////////////

  if (0) {
    _createPipelineReport(pipeline);
  }

  ///////////////////////////////////////////////////

  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VkPipelineLayoutCreateInfo VkFxInterface::_createPipelineLayoutData(vkpipelinestate_ptr_t pipeline) {

  auto vk_program = _currentVKPASS;
  OrkAssert(vk_program != nullptr);

  VkPipelineLayoutCreateInfo PLCI;

  initializeVkStruct(PLCI, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);

  ////////////////////////////////////////////////////
  // push constants
  ////////////////////////////////////////////////////

  if (vk_program->_pushConstantBlock and (vk_program->_pushConstantBlock->_ranges.size() > 0)) {
    PLCI.pushConstantRangeCount = vk_program->_pushConstantBlock->_ranges.size();
    PLCI.pPushConstantRanges    = vk_program->_pushConstantBlock->_ranges.data();
  }

  ////////////////////////////////////////////////////
  // descriptorset layouts
  ////////////////////////////////////////////////////

  pipeline->_dset_layouts.clear();

  auto resources = _currentVKPASS->_merged_resources;

  if (0) {
    printf(
        "_createPipelineLayoutData: TEK<%s> merged_resources=%p, num_descriptor_sets=%zu\n",
        vk_program->_tek_name.c_str(),
        resources.get(),
        resources ? resources->descriptor_sets.size() : 0);
    if (resources) {
      for (const auto& [set_id, sources] : resources->descriptor_sets) {
        printf("  descriptor_set[%d] has %zu sources\n", set_id, sources.size());
        for (const auto& source : sources) {
          printf("    source<%s> has %zu bindings\n", source->source_name.c_str(), source->bindings.size());
          for (const auto& binding : source->bindings) {
            const char* type_str = binding->type == VkMergedResourceBinding::Type::StorageBuffer  ? "SSBO"
                                   : binding->type == VkMergedResourceBinding::Type::UniformBlock ? "UBO"
                                   : binding->type == VkMergedResourceBinding::Type::Sampler      ? "SAMPLER"
                                                                                                  : "?";
            printf("      binding[%u] = %s<%s>\n", binding->binding_id, type_str, binding->name.c_str());
          }
        }
      }
    }
  }

  if (not resources->descriptor_sets.empty()) {

    ///////////////////////////////////////////////////////////
    // Create descriptor set layout from merged resource data
    ///////////////////////////////////////////////////////////

    // Ordered state vectors are program-level — only build once for the first pipeline.
    auto* shader_state = pipeline->_shader_state;
    bool build_ordered = shader_state->_ordered_uniform_states.empty() && shader_state->_ordered_storage_states.empty();

    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (const auto& [set_id, sources] : resources->descriptor_sets) {

      bindings.clear();

      for (const auto& source : sources) {

        for (const auto& binding : source->bindings) {

          VkDescriptorSetLayoutBinding vk_binding = {};
          vk_binding.binding                      = binding->binding_id;

          ///////////////////////////////////////////////////////////
          // Map resource type to Vulkan descriptor type
          ///////////////////////////////////////////////////////////

          switch (binding->type) {
            case VkMergedResourceBinding::Type::Sampler:
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
              break;
            case VkMergedResourceBinding::Type::UniformBlock: {
              // Uniform blocks are dynamic (ring-suballocated per draw) unless
              // named per-frame-constant — see isNonDynamicUniformBlock. Those
              // bind their own buffer and stay off the 15-descriptor dynamic
              // budget the offending layouts were blowing past.
              const bool nondynamic     = isNonDynamicUniformBlock(binding->name);
              vk_binding.descriptorType = nondynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER //
                                                     : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
              if (0)
                printf(
                    "LAYOUT-BINDING: ubo<%s> binding=%u type=%s\n",
                    binding->name.c_str(),
                    binding->binding_id,
                    nondynamic ? "STATIC" : "DYNAMIC");
              // Look up the UBO from the program's uniform blocks
              // These were loaded from the datablock
              auto ubo_it = vk_program->_vk_uniformblks.find(binding->name);
              if (ubo_it == vk_program->_vk_uniformblks.end()) {
                // Fatal error: shader declares a UBO that wasn't in the datablock
                logchan_vkpipc->log(
                    "FATAL: UBO '%s' declared in merged resources but not found in datablock", binding->name.c_str());
                OrkAssert(false);
              }

              VkFxShaderUniformBlock* ubo = ubo_it->second.get();
              OrkAssert(ubo != nullptr);
              OrkAssert(ubo->_orkparamblock != nullptr);

              //////////////////////////////////////////////////////
              // Attach shader context's UBO context to the pipeline, recording binding ID.
              //////////////////////////////////////////////////////

              // _ordered_uniform_states IS the pDynamicOffsets array: one entry
              // per DYNAMIC descriptor, in layout binding order. A non-dynamic
              // block must not appear here or every offset after it shifts by
              // one and the draw reads another block's bytes.
              auto* ub_ctx = uniformStateForBlock(ubo);
              if (ub_ctx && build_ordered && not nondynamic) {
                ub_ctx->_binding_id = binding->binding_id;
                shader_state->_ordered_uniform_states.push_back(ub_ctx);
              }

              break;
            }
            case VkMergedResourceBinding::Type::StorageBuffer: {
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

              //////////////////////////////////////////////////////
              // Find the corresponding SSBO
              //////////////////////////////////////////////////////

              VkFxShaderStorageBlock* ssbo = nullptr;
              auto it                      = vk_program->_vk_ssbo_blocks.find(binding->name);
              if (it != vk_program->_vk_ssbo_blocks.end()) {
                ssbo = it->second.get();
              }

              if (ssbo) {
                //////////////////////////////////////////////////////
                // Attach shader context's SSBO context to the pipeline, recording binding ID.
                //////////////////////////////////////////////////////

                auto* ssbo_ctx = storageStateForBlock(ssbo);
                if (ssbo_ctx && build_ordered) {
                  ssbo_ctx->_binding_id = binding->binding_id;
                  shader_state->_ordered_storage_states.push_back(ssbo_ctx);
                }
              }

              //////////////////////////////////////////////////////
              // Determine which shader stages use this SSBO
              // MoltenVK requires accurate stage flags for argument buffer encoding
              //////////////////////////////////////////////////////
              VkShaderStageFlags ssbo_stages = 0;
              if (vk_program->_vtxshader && vk_program->_vtxshader->_ssbo_refs) {
                if (vk_program->_vtxshader->_ssbo_refs->_ssbo_blocks.count(binding->name) > 0) {
                  ssbo_stages |= VK_SHADER_STAGE_VERTEX_BIT;
                }
              }
              if (vk_program->_geoshader && vk_program->_geoshader->_ssbo_refs) {
                if (vk_program->_geoshader->_ssbo_refs->_ssbo_blocks.count(binding->name) > 0) {
                  ssbo_stages |= VK_SHADER_STAGE_GEOMETRY_BIT;
                }
              }
              if (vk_program->_mshshader && vk_program->_mshshader->_ssbo_refs) {
                if (vk_program->_mshshader->_ssbo_refs->_ssbo_blocks.count(binding->name) > 0) {
                  ssbo_stages |= VK_SHADER_STAGE_MESH_BIT_EXT;
                }
              }
              if (vk_program->_tskshader && vk_program->_tskshader->_ssbo_refs) {
                if (vk_program->_tskshader->_ssbo_refs->_ssbo_blocks.count(binding->name) > 0) {
                  ssbo_stages |= VK_SHADER_STAGE_TASK_BIT_EXT;
                }
              }
              if (vk_program->_frgshader && vk_program->_frgshader->_ssbo_refs) {
                if (vk_program->_frgshader->_ssbo_refs->_ssbo_blocks.count(binding->name) > 0) {
                  ssbo_stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
                }
              }
              // Use the computed stages, or fall back to the pass's front stage if none found
              VkShaderStageFlags ssbo_fallback =
                  vk_program->_mshshader ? VK_SHADER_STAGE_MESH_BIT_EXT : VK_SHADER_STAGE_VERTEX_BIT;
              binding->stage_flags = ssbo_stages ? ssbo_stages : ssbo_fallback;

              break;
            }
            default:
              OrkAssert(false); // Unknown resource type
              break;
          }

          vk_binding.descriptorCount = 1;
          // Use binding-specific stage flags for storage buffers, ALL_GRAPHICS for others
          if (binding->type == VkMergedResourceBinding::Type::StorageBuffer && binding->stage_flags != 0) {
            vk_binding.stageFlags = binding->stage_flags;
          } else {
            vk_binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            // ALL_GRAPHICS predates mesh shading and covers NEITHER amplification stage
            if (vk_program->_mshshader)
              vk_binding.stageFlags |= VK_SHADER_STAGE_MESH_BIT_EXT;
            if (vk_program->_tskshader)
              vk_binding.stageFlags |= VK_SHADER_STAGE_TASK_BIT_EXT;
          }
          vk_binding.pImmutableSamplers = nullptr;

          bindings.push_back(vk_binding);

        } // for (const auto& binding : source->bindings) {

      } // for (const auto& source : sources) {

      //////////////////////////////////////////////////////
      // Create descriptor set layout for this descriptor set
      //////////////////////////////////////////////////////

      if (!bindings.empty()) {

        //////////////////////////////////////////////////////////
        // CRITICAL: Sort bindings by binding number BEFORE layout creation!
        //
        // Per Vulkan spec (vkCmdBindDescriptorSets):
        // "The order of the dynamic descriptor bindings within each descriptor set
        //  is the order in which they appear in the pBindings array passed to
        //  vkCreateDescriptorSetLayout."
        //
        // We later sort _ubo_states by binding_id and build dynamic offsets
        // from that sorted order. If pBindings isn't also sorted, the dynamic
        // offsets will be consumed in the wrong order, causing each UBO to
        // read from the wrong memory location.
        //////////////////////////////////////////////////////////
        std::sort(
            bindings.begin(), bindings.end(), [](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b) {
              return a.binding < b.binding;
            });

        VkDescriptorSetLayoutCreateInfo LCI = {};
        initializeVkStruct(LCI, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        LCI.bindingCount = bindings.size();
        LCI.pBindings    = bindings.data();
        // LCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        // LCI.pNext = &bindingFlagsInfo;
        // LCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

        VkDescriptorSetLayout dset_layout;
        VkResult OK = vkCreateDescriptorSetLayout(_contextVK->_vkdevice, &LCI, nullptr, &dset_layout);
        OrkAssert(VK_SUCCESS == OK);

        pipeline->_dset_layouts.push_back(dset_layout);
      }

    } // for (const auto& [set_id, sources] : resources->descriptor_sets) {

    //////////////////////////////////////////////////////////////////////////
    // ORKID_VK_LAYOUT_CENSUS : per-program descriptor budget, default-off.
    //
    // The over-budget layout VUIDs (03016 samplers, 03030/03038 dynamic UBOs)
    // report a COUNT and nothing else — not the program, not the technique — so
    // a device that trips one gives you no way to know WHICH shader to put on a
    // diet. Counts are a property of the program, not the device, so this census
    // attributes them on any machine, including ones whose limits are generous
    // enough never to complain. One line per (technique, shader), first build only.
    //////////////////////////////////////////////////////////////////////////

    static const bool census_enabled = (nullptr != std::getenv("ORKID_VK_LAYOUT_CENSUS"));
    if (census_enabled) {
      size_t n_samplers = 0, n_ubo_dyn = 0, n_ubo_static = 0, n_ssbo = 0;
      for (const auto& [set_id, sources] : resources->descriptor_sets) {
        for (const auto& source : sources) {
          for (const auto& binding : source->bindings) {
            switch (binding->type) {
              case VkMergedResourceBinding::Type::Sampler:
                n_samplers++;
                break;
              case VkMergedResourceBinding::Type::UniformBlock:
                isNonDynamicUniformBlock(binding->name) ? n_ubo_static++ : n_ubo_dyn++;
                break;
              case VkMergedResourceBinding::Type::StorageBuffer:
                n_ssbo++;
                break;
              default:
                break;
            }
          }
        }
      }
      auto shfile = vk_program->_shader_file;
      auto key    = FormatString(
          "%s|%s", vk_program->_tek_name.c_str(), shfile ? shfile->_shader_name.c_str() : "?");
      static std::set<std::string> s_seen;
      if (s_seen.insert(key).second) {
        printf(
            "[LAYOUTCENSUS] samplers<%zu> ubo_dyn<%zu> ubo_static<%zu> ssbo<%zu> tek<%s> shader<%s>\n",
            n_samplers,
            n_ubo_dyn,
            n_ubo_static,
            n_ssbo,
            vk_program->_tek_name.c_str(),
            shfile ? shfile->_shader_name.c_str() : "?");
        fflush(stdout);
      }
    }

    //////////////////////////////////////////////////////
    // Sort UBO context pointers by (descriptor_set_id, binding_id) for consistent
    // ordering with dynamic offsets consumed by vkCmdBindDescriptorSets.
    //////////////////////////////////////////////////////

    if (build_ordered) {
      std::sort(
          shader_state->_ordered_uniform_states.begin(),
          shader_state->_ordered_uniform_states.end(),
          [](const VkFxShaderUniformBlockState* a, const VkFxShaderUniformBlockState* b) {
            auto* blk_a = a->_shader_uniform_block;
            auto* blk_b = b->_shader_uniform_block;
            if (blk_a->_descriptor_set_id != blk_b->_descriptor_set_id)
              return blk_a->_descriptor_set_id < blk_b->_descriptor_set_id;
            return a->_binding_id < b->_binding_id;
          });

      std::sort(
          shader_state->_ordered_storage_states.begin(),
          shader_state->_ordered_storage_states.end(),
          [](const VkFxShaderStorageBlockState* a, const VkFxShaderStorageBlockState* b) {
            auto* blk_a = a->_shader_storage_block;
            auto* blk_b = b->_shader_storage_block;
            if (blk_a->_descriptor_set_id != blk_b->_descriptor_set_id)
              return blk_a->_descriptor_set_id < blk_b->_descriptor_set_id;
            return a->_binding_id < b->_binding_id;
          });
    }

    PLCI.setLayoutCount = pipeline->_dset_layouts.size();
    PLCI.pSetLayouts    = pipeline->_dset_layouts.data();

    // Store the merged resource layouts in the pipeline object for descriptor set allocation

  } else {
    // No descriptor sets available - this is valid for shaders that only use push constants
    logchan_vkpipc->log("No descriptor sets available - shader uses only push constants");
    PLCI.setLayoutCount = 0;
    PLCI.pSetLayouts    = nullptr;
  }
  return PLCI;
}

///////////////////////////////////////////////////////////////////////////////
// SSBO-only pipeline creation (no vertex buffer)
///////////////////////////////////////////////////////////////////////////////

vkpipelinestate_ptr_t VkFxInterface::_createPipelineSSBO(vkprimclass_ptr_t primclass, vkrasterstate_ptr_t vkrstate) {

  OrkAssert(_currentVKPASS != nullptr);
  vkpipelinestate_ptr_t pipeline = std::make_shared<VkPipelineState>(_contextVK);
  auto shprog = _currentVKPASS;
  pipeline->_shader_state       = &_shader_pass_states[shprog];
  pipeline->_rasterstate        = vkrstate;
  auto fbi                      = _contextVK->_fbi;
  auto rtg                      = fbi->_active_rtgroup;
  auto rtg_impl                 = rtg->_impl.getShared<VkRtGroupImpl>();

  ////////////////////////////////////////////////////
  // create pipeline info
  ////////////////////////////////////////////////////

  auto& PIPE_CREATE_INFO = pipeline->_VKGFXPCI;
  initializeVkStruct(PIPE_CREATE_INFO, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);

  PIPE_CREATE_INFO.flags      = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
  PIPE_CREATE_INFO.renderPass = VK_NULL_HANDLE;
  PIPE_CREATE_INFO.subpass    = 0;

  ////////////////////////////////////////////////////
  // Dynamic rendering info
  ////////////////////////////////////////////////////

  rtg_impl->_prinfo_retain = std::make_shared<VulkanPipelineRenderInfo>(rtg);
  OrkAssert(rtg_impl->_prinfo_retain);
  PIPE_CREATE_INFO.pNext = &rtg_impl->_prinfo_retain->_createInfo;

  ////////////////////////////////////////////////////
  // count/assign shader stages
  ////////////////////////////////////////////////////

  std::vector<VkPipelineShaderStageCreateInfo> stages;
  if (shprog->_vtxshader)
    stages.push_back(shprog->_vtxshader->_shaderstageinfo);
  if (shprog->_geoshader)
    stages.push_back(shprog->_geoshader->_shaderstageinfo);
  if (shprog->_frgshader)
    stages.push_back(shprog->_frgshader->_shaderstageinfo);

  ////////////////////////////////////////////////////
  // Empty vertex input state (SSBO-only rendering)
  // Vertex shader reads from storage buffer via gl_VertexID
  ////////////////////////////////////////////////////

  VkPipelineVertexInputStateCreateInfo empty_vertex_input = {};
  initializeVkStruct(empty_vertex_input, VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
  empty_vertex_input.vertexBindingDescriptionCount   = 0;
  empty_vertex_input.pVertexBindingDescriptions      = nullptr;
  empty_vertex_input.vertexAttributeDescriptionCount = 0;
  empty_vertex_input.pVertexAttributeDescriptions    = nullptr;

  PIPE_CREATE_INFO.stageCount          = stages.size();
  PIPE_CREATE_INFO.pStages             = stages.data();
  PIPE_CREATE_INFO.pVertexInputState   = &empty_vertex_input;
  PIPE_CREATE_INFO.pInputAssemblyState = &primclass->_input_assembly_state;

  ////////////////////////////////////////////////////
  // dynamic states (viewport, scissor, blend constants, cull mode, depth write)
  //  depth write MUST be dynamic: the same material pipeline is submitted into
  //  both the depth prepass (read/write depth) and the color pass, which runs
  //  with the depth attachment in DEPTH_READ_ONLY_OPTIMAL. A baked
  //  depthWriteEnable=TRUE there violates VUID-vkCmdDraw-None-06886.
  //  _bindPipeline sets it per draw from (baked value AND not read-only pass).
  ////////////////////////////////////////////////////

  std::vector<VkDynamicState> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_BLEND_CONSTANTS,
      VK_DYNAMIC_STATE_CULL_MODE_EXT};
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  initializeVkStruct(dynamicState, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
  dynamicState.dynamicStateCount = dynamic_states.size();
  dynamicState.pDynamicStates    = dynamic_states.data();

  PIPE_CREATE_INFO.pDynamicState = &dynamicState;

  VkPipelineViewportStateCreateInfo VPSTATE = {};
  VPSTATE.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  VPSTATE.viewportCount                     = 1;
  VPSTATE.pViewports                        = nullptr;
  VPSTATE.scissorCount                      = 1;
  VPSTATE.pScissors                         = nullptr;

  PIPE_CREATE_INFO.pViewportState = &VPSTATE;

  ////////////////////////////////////////////////////
  // MSAA state
  ////////////////////////////////////////////////////

  VkPipelineMultisampleStateCreateInfo MSAA = {};
  initializeVkStruct(MSAA, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  MSAA.sampleShadingEnable   = VK_FALSE;
  MSAA.rasterizationSamples  = (VkSampleCountFlagBits)msaaEnumToInt(rtg->_msaa_samples); // match RTG attachments
  MSAA.minSampleShading      = 1.0f;
  MSAA.pSampleMask           = nullptr;
  MSAA.alphaToCoverageEnable = VK_FALSE;
  MSAA.alphaToOneEnable      = VK_FALSE;

  PIPE_CREATE_INFO.pMultisampleState = &MSAA;

  ////////////////////////////////////////////////////
  // raster states
  ////////////////////////////////////////////////////

  PIPE_CREATE_INFO.pRasterizationState = &pipeline->_rasterstate->_VKRSCI;
  PIPE_CREATE_INFO.pDepthStencilState  = &pipeline->_rasterstate->_VKDSSCI;
  PIPE_CREATE_INFO.pColorBlendState    = &pipeline->_rasterstate->_VKCBSI;

  ///////////////////////////////////////////////////
  // create pipeline layout
  ////////////////////////////////////////////////////

  auto PLCI = _createPipelineLayoutData(pipeline);

  VkResult OK = vkCreatePipelineLayout(_contextVK->_vkdevice, &PLCI, nullptr, &pipeline->_pipelineLayout);
  OrkAssert(VK_SUCCESS == OK);

  PIPE_CREATE_INFO.layout = pipeline->_pipelineLayout;

  ///////////////////////////////////////////////////
  // create the graphics pipeline
  ///////////////////////////////////////////////////

  OK = vkCreateGraphicsPipelines(_contextVK->_vkdevice, _contextVK->_vkPipelineCache, 1, &PIPE_CREATE_INFO, nullptr, &pipeline->_pipeline);

  if (OK != VK_SUCCESS) {
    printf("_createPipelineSSBO: vkCreateGraphicsPipelines failed with VkResult=%d\n", OK);
    auto& prci = rtg_impl->_prinfo_retain->_createInfo;
    printf(
        "  tek<%s> rtg<%s> colorAttachmentCount<%u> blendAttachmentCount<%u> samples<%d>\n",
        shprog->_tek_name.c_str(),
        rtg->_name.c_str(),
        prci.colorAttachmentCount,
        PIPE_CREATE_INFO.pColorBlendState->attachmentCount,
        int(MSAA.rasterizationSamples));
    for (uint32_t i = 0; i < prci.colorAttachmentCount; i++)
      printf("  colorFormat[%u]=%d\n", i, int(prci.pColorAttachmentFormats[i]));
    printf("  depthFormat=%d stagecount<%zu> frg<%p>\n", int(prci.depthAttachmentFormat), stages.size(), (void*)shprog->_frgshader.get());
  }
  OrkAssert(VK_SUCCESS == OK);

  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////
// LocalSize declared by a stage's SPIR-V, as the product x*y*z. 0 when the module
//  declares none, or declares it via OpExecutionModeId (spec-constant sized) which
//  cannot be resolved without evaluating the constant — callers must treat 0 as
//  "unknown", never as "zero invocations".
///////////////////////////////////////////////////////////////////////////////

static uint32_t spirvLocalSizeProduct(const vkfxshader_bin_t& bin) {
  static constexpr uint32_t kSpirvMagic       = 0x07230203;
  static constexpr uint32_t kOpExecutionMode  = 16;
  static constexpr uint32_t kModeLocalSize    = 17;
  static constexpr size_t kHeaderWords        = 5;
  if (bin.size() <= kHeaderWords or bin[0] != kSpirvMagic) {
    return 0;
  }
  for (size_t i = kHeaderWords; i < bin.size();) {
    uint32_t wordcount = bin[i] >> 16;
    uint32_t opcode    = bin[i] & 0xFFFF;
    if (wordcount == 0 or (i + wordcount) > bin.size()) {
      break; // malformed — the driver will reject it with a real diagnostic
    }
    // OpExecutionMode <entrypoint> LocalSize <x> <y> <z>
    if (opcode == kOpExecutionMode and wordcount == 6 and bin[i + 2] == kModeLocalSize) {
      return bin[i + 3] * bin[i + 4] * bin[i + 5];
    }
    i += wordcount;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Mesh pipeline creation ([TASK +] MESH + FRAGMENT).
//  The mesh stage produces primitives directly, so the pipeline has NO vertex input
//  state and NO input assembly state — both are ignored when a mesh stage is present
//  and must be left null (a mesh pipeline with an input-assembly state is invalid).
//  When the pass also binds a TASK (amplification) stage, that stage fronts the pipeline
//  and the draw's workgroup counts become TASK workgroups; the mesh grid is then whatever
//  each task workgroup emits.
///////////////////////////////////////////////////////////////////////////////

vkpipelinestate_ptr_t VkFxInterface::_createPipelineMesh(vkrasterstate_ptr_t vkrstate) {

  OrkAssert(_currentVKPASS != nullptr);
  vkpipelinestate_ptr_t pipeline = std::make_shared<VkPipelineState>(_contextVK);
  auto shprog = _currentVKPASS;
  pipeline->_shader_state       = &_shader_pass_states[shprog];
  pipeline->_rasterstate        = vkrstate;
  auto fbi                      = _contextVK->_fbi;
  auto rtg                      = fbi->_active_rtgroup;
  auto rtg_impl                 = rtg->_impl.getShared<VkRtGroupImpl>();

  OrkAssertI(
      _contextVK->supportsMeshShader(), //
      "a mesh technique was selected on a device without VK_EXT_mesh_shader enabled");
  OrkAssertI(shprog->_mshshader != nullptr, "_createPipelineMesh with no mesh stage in the pass");
  OrkAssertIFMT(
      (shprog->_tskshader == nullptr) or _contextVK->supportsTaskShader(),
      "tek<%s> binds a task_shader but this device did not enable the VK_EXT_mesh_shader "
      "taskShader feature — the amplification pair cannot be created here",
      shprog->_tek_name.c_str());

  ////////////////////////////////////////////////////
  // mesh local_size vs maxMeshWorkGroupInvocations.
  //  Over-running this limit is NOT diagnosed by the driver: NVIDIA's SPIR-V compiler
  //  SEGFAULTS inside vkCreateGraphicsPipelines (libnvidia-glvkspirv -> libnvidia-gpucomp),
  //  which reads as an engine crash on the first mesh draw and hides the shader entirely.
  //  Fail here, named, with both numbers.
  ////////////////////////////////////////////////////

  uint32_t msh_localsize = spirvLocalSizeProduct(shprog->_mshshader->_spirv_binary);
  uint32_t msh_maxinvoc  = _contextVK->_vkdeviceinfo->_maxMeshWkgInvocations;
  OrkAssertIFMT(
      not(msh_localsize and msh_maxinvoc and (msh_localsize > msh_maxinvoc)),
      "mesh stage of tek<%s> declares local_size product<%u>, exceeding this device's "
      "maxMeshWorkGroupInvocations<%u> — shrink the mesh workgroup",
      shprog->_tek_name.c_str(),
      msh_localsize,
      msh_maxinvoc);

  // same trap on the task side, against its own (separately reported) ceiling.
  if (shprog->_tskshader) {
    uint32_t tsk_localsize = spirvLocalSizeProduct(shprog->_tskshader->_spirv_binary);
    uint32_t tsk_maxinvoc  = _contextVK->_vkdeviceinfo->_maxTaskWkgInvocations;
    OrkAssertIFMT(
        not(tsk_localsize and tsk_maxinvoc and (tsk_localsize > tsk_maxinvoc)),
        "task stage of tek<%s> declares local_size product<%u>, exceeding this device's "
        "maxTaskWorkGroupInvocations<%u> — shrink the task workgroup",
        shprog->_tek_name.c_str(),
        tsk_localsize,
        tsk_maxinvoc);
  }

  ////////////////////////////////////////////////////
  // create pipeline info
  ////////////////////////////////////////////////////

  auto& PIPE_CREATE_INFO = pipeline->_VKGFXPCI;
  initializeVkStruct(PIPE_CREATE_INFO, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);

  PIPE_CREATE_INFO.flags      = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
  PIPE_CREATE_INFO.renderPass = VK_NULL_HANDLE;
  PIPE_CREATE_INFO.subpass    = 0;

  ////////////////////////////////////////////////////
  // Dynamic rendering info
  ////////////////////////////////////////////////////

  rtg_impl->_prinfo_retain = std::make_shared<VulkanPipelineRenderInfo>(rtg);
  OrkAssert(rtg_impl->_prinfo_retain);
  PIPE_CREATE_INFO.pNext = &rtg_impl->_prinfo_retain->_createInfo;

  ////////////////////////////////////////////////////
  // count/assign shader stages — [task] + mesh + fragment, in pipeline order
  ////////////////////////////////////////////////////

  std::vector<VkPipelineShaderStageCreateInfo> stages;
  if (shprog->_tskshader)
    stages.push_back(shprog->_tskshader->_shaderstageinfo);
  stages.push_back(shprog->_mshshader->_shaderstageinfo);
  if (shprog->_frgshader)
    stages.push_back(shprog->_frgshader->_shaderstageinfo);

  PIPE_CREATE_INFO.stageCount          = stages.size();
  PIPE_CREATE_INFO.pStages             = stages.data();
  PIPE_CREATE_INFO.pVertexInputState   = nullptr;
  PIPE_CREATE_INFO.pInputAssemblyState = nullptr;

  ////////////////////////////////////////////////////
  // dynamic states (viewport, scissor, blend constants, cull mode, depth write)
  //  depth write MUST be dynamic: the same material pipeline is submitted into
  //  both the depth prepass (read/write depth) and the color pass, which runs
  //  with the depth attachment in DEPTH_READ_ONLY_OPTIMAL. A baked
  //  depthWriteEnable=TRUE there violates VUID-vkCmdDraw-None-06886.
  //  _bindPipeline sets it per draw from (baked value AND not read-only pass).
  ////////////////////////////////////////////////////

  std::vector<VkDynamicState> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_BLEND_CONSTANTS,
      VK_DYNAMIC_STATE_CULL_MODE_EXT};
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  initializeVkStruct(dynamicState, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
  dynamicState.dynamicStateCount = dynamic_states.size();
  dynamicState.pDynamicStates    = dynamic_states.data();

  PIPE_CREATE_INFO.pDynamicState = &dynamicState;

  VkPipelineViewportStateCreateInfo VPSTATE = {};
  VPSTATE.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  VPSTATE.viewportCount                     = 1;
  VPSTATE.pViewports                        = nullptr;
  VPSTATE.scissorCount                      = 1;
  VPSTATE.pScissors                         = nullptr;

  PIPE_CREATE_INFO.pViewportState = &VPSTATE;

  ////////////////////////////////////////////////////
  // MSAA state
  ////////////////////////////////////////////////////

  VkPipelineMultisampleStateCreateInfo MSAA = {};
  initializeVkStruct(MSAA, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  MSAA.sampleShadingEnable   = VK_FALSE;
  MSAA.rasterizationSamples  = (VkSampleCountFlagBits)msaaEnumToInt(rtg->_msaa_samples); // match RTG attachments
  MSAA.minSampleShading      = 1.0f;
  MSAA.pSampleMask           = nullptr;
  MSAA.alphaToCoverageEnable = VK_FALSE;
  MSAA.alphaToOneEnable      = VK_FALSE;

  PIPE_CREATE_INFO.pMultisampleState = &MSAA;

  ////////////////////////////////////////////////////
  // raster states
  ////////////////////////////////////////////////////

  PIPE_CREATE_INFO.pRasterizationState = &pipeline->_rasterstate->_VKRSCI;
  PIPE_CREATE_INFO.pDepthStencilState  = &pipeline->_rasterstate->_VKDSSCI;
  PIPE_CREATE_INFO.pColorBlendState    = &pipeline->_rasterstate->_VKCBSI;

  ///////////////////////////////////////////////////
  // create pipeline layout
  ////////////////////////////////////////////////////

  auto PLCI = _createPipelineLayoutData(pipeline);

  VkResult OK = vkCreatePipelineLayout(_contextVK->_vkdevice, &PLCI, ORK_VK_ALLOC, &pipeline->_pipelineLayout);
  OrkAssert(VK_SUCCESS == OK);

  PIPE_CREATE_INFO.layout = pipeline->_pipelineLayout;

  ///////////////////////////////////////////////////
  // create the graphics pipeline
  ///////////////////////////////////////////////////

  OK = vkCreateGraphicsPipelines(
      _contextVK->_vkdevice, _contextVK->_vkPipelineCache, 1, &PIPE_CREATE_INFO, ORK_VK_ALLOC, &pipeline->_pipeline);

  if (OK != VK_SUCCESS) {
    printf("_createPipelineMesh: vkCreateGraphicsPipelines failed with VkResult=%d\n", OK);
    auto& prci = rtg_impl->_prinfo_retain->_createInfo;
    printf(
        "  tek<%s> rtg<%s> colorAttachmentCount<%u> blendAttachmentCount<%u> samples<%d>\n",
        shprog->_tek_name.c_str(),
        rtg->_name.c_str(),
        prci.colorAttachmentCount,
        PIPE_CREATE_INFO.pColorBlendState->attachmentCount,
        int(MSAA.rasterizationSamples));
    for (uint32_t i = 0; i < prci.colorAttachmentCount; i++)
      printf("  colorFormat[%u]=%d\n", i, int(prci.pColorAttachmentFormats[i]));
    printf("  depthFormat=%d stagecount<%zu> frg<%p>\n", int(prci.depthAttachmentFormat), stages.size(), (void*)shprog->_frgshader.get());
  }
  OrkAssert(VK_SUCCESS == OK);

  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
