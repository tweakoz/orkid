////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include "vulkan_ub_layout.inl"
#include <ork/lev2/gfx/shadman.h>
#include <ork/util/hexdump.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_vkpip = logger()->createChannel("VKPIP", fvec3(1,1,.2), true);

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
  vkrasterstate_ptr_t vkrstate;
  if (auto try_vkrs = _current_rasterstate->_impl.tryAsShared<VkRasterState>()) {
    vkrstate = try_vkrs.value();
  } else {
    vkrstate = _current_rasterstate->_impl.makeShared<VkRasterState>(_current_rasterstate);
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

  uint64_t rtg_pbits = check_pb_range(rtg_impl->_pipeline_bits, 4);
  uint64_t pc_pbits  = check_pb_range(primclass->_pipeline_bits, 4);

  int vb_pbits = check_pb_range(vb->pipelineBitsForFormat(),4);
  
  uint64_t sh_pbits = _pipelineBitsForShader(shprog);
  sh_pbits          = check_pb_range(sh_pbits, 24);

  uint64_t rs_pbits = check_pb_range(vkrstate->_pipeline_bits, 8);

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
              case VkMergedResourceBinding::Type::UniformBlock:
                vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                break;
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
      
      PLCI.setLayoutCount = descriptor_set_layouts.size();
      PLCI.pSetLayouts = descriptor_set_layouts.data();
      
      // Store the merged resource layouts in the pipeline object for descriptor set allocation
      rval->_merged_resource_descriptor_set_layouts = descriptor_set_layouts;
      
      logchan_vkpip->log("Pipeline layout will have %zu descriptor set layouts", descriptor_set_layouts.size());
      
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

void VkFxInterface::_bindPipeline(VkCommandBuffer cmdbuf, vkpipeline_obj_ptr_t pipe) {

  if (_currentPipeline != pipe) {
    vkCmdBindPipeline(
        cmdbuf,                          // command buffer
        VK_PIPELINE_BIND_POINT_GRAPHICS, // pipeline type
        pipe->_pipeline);                // pipeline
    _currentPipeline            = pipe;
    _currentPipeline->_viewport = nullptr;
    _currentPipeline->_scissor  = nullptr;
  }

  auto fbi    = _contextVK->_fbi;
  auto fbi_vp = fbi->_viewportTracker;
  auto fbi_sc = fbi->_scissorTracker;

  if (pipe->_viewport != fbi_vp) {
    pipe->_viewport = fbi_vp;
    VkViewport vkvp = {};
    vkvp.x          = fbi_vp->_x;
    vkvp.width      = fbi_vp->_width;

    vkvp.minDepth = 0.0f;
    vkvp.maxDepth = 1.0f;

    // vkvp.y = fbi_vp->_y;
    // v/kvp.height = fbi_vp->_height;
    //  flipped (vk origin at upper left)
    vkvp.y      = (fbi_vp->_y + fbi_vp->_height);
    vkvp.height = -fbi_vp->_height;

    // printf( "SETVP<%p> x<%f> y<%f> w<%f> h<%f>\n", pipe.get(), vkvp.x, vkvp.y, vkvp.width, vkvp.height);
    vkCmdSetViewport(
        cmdbuf, // command buffer
        0,      // first viewport
        1,      // viewport count
        &vkvp); // viewport data
  }
  if (pipe->_scissor != fbi_sc) {
    pipe->_scissor     = fbi_sc;
    VkRect2D vksc      = {};
    vksc.offset.x      = fbi_sc->_x;
    vksc.offset.y      = fbi_sc->_y;
    vksc.extent.width  = fbi_sc->_width;
    vksc.extent.height = fbi_sc->_height;
    // printf( "SETSC<%p> x<%d> y<%d> w<%d> h<%d>\n", pipe.get(), vksc.offset.x, vksc.offset.y, vksc.extent.width,
    // vksc.extent.height);
    vkCmdSetScissor(
        cmdbuf, // command buffer
        0,      // first scissor
        1,      // scissor count
        &vksc); // scissor data
  }
}

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

  if (num_params) {
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
          printf(
              "parm<%s:%s:%zu> range_offset<%d> dst_offset<%zu> ", //
              parm_type.c_str(),
              parm_name.c_str(),
              parm_size,
              int(ranges[0].offset),
              dst_offset);
          printf("\n");
        }
        auto dest_base = data + ranges[0].offset;
        OrkAssert((dst_offset + parm_size) <= blocksize);
        memcpy(dest_base + dst_offset, item._value.data(), parm_size);
      }
    }
    // hexdumpbytes(data,blocksize);
    vkCmdPushConstants(
        cmdbuf,
        _pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,         // dest-offset
        blocksize, // size
        data       // src-data
    );
    _vk_program->_pending_params.clear();
  }
  for (auto op : _vk_program->_pending_param_ops) {
    op();
  }
  _vk_program->_pending_param_ops.clear();
}

