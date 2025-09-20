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

  vkpipeline_obj_ptr_t rval = std::make_shared<VkPipelineObject>(_contextVK);
  auto shprog = _currentVKPASS->_vk_program;
  rval->_vk_program         = shprog;
  auto fbi = _contextVK->_fbi;
  auto gbi = _contextVK->_gbi;
  auto rtg       = fbi->_active_rtgroup;
  auto rtg_impl  = rtg->_impl.getShared<VkRtGroupImpl>();

  ///////////////////////////////////////////////////
  // pipeline report
  ///////////////////////////////////////////////////
  std::string report_filename;
  if (0) {

    // Generate pipeline report for debugging descriptor set issues

    // Generate filename matching shader report schema
    // Use shader filename and technique name, process URI like in shader reports
    std::string shader_name_raw = shprog->_shader_file ? shprog->_shader_file->_shader_name : "unknown";

    // Process shader name to extract just the filename from URI (e.g., "orkshader://pbr.fxv2" -> "pbr.fxv2")
    file::Path shader_path  = shader_name_raw;
    auto shader_leaf        = shader_path.toBFS().leaf();
    std::string shader_name = shader_leaf.string();

    std::string technique_name = _currentORKTEK->_techniqueName;

    // Find pass index by searching through technique's passes
    int pass_num = 0;
    for (size_t i = 0; i < _currentVKTEK->_vk_passes.size(); ++i) {
      if (_currentVKTEK->_vk_passes[i] == _currentVKPASS) {
        pass_num = i;
        break;
      }
    }

    // Get stage directory
    const char* stage_env = std::getenv("OBT_STAGE");
    if (stage_env) {
      std::string stage_dir  = std::string(stage_env);
      std::string report_dir = stage_dir + "/vulkanpipe_reports";

      // Create directory if it doesn't exist
      file::Path report_path(report_dir);
      report_path.ensureDirectoryExists();

      // Generate report filename: shadername.technique.passnum.md
      report_filename = FormatString("%s/%s.%s.%d.md", report_dir.c_str(), shader_name.c_str(), technique_name.c_str(), pass_num);

      logchan_vkpipc->log("Pipeline report will be written to: %s", report_filename.c_str());
    }
  }

  auto& CINFO = rval->_VKGFXPCI;
  initializeVkStruct(CINFO, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);

  CINFO.flags      = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
  CINFO.renderPass = VK_NULL_HANDLE;
  CINFO.subpass    = 0;

  // Dynamic rendering info
  rtg_impl->_prinfo_retain = std::make_shared<VulkanPipelineRenderInfo>(rtg);

  OrkAssert(rtg_impl->_prinfo_retain);
  CINFO.pNext = &rtg_impl->_prinfo_retain->_createInfo; // Set the dynamic rendering info
  // count shader stages
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

  CINFO.stageCount          = stages.size();
  CINFO.pStages             = stages.data();
  CINFO.pVertexInputState   = &vtx_state->_vertex_input_state;
  CINFO.pInputAssemblyState = &primclass->_input_assembly_state;

  ////////////////////////////////////////////////////
  // dynamic states (viewport, scissor)
  ////////////////////////////////////////////////////

  std::vector<VkDynamicState> dynamic_states    = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  initializeVkStruct(dynamicState, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
  dynamicState.dynamicStateCount = dynamic_states.size(); // We have two dynamic states: viewport and scissor
  dynamicState.pDynamicStates    = dynamic_states.data();

  CINFO.pDynamicState = &dynamicState;

  VkPipelineViewportStateCreateInfo VPSTATE = {};
  VPSTATE.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  VPSTATE.viewportCount                     = 1;       // You can adjust this based on your needs
  VPSTATE.pViewports                        = nullptr; // Since you're setting this dynamically
  VPSTATE.scissorCount                      = 1;       // This should match viewportCount
  VPSTATE.pScissors                         = nullptr; // Assuming you're also setting scissor dynamically

  CINFO.pViewportState = &VPSTATE;

  ////////////////////////////////////////////////////
  // msaa state
  ////////////////////////////////////////////////////

  VkPipelineMultisampleStateCreateInfo MSAA = {};
  initializeVkStruct(MSAA, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  MSAA.sampleShadingEnable   = VK_FALSE;              // Enable/Disable sample shading
  MSAA.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT; // No multisampling
  MSAA.minSampleShading      = 1.0f;                  // Minimum fraction for sample shading; closer to 1 is smoother
  MSAA.pSampleMask           = nullptr;               // Optional
  MSAA.alphaToCoverageEnable = VK_FALSE;              // Enable/Disable alpha to coverage
  MSAA.alphaToOneEnable      = VK_FALSE;              // Enable/Disable alpha to one

  CINFO.pMultisampleState = &MSAA; // msaa_impl->_VKSTATE; // todo : dynamic

  ////////////////////////////////////////////////////
  // raster states
  ////////////////////////////////////////////////////

  CINFO.pRasterizationState = &vkrstate->_VKRSCI;
  CINFO.pDepthStencilState  = &vkrstate->_VKDSSCI;
  CINFO.pColorBlendState    = &vkrstate->_VKCBSI;

  ////////////////////////////////////////////////////
  // pipeline layout...
  ////////////////////////////////////////////////////

  VkPipelineLayoutCreateInfo PLCI;

  initializeVkStruct(PLCI, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);

  ////////////////////////////////////////////////////
  // push constants
  ////////////////////////////////////////////////////

  if (shprog->_pushConstantBlock) {
    PLCI.pushConstantRangeCount = shprog->_pushConstantBlock->_ranges.size();
    PLCI.pPushConstantRanges    = shprog->_pushConstantBlock->_ranges.data();
  }

  ////////////////////////////////////////////////////
  // descriptors - NEW: Use merged resource data instead of legacy reflection
  ////////////////////////////////////////////////////
  if (shprog->_tek_name == "FWD_DEPTHPREPASS_RI_NI_MO") {
    // OrkBreak();
  }

  // Store descriptor set layouts for cleanup later
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts;

  // Debug: Check merged resources availability
  logchan_vkpipc->log("DEBUG: Checking merged resources for pipeline creation");
  logchan_vkpipc->log("  _currentVKPASS: %s", _currentVKPASS ? "valid" : "null");
  if (_currentVKPASS) {
    logchan_vkpipc->log("  _currentVKPASS->_merged_resources: %s", _currentVKPASS->_merged_resources ? "valid" : "null");
    if (_currentVKPASS->_merged_resources) {
      logchan_vkpipc->log("  descriptor_sets.size(): %zu", _currentVKPASS->_merged_resources->descriptor_sets.size());
      for (const auto& [set_id, sources] : _currentVKPASS->_merged_resources->descriptor_sets) {
        logchan_vkpipc->log("    Set %d: %zu sources", set_id, sources.size());
      }
    }
  }

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
              rval->_uniform_blocks.push_back(ubo);
              rval->_ubo_by_binding[binding->binding_id] = ubo;

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

        descriptor_set_layouts.push_back(dset_layout);

        logchan_vkpipc->log("  Created descriptor set layout for set %d with %zu bindings", set_id, bindings.size());
      }
    }

    // Sort uniform blocks by binding ID for consistent ordering with dynamic offsets
    // Build a reverse map to get binding IDs for each UBO
    std::map<VkFxShaderUniformBlk*, uint32_t> ubo_to_binding;
    for (const auto& [binding_id, ubo] : rval->_ubo_by_binding) {
      ubo_to_binding[ubo] = binding_id;
    }

    std::sort(
        rval->_uniform_blocks.begin(),
        rval->_uniform_blocks.end(),
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

    logchan_vkpipc->log("Pipeline has %zu uniform blocks tracked for dynamic updates", rval->_uniform_blocks.size());

    PLCI.setLayoutCount = descriptor_set_layouts.size();
    PLCI.pSetLayouts    = descriptor_set_layouts.data();

    // Store the merged resource layouts in the pipeline object for descriptor set allocation
    rval->_merged_resource_descriptor_set_layouts = descriptor_set_layouts;

    logchan_vkpipc->log("Pipeline layout will have %zu descriptor set layouts", descriptor_set_layouts.size());

    // Store report filename in pipeline for later updates

    ///////////////////////////////////////////////////
    // Write pipeline report if filename was generated
    ///////////////////////////////////////////////////

    if (!report_filename.empty() && _currentVKPASS->_merged_resources) {
      rval->_report_filename = report_filename;
      FILE* fp               = fopen(report_filename.c_str(), "w");
      if (fp) {
        // Header - use same shader name processing as filename
        std::string shader_name_raw = shprog->_shader_file ? shprog->_shader_file->_shader_name : "unknown";
        file::Path shader_path      = shader_name_raw;
        auto shader_leaf            = shader_path.toBFS().leaf();
        std::string shader_name     = shader_leaf.string();

        int pass_num = 0;
        for (size_t i = 0; i < _currentVKTEK->_vk_passes.size(); ++i) {
          if (_currentVKTEK->_vk_passes[i] == _currentVKPASS) {
            pass_num = i;
            break;
          }
        }
        fprintf(
            fp,
            "# Vulkan Pipeline Report: %s - %s - Pass %d\n",
            shader_name.c_str(),
            _currentORKTEK->_techniqueName.c_str(),
            pass_num);
        time_t now = time(0);
        fprintf(fp, "Generated: %s", ctime(&now));
        //fprintf(fp, "Pipeline Hash: 0x%016llx\n\n", pipeline_hash);

        // Descriptor Set Layout Creation
        fprintf(fp, "## Descriptor Set Layout Creation\n\n");

        int layout_index = 0;
        for (const auto& [set_id, sources] : _currentVKPASS->_merged_resources->descriptor_sets) {
          fprintf(fp, "### Descriptor Set %d\n\n", set_id);
          fprintf(fp, "**Layout Index:** %d | **Sources:** %zu\n\n", layout_index++, sources.size());

          // Table of bindings as created in layout
          fprintf(fp, "```\n");
          fprintf(fp, "Bind | Type         | Stages | Source              | Name\n");
          fprintf(fp, "-----|--------------|--------|---------------------|--------------------------------\n");

          // Collect all bindings for this set
          std::vector<std::tuple<int, std::string, std::string, std::string, std::string>> layout_bindings;

          for (const auto& source : sources) {
            for (const auto& binding : source->bindings) {
              std::string type_str;
              switch (binding->type) {
                case VkMergedResourceBinding::Type::Sampler:
                  type_str = "Sampler";
                  break;
                case VkMergedResourceBinding::Type::UniformBlock:
                  type_str = "UBO";
                  break;
                case VkMergedResourceBinding::Type::StorageBuffer:
                  type_str = "SSBO";
                  break;
                default:
                  type_str = "Unknown";
                  break;
              }

              layout_bindings.push_back(
                  std::make_tuple(
                      binding->binding_id,
                      type_str,
                      "ALL_GFX", // We use VK_SHADER_STAGE_ALL_GRAPHICS
                      binding->original_source,
                      binding->name));
            }
          }

          // Sort by binding ID for clarity
          std::sort(layout_bindings.begin(), layout_bindings.end(), [](const auto& a, const auto& b) {
            return std::get<0>(a) < std::get<0>(b);
          });

          for (const auto& [bind_id, type, stages, source, name] : layout_bindings) {
            fprintf(fp, "%4d | %-12s | %-6s | %-19s | %s\n", bind_id, type.c_str(), stages.c_str(), source.c_str(), name.c_str());
          }
          fprintf(fp, "```\n\n");
        }

        fclose(fp);
        logchan_vkpipc->log("Pipeline report written to: %s", report_filename.c_str());
      }
    }

  } else {
    // No descriptor sets available - this is valid for shaders that only use push constants
    logchan_vkpipc->log("No descriptor sets available - shader uses only push constants");
    PLCI.setLayoutCount = 0;
    PLCI.pSetLayouts    = nullptr;
  }

  ////////////////////////////////////////////////////

  VkResult OK = vkCreatePipelineLayout(
      _contextVK->_vkdevice,   // device
      &PLCI,                   // pipeline layout create info
      nullptr,                 // allocator
      &rval->_pipelineLayout); // pipeline layout
  OrkAssert(VK_SUCCESS == OK);

  CINFO.layout = rval->_pipelineLayout;

  OK = vkCreateGraphicsPipelines(
      _contextVK->_vkdevice, // device
      VK_NULL_HANDLE,        // pipeline cache
      1,                     // count
      &CINFO,                // create info
      nullptr,               // allocator
      &rval->_pipeline);

  OrkAssert(VK_SUCCESS == OK);

  return rval;
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
