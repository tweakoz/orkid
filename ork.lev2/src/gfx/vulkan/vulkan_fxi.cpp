////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkFxInterface::VkFxInterface(vkcontext_rawptr_t ctx)
    : _contextVK(ctx) {
    _slp_cache = _GVI->_slp_cache;
}

///////////////////////////////////////////////////////////////////////////////

VkFxInterface::~VkFxInterface(){

}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_doBeginFrame() {
}

///////////////////////////////////////////////////////////////////////////////

vkpipeline_obj_ptr_t VkFxInterface::_fetchPipeline(vkvtxbuf_ptr_t vb, //
                                                   vkprimclass_ptr_t primclass){ //

  vkpipeline_obj_ptr_t rval;
  auto fbi = _contextVK->_fbi;
  auto gbi = _contextVK->_gbi;

  ////////////////////////////////////////////////////
  // get pipeline hash from permutations
  ////////////////////////////////////////////////////

  auto check_pb_range = [](int& inp,int nbits) -> int {
    int maxval = (1<<(nbits-1));
    OrkAssert(inp>=0);
    OrkAssert(inp<maxval);
    return inp;
  };

  int vb_pbits = check_pb_range(vb->_vertexConfig->_pipeline_bits,4);

  auto rtg = fbi->_active_rtgroup;
  auto rtg_impl = rtg->_impl.getShared<VkRtGroupImpl>();
  int rtg_pbits = check_pb_range(rtg_impl->_pipeline_bits,4);

  auto shprog = _currentVKPASS->_vk_program;
  int sh_pbits = check_pb_range(shprog->_pipeline_bits,8);

  int pc_pbits = check_pb_range(primclass->_pipeline_bits,4);

  uint64_t pipeline_hash = vb_pbits
                         | (rtg_pbits<<4)
                         | (pc_pbits<<8)
                         | (sh_pbits<<12);


  ////////////////////////////////////////////////////
  // find or create pipeline
  ////////////////////////////////////////////////////

  auto it = _pipelines.find(pipeline_hash);
  if( it == _pipelines.end() ){ // create pipeline
    rval = std::make_shared<VkPipelineObject>();
    _pipelines[pipeline_hash] = rval;

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
    CINFO.pViewportState = nullptr; // TODO
    CINFO.pRasterizationState = nullptr; // TODO (from srasterstate)
    CINFO.pDepthStencilState = nullptr; // TODO (from srasterstate)
    CINFO.pColorBlendState = nullptr; // TODO (from srasterstate)
    CINFO.pMultisampleState = nullptr; // TODO (from fbi)
    CINFO.pDynamicState = nullptr; 

    /*
    VkDescriptorSetLayoutBinding bindings[16];
    VkDescriptorSetLayoutCreateInfo layoutinfo;
    VkDescriptorSetLayout dset_layout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
    } 
    CINFO.layout = pipelineLayout;
    */

    CINFO.layout = VkPipelineLayout(); // shader input data layout / bindings
                                       // TODO: from geometry, shader
    if(0)
    vkCreateGraphicsPipelines( _contextVK->_vkdevice, // device
                               VK_NULL_HANDLE,        // pipeline cache
                               1,                     // count
                               &CINFO,                // create info
                               nullptr,               // allocator
                               &rval->_pipeline);
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
