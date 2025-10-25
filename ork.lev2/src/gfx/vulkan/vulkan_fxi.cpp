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

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkFxInterface::VkFxInterface(vkcontext_rawptr_t ctx)
    : _contextVK(ctx) {
    _slp_cache = _GVI->_slp_cache;

    _default_rasterstate = std::make_shared<lev2::RasterState>();
    _default_rasterstate->_depthtest = EDepthTest::LESS;
    _default_rasterstate->_culltest = ECullTest::PASS_FRONT;
    _default_rasterstate->_frontface = FLIP_Y_LIKE_OPENGL 
                                     ? EFrontFace::CLOCKWISE 
                                     : EFrontFace::COUNTER_CLOCKWISE;

    _default_rasterstate->_name = "vkdefault";
    
    // Dynamic UBO system will be initialized after Vulkan setup
}

///////////////////////////////////////////////////////////////////////////////

VkFxInterface::~VkFxInterface(){

}

// SSBO implementations moved to vulkan_fxi_buffer.cpp

///////////////////////////////////////////////////////////////////////////////

void VkComputeInterface::bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) {
  if (!shader || !buffer) {
    return;
  }

  auto vk_program = shader->_impl.get<vkfxsprg_ptr_t>();
  if (!vk_program) {
    return;
  }

  auto vk_buffer = buffer->_impl.getShared<VulkanBuffer>();
  if (!vk_buffer) {
    return;
  }

  // Find the SSBO block by binding index in the compute shader program
  // Note: This assumes the binding index corresponds to the descriptor set binding
  // In a more complete implementation, we'd need to track the binding ID per SSBO
  for (auto& [name, ssbo_block] : vk_program->_vk_ssbo_blocks) {
    // For now, bind to all SSBOs (compute shaders typically have one SSBO)
    // A more sophisticated approach would track binding IDs
    ssbo_block->_bound_buffer = vk_buffer;
    ssbo_block->_bound_ssbo = buffer;
    // TODO: Match by actual binding index when that info is available
  }
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
  _rasterstate_stack.push(rs);  // Push NEW value onto priority stack (not old top)
  _rasterstate_top = rs;         // Update cached top
  std::string name = "null";
  if( rs ){
    name = FormatString("%s-pri<%d>",
                       rs->_name.c_str(),
                       rs->_priority);
  }
  if(0) printf("PUSH rasterstate<%s> stack size: %zu\n", name.c_str(), _rasterstate_stack.size());
}
rasterstate_ptr_t VkFxInterface::_doPopRasterState() {
  OrkAssert(!_rasterstate_stack.empty());
  auto popped = _rasterstate_stack.top();  // Get what we're popping (for debug print)
  _rasterstate_stack.pop();

  std::string name = popped ? popped->_name : "null";
  if(0) printf("POP  rasterstate<%s> stack size: %zu\n", name.c_str(), _rasterstate_stack.size());

  // Don't call resolve() here - too slow (O(n) since pop invalidated cache)
  // _fetchPipeline will call resolve() when actually needed
  return popped;
}

///////////////////////////////////////////////////////////////////////////////

