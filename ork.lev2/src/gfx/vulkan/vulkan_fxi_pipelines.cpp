////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"
#include "vulkan_ub_layout.inl"
#include <ork/lev2/gfx/shadman.h>
#include <ork/util/hexdump.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

vkpipeline_obj_ptr_t VkFxInterface::_fetchPipeline(
    vkvtxbuf_ptr_t vb,             //
    vkprimclass_ptr_t primclass) { //

  vkpipeline_obj_ptr_t rval;
  auto fbi = _contextVK->_fbi;
  auto gbi = _contextVK->_gbi;

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

  auto check_pb_range = [](int inp, int nbits) -> int {
    int maxval = (1 << nbits);
    // printf( "check_pb_range nbits<%d> maxval<%d> inp<%d>\n", nbits, maxval, inp);
    OrkAssert(inp >= 0);
    OrkAssert(inp < maxval);
    return inp;
  };

  int vb_pbits = 0;//check_pb_range(vb->_vertexConfig->_pipeline_bits, 4);

  auto rtg       = fbi->_active_rtgroup;
  auto rtg_impl  = rtg->_impl.getShared<VkRtGroupImpl>();
  auto msaa_impl = rtg_impl->_msaaState;

  int rtg_pbits = check_pb_range(rtg_impl->_pipeline_bits, 4);
  int pc_pbits  = check_pb_range(primclass->_pipeline_bits, 4);

  auto shprog  = _currentVKPASS->_vk_program;

  int sh_pbits = _pipelineBitsForShader(shprog);
  sh_pbits = check_pb_range(sh_pbits, 16);

  int rs_pbits = check_pb_range(vkrstate->_pipeline_bits, 8);
  auto rpass = _contextVK->_renderpasses.back();
  auto rp_impl = rpass->_impl.getShared<VulkanRenderPass>();
  // hash renderpass ?
  
  uint64_t pipeline_hash = vb_pbits //
                         | (rtg_pbits << 4) //
                         | (pc_pbits << 8) //
                         | (sh_pbits << 16) //
                         | (rs_pbits << 32);

  ////////////////////////////////////////////////////
  // find or create pipeline
  ////////////////////////////////////////////////////

  auto it = _pipelines.find(pipeline_hash);
  if (it == _pipelines.end()) { // create pipeline

    auto VIF       = shprog->_vertexinterface;
    OrkAssert(VIF);

    rval                      = std::make_shared<VkPipelineObject>(_contextVK);
    _pipelines[pipeline_hash] = rval;
    rval->_vk_program         = shprog;

    auto& CINFO = rval->_VKGFXPCI;
    initializeVkStruct(CINFO, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);

    CINFO.flags      = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
    CINFO.renderPass = rp_impl->_vkrp;
    CINFO.subpass    = 0;

    // count shader stages
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    if (shprog->_vtxshader)
      stages.push_back(shprog->_vtxshader->_shaderstageinfo);
    if (shprog->_frgshader)
      stages.push_back(shprog->_frgshader->_shaderstageinfo);

    auto vtx_state = gbi->vertexInputState(vb,VIF);
    OrkAssert(vtx_state);

    CINFO.stageCount          = stages.size();
    CINFO.pStages             = stages.data();
    CINFO.pVertexInputState   = & vtx_state->_vertex_input_state;
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
    // descriptors
    ////////////////////////////////////////////////////

    OrkAssert(shprog->_descriptors);

    if (shprog->_descriptors) {
      size_t num_bindings = shprog->_descriptors->_vkbindings.size();
      size_t num_samplers = shprog->_descriptors->_sampler_count;
      OrkAssert(num_bindings == num_samplers);
      VkDescriptorSetLayoutCreateInfo LCI = {};
      initializeVkStruct(LCI, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
      LCI.bindingCount = shprog->_descriptors->_vkbindings.size();
      LCI.pBindings    = shprog->_descriptors->_vkbindings.data();

      VkResult OK = vkCreateDescriptorSetLayout(
          _contextVK->_vkdevice,               // device
          &LCI,                                // create info
          nullptr,                             // allocator
          &shprog->_descriptors->_dsetlayout); // descriptor set layout

      OrkAssert(VK_SUCCESS == OK);

      PLCI.setLayoutCount = 1;
      PLCI.pSetLayouts    = &shprog->_descriptors->_dsetlayout;
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

void VkFxInterface::_bindPipeline(vkpipeline_obj_ptr_t pipe) {

  auto cmdbuf = _contextVK->_cmdbufcur_gfx->_vkcmdbuf;

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

void VkFxInterface::_flushRenderPassScopedState(){
  for( int slot=0; slot<4; slot++ ){
    _active_vbs[slot] = nullptr;
    _active_gfx_descriptorSets[slot] = nullptr;
  }
  _currentPipeline = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_bindGfxDescriptorSetOnSlot(vkdescriptorset_ptr_t desc_set, size_t slot) {
  if (_active_gfx_descriptorSets[slot] != desc_set) {
    auto& CB = _contextVK->_cmdbufcur_gfx;
    vkCmdBindDescriptorSets(
        CB->_vkcmdbuf,
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

void VkFxInterface::_bindVertexBufferOnSlot(vkvtxbuf_ptr_t vb, size_t slot) {
  if (_active_vbs[slot] != vb) {
    auto& CB            = _contextVK->_cmdbufcur_gfx;
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(
        CB->_vkcmdbuf,             // command buffer
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

void VkPipelineObject::applyPendingPushConstants(vkcmdbufimpl_ptr_t cmdbuf) { //

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
              "parm<%s:%s:%zu> range_offset<%d> dst_offset<%d> ", //
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
        cmdbuf->_vkcmdbuf,
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

vkdescriptorset_ptr_t VulkanDescriptorSetCache::fetchDescriptorSetForProgram(vkfxsprg_ptr_t program) {

  /////////////////////////////////
  // todo: this is the slow path
  //    for the sake of efficiency,
  //    we will (over time) expose descriptor sets to higher level systems
  /////////////////////////////////

  boost::Crc64 crc64;
  crc64.init();
  for (auto it : program->_textures_by_binding) {
    auto binding_index = it.first;
    auto vk_tex        = it.second;
    crc64.accumulateItem(binding_index);
    crc64.accumulateItem(vk_tex.get());
  }
  crc64.finish();
  uint64_t descset_bits = crc64.result();

  auto it = _vkDescriptorSetByHash.find(descset_bits);

  vkdescriptorset_ptr_t descset_ptr = nullptr;
  if (it == _vkDescriptorSetByHash.end()) {
    // make new descriptor set
    descset_ptr                          = std::make_shared<VulkanDescriptorSet>();
    _vkDescriptorSetByHash[descset_bits] = descset_ptr;

    VkDescriptorSetAllocateInfo DSAI;
    initializeVkStruct(DSAI, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
    DSAI.descriptorPool     = _ctxVK->_vkDescriptorPool;
    DSAI.descriptorSetCount = 1;
    DSAI.pSetLayouts        = &program->_descriptors->_dsetlayout;

    VkResult OK = vkAllocateDescriptorSets(_ctxVK->_vkdevice, &DSAI, &descset_ptr->_vkdescset);
    OrkAssert(VK_SUCCESS == OK);

    for (auto it : program->_textures_by_binding) {
      auto binding_index = it.first;
      auto vk_tex        = it.second;
      auto& desc_info    = vk_tex->_vkdescriptor_info;
      OrkAssert(desc_info.imageView != VK_NULL_HANDLE);
      VkWriteDescriptorSet DWRITE = {};
      initializeVkStruct(DWRITE, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
      DWRITE.dstSet          = descset_ptr->_vkdescset;
      DWRITE.dstBinding      = binding_index; // The binding point in the shader
      DWRITE.descriptorCount = 1;
      DWRITE.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      DWRITE.pImageInfo      = &desc_info;

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
  vktexobj_ptr_t vk_tex;
  if (auto as_to = pTex->_impl.tryAsShared<VulkanTextureObject>()) {
    vk_tex = as_to.value();
  } else {
    printf( "No Texture impl tex<%p:%s>\n", pTex, pTex->_debugName.c_str() );
    //OrkAssert(false);
    return;
  }
  auto it = _samplers_by_orkparam.find(param);
  OrkAssert(it != _samplers_by_orkparam.end());
  size_t binding_index                = it->second;
  _textures_by_orkparam[param]        = vk_tex;
  _textures_by_binding[binding_index] = vk_tex;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
