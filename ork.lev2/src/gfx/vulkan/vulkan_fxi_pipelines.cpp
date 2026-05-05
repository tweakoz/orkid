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
static logchannel_ptr_t logchan_vkpip = logger()->configureChannel("VKPIP", fvec3(1, 1, .2), false);
///////////////////////////////////////////////////////////////////////////////

VkPipelineState::VkPipelineState(vkcontext_rawptr_t ctx) {
  _descriptorSetCache = std::make_shared<VulkanDescriptorSetCacheState>(ctx);
}

///////////////////////////////////////////////////////////////////////////////

vkpipelinestate_rawptr_t VkFxInterface::_fetchPipeline(
    vkvtxbuf_ptr_t vb,             //
    vkprimclass_ptr_t primclass) { //

  auto fbi                = _contextVK->_fbi;
  auto gbi                = _contextVK->_gbi;
  EVtxStreamFormat vb_fmt = vb->_ork_vtxbuf.meStreamFormat;

  auto shprog = _currentVKPASS;

  if (0) {
    printf(
        "_fetchPipeline: tek<%s> shprog<%p> vif<%s>\n",
        _currentORKTEK->_techniqueName.c_str(),
        shprog,
        shprog->_vertexinterface ? shprog->_vertexinterface->_name.c_str() : "null");
  }

  ////////////////////////////////////////////////////
  // Get attachment count and formats from active render target group
  ////////////////////////////////////////////////////

  auto rtg             = fbi->_active_rtgroup;
  auto rtg_impl        = rtg->_impl.getShared<VkRtGroupImpl>();
  auto msaa_impl       = rtg_impl->_msaaState;
  int attachment_count = rtg->numImageBuffers(); // Get number of color attachments

  // Get formats for each attachment
  std::vector<VkFormat> formats;
  for (int i = 0; i < attachment_count; i++) {
    auto buffer = rtg->buffer(i);
    if (buffer) {
      auto vk_fmt = VkFormatConverter::convertBufferFormat(buffer->format());
      formats.push_back(vk_fmt);
    }
  }

  /////////////////////////////////////////////////////////////////////
  // resolve effective rasterstate
  //   this is an interplay between:
  //     - current rasterstate stack top
  //     - pass stateblock rasterstate (if any)
  //   higher priority state wins
  /////////////////////////////////////////////////////////////////////

  rasterstate_ptr_t effective_rasterstate = _rasterstate_stack.resolve();
  int iraspri = effective_rasterstate ? effective_rasterstate->_priority : 0;
  if (_currentVKPASS && _currentVKPASS->_stateblock_rasterstate) {
    // State block was pre-resolved at shader load time - just use it!
    auto try_rs = _currentVKPASS->_stateblock_rasterstate;
    if(try_rs->_priority>=iraspri){
      effective_rasterstate = _currentVKPASS->_stateblock_rasterstate;
    }
  }

  /////////////////////////////////////////////////
  // first check if we already have a VkRasterState
  /////////////////////////////////////////////////

  vkrasterstate_ptr_t vkrstate;

  if (auto try_vkrs = effective_rasterstate->_impl.tryAsShared<VkRasterState>()) {
    vkrstate = try_vkrs.value();

    // Check if attachment count or formats changed (need to recreate if different)
    bool invalidate = false;

    if (vkrstate->_attachment_count != attachment_count) {
      invalidate = true;
    }

    // Check if formats match
    if (!invalidate && vkrstate->_attachment_count == formats.size()) {
      for (int i = 0; i < attachment_count; i++) {
        if (vkrstate->_vkformats[i] != formats[i]) {
          invalidate = true;
          break;
        }
      }
    } else if (!invalidate) {
      invalidate = true; // Size mismatch
    }

    if (invalidate) {
      vkrstate = nullptr;
    }
  }

  /////////////////////////////////////////////////
  // we do not, so create one
  /////////////////////////////////////////////////

  if (nullptr == vkrstate) { 
    vkrstate = effective_rasterstate->_impl.makeShared<VkRasterState>(
        effective_rasterstate, //
        attachment_count,      //
        &formats);             //
        vkrstate->_ork_rasterstate = effective_rasterstate.get();
  }

  /////////////////////////////////////////////////////////////////////
  // compute pipeline bits (for hashing pipeline state)
  /////////////////////////////////////////////////////////////////////

  auto check_plbits_range = [](uint64_t inp, int nbits) -> uint64_t {
    uint64_t maxval = (1 << nbits);
    // printf( "check_plbits_range nbits<%d> maxval<%d> inp<%d>\n", nbits, maxval, inp);
    OrkAssert(inp < maxval);
    return inp;
  };

  uint64_t rtg_pbits = check_plbits_range(rtg_impl->_pipeline_bits, 4);
  uint64_t pc_pbits  = check_plbits_range(primclass->_pipeline_bits, 4);

  int vb_pbits = check_plbits_range(vb->pipelineBitsForFormat(), 4);

  uint64_t sh_pbits = _pipelineBitsForShader(shprog);
  sh_pbits          = check_plbits_range(sh_pbits, 24);

  uint64_t rs_pbits = check_plbits_range(vkrstate->_pipeline_bits, 8);

  if (0)
    printf("RS_PBITS<%llx>\n", (ull)rs_pbits);

  // hash renderpass ?

  uint64_t pipeline_hash = vb_pbits            // 4  (4)
                           | (rtg_pbits << 4)  // 4  (8)
                           | (pc_pbits << 8)   // 4  (12)
                           | (sh_pbits << 12)  // 24 (36)
                           | (rs_pbits << 36); // 8  (44)

  ////////////////////////////////////////////////////
  // find or create pipeline
  ////////////////////////////////////////////////////

  auto& pipeline = _pipelines[pipeline_hash];
  if (!pipeline) 
    pipeline = _createPipeline(vb, primclass, vkrstate);

  OrkAssert(pipeline != nullptr);
  return pipeline.get();
}

