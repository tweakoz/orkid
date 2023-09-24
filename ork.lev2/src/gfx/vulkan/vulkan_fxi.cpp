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

VkFxInterface::VkFxInterface(vkcontext_rawptr_t ctx)
    : _contextVK(ctx) {
    _slp_cache = _GVI->_slp_cache;

    _default_rasterstate = std::make_shared<lev2::SRasterState>();
}

///////////////////////////////////////////////////////////////////////////////

VkFxInterface::~VkFxInterface(){

}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_doBeginFrame() {
  _currentPipeline = nullptr;
  pushRasterState(_default_rasterstate);
}
void VkFxInterface::_doEndFrame() {
  popRasterState();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_doPushRasterState(rasterstate_ptr_t rs) {
  _rasterstate_stack.push(_current_rasterstate);
  _current_rasterstate = rs;
}
rasterstate_ptr_t VkFxInterface::_doPopRasterState() {
  _current_rasterstate = _rasterstate_stack.top();
  _rasterstate_stack.pop();
  return _current_rasterstate;
}

///////////////////////////////////////////////////////////////////////////////

LockedResource<VkRasterState::rsmap_t> VkRasterState::_global_rasterstate_map;

VkRasterState::VkRasterState(rasterstate_ptr_t rstate){
  initializeVkStruct(_VKRSCI, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
  initializeVkStruct(_VKDSSCI, VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
  initializeVkStruct(_VKCBSI, VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);

  boost::Crc64 hasher;
  hasher.init();

  _VKRSCI.depthClampEnable = VK_FALSE;
  _VKRSCI.rasterizerDiscardEnable = VK_FALSE;
  _VKRSCI.polygonMode = VK_POLYGON_MODE_FILL;
  _VKRSCI.lineWidth = 1.0f;
  _VKRSCI.depthBiasEnable = VK_FALSE;
  _VKRSCI.depthBiasConstantFactor = 0.0f; // Optional
  _VKRSCI.depthBiasClamp = 0.0f;          // Optional
  _VKRSCI.depthBiasSlopeFactor = 0.0f;    // Optional

  hasher.accumulateItem(rstate->_depthtest);
  switch( rstate->_depthtest ){
    case EDepthTest::OFF: {
      _VKDSSCI.depthTestEnable = VK_FALSE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_ALWAYS;
      break;
    }
    case EDepthTest::LESS: {
      _VKDSSCI.depthTestEnable = VK_TRUE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_LESS;
      break;
    }
    case EDepthTest::LEQUALS: {
      _VKDSSCI.depthTestEnable = VK_TRUE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
      break;
    }
    case EDepthTest::GREATER: {
      _VKDSSCI.depthTestEnable = VK_TRUE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_GREATER;
      break;
    }
    case EDepthTest::GEQUALS: {
      _VKDSSCI.depthTestEnable = VK_TRUE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
      break;
    }
    case EDepthTest::EQUALS: {
      _VKDSSCI.depthTestEnable = VK_TRUE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_EQUAL;
      break;
    }
    case EDepthTest::ALWAYS: {
      _VKDSSCI.depthTestEnable = VK_TRUE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_ALWAYS;
      break;
    }
  }
  hasher.accumulateItem(rstate->_culltest);
  switch( rstate->_culltest ){
    case ECullTest::OFF: {
      _VKRSCI.cullMode = VK_CULL_MODE_NONE;
      break;
    }
    case ECullTest::PASS_FRONT: {
      _VKRSCI.cullMode = VK_CULL_MODE_BACK_BIT;
      break;
    }
    case ECullTest::PASS_BACK: {
      _VKRSCI.cullMode = VK_CULL_MODE_FRONT_BIT;
      break;
    }
  }
  hasher.accumulateItem(rstate->_frontface);
  switch(rstate->_frontface){
    case EFrontFace::CLOCKWISE: {
      _VKRSCI.frontFace = VK_FRONT_FACE_CLOCKWISE;
      break;
    }
    case EFrontFace::COUNTER_CLOCKWISE: {
      _VKRSCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      break;
    }
  }

  _VKDSSCI.depthWriteEnable = rstate->_writemaskZ ? VK_TRUE : VK_FALSE;
  _VKDSSCI.depthBoundsTestEnable = VK_FALSE;
  _VKDSSCI.minDepthBounds = 0.0f; // Optional
  _VKDSSCI.maxDepthBounds = 1.0f; // Optional
  _VKDSSCI.stencilTestEnable = VK_FALSE;
  _VKDSSCI.front = {}; // Optional
  _VKDSSCI.back = {};  // Optional

  hasher.accumulateItem(rstate->_writemaskZ);


  _VKCBATT.colorWriteMask = rstate->_writemaskRGB //
                          ? VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT // 
                          : 0;
  
  hasher.accumulateItem(rstate->_writemaskRGB);

  _VKCBATT.blendEnable = rstate->_blendEnable ? VK_TRUE : VK_FALSE;

  hasher.accumulateItem(rstate->_blendEnable);

  auto do_blend_factor = [&](BlendingFactor bf) -> VkBlendFactor {

    hasher.accumulateItem(bf);

    VkBlendFactor rval;
    switch(bf){
      case BlendingFactor::ZERO:
        rval = VK_BLEND_FACTOR_ZERO;
        break;
      case BlendingFactor::ONE:
        rval = VK_BLEND_FACTOR_ONE;
        break;
      case BlendingFactor::SRC_COLOR:
        rval = VK_BLEND_FACTOR_SRC_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_SRC_COLOR:
        rval = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        break;
      case BlendingFactor::DST_COLOR:
        rval = VK_BLEND_FACTOR_DST_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_DST_COLOR:
        rval = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        break;
      case BlendingFactor::SRC_ALPHA:
        rval = VK_BLEND_FACTOR_SRC_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_SRC_ALPHA:
        rval = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;
      case BlendingFactor::DST_ALPHA:
        rval = VK_BLEND_FACTOR_DST_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_DST_ALPHA:
        rval = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        break;
      case BlendingFactor::CONSTANT_COLOR:
        rval = VK_BLEND_FACTOR_CONSTANT_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_CONSTANT_COLOR:
        rval = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        break;
      case BlendingFactor::CONSTANT_ALPHA:
        rval = VK_BLEND_FACTOR_CONSTANT_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_CONSTANT_ALPHA:
        rval = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        break;
      case BlendingFactor::SRC_ALPHA_SATURATE:
        rval = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        break;
      default:
        OrkAssert(false);
        break;
    }
    return rval;
  };
  auto do_blend_op = [&](BlendingOp bo) -> VkBlendOp {
    hasher.accumulateItem(bo);
    VkBlendOp rval;
    switch(bo){
      case BlendingOp::ADD:
        rval = VK_BLEND_OP_ADD;
        break;
      case BlendingOp::SUBTRACT:
        rval = VK_BLEND_OP_SUBTRACT;
        break;
      case BlendingOp::REVSUBTRACT:
        rval = VK_BLEND_OP_REVERSE_SUBTRACT;
        break;
      case BlendingOp::MIN:
        rval = VK_BLEND_OP_MIN;
        break;
      case BlendingOp::MAX:
        rval = VK_BLEND_OP_MAX;
        break;
      default:
        OrkAssert(false);
       break;
    }
    return rval;
  };

  _VKCBATT.srcColorBlendFactor = do_blend_factor(rstate->_blendFactorSrcRGB);
  _VKCBATT.dstColorBlendFactor = do_blend_factor(rstate->_blendFactorDstRGB);
  _VKCBATT.colorBlendOp = do_blend_op(rstate->_blendOpRGB);
  _VKCBATT.srcAlphaBlendFactor = do_blend_factor(rstate->_blendFactorSrcA);
  _VKCBATT.dstAlphaBlendFactor = do_blend_factor(rstate->_blendFactorDstA);
  _VKCBATT.alphaBlendOp = do_blend_op(rstate->_blendOpA);

  _VKCBSI.logicOpEnable = VK_FALSE;
  _VKCBSI.logicOp = VK_LOGIC_OP_COPY; // Optional
  _VKCBSI.attachmentCount = 1;
  _VKCBSI.pAttachments = &_VKCBATT;
  _VKCBSI.blendConstants[0] = rstate->_blendConstant.x; 
  _VKCBSI.blendConstants[1] = rstate->_blendConstant.y; 
  _VKCBSI.blendConstants[2] = rstate->_blendConstant.z; 
  _VKCBSI.blendConstants[3] = rstate->_blendConstant.w; 
  hasher.accumulateItem(rstate->_blendConstant);

  hasher.finish();
  uint64_t hashed = hasher.result();

  auto op = [&](VkRasterState::rsmap_t& unlocked){

    auto it = unlocked.find(hashed);
    if( it == unlocked.end() ){
      _pipeline_bits = unlocked.size();
      //printf( "VkRasterState::VkRasterState hashed<%016llx> NEW<%d>\n", hashed, _pipeline_bits );
      unlocked[hashed] = _pipeline_bits;
      OrkAssert(_pipeline_bits<256);
      OrkAssert(_pipeline_bits>=0);
    }
    else{
      _pipeline_bits = it->second;
      //printf( "VkRasterState::VkRasterState hashed<%016llx> PREV<%d>\n", hashed, _pipeline_bits );
      OrkAssert(_pipeline_bits<256);
      OrkAssert(_pipeline_bits>=0);
    }
  };
  _global_rasterstate_map.atomicOp(op);
}

///////////////////////////////////////////////////////////////////////////////

vkpipeline_obj_ptr_t VkFxInterface::_fetchPipeline(vkvtxbuf_ptr_t vb, //
                                                   vkprimclass_ptr_t primclass){ //

  vkpipeline_obj_ptr_t rval;
  auto fbi = _contextVK->_fbi;
  auto gbi = _contextVK->_gbi;

  ////////////////////////////////////////////////////
  // rasterstate info
  ////////////////////////////////////////////////////

  OrkAssert(_current_rasterstate!=nullptr);
  vkrasterstate_ptr_t vkrstate;
  if( auto try_vkrs = _current_rasterstate->_impl.tryAsShared<VkRasterState>() ){
    vkrstate = try_vkrs.value();
  }
  else{
    vkrstate = _current_rasterstate->_impl.makeShared<VkRasterState>(_current_rasterstate);
  }

  ////////////////////////////////////////////////////
  // get pipeline hash from permutations
  ////////////////////////////////////////////////////

  auto check_pb_range = [](int inp,int nbits) -> int {
    int maxval = (1<<nbits);
    //printf( "check_pb_range nbits<%d> maxval<%d> inp<%d>\n", nbits, maxval, inp);
    OrkAssert(inp>=0);
    OrkAssert(inp<maxval);
    return inp;
  };

  int vb_pbits = check_pb_range(vb->_vertexConfig->_pipeline_bits,4);

  auto rtg = fbi->_active_rtgroup;
  auto rtg_impl = rtg->_impl.getShared<VkRtGroupImpl>();
  auto msaa_impl = rtg_impl->_msaaState;

  int rtg_pbits = check_pb_range(rtg_impl->_pipeline_bits,4);
  int pc_pbits = check_pb_range(primclass->_pipeline_bits,4);

  auto shprog = _currentVKPASS->_vk_program;
  int sh_pbits = check_pb_range(shprog->_pipeline_bits,8);

  int rs_shbits = check_pb_range(vkrstate->_pipeline_bits,8);

  uint64_t pipeline_hash = vb_pbits
                         | (rtg_pbits<<4)
                         | (pc_pbits<<8)
                         | (sh_pbits<<16)
                         | (rs_shbits<<24);


  ////////////////////////////////////////////////////
  // find or create pipeline
  ////////////////////////////////////////////////////

  auto it = _pipelines.find(pipeline_hash);
  if( it == _pipelines.end() ){ // create pipeline
    rval = std::make_shared<VkPipelineObject>();
    _pipelines[pipeline_hash] = rval;
    rval->_vk_program = shprog;

    auto& CINFO = rval->_VKGFXPCI;
    initializeVkStruct(CINFO, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);

    CINFO.flags = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
    CINFO.renderPass =  _contextVK->_fbi->_swapchain->_mainRenderPass->_vkrp;
    CINFO.subpass = 0;

    // count shader stages
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    if( shprog->_vtxshader )
      stages.push_back(shprog->_vtxshader->_shaderstageinfo);
    if( shprog->_frgshader )
      stages.push_back(shprog->_frgshader->_shaderstageinfo);
      
    CINFO.stageCount = stages.size();
    CINFO.pStages = stages.data();
    CINFO.pVertexInputState = &vb->_vertexConfig->_vertex_input_state;
    CINFO.pInputAssemblyState = &primclass->_input_assembly_state;

    ////////////////////////////////////////////////////
    // dynamic states (viewport, scissor)
    ////////////////////////////////////////////////////

    std::vector<VkDynamicState> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    initializeVkStruct(dynamicState, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
    dynamicState.dynamicStateCount = dynamic_states.size(); // We have two dynamic states: viewport and scissor
    dynamicState.pDynamicStates = dynamic_states.data();

    CINFO.pDynamicState = & dynamicState; 

    VkPipelineViewportStateCreateInfo VPSTATE = {};
    VPSTATE.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    VPSTATE.viewportCount = 1; // You can adjust this based on your needs
    VPSTATE.pViewports = nullptr; // Since you're setting this dynamically
    VPSTATE.scissorCount = 1; // This should match viewportCount
    VPSTATE.pScissors = nullptr; // Assuming you're also setting scissor dynamically
    
    CINFO.pViewportState = &VPSTATE;

    ////////////////////////////////////////////////////
    // msaa state
    ////////////////////////////////////////////////////

    VkPipelineMultisampleStateCreateInfo MSAA = {};
    initializeVkStruct(MSAA, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
    MSAA.sampleShadingEnable = VK_FALSE; // Enable/Disable sample shading
    MSAA.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // No multisampling
    MSAA.minSampleShading = 1.0f; // Minimum fraction for sample shading; closer to 1 is smoother
    MSAA.pSampleMask = nullptr; // Optional
    MSAA.alphaToCoverageEnable = VK_FALSE; // Enable/Disable alpha to coverage
    MSAA.alphaToOneEnable = VK_FALSE; // Enable/Disable alpha to one

    CINFO.pMultisampleState = &MSAA; //msaa_impl->_VKSTATE; // todo : dynamic

    ////////////////////////////////////////////////////
    // raster states
    ////////////////////////////////////////////////////

    CINFO.pRasterizationState = & vkrstate->_VKRSCI; 
    CINFO.pDepthStencilState = & vkrstate->_VKDSSCI; 
    CINFO.pColorBlendState = & vkrstate->_VKCBSI; 

    ////////////////////////////////////////////////////
    // pipeline layout...
    ////////////////////////////////////////////////////

    VkPipelineLayoutCreateInfo PLCI;

    initializeVkStruct(PLCI, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);

    ////////////////////////////////////////////////////
    // push constants
    ////////////////////////////////////////////////////

    if(shprog->_pushConstantBlock){
      PLCI.pushConstantRangeCount = shprog->_pushConstantBlock->_ranges.size();
      PLCI.pPushConstantRanges = shprog->_pushConstantBlock->_ranges.data();
    }

    ////////////////////////////////////////////////////
    // descriptors
    ////////////////////////////////////////////////////

    if(shprog->_descriptors){
      size_t num_vtx_bindings = shprog->_descriptors->_vkbindings.size();
      size_t num_samplers = shprog->_descriptors->_sampler_count;
      OrkAssert(num_vtx_bindings==num_samplers);
      VkDescriptorSetLayoutCreateInfo LCI = {};
      initializeVkStruct(LCI, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
      LCI.bindingCount = shprog->_descriptors->_vkbindings.size();
      LCI.pBindings = shprog->_descriptors->_vkbindings.data();

      vkCreateDescriptorSetLayout(_contextVK->_vkdevice, &LCI, nullptr, &shprog->_descriptors->_dsetlayout);
      PLCI.setLayoutCount = 1;
      PLCI.pSetLayouts = &shprog->_descriptors->_dsetlayout;
    }

    ////////////////////////////////////////////////////

    VkResult OK = vkCreatePipelineLayout( _contextVK->_vkdevice,   // device 
                                          &PLCI,                   // pipeline layout create info
                                          nullptr,                 // allocator
                                          &rval->_pipelineLayout); // pipeline layout
    OrkAssert(VK_SUCCESS == OK);

    CINFO.layout = rval->_pipelineLayout; 
    
    if(1){
      OK = vkCreateGraphicsPipelines( _contextVK->_vkdevice, // device
                                     VK_NULL_HANDLE,        // pipeline cache
                                     1,                     // count
                                     &CINFO,                // create info
                                     nullptr,               // allocator
                                     &rval->_pipeline);

      OrkAssert(VK_SUCCESS == OK);
    }

    VkDescriptorSetAllocateInfo DSAI;
    initializeVkStruct(DSAI, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
    DSAI.descriptorPool = _contextVK->_vkDescriptorPool;
    DSAI.descriptorSetCount = 1;
    DSAI.pSetLayouts = &shprog->_descriptors->_dsetlayout;

    OK = vkAllocateDescriptorSets(_contextVK->_vkdevice, &DSAI, &rval->_vkDescriptorSet);
    OrkAssert(VK_SUCCESS == OK);

  }
  else{ // pipeline already cached!
    rval = it->second;
  }

  ////////////////////////////////////////////////////
  OrkAssert(rval!=nullptr);
  ////////////////////////////////////////////////////
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
  
VkFxShaderProgram::VkFxShaderProgram(){
  _pushdatabuffer.reserve(1024); // todo : grow as needed
}

///////////////////////////////////////////////////////////////////////////////

void VkPipelineObject::applyPendingParams(vkcmdbufimpl_ptr_t cmdbuf ){ //

  OrkAssert(_vk_program->_pushConstantBlock!=nullptr);
  size_t num_params = _vk_program->_pending_params.size();

  if( num_params ){
    auto data_layout = _vk_program->_pushConstantBlock->_data_layout;
    auto& ranges = _vk_program->_pushConstantBlock->_ranges;
    size_t blocksize = _vk_program->_pushConstantBlock->_blockSize;

    auto data = _vk_program->_pushdatabuffer.data();

    for( auto item : _vk_program->_pending_params ){
      auto dst_offset = data_layout->offsetForParam(item._ork_param);
      if( dst_offset != -1 ){
        auto parm_name = item._ork_param->_name;
        auto parm_type = item._vk_param->_datatype;
        size_t parm_size = item._value.size();
        if(0){
          printf( "parm<%s:%s:%zu> range_offset<%d> dst_offset<%d> ", //
                parm_type.c_str(), 
                parm_name.c_str(), 
                parm_size,
                ranges[0].offset, 
                dst_offset);
          printf( "\n");
        }
        auto dest_base = data + ranges[0].offset;
        OrkAssert( (dst_offset+parm_size) <= blocksize );
        memcpy( dest_base + dst_offset, item._value.data(), parm_size );
      }
    }
    //hexdumpbytes(data,blocksize);
    vkCmdPushConstants(
        cmdbuf->_vkcmdbuf,
        _pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
        0,         // dest-offset
        blocksize, // size
        data       // src-data
    );
    _vk_program->_pending_params.clear();
  }

}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_bindPipeline(vkpipeline_obj_ptr_t pipe){

  auto cmdbuf = _contextVK->_cmdbufcur_gfx->_vkcmdbuf;

  if( _currentPipeline != pipe ){
    vkCmdBindPipeline( cmdbuf,                         // command buffer
                      VK_PIPELINE_BIND_POINT_GRAPHICS, // pipeline type
                      pipe->_pipeline);                // pipeline
    _currentPipeline = pipe;
    _currentPipeline->_viewport = nullptr;
    _currentPipeline->_scissor = nullptr;
  }

  auto fbi = _contextVK->_fbi;
  auto fbi_vp = fbi->_viewportTracker;
  auto fbi_sc = fbi->_scissorTracker;

  if( pipe->_viewport != fbi_vp ){
    pipe->_viewport = fbi_vp;
    VkViewport vkvp = {};
    vkvp.x = fbi_vp->_x;
    vkvp.width = fbi_vp->_width;

    vkvp.minDepth = 0.0f;
    vkvp.maxDepth = 1.0f;

    //vkvp.y = fbi_vp->_y;
    //v/kvp.height = fbi_vp->_height;
    // flipped (vk origin at upper left)
    vkvp.y = (fbi_vp->_y + fbi_vp->_height); 
    vkvp.height = -fbi_vp->_height; 

    //printf( "SETVP<%p> x<%f> y<%f> w<%f> h<%f>\n", pipe.get(), vkvp.x, vkvp.y, vkvp.width, vkvp.height);
    vkCmdSetViewport( cmdbuf, // command buffer
                      0,      // first viewport
                      1,      // viewport count
                      &vkvp );// viewport data
  }
  if( pipe->_scissor != fbi_sc ){
    pipe->_scissor = fbi_sc;
    VkRect2D vksc = {};
    vksc.offset.x = fbi_sc->_x;
    vksc.offset.y = fbi_sc->_y;
    vksc.extent.width = fbi_sc->_width;
    vksc.extent.height = fbi_sc->_height;
    //printf( "SETSC<%p> x<%d> y<%d> w<%d> h<%d>\n", pipe.get(), vksc.offset.x, vksc.offset.y, vksc.extent.width, vksc.extent.height);
    vkCmdSetScissor( cmdbuf, // command buffer
                     0,      // first scissor
                     1,      // scissor count
                     &vksc );// scissor data
  }

}

///////////////////////////////////////////////////////////////////////////////

int VkFxInterface::BeginBlock(fxtechnique_constptr_t tek, const RenderContextInstData& data) {
  auto vk_tek = tek->_impl.get<VkFxShaderTechnique*>();
  _currentORKTEK = tek;
  _currentVKTEK = vk_tek;
  return (int) vk_tek->_vk_passes.size();
}

///////////////////////////////////////////////////////////////////////////////

bool VkFxInterface::BindPass(int ipass) {
  auto pass = _currentVKTEK->_vk_passes[ipass];
  _currentVKPASS = pass;
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::EndPass() {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::EndBlock() {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::CommitParams(void) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::reset() {
}

///////////////////////////////////////////////////////////////////////////////

VkFxShaderTechnique::VkFxShaderTechnique(){
  _orktechnique = std::make_shared<FxShaderTechnique>();
  _orktechnique->_impl.set<VkFxShaderTechnique*>(this);
}

VkFxShaderTechnique::~VkFxShaderTechnique(){
  _orktechnique->_impl.set<void*>(nullptr);
  _orktechnique->_techniqueName = "destroyed";
  _orktechnique->_passes.clear();
  _orktechnique->_shader = nullptr;
  _orktechnique->_validated = false;
  _orktechnique = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
