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
static logchannel_ptr_t logchan_vkpip = logger()->configureChannel("VKPIP", fvec3(1,1,.2), false);

vkpipeline_obj_ptr_t VkFxInterface::_fetchPipeline(
    vkvtxbuf_ptr_t vb,             //
    vkprimclass_ptr_t primclass) { //

  vkpipeline_obj_ptr_t rval;
  auto fbi = _contextVK->_fbi;
  auto gbi = _contextVK->_gbi;

    auto shprog = _currentVKPASS->_vk_program;

  if(0)printf("_fetchPipeline: tek<%s> shprog<%p> vif<%s>\n", 
         _currentORKTEK->_techniqueName.c_str(),
         shprog.get(), 
         shprog->_vertexinterface ? shprog->_vertexinterface->_name.c_str() : "null");

  ////////////////////////////////////////////////////
  // rasterstate info
  ////////////////////////////////////////////////////

  OrkAssert(_current_rasterstate != nullptr);
  rasterstate_ptr_t effective_rasterstate = _current_rasterstate;

  // Use pre-resolved state block rasterstate if present
  if (_currentVKPASS && _currentVKPASS->_stateblock_rasterstate) {
    // State block was pre-resolved at shader load time - just use it!
    effective_rasterstate = _currentVKPASS->_stateblock_rasterstate;
  }

  ////////////////////////////////////////////////////
  // get pipeline hash from permutations
  ////////////////////////////////////////////////////

  auto check_pb_range = [](uint64_t inp, int nbits) -> uint64_t {
    uint64_t maxval = (1 << nbits);
    // printf( "check_pb_range nbits<%d> maxval<%d> inp<%d>\n", nbits, maxval, inp);
    OrkAssert(inp < maxval);
    return inp;
  };

  EVtxStreamFormat vb_fmt = vb->_ork_vtxbuf.meStreamFormat;

  auto rtg       = fbi->_active_rtgroup;
  auto rtg_impl  = rtg->_impl.getShared<VkRtGroupImpl>();
  auto msaa_impl = rtg_impl->_msaaState;
  
  // Get attachment count and formats from active render target group
  int attachment_count = rtg->numImageBuffers(); // Get number of color attachments
  if (attachment_count == 0) {
    attachment_count = 1; // Default to 1 if no MRT
  }
  
  // Get formats for each attachment
  std::vector<VkFormat> formats;
  for (int i = 0; i < attachment_count; i++) {
    auto buffer = rtg->buffer(i);
    if (buffer) {
      auto vk_fmt = VkFormatConverter::convertBufferFormat(buffer->format());
      formats.push_back(vk_fmt);
    }
  }
if(shprog->_tek_name == "FWD_DEPTHPREPASS_RI_NI_MO"){
    printf("WTF\n");
}

  vkrasterstate_ptr_t vkrstate;
  if (auto try_vkrs = effective_rasterstate->_impl.tryAsShared<VkRasterState>()) {
    vkrstate = try_vkrs.value();
    // Check if attachment count matches (need to recreate if different)
    if (vkrstate->_attachment_count != attachment_count) {
      // Need to recreate with correct attachment count and formats
      vkrstate = std::make_shared<VkRasterState>(effective_rasterstate, attachment_count, &formats);
      effective_rasterstate->_impl.set<vkrasterstate_ptr_t>(vkrstate);
    }
  } else {
    vkrstate = effective_rasterstate->_impl.makeShared<VkRasterState>(effective_rasterstate, attachment_count, &formats);
  }

  uint64_t rtg_pbits = check_pb_range(rtg_impl->_pipeline_bits, 4);
  uint64_t pc_pbits  = check_pb_range(primclass->_pipeline_bits, 4);

  int vb_pbits = check_pb_range(vb->pipelineBitsForFormat(),4);
  
  uint64_t sh_pbits = _pipelineBitsForShader(shprog);
  sh_pbits          = check_pb_range(sh_pbits, 24);

  uint64_t rs_pbits = check_pb_range(vkrstate->_pipeline_bits, 8);

  if(0)printf( "RS_PBITS<%llx>\n", rs_pbits );

  // hash renderpass ?

  uint64_t pipeline_hash = vb_pbits            // 4  (4)
                           | (rtg_pbits << 4)  // 4  (8)
                           | (pc_pbits << 8)   // 4  (12)
                           | (sh_pbits << 12)  // 24 (36)
                           | (rs_pbits << 36); // 8  (44)

  ////////////////////////////////////////////////////
  // find or create pipeline
  ////////////////////////////////////////////////////

  auto it = _pipelines.find(pipeline_hash);
  if (it == _pipelines.end()) { // create pipeline

    logchan_vkpip->log(
        "CREATE PIPELINE<%016llx> vb_pbits<%d> rtg_pbits<%llx> pc_pbits<%llx> sh_pbits<%llx> rs_pbits<%llx>", //
        pipeline_hash,
        vb_pbits,
        rtg_pbits,
        pc_pbits,
        sh_pbits,
        rs_pbits);


    rval                      = std::make_shared<VkPipelineObject>(_contextVK);
    _pipelines[pipeline_hash] = rval;
    rval->_vk_program         = shprog;

    ///////////////////////////////////////////////////
    // pipeline report
    ///////////////////////////////////////////////////
    std::string report_filename;
    if(0){

      // Generate pipeline report for debugging descriptor set issues
      
      // Generate filename matching shader report schema
      // Use shader filename and technique name, process URI like in shader reports
      std::string shader_name_raw = shprog->_shader_file ? shprog->_shader_file->_shader_name : "unknown";
      
      // Process shader name to extract just the filename from URI (e.g., "orkshader://pbr.fxv2" -> "pbr.fxv2")
      file::Path shader_path = shader_name_raw;
      auto shader_leaf = shader_path.toBFS().leaf();
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
        std::string stage_dir = std::string(stage_env);
        std::string report_dir = stage_dir + "/vulkanpipe_reports";
        
        // Create directory if it doesn't exist
        file::Path report_path(report_dir);
        report_path.ensureDirectoryExists();
        
        // Generate report filename: shadername.technique.passnum.md
        report_filename = FormatString("%s/%s.%s.%d.md",
                                      report_dir.c_str(),
                                      shader_name.c_str(),
                                      technique_name.c_str(),
                                      pass_num);
        
        logchan_vkpip->log("Pipeline report will be written to: %s", report_filename.c_str());
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

    auto VIF = shprog->_vertexinterface;
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
  if(shprog->_tek_name == "FWD_DEPTHPREPASS_RI_NI_MO"){
    //OrkBreak();
  }

    // Store descriptor set layouts for cleanup later
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    
    // Debug: Check merged resources availability
    logchan_vkpip->log("DEBUG: Checking merged resources for pipeline creation");
    logchan_vkpip->log("  _currentVKPASS: %s", _currentVKPASS ? "valid" : "null");
    if (_currentVKPASS) {
      logchan_vkpip->log("  _currentVKPASS->_merged_resources: %s", _currentVKPASS->_merged_resources ? "valid" : "null");
      if (_currentVKPASS->_merged_resources) {
        logchan_vkpip->log("  descriptor_sets.size(): %zu", _currentVKPASS->_merged_resources->descriptor_sets.size());
        for (const auto& [set_id, sources] : _currentVKPASS->_merged_resources->descriptor_sets) {
          logchan_vkpip->log("    Set %d: %zu sources", set_id, sources.size());
        }
      }
    }
    
    if (_currentVKPASS && _currentVKPASS->_merged_resources && !_currentVKPASS->_merged_resources->descriptor_sets.empty()) {
      logchan_vkpip->log("Creating descriptor set layouts from merged resources");
      
      // Create descriptor set layouts from merged resource data
      for (const auto& [set_id, sources] : _currentVKPASS->_merged_resources->descriptor_sets) {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        
        logchan_vkpip->log("  Descriptor Set %d: %zu sources", set_id, sources.size());
        
        for (const auto& source : sources) {
          logchan_vkpip->log("    Source: %s (%s) - %zu bindings", 
                             source->source_name.c_str(),
                             source->source_type.c_str(),
                             source->bindings.size());
          
          for (const auto& binding : source->bindings) {
            VkDescriptorSetLayoutBinding vk_binding = {};
            vk_binding.binding = binding->binding_id;
            
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
                  logchan_vkpip->log("FATAL: UBO '%s' declared in merged resources but not found in datablock", binding->name.c_str());
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
            
            vk_binding.descriptorCount = 1;
            vk_binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS; // TODO: Make more specific per shader stage
            vk_binding.pImmutableSamplers = nullptr;
            
            bindings.push_back(vk_binding);
            
            logchan_vkpip->log("      Binding %d: %s (%s) from %s", 
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
          LCI.pBindings = bindings.data();
          
          VkDescriptorSetLayout dset_layout;
          VkResult OK = vkCreateDescriptorSetLayout(_contextVK->_vkdevice, &LCI, nullptr, &dset_layout);
          OrkAssert(VK_SUCCESS == OK);
          
          descriptor_set_layouts.push_back(dset_layout);
          
          logchan_vkpip->log("  Created descriptor set layout for set %d with %zu bindings", set_id, bindings.size());
        }
      }
      
      // Sort uniform blocks by binding ID for consistent ordering with dynamic offsets
      // Build a reverse map to get binding IDs for each UBO
      std::map<VkFxShaderUniformBlk*, uint32_t> ubo_to_binding;
      for (const auto& [binding_id, ubo] : rval->_ubo_by_binding) {
        ubo_to_binding[ubo] = binding_id;
      }
      
      std::sort(rval->_uniform_blocks.begin(),
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
      
      logchan_vkpip->log("Pipeline has %zu uniform blocks tracked for dynamic updates", rval->_uniform_blocks.size());
      
      PLCI.setLayoutCount = descriptor_set_layouts.size();
      PLCI.pSetLayouts = descriptor_set_layouts.data();
      
      // Store the merged resource layouts in the pipeline object for descriptor set allocation
      rval->_merged_resource_descriptor_set_layouts = descriptor_set_layouts;
      
      logchan_vkpip->log("Pipeline layout will have %zu descriptor set layouts", descriptor_set_layouts.size());
      
      // Store report filename in pipeline for later updates
      
      ///////////////////////////////////////////////////
      // Write pipeline report if filename was generated
      ///////////////////////////////////////////////////

      if (!report_filename.empty() && _currentVKPASS->_merged_resources) {
        rval->_report_filename = report_filename;
        FILE* fp = fopen(report_filename.c_str(), "w");
        if (fp) {
          // Header - use same shader name processing as filename
          std::string shader_name_raw = shprog->_shader_file ? shprog->_shader_file->_shader_name : "unknown";
          file::Path shader_path = shader_name_raw;
          auto shader_leaf = shader_path.toBFS().leaf();
          std::string shader_name = shader_leaf.string();
          
          int pass_num = 0;
          for (size_t i = 0; i < _currentVKTEK->_vk_passes.size(); ++i) {
            if (_currentVKTEK->_vk_passes[i] == _currentVKPASS) {
              pass_num = i;
              break;
            }
          }
          fprintf(fp, "# Vulkan Pipeline Report: %s - %s - Pass %d\n", 
                  shader_name.c_str(),
                  _currentORKTEK->_techniqueName.c_str(),
                  pass_num);
          time_t now = time(0);
          fprintf(fp, "Generated: %s", ctime(&now));
          fprintf(fp, "Pipeline Hash: 0x%016llx\n\n", pipeline_hash);
          
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
                
                layout_bindings.push_back(std::make_tuple(
                  binding->binding_id,
                  type_str,
                  "ALL_GFX",  // We use VK_SHADER_STAGE_ALL_GRAPHICS
                  binding->original_source,
                  binding->name
                ));
              }
            }
            
            // Sort by binding ID for clarity
            std::sort(layout_bindings.begin(), layout_bindings.end(),
                     [](const auto& a, const auto& b) {
                       return std::get<0>(a) < std::get<0>(b);
                     });
            
            for (const auto& [bind_id, type, stages, source, name] : layout_bindings) {
              fprintf(fp, "%4d | %-12s | %-6s | %-19s | %s\n",
                      bind_id, type.c_str(), stages.c_str(), source.c_str(), name.c_str());
            }
            fprintf(fp, "```\n\n");
          }
          
          fclose(fp);
          logchan_vkpip->log("Pipeline report written to: %s", report_filename.c_str());
        }
      }
      
    } else {
      // No descriptor sets available - this is valid for shaders that only use push constants
      logchan_vkpip->log("No descriptor sets available - shader uses only push constants");
      PLCI.setLayoutCount = 0;
      PLCI.pSetLayouts = nullptr;
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

  } else { // pipeline already cached!
    rval = it->second;
  }

  ////////////////////////////////////////////////////
  OrkAssert(rval != nullptr);
  ////////////////////////////////////////////////////
  return rval;
}

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
        pipeline->_pipeline);                // pipeline
    _currentPipeline            = pipeline;
    _currentPipeline->_viewport = nullptr;
    _currentPipeline->_scissor  = nullptr;
  }

  ////////////////////////////////////////
  // set dynamic viewport (if changed)
  ////////////////////////////////////////

  if (pipeline->_viewport != fbi_vp) {
    pipeline->_viewport = fbi_vp;
    VkViewport vkvp = {};
    vkvp.x          = fbi_vp->_x;
    vkvp.width      = fbi_vp->_width;

    vkvp.minDepth = 0.0f;
    vkvp.maxDepth = 1.0f;

    if(not FLIP_Y_LIKE_OPENGL) {
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
    pipeline->_scissor     = fbi_sc;
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

void VkFxInterface::_uploadPipelineData(VkCommandBuffer CB, 
                                        vkpipeline_obj_ptr_t pipeline){
  auto prog      = _currentVKPASS->_vk_program;
  
  // Apply dynamic UBO updates for this draw
  // This allocates per-draw memory and copies shadow buffers
  static uint32_t frame_index = 0; // TODO: Get actual frame index from swapchain
  pipeline->applyPendingUboUpdates(CB, frame_index);
  
  // Flush uniform blocks BEFORE fetching descriptor set
  // This ensures the GPU buffers have the correct data when bound
  // Note: With dynamic UBOs, this may become unnecessary
  _flushDirtyUniformBlocks();
  
  if(prog->_tek_name=="FWD_DEPTHPREPASS_RI_NI_MO"){
    //OrkBreak();
  }
  auto desc_set = pipeline->_descriptorSetCache->fetchDescriptorSetForProgram(prog);
  if (desc_set) {
    // Bind descriptor set with dynamic offsets from applyPendingUboUpdates
    if (!pipeline->_dynamic_offsets.empty()) {
      vkCmdBindDescriptorSets(
        CB,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->_pipelineLayout,
        0,  // first set
        1,  // set count
        &desc_set->_vkdescset,
        pipeline->_dynamic_offsets.size(),
        pipeline->_dynamic_offsets.data()
      );
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
        cmdbuf,             // command buffer
        slot,                      // slot to bind to
        1,                         // binding count
        &vb->_vkbuffer->_vkbuffer, // buffers
        &offset);                  // offsets
    _active_vbs[slot] = vb;
  }
}

///////////////////////////////////////////////////////////////////////////////

VkPipelineObject::VkPipelineObject(vkcontext_rawptr_t ctx) {
  _descriptorSetCache = std::make_shared<VulkanDescriptorSetCache>(ctx);
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
  //hexdumpbytes(data,blocksize);
  
  // Push each range separately so shaders see their data at offset 0
  for (const auto& range : ranges) {
    // Each range gets pushed to offset 0 for its shader stage
    // The shader sees its uniform_set starting at offset 0
    vkCmdPushConstants(
        cmdbuf,
        _pipelineLayout,
        range.stageFlags,    // Only the stages that use this range
        0,                    // Shader sees it at offset 0
        range.size,           // Size of this range
        data + range.offset   // Source data at the range's offset in our buffer
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
    auto allocation = g_dynamic_ubo_system->allocate(
      ubo->_shadow_buffer.size(),
      frame_index
    );
    
    // Copy shadow buffer to dynamic allocation
    memcpy(allocation.cpu_ptr,
           ubo->_shadow_buffer.data(),
           ubo->_shadow_buffer.size());
        
    // Track offset for descriptor binding
    _dynamic_offsets.push_back(allocation.dynamic_offset);
  }
  
  // Note: The actual descriptor set binding with dynamic offsets will happen
  // in the draw call when descriptor sets are bound
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_bindGfxDescriptorSetOnSlot(VkCommandBuffer cmdbuf,         //
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
    logchan_vkpip->log("Program<%s> has no merged resource bindings - returning null descriptor set", program->_tek_name.c_str());
    return nullptr; // No descriptor sets needed for push constants only
  }

  boost::Crc64 crc64;
  crc64.init();
  
  // Include merged resource bindings in hash calculation
  for (auto it : program->_merged_resource_bindings) {
    auto param = it.first;
    auto [set_id, binding_id] = it.second;
    
    crc64.accumulateItem(set_id);
    crc64.accumulateItem(binding_id);
    
    // Check if this is a texture binding
    auto tex_it = program->_textures_by_orkparam.find(param);
    if (tex_it != program->_textures_by_orkparam.end()) {
      auto vk_tex = tex_it->second;
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
  auto it = _vkDescriptorSetByHash.find(descset_bits);
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
    if (_ctxVK->_fxi->_currentPipeline && 
        !_ctxVK->_fxi->_currentPipeline->_merged_resource_descriptor_set_layouts.empty()) {
      // Use the first merged resource layout (assuming single descriptor set for now)
      layout_to_use = _ctxVK->_fxi->_currentPipeline->_merged_resource_descriptor_set_layouts[0];
      logchan_vkpip->log("Using merged resource descriptor set layout: %p", (void*)layout_to_use);
    } else {
      OrkAssert(false); // No valid descriptor set layout found - merged resources should always be available
    }
    
    DSAI.pSetLayouts = &layout_to_use;

    //printf("ALLOC DESC SET<%d:%p>\n", descset_count, descset_ptr.get());
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
    auto param = it.first;
    auto [set_id, binding_id] = it.second;
    // Only process textures here, skip UBOs
    auto tex_it = program->_textures_by_orkparam.find(param);
    if (tex_it != program->_textures_by_orkparam.end()) {
      auto vk_tex = tex_it->second;
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
              logchan_vkpip->log("update descset (merged): set<%d> bidx<%d> tex<%p> name<%s>", 
                                set_id, binding->binding_id, (void*)vk_tex.get(), binding->name.c_str());
            } else {
              // Use default texture for unbound samplers
              // Determine texture type from datatype string if possible
              if (binding->datatype.find("Cube") != std::string::npos) {
                vk_tex = _ctxVK->_defaultTexImplCube;
              } else if (binding->datatype.find("Array") != std::string::npos || 
                        binding->datatype.find("2DA") != std::string::npos) {
                vk_tex = _ctxVK->_defaultTexImpl2DArray;
              } else if (binding->datatype.find("3D") != std::string::npos) {
                vk_tex = _ctxVK->_defaultTexImpl3D;
              } else {
                vk_tex = _ctxVK->_defaultTexImpl2D; // Default to 2D
              }
              logchan_vkpip->log("update descset (default): set<%d> bidx<%d> tex<%p> name<%s> type<%s>", 
                                set_id, binding->binding_id, (void*)vk_tex.get(), 
                                binding->name.c_str(), binding->datatype.c_str());
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
  if(1)logchan_vkpip->log("UBO_DESC_CHECK: _currentVKPASS<%p>", (void*)_ctxVK->_fxi->_currentVKPASS.get());
  if (_ctxVK->_fxi->_currentVKPASS) {
    if(1)logchan_vkpip->log("UBO_DESC_CHECK: _merged_resources<%p>", (void*)_ctxVK->_fxi->_currentVKPASS->_merged_resources.get());
  }
  if (_ctxVK->_fxi->_currentVKPASS && _ctxVK->_fxi->_currentVKPASS->_merged_resources) {
    auto merged_resources = _ctxVK->_fxi->_currentVKPASS->_merged_resources;
    auto vk_program = _ctxVK->_fxi->_currentVKPASS->_vk_program;
    if(1)logchan_vkpip->log("UBO_DESC_CHECK: Found merged_resources with %zu descriptor sets", merged_resources->descriptor_sets.size());
    
    for (const auto& [set_id, sources] : merged_resources->descriptor_sets) {
      for (const auto& source : sources) {
        for (const auto& binding : source->bindings) {
          if(1)logchan_vkpip->log("UBO_DESC_CHECK: Binding<%s> type<%d> UniformBlock=%d",
                 binding->name.c_str(),
                 (int)binding->type,
                 (int)VkMergedResourceBinding::Type::UniformBlock);
          if (binding->type == VkMergedResourceBinding::Type::UniformBlock) {
            // Find the corresponding VkFxShaderUniformBlk
            VkFxShaderUniformBlk* ubo_block = nullptr;
            
            // Search in the program's uniform blocks
            if(1)logchan_vkpip->log("UBO_DESC_CHECK: Looking for UBO<%s> in program's _vk_uniformblks", binding->name.c_str());
            auto it = vk_program->_vk_uniformblks.find(binding->name);
            if (it != vk_program->_vk_uniformblks.end()) {
              ubo_block = it->second.get();
              if(1)logchan_vkpip->log("UBO_DESC_CHECK: Found UBO<%s> ptr<%p>", binding->name.c_str(), (void*)ubo_block);
            } else {
              if(1)logchan_vkpip->log("UBO_DESC_CHECK: UBO<%s> NOT FOUND in _vk_uniformblks", binding->name.c_str());
            }
            
            if (ubo_block) {
              if(1)logchan_vkpip->log("UBO_DESC_CHECK: UBO<%s> _buffer_size<%zu>",
                     binding->name.c_str(),
                     ubo_block->_buffer_size);
            }
            
            if (ubo_block && ubo_block->_buffer_size > 0) {
              // Use global dynamic UBO buffer
              extern VkDynamicUBOSystem* g_dynamic_ubo_system;
              OrkAssert(g_dynamic_ubo_system != nullptr);
              auto global_buffer = g_dynamic_ubo_system->get_buffer();
              OrkAssert(global_buffer != nullptr);
              
              VkDescriptorBufferInfo buffer_info = {};
              buffer_info.buffer = global_buffer->_vkbuffer;
              buffer_info.offset = 0;  // Dynamic offset will be provided at bind time
              buffer_info.range = ubo_block->_buffer_size;
              buffer_infos.push_back(buffer_info);
              
              VkWriteDescriptorSet DWRITE = {};
              initializeVkStruct(DWRITE, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
              DWRITE.dstSet          = descset_ptr->_vkdescset;
              DWRITE.dstBinding      = binding->binding_id;
              DWRITE.descriptorCount = 1;
              DWRITE.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
              DWRITE.pBufferInfo     = &buffer_infos.back();
              
              logchan_vkpip->log("UBO_DESC_UPDATE: ubo<%s> binding<%d> global_buffer<%p> size<%zu> block_ptr<%p>",
                     binding->name.c_str(), binding->binding_id,
                     (void*)global_buffer->_vkbuffer, ubo_block->_buffer_size,
                     (void*)ubo_block);
              
              descriptor_writes.push_back(DWRITE);
            } else if (ubo_block) {
              if(0)logchan_vkpip->log("UBO_DESC_CHECK: SKIPPING UBO<%s> - zero size", binding->name.c_str());
            }
          }
        }
      }
    }
  }
  
  // Update all descriptors at once
  if (!descriptor_writes.empty()) {
    vkUpdateDescriptorSets(
        _ctxVK->_vkdevice,
        descriptor_writes.size(),
        descriptor_writes.data(),
        0,
        nullptr
    );
    
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
                    if (binding->binding_id == write.dstBinding && 
                        binding->type == VkMergedResourceBinding::Type::UniformBlock) {
                      resource_str = binding->name;
                      break;
                    }
                  }
                  if (!resource_str.empty()) break;
                }
                if (!resource_str.empty()) break;
              }
            }
          }
          
          updates.push_back(std::make_tuple(write.dstBinding, type_str, resource_str));
        }
        
        std::sort(updates.begin(), updates.end(),
                 [](const auto& a, const auto& b) {
                   return std::get<0>(a) < std::get<0>(b);
                 });
        
        for (const auto& [bind_id, type, resource] : updates) {
          fprintf(fp, "%4d | %-7s | %s\n", bind_id, type.c_str(), resource.c_str());
        }
        fprintf(fp, "```\n\n");
        
        // Compare with expected layout
        fprintf(fp, "### Binding Verification\n\n");
        fprintf(fp, "Comparing descriptor set updates with layout creation to identify mismatches.\n\n");
        
        fclose(fp);
        logchan_vkpip->log("Appended descriptor set update to pipeline report");
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
      //printf("No Texture impl tex<%p:%s>\n", pTex, pTex->_debugName.c_str());
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
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
