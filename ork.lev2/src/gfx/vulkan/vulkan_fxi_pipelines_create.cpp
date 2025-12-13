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
static logchannel_ptr_t logchan_vkpipc = logger()->configureChannel("VKPIPC", fvec3(1, 1, .2), false);
///////////////////////////////////////////////////////////////////////////////

vkpipeline_obj_ptr_t VkFxInterface::_createPipeline(vkvtxbuf_ptr_t vb,               //
                                                    vkprimclass_ptr_t primclass,     //
                                                    vkrasterstate_ptr_t vkrstate ) { //

  OrkAssert(_currentVKPASS!=nullptr);
  vkpipeline_obj_ptr_t pipeline = std::make_shared<VkPipelineObject>(_contextVK);
  auto shprog = _currentVKPASS->_vk_program;
  pipeline->_vk_program  = shprog;
  pipeline->_rasterstate = vkrstate;
  auto fbi = _contextVK->_fbi;
  auto gbi = _contextVK->_gbi;
  auto rtg       = fbi->_active_rtgroup;
  auto rtg_impl  = rtg->_impl.getShared<VkRtGroupImpl>();

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

  auto VIF       = shprog->_vertexinterface;
  auto vtx_state = gbi->vertexInputState(vb, VIF);
  OrkAssert(vtx_state);

  PIPE_CREATE_INFO.stageCount          = stages.size();
  PIPE_CREATE_INFO.pStages             = stages.data();
  PIPE_CREATE_INFO.pVertexInputState   = &vtx_state->_vertex_input_state;
  PIPE_CREATE_INFO.pInputAssemblyState = &primclass->_input_assembly_state;

  ////////////////////////////////////////////////////
  // dynamic states (viewport, scissor, blend constants)
  ////////////////////////////////////////////////////

  std::vector<VkDynamicState> dynamic_states    = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
    VK_DYNAMIC_STATE_BLEND_CONSTANTS
  };
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
  MSAA.sampleShadingEnable   = VK_FALSE;              // Enable/Disable sample shading
  MSAA.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT; // No multisampling
  MSAA.minSampleShading      = 1.0f;                  // Minimum fraction for sample shading; closer to 1 is smoother
  MSAA.pSampleMask           = nullptr;               // Optional
  MSAA.alphaToCoverageEnable = VK_FALSE;              // Enable/Disable alpha to coverage
  MSAA.alphaToOneEnable      = VK_FALSE;              // Enable/Disable alpha to one

  PIPE_CREATE_INFO.pMultisampleState = &MSAA; // msaa_impl->_VKSTATE; // todo : dynamic

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
      _contextVK->_vkdevice,   // device
      &PLCI,                   // pipeline layout create info
      nullptr,                 // allocator
      &pipeline->_pipelineLayout); // pipeline layout
  OrkAssert(VK_SUCCESS == OK);

  PIPE_CREATE_INFO.layout = pipeline->_pipelineLayout;

  ///////////////////////////////////////////////////
  // create the graphics pipeline
  ///////////////////////////////////////////////////

  OK = vkCreateGraphicsPipelines(
      _contextVK->_vkdevice, // device
      VK_NULL_HANDLE,        // pipeline cache
      1,                     // count
      &PIPE_CREATE_INFO,       // create info
      nullptr,               // allocator
      &pipeline->_pipeline);

  OrkAssert(VK_SUCCESS == OK);

  ///////////////////////////////////////////////////
  // pipeline report (generates report and stores filename in pipeline)
  ///////////////////////////////////////////////////

  if(0) {
    _createPipelineReport(pipeline);
  }

  ///////////////////////////////////////////////////

  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VkPipelineLayoutCreateInfo VkFxInterface::_createPipelineLayoutData(vkpipeline_obj_ptr_t pipeline) {

  auto vk_program = _currentVKPASS->_vk_program;
  OrkAssert(vk_program != nullptr);

  VkPipelineLayoutCreateInfo PLCI;

  initializeVkStruct(PLCI, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);

  ////////////////////////////////////////////////////
  // push constants
  ////////////////////////////////////////////////////

  if (vk_program->_pushConstantBlock and (vk_program->_pushConstantBlock->_ranges.size()>0)) {
    PLCI.pushConstantRangeCount = vk_program->_pushConstantBlock->_ranges.size();
    PLCI.pPushConstantRanges    = vk_program->_pushConstantBlock->_ranges.data();
  }

  ////////////////////////////////////////////////////
  // descriptorset layouts
  ////////////////////////////////////////////////////

  pipeline->_dset_layouts.clear();

  auto resources = _currentVKPASS->_merged_resources;

  if(0){
    printf("_createPipelineLayoutData: merged_resources=%p, num_descriptor_sets=%zu\n",
         resources.get(), resources ? resources->descriptor_sets.size() : 0);
    if (resources) {
      for (const auto& [set_id, sources] : resources->descriptor_sets) {
        printf("  descriptor_set[%d] has %zu sources\n", set_id, sources.size());
        for (const auto& source : sources) {
          printf("    source<%s> has %zu bindings\n", source->source_name.c_str(), source->bindings.size());
          for (const auto& binding : source->bindings) {
            const char* type_str = binding->type == VkMergedResourceBinding::Type::StorageBuffer ? "SSBO" :
                                   binding->type == VkMergedResourceBinding::Type::UniformBlock ? "UBO" :
                                   binding->type == VkMergedResourceBinding::Type::Sampler ? "SAMPLER" : "?";
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
              // ALL uniform blocks are now dynamic
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
              // Look up the UBO from the program's uniform blocks
              // These were loaded from the datablock
              auto ubo_it = vk_program->_vk_uniformblks.find(binding->name);
              if (ubo_it == vk_program->_vk_uniformblks.end()) {
                // Fatal error: shader declares a UBO that wasn't in the datablock
                logchan_vkpipc->log("FATAL: UBO '%s' declared in merged resources but not found in datablock", binding->name.c_str());
                OrkAssert(false);
              }

              VkFxShaderUniformBlk* ubo = ubo_it->second.get();
              OrkAssert(ubo != nullptr);
              OrkAssert(ubo->_orkparamblock != nullptr);

              //////////////////////////////////////////////////////
              // Track this UBO for the pipeline with its binding ID
              //////////////////////////////////////////////////////

              pipeline->_uniform_blocks.push_back(ubo);
              pipeline->_ubo_by_binding[binding->binding_id] = ubo;

              break;
            }
            case VkMergedResourceBinding::Type::StorageBuffer: {
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

              //////////////////////////////////////////////////////
              // Find the corresponding SSBO
              //////////////////////////////////////////////////////

              VkFxShaderStorageBlock* ssbo = nullptr;
              auto it = pipeline->_vk_program->_vk_ssbo_blocks.find(binding->name);
              if (it != pipeline->_vk_program->_vk_ssbo_blocks.end()) {
                ssbo = it->second.get();
              }

              if (ssbo) {
                //////////////////////////////////////////////////////
                // Track this SSBO for the pipeline with its binding ID
                //////////////////////////////////////////////////////

                pipeline->_storage_blocks.push_back(ssbo);
                pipeline->_ssbo_by_binding[binding->binding_id] = ssbo;
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
              if (vk_program->_frgshader && vk_program->_frgshader->_ssbo_refs) {
                if (vk_program->_frgshader->_ssbo_refs->_ssbo_blocks.count(binding->name) > 0) {
                  ssbo_stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
                }
              }
              // Use the computed stages, or fall back to vertex if none found
              binding->stage_flags = ssbo_stages ? ssbo_stages : VK_SHADER_STAGE_VERTEX_BIT;

              break;
            }
            default:
              OrkAssert(false); // Unknown resource type
              break;
          }

          vk_binding.descriptorCount    = 1;
          // Use binding-specific stage flags for storage buffers, ALL_GRAPHICS for others
          if (binding->type == VkMergedResourceBinding::Type::StorageBuffer && binding->stage_flags != 0) {
            vk_binding.stageFlags = binding->stage_flags;
          } else {
            vk_binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
          }
          vk_binding.pImmutableSamplers = nullptr;

          bindings.push_back(vk_binding);

        } // for (const auto& binding : source->bindings) {

      } // for (const auto& source : sources) {

      //////////////////////////////////////////////////////
      // Create descriptor set layout for this descriptor set
      //////////////////////////////////////////////////////

      if (!bindings.empty()) {

        //std::vector<VkDescriptorBindingFlags> bindingFlags;
        //bindingFlags.resize(bindings.size(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

        //VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
        //bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        //bindingFlagsInfo.bindingCount = bindingFlags.size();
        //bindingFlagsInfo.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo LCI = {};
        initializeVkStruct(LCI, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        LCI.bindingCount = bindings.size();
        LCI.pBindings    = bindings.data();
        //LCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        //LCI.pNext = &bindingFlagsInfo;
        //LCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

        VkDescriptorSetLayout dset_layout;
        VkResult OK = vkCreateDescriptorSetLayout(_contextVK->_vkdevice, &LCI, nullptr, &dset_layout);
        OrkAssert(VK_SUCCESS == OK);

        pipeline->_dset_layouts.push_back(dset_layout);
      }

    } // for (const auto& [set_id, sources] : resources->descriptor_sets) {

    //////////////////////////////////////////////////////
    // Sort uniform blocks by binding ID for consistent ordering with dynamic offsets
    // Build a reverse map to get binding IDs for each UBO
    //////////////////////////////////////////////////////

    std::map<VkFxShaderUniformBlk*, uint32_t> ubo_to_binding;
    for (const auto& [binding_id, ubo] : pipeline->_ubo_by_binding) {
      ubo_to_binding[ubo] = binding_id;
    }

    std::sort(
        pipeline->_uniform_blocks.begin(),
        pipeline->_uniform_blocks.end(),
        [&ubo_to_binding](const VkFxShaderUniformBlk* a, const VkFxShaderUniformBlk* b) {
          // First sort by descriptor set, then by binding within the set
          if (a->_descriptor_set_id != b->_descriptor_set_id) {
            return a->_descriptor_set_id < b->_descriptor_set_id;
          }
          // Look up binding IDs from the map
          uint32_t binding_a = ubo_to_binding.at(const_cast<VkFxShaderUniformBlk*>(a));
          uint32_t binding_b = ubo_to_binding.at(const_cast<VkFxShaderUniformBlk*>(b));
          return binding_a < binding_b;
        });

    // Sort SSBOs by descriptor set and binding for consistent ordering
    std::map<VkFxShaderStorageBlock*, uint32_t> ssbo_to_binding;
    for (const auto& [binding_id, ssbo] : pipeline->_ssbo_by_binding) {
      ssbo_to_binding[ssbo] = binding_id;
    }

    std::sort(
        pipeline->_storage_blocks.begin(),
        pipeline->_storage_blocks.end(),
        [&ssbo_to_binding](const VkFxShaderStorageBlock* a, const VkFxShaderStorageBlock* b) {
          // First sort by descriptor set, then by binding within the set
          if (a->_descriptor_set_id != b->_descriptor_set_id) {
            return a->_descriptor_set_id < b->_descriptor_set_id;
          }
          // Look up binding IDs from the map
          uint32_t binding_a = ssbo_to_binding.at(const_cast<VkFxShaderStorageBlock*>(a));
          uint32_t binding_b = ssbo_to_binding.at(const_cast<VkFxShaderStorageBlock*>(b));
          return binding_a < binding_b;
        });

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

vkpipeline_obj_ptr_t VkFxInterface::_createPipelineSSBO(vkprimclass_ptr_t primclass,
                                                         vkrasterstate_ptr_t vkrstate) {

  OrkAssert(_currentVKPASS != nullptr);
  vkpipeline_obj_ptr_t pipeline = std::make_shared<VkPipelineObject>(_contextVK);
  auto shprog = _currentVKPASS->_vk_program;
  pipeline->_vk_program  = shprog;
  pipeline->_rasterstate = vkrstate;
  auto fbi = _contextVK->_fbi;
  auto rtg = fbi->_active_rtgroup;
  auto rtg_impl = rtg->_impl.getShared<VkRtGroupImpl>();

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
  // dynamic states (viewport, scissor, blend constants)
  ////////////////////////////////////////////////////

  std::vector<VkDynamicState> dynamic_states = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
    VK_DYNAMIC_STATE_BLEND_CONSTANTS
  };
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  initializeVkStruct(dynamicState, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
  dynamicState.dynamicStateCount = dynamic_states.size();
  dynamicState.pDynamicStates    = dynamic_states.data();

  PIPE_CREATE_INFO.pDynamicState = &dynamicState;

  VkPipelineViewportStateCreateInfo VPSTATE = {};
  VPSTATE.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  VPSTATE.viewportCount = 1;
  VPSTATE.pViewports    = nullptr;
  VPSTATE.scissorCount  = 1;
  VPSTATE.pScissors     = nullptr;

  PIPE_CREATE_INFO.pViewportState = &VPSTATE;

  ////////////////////////////////////////////////////
  // MSAA state
  ////////////////////////////////////////////////////

  VkPipelineMultisampleStateCreateInfo MSAA = {};
  initializeVkStruct(MSAA, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  MSAA.sampleShadingEnable   = VK_FALSE;
  MSAA.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
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

  VkResult OK = vkCreatePipelineLayout(
      _contextVK->_vkdevice,
      &PLCI,
      nullptr,
      &pipeline->_pipelineLayout);
  OrkAssert(VK_SUCCESS == OK);

  PIPE_CREATE_INFO.layout = pipeline->_pipelineLayout;

  ///////////////////////////////////////////////////
  // create the graphics pipeline
  ///////////////////////////////////////////////////

  OK = vkCreateGraphicsPipelines(
      _contextVK->_vkdevice,
      VK_NULL_HANDLE,
      1,
      &PIPE_CREATE_INFO,
      nullptr,
      &pipeline->_pipeline);

  if (OK != VK_SUCCESS) {
    printf("_createPipelineSSBO: vkCreateGraphicsPipelines failed with VkResult=%d\n", OK);
  }
  OrkAssert(VK_SUCCESS == OK);

  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