///////////////////////////////////////////////////////////////////////////////

VulkanDescriptorSetCache::VulkanDescriptorSetCache(vkcontext_rawptr_t ctx)
    : _ctxVK(ctx) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindDescriptorSet(fxdescriptorsetbindpoint_constptr_t bindingpoint, fxdescriptorset_constptr_t the_set) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_bindGfxDescriptorSetOnSlot(VkCommandBuffer cmdbuf, vkdescriptorset_ptr_t desc_set, size_t slot) {
  // Only bind if desc_set is not nullptr (i.e., there are descriptor sets)
  if (desc_set) {
    vkCmdBindDescriptorSets(
        cmdbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,   // pipeline bind point
        _currentPipeline->_pipelineLayout, // pipeline layout
        slot,                              // index into descriptor sets slots
        1,
        &desc_set->_vkdescset, // bind 1 descriptor set
        0,
        nullptr); // dynamic offsets
    _active_gfx_descriptorSets[slot] = desc_set;
  }
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
    logchan_vkpip->log("Program has no merged resource bindings - returning null descriptor set");
    return nullptr; // No descriptor sets needed for push constants only
  }

  boost::Crc64 crc64;
  crc64.init();
  
  // Include merged resource bindings in hash calculation
  for (auto it : program->_merged_resource_bindings) {
    auto param = it.first;
    auto [set_id, binding_id] = it.second;
    auto vk_tex = program->_textures_by_orkparam[param];
    auto img_obj = vk_tex->_imgobj;
    
    crc64.accumulateItem(set_id);
    crc64.accumulateItem(binding_id);
    crc64.accumulateItem(vk_tex.get());
    crc64.accumulateItem(img_obj.get());
    crc64.accumulateItem(vk_tex->_image_params_hash);
    crc64.accumulateItem(vk_tex->_vkdescriptor_info.imageView);
  }
  
  crc64.finish();
  uint64_t descset_bits = crc64.result();
  // printf( "dscache<%p> descset_bits<%016llx>\n", this, descset_bits );
  auto it = _vkDescriptorSetByHash.find(descset_bits);

  vkdescriptorset_ptr_t descset_ptr = nullptr;
  if (it == _vkDescriptorSetByHash.end()) {
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

    // Update descriptor set with merged resource bindings
    for (auto it : program->_merged_resource_bindings) {
      auto param = it.first;
      auto [set_id, binding_id] = it.second;
      auto vk_tex = program->_textures_by_orkparam[param];
      auto& desc_info = vk_tex->_vkdescriptor_info;
      OrkAssert(desc_info.imageView != VK_NULL_HANDLE);
      
      VkWriteDescriptorSet DWRITE = {};
      initializeVkStruct(DWRITE, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
      DWRITE.dstSet          = descset_ptr->_vkdescset;
      DWRITE.dstBinding      = binding_id; // Use merged resource binding ID
      DWRITE.descriptorCount = 1;
      DWRITE.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      DWRITE.pImageInfo      = &desc_info;

      logchan_vkpip->log("update descset (merged): set<%d> bidx<%d> tex<%p> param<%s>", 
                         set_id, binding_id, (void*)vk_tex.get(), param->_name.c_str());

      vkUpdateDescriptorSets(
          _ctxVK->_vkdevice, // device
          1,
          &DWRITE, // descriptor write
          0,
          nullptr // descriptor copy
      );
    }
  } else {
    descset_ptr = it->second;
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
