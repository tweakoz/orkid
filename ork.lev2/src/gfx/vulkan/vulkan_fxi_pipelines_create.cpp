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
  // dynamic states (viewport, scissor)
  ////////////////////////////////////////////////////

  std::vector<VkDynamicState> dynamic_states    = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  initializeVkStruct(dynamicState, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
  dynamicState.dynamicStateCount = dynamic_states.size(); // We have two dynamic states: viewport and scissor
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

  auto shprog = _currentVKPASS->_vk_program;

  VkPipelineLayoutCreateInfo PLCI;

  initializeVkStruct(PLCI, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);

  // push constants

  if (shprog->_pushConstantBlock) {
    PLCI.pushConstantRangeCount = shprog->_pushConstantBlock->_ranges.size();
    PLCI.pPushConstantRanges    = shprog->_pushConstantBlock->_ranges.data();
  }

  // descriptors - NEW: Use merged resource data instead of legacy reflection

  // Store descriptor set layouts for cleanup later
  pipeline->_merged_resource_descriptor_set_layouts.clear();

  if (_currentVKPASS && _currentVKPASS->_merged_resources && !_currentVKPASS->_merged_resources->descriptor_sets.empty()) {
    logchan_vkpipc->log("Creating descriptor set layouts from merged resources");

    // Create descriptor set layouts from merged resource data
    for (const auto& [set_id, sources] : _currentVKPASS->_merged_resources->descriptor_sets) {
      std::vector<VkDescriptorSetLayoutBinding> bindings;

      logchan_vkpipc->log("  Descriptor Set %d: %zu sources", set_id, sources.size());

      for (const auto& source : sources) {
        logchan_vkpipc->log(
            "    Source: %s (%s) - %zu bindings",
            source->source_name.c_str(),
            source->source_type.c_str(),
            source->bindings.size());

        for (const auto& binding : source->bindings) {
          VkDescriptorSetLayoutBinding vk_binding = {};
          vk_binding.binding                      = binding->binding_id;

          // Map resource type to Vulkan descriptor type
          switch (binding->type) {
            case VkMergedResourceBinding::Type::Sampler:
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
              break;
            case VkMergedResourceBinding::Type::UniformBlock: {
              // ALL uniform blocks are now dynamic
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

              // Look up the UBO from the program's uniform blocks
              // These were loaded from the datablock
              auto vk_program = _currentVKPASS->_vk_program;
              OrkAssert(vk_program != nullptr);

              auto ubo_it = vk_program->_vk_uniformblks.find(binding->name);
              if (ubo_it == vk_program->_vk_uniformblks.end()) {
                // Fatal error: shader declares a UBO that wasn't in the datablock
                logchan_vkpipc->log(
                    "FATAL: UBO '%s' declared in merged resources but not found in datablock", binding->name.c_str());
                OrkAssert(false);
              }

              VkFxShaderUniformBlk* ubo = ubo_it->second.get();
              OrkAssert(ubo != nullptr);
              OrkAssert(ubo->_orkparamblock != nullptr);

              // Track this UBO for the pipeline with its binding ID
              pipeline->_uniform_blocks.push_back(ubo);
              pipeline->_ubo_by_binding[binding->binding_id] = ubo;

              break;
            }
            case VkMergedResourceBinding::Type::StorageBuffer:
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
              break;
            default:
              OrkAssert(false); // Unknown resource type
              break;
          }

          vk_binding.descriptorCount    = 1;
          vk_binding.stageFlags         = VK_SHADER_STAGE_ALL_GRAPHICS; // TODO: Make more specific per shader stage
          vk_binding.pImmutableSamplers = nullptr;

          bindings.push_back(vk_binding);

          logchan_vkpipc->log(
              "      Binding %d: %s (%s) from %s",
              binding->binding_id,
              binding->name.c_str(),
              binding->datatype.c_str(),
              binding->original_source.c_str());
        }
      }

      if (!bindings.empty()) {
        VkDescriptorSetLayoutCreateInfo LCI = {};
        initializeVkStruct(LCI, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        LCI.bindingCount = bindings.size();
        LCI.pBindings    = bindings.data();

        VkDescriptorSetLayout dset_layout;
        VkResult OK = vkCreateDescriptorSetLayout(_contextVK->_vkdevice, &LCI, nullptr, &dset_layout);
        OrkAssert(VK_SUCCESS == OK);

        pipeline->_merged_resource_descriptor_set_layouts.push_back(dset_layout);

        logchan_vkpipc->log("  Created descriptor set layout for set %d with %zu bindings", set_id, bindings.size());
      }
    }

    // Sort uniform blocks by binding ID for consistent ordering with dynamic offsets
    // Build a reverse map to get binding IDs for each UBO
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

    logchan_vkpipc->log("Pipeline has %zu uniform blocks tracked for dynamic updates", pipeline->_uniform_blocks.size());

    PLCI.setLayoutCount = pipeline->_merged_resource_descriptor_set_layouts.size();
    PLCI.pSetLayouts    = pipeline->_merged_resource_descriptor_set_layouts.data();

    // Store the merged resource layouts in the pipeline object for descriptor set allocation

    logchan_vkpipc->log("Pipeline layout will have %zu descriptor set layouts", pipeline->_merged_resource_descriptor_set_layouts.size());

  } else {
    // No descriptor sets available - this is valid for shaders that only use push constants
    logchan_vkpipc->log("No descriptor sets available - shader uses only push constants");
    PLCI.setLayoutCount = 0;
    PLCI.pSetLayouts    = nullptr;
  }
  return PLCI;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