///////////////////////////////////////////////////////////////////////////////

uint64_t VkFxShaderPassState::samplersHash() const {
  // Always recalculate to pick up changes in texture/SSBO bindings
  boost::Crc64 the_crc;
  the_crc.init();
  for (auto& [param, tex] : _textures_by_orkparam) {
    the_crc.accumulateItem(reinterpret_cast<uintptr_t>(tex.get()));
    the_crc.accumulateItem(tex->_format_hash);
    the_crc.accumulateItem(tex->_imgview_hash.result());
  }
  // Include storage buffer pointers so different SSBOs produce different cache keys
  for (auto* storage_state : _ordered_storage_states) {
    the_crc.accumulateItem(reinterpret_cast<uintptr_t>(storage_state->_bound_buffer.get()));
  }
  return the_crc.finished();
}

///////////////////////////////////////////////////////////////////////////////
// SSBO-only pipeline (no vertex buffer, shader reads from storage buffer)
///////////////////////////////////////////////////////////////////////////////

vkpipelinestate_rawptr_t VkFxInterface::_fetchPipelineSSBO(vkprimclass_ptr_t primclass) {

  auto fbi    = _contextVK->_fbi;
  auto shprog = _currentVKPASS;

  ////////////////////////////////////////////////////
  // Get attachment count and formats from active render target group
  ////////////////////////////////////////////////////

  auto rtg             = fbi->_active_rtgroup;
  auto rtg_impl        = rtg->_impl.getShared<VkRtGroupImpl>();
  int attachment_count = rtg->numImageBuffers();

  std::vector<VkFormat> formats;
  for (int i = 0; i < attachment_count; i++) {
    auto buffer = rtg->buffer(i);
    if (buffer) {
      auto vk_fmt = VkFormatConverter::convertBufferFormat(buffer->format());
      formats.push_back(vk_fmt);
    }
  }

  /////////////////////////////////////////////////////////////////////
  // resolve effective rasterstate
  /////////////////////////////////////////////////////////////////////

  rasterstate_ptr_t effective_rasterstate = _rasterstate_stack.resolve();
  int iraspri = effective_rasterstate ? effective_rasterstate->_priority : 0;
  if (_currentVKPASS && _currentVKPASS->_stateblock_rasterstate) {
    auto try_rs = _currentVKPASS->_stateblock_rasterstate;
    if(try_rs->_priority >= iraspri){
      effective_rasterstate = _currentVKPASS->_stateblock_rasterstate;
    }
  }

  /////////////////////////////////////////////////
  // get or create VkRasterState
  /////////////////////////////////////////////////

  vkrasterstate_ptr_t vkrstate;

  if (auto try_vkrs = effective_rasterstate->_impl.tryAsShared<VkRasterState>()) {
    vkrstate = try_vkrs.value();
    bool invalidate = false;
    if (vkrstate->_attachment_count != attachment_count) {
      invalidate = true;
    }
    if (!invalidate && vkrstate->_attachment_count == formats.size()) {
      for (int i = 0; i < attachment_count; i++) {
        if (vkrstate->_vkformats[i] != formats[i]) {
          invalidate = true;
          break;
        }
      }
    } else if (!invalidate) {
      invalidate = true;
    }
    if (invalidate) {
      vkrstate = nullptr;
    }
  }

  if (nullptr == vkrstate) {
    vkrstate = effective_rasterstate->_impl.makeShared<VkRasterState>(
        effective_rasterstate, attachment_count, &formats);
    vkrstate->_ork_rasterstate = effective_rasterstate.get();
  }

  /////////////////////////////////////////////////////////////////////
  // compute pipeline hash (without vertex buffer format)
  /////////////////////////////////////////////////////////////////////

  auto check_plbits_range = [](uint64_t inp, int nbits) -> uint64_t {
    uint64_t maxval = (1 << nbits);
    OrkAssert(inp < maxval);
    return inp;
  };

  uint64_t rtg_pbits = check_plbits_range(rtg_impl->_pipeline_bits, 4);
  uint64_t pc_pbits  = check_plbits_range(primclass->_pipeline_bits, 4);
  uint64_t sh_pbits  = check_plbits_range(_pipelineBitsForShader(shprog), 24);
  uint64_t rs_pbits  = check_plbits_range(vkrstate->_pipeline_bits, 8);

  // Use 0xF for vertex format bits to indicate SSBO-only (no vertex buffer)
  uint64_t vb_pbits = 0xF;

  uint64_t pipeline_hash = vb_pbits            // 4  (4)
                           | (rtg_pbits << 4)  // 4  (8)
                           | (pc_pbits << 8)   // 4  (12)
                           | (sh_pbits << 12)  // 24 (36)
                           | (rs_pbits << 36); // 8  (44)

  ////////////////////////////////////////////////////
  // find or create pipeline
  ////////////////////////////////////////////////////

  auto& pipeline = _pipelines[pipeline_hash];
  if (!pipeline)
    pipeline = _createPipelineSSBO(primclass, vkrstate);

  OrkAssert(pipeline != nullptr);
  return pipeline.get();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
