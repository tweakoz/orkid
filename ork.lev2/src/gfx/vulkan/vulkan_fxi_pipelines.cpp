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
// resolve effective rasterstate
//   this is an interplay between:
//     - current rasterstate stack top
//     - pass stateblock rasterstate (if any)
//   higher priority state wins
///////////////////////////////////////////////////////////////////////////////

rasterstate_ptr_t VkFxInterface::_effectiveRasterState() {
  rasterstate_ptr_t effective_rasterstate = _rasterstate_stack.resolve();
  int iraspri = effective_rasterstate ? effective_rasterstate->_priority : 0;
  if (_currentVKPASS && _currentVKPASS->_stateblock_rasterstate) {
    // State block was pre-resolved at shader load time - just use it!
    auto try_rs = _currentVKPASS->_stateblock_rasterstate;
    if (try_rs->_priority >= iraspri) {
      effective_rasterstate = _currentVKPASS->_stateblock_rasterstate;
    }
  }
  return effective_rasterstate;
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
  // Attachment count/formats come from the IMPL (the actually-attached
  // buffers): texture-array slice RTGs have no ork-level buffers, and blend
  // state sized off the ork level then mismatched the real pass layout
  // (same defect class as VulkanPipelineRenderInfo — see its note).
  int attachment_count = (int)rtg_impl->_color_buffer_impls.size();

  // Get formats for each attachment
  std::vector<VkFormat> formats;
  for (auto& cbi : rtg_impl->_color_buffer_impls) {
    formats.push_back(cbi->_vkfmt);
  }

  /////////////////////////////////////////////////////////////////////

  rasterstate_ptr_t effective_rasterstate = _effectiveRasterState();

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

  uint64_t rtg_pbits = check_plbits_range(rtg_impl->layoutBits(), 4);
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

uint64_t VkFxShaderPassState::samplersHash() {
  // Cached. bindParam* paths zero _samplers_hash when a binding changes,
  // forcing a recompute on the next call. 0 is reserved for "dirty".
  if (_samplers_hash != 0) {
    return _samplers_hash;
  }
  boost::Crc64 the_crc;
  the_crc.init();
  for (auto& [param, tex] : _textures_by_orkparam) {
    // Acquire-load _front_idx to synchronize with the streaming swap
    // callback's release-store. Without this, _imgview_hash (a non-atomic
    // Crc64) and _descset_sampling (a non-atomic shared_ptr assignment)
    // can be read mid-write — producing torn / mixed-generation state and
    // poisoning the descriptor-set cache with H_new → desc_OLD entries
    // that cause the visible texture to appear to revert a cycle.
    // We mix the front_idx into the hash too so the cache invalidates
    // whenever a streaming swap fires (non-streaming textures have
    // front_idx pinned to 0 so this is a no-op for them).
    int front_idx = tex->_front_idx.load(std::memory_order_acquire);
    the_crc.accumulateItem(reinterpret_cast<uintptr_t>(tex.get()));
    // The ADDRESS above is reused across publish-and-free cycles; the serial
    // is monotonic, so the key moves even for a successor object that landed
    // on its predecessor's address (and even if some creation path forgets to
    // seed _imgview_hash below).
    the_crc.accumulateItem(tex->_serial_number);
    the_crc.accumulateItem(tex->_format_hash);
    the_crc.accumulateItem(tex->_imgview_hash.result());
    the_crc.accumulateItem(uint64_t(front_idx));
    // Sampler is part of the descriptor set's combined-image-sampler binding;
    // a swap from ApplySamplingMode must invalidate the cached descriptor set.
    the_crc.accumulateItem(reinterpret_cast<uintptr_t>(tex->_vksampler.get()));
  }
  // Include storage buffer pointers so different SSBOs produce different cache keys
  for (auto* storage_state : _ordered_storage_states) {
    the_crc.accumulateItem(reinterpret_cast<uintptr_t>(storage_state->_bound_buffer.get()));
  }
  uint64_t result = the_crc.finished();
  if (result == 0) result = 1; // reserve 0 for "dirty"
  _samplers_hash = result;
  return _samplers_hash;
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
  // IMPL-sourced attachments — see the note in _fetchPipeline.
  int attachment_count = (int)rtg_impl->_color_buffer_impls.size();

  std::vector<VkFormat> formats;
  for (auto& cbi : rtg_impl->_color_buffer_impls) {
    formats.push_back(cbi->_vkfmt);
  }

  /////////////////////////////////////////////////////////////////////

  rasterstate_ptr_t effective_rasterstate = _effectiveRasterState();

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

  uint64_t rtg_pbits = check_plbits_range(rtg_impl->layoutBits(), 4);
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
// Taskless mesh pipeline (MESH+FRAGMENT). No vertex buffer AND no primclass — the
// mesh stage declares its own output topology, so neither vertex-format nor
// input-assembly bits participate in the cache key.
///////////////////////////////////////////////////////////////////////////////

vkpipelinestate_rawptr_t VkFxInterface::_fetchPipelineMesh() {

  auto fbi    = _contextVK->_fbi;
  auto shprog = _currentVKPASS;

  ////////////////////////////////////////////////////
  // Get attachment count and formats from active render target group
  ////////////////////////////////////////////////////

  auto rtg             = fbi->_active_rtgroup;
  auto rtg_impl        = rtg->_impl.getShared<VkRtGroupImpl>();
  // IMPL-sourced attachments — see the note in _fetchPipeline.
  int attachment_count = (int)rtg_impl->_color_buffer_impls.size();

  std::vector<VkFormat> formats;
  for (auto& cbi : rtg_impl->_color_buffer_impls) {
    formats.push_back(cbi->_vkfmt);
  }

  /////////////////////////////////////////////////////////////////////

  rasterstate_ptr_t effective_rasterstate = _effectiveRasterState();

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
  // compute pipeline hash (no vertex format, no primclass)
  /////////////////////////////////////////////////////////////////////

  auto check_plbits_range = [](uint64_t inp, int nbits) -> uint64_t {
    uint64_t maxval = (1 << nbits);
    OrkAssert(inp < maxval);
    return inp;
  };

  uint64_t rtg_pbits = check_plbits_range(rtg_impl->layoutBits(), 4);
  uint64_t sh_pbits  = check_plbits_range(_pipelineBitsForShader(shprog), 24);
  uint64_t rs_pbits  = check_plbits_range(vkrstate->_pipeline_bits, 8);

  // sentinel vertex-format bits: 0xF marks SSBO-only, 0xE marks mesh-stage-only
  uint64_t vb_pbits = 0xE;
  uint64_t pc_pbits = 0;

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
    pipeline = _createPipelineMesh(vkrstate);

  OrkAssert(pipeline != nullptr);
  return pipeline.get();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
