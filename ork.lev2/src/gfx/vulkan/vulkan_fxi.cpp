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

vkpipeline_obj_ptr_t VkFxInterface::_fetchPipeline(vkvtxbuf_ptr_t vb){

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

  uint64_t pipeline_hash = vb_pbits
                         | (rtg_pbits<<4)
                         | (sh_pbits<<8);


  ////////////////////////////////////////////////////
  // find or create pipeline
  ////////////////////////////////////////////////////

  auto it = _pipelines.find(pipeline_hash);
  if( it == _pipelines.end() ){ // create pipeline
    
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
  auto vk_tek = tek->_impl.getShared<VkFxShaderTechnique>();
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