int VkFxInterface::BeginBlock(fxtechnique_constptr_t tek, const RenderContextInstData& data) {
  auto vk_tek = tek->_impl.get<VkFxShaderTechnique*>();
  _currentORKTEK = tek;
  _currentVKTEK = vk_tek;
  int passcount = (int) vk_tek->_vk_passes.size();
  if(passcount==1){
    auto pass = _currentVKTEK->_vk_passes[0];
    _currentVKPASS = pass;
  }
  return passcount;
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

int VkFxInterface::_pipelineBitsForShader(vkfxsprg_ptr_t shprog){

  if(shprog->_pipeline_bits_composite == -1){ // compute ?

    auto vtx_shader = shprog->_vtxshader;
    auto frg_shader = shprog->_frgshader;

    if(0)printf("/////////////////\nshprog<v:%s <f:%s> pipeline_bits_composite<%d>\n", //
           vtx_shader->_name.c_str(), //
           frg_shader->_name.c_str(),
           shprog->_pipeline_bits_composite);
           
    ////////////////////////////
    // compute VIF bits
    ////////////////////////////

    auto vif_id = vtx_shader->_vk_interfaces[0];
    auto it_vif = shprog->_shader_file->_vk_vtxinterfaces.find(vif_id);
    OrkAssert(it_vif!=shprog->_shader_file->_vk_vtxinterfaces.end());
    vkvertexinterface_ptr_t VIF = it_vif->second;
    shprog->_vertexinterface = VIF;

    boost::Crc64 crc;
    crc.init();
    for( auto input : VIF->_inputs ){
      crc.accumulateString(input->_datatype);
      crc.accumulateString(input->_semantic);
      if(0)printf("dt<%s> sem<%s>\n", input->_datatype.c_str(), input->_semantic.c_str());
    }
    crc.finish();
    uint64_t hash = crc.result();

    auto it_cvid = _vk_vtxinterface_cache.find(hash);
    if( it_cvid != _vk_vtxinterface_cache.end() ){
      VIF->_pipeline_bits = it_cvid->second;
      VIF->_hash = hash;
    }
    else{
      int new_index = _vk_vtxinterface_cache.size();
      _vk_vtxinterface_cache[hash] = new_index;
      VIF->_pipeline_bits = new_index;
      VIF->_hash = hash;
    }

    ////////////////////////////
    // compute GIF bits
    ////////////////////////////

    auto geo_shader = shprog->_geoshader;
    vkgeometryinterface_ptr_t GIF;
    if(geo_shader){
      auto gif_id = geo_shader->_vk_interfaces[0];
      auto it_gif = shprog->_shader_file->_vk_geointerfaces.find(gif_id);
      if( it_gif == shprog->_shader_file->_vk_geointerfaces.end() ){
        printf( "shader<%s> gif_id<%s> not found!!\n", //
                geo_shader->_name.c_str(), //
                gif_id.c_str() );



        for( auto gif : geo_shader->_vk_interfaces){
          printf( "  desired gif<%s>\n", gif.c_str() );
        }                
        for( auto gifitem : shprog->_shader_file->_vk_geointerfaces ){
          printf( "  present gif<%s>\n", gifitem.first.c_str() );
        }
        OrkAssert(false);
      }
      GIF = it_gif->second;
      shprog->_geometryinterface = GIF;

      crc.init();
      for( auto input : GIF->_inputs ){
        crc.accumulateString(input->_datatype);
        crc.accumulateString(input->_semantic);
      }
      crc.finish();
      hash = crc.result();

      auto it_cgid = _vk_geointerface_cache.find(hash);
      if( it_cgid != _vk_geointerface_cache.end() ){
        GIF->_pipeline_bits = it_cgid->second;
        GIF->_hash = hash;
      }
      else{
        int new_index = _vk_geointerface_cache.size();
        _vk_geointerface_cache[hash] = new_index;
        GIF->_pipeline_bits = new_index;
      }
    }

    ////////////////////////////
    // compute composite bits
    ////////////////////////////

    OrkAssert(VIF->_pipeline_bits<256);
    OrkAssert(shprog->_pipeline_bits_prg<256);
    shprog->_pipeline_bits_composite = (shprog->_pipeline_bits_prg<<0)
                                     | (VIF->_pipeline_bits<<8);

    if(GIF){
      OrkAssert(GIF->_pipeline_bits<256);
      shprog->_pipeline_bits_composite |= (GIF->_pipeline_bits<<16);
    }


  }
  return shprog->_pipeline_bits_composite;
}

///////////////////////////////////////////////////////////////////////////////

VkFxShaderTechnique::VkFxShaderTechnique(){
  _orktechnique = std::make_shared<FxShaderTechnique>();
  _orktechnique->_impl.set<VkFxShaderTechnique*>(this);
}

///////////////////////////////////////////////////////////////////////////////

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
