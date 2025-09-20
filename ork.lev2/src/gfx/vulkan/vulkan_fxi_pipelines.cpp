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

vkpipeline_obj_ptr_t VkFxInterface::_fetchPipeline(
    vkvtxbuf_ptr_t vb,             //
    vkprimclass_ptr_t primclass) { //

  auto fbi = _contextVK->_fbi;
  auto gbi = _contextVK->_gbi;

  auto shprog = _currentVKPASS->_vk_program;

  if (0)
    printf(
        "_fetchPipeline: tek<%s> shprog<%p> vif<%s>\n",
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

  // Get formats for each attachment
  std::vector<VkFormat> formats;
  for (int i = 0; i < attachment_count; i++) {
    auto buffer = rtg->buffer(i);
    if (buffer) {
      auto vk_fmt = VkFormatConverter::convertBufferFormat(buffer->format());
      formats.push_back(vk_fmt);
    }
  }
  if (shprog->_tek_name == "FWD_DEPTHPREPASS_RI_NI_MO") {
    printf("WTF\n");
  }

  vkrasterstate_ptr_t vkrstate;
  if (auto try_vkrs = effective_rasterstate->_impl.tryAsShared<VkRasterState>()) {
    vkrstate = try_vkrs.value();
    if (vkrstate->_attachment_count != attachment_count) {
      // Check if attachment count matches (need to recreate if different)
      // invalidate existing VkRasterState
      vkrstate = nullptr;
    }
  }
  if(nullptr == vkrstate) { // If not already a VkRasterState, create one
    vkrstate = effective_rasterstate->_impl.makeShared<VkRasterState>(effective_rasterstate, attachment_count, &formats);
  }

  uint64_t rtg_pbits = check_pb_range(rtg_impl->_pipeline_bits, 4);
  uint64_t pc_pbits  = check_pb_range(primclass->_pipeline_bits, 4);

  int vb_pbits = check_pb_range(vb->pipelineBitsForFormat(), 4);

  uint64_t sh_pbits = _pipelineBitsForShader(shprog);
  sh_pbits          = check_pb_range(sh_pbits, 24);

  uint64_t rs_pbits = check_pb_range(vkrstate->_pipeline_bits, 8);

  if (0)
    printf("RS_PBITS<%llx>\n", rs_pbits);

  // hash renderpass ?

  uint64_t pipeline_hash = vb_pbits            // 4  (4)
                           | (rtg_pbits << 4)  // 4  (8)
                           | (pc_pbits << 8)   // 4  (12)
                           | (sh_pbits << 12)  // 24 (36)
                           | (rs_pbits << 36); // 8  (44)

  ////////////////////////////////////////////////////
  // find or create pipeline
  ////////////////////////////////////////////////////

  vkpipeline_obj_ptr_t rval;

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
    rval = _createPipeline(vb, primclass,vkrstate);
    _pipelines[pipeline_hash] = rval;
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
        pipeline->_pipeline);            // pipeline
    _currentPipeline            = pipeline;
    _currentPipeline->_viewport = nullptr;
    _currentPipeline->_scissor  = nullptr;
  }

  ////////////////////////////////////////
  // set dynamic viewport (if changed)
  ////////////////////////////////////////

  if (pipeline->_viewport != fbi_vp) {
    pipeline->_viewport = fbi_vp;
    VkViewport vkvp     = {};
    vkvp.x              = fbi_vp->_x;
    vkvp.width          = fbi_vp->_width;

    vkvp.minDepth = 0.0f;
    vkvp.maxDepth = 1.0f;

    if (not FLIP_Y_LIKE_OPENGL) {
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
    pipeline->_scissor = fbi_sc;
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

void VkFxInterface::_uploadPipelineData(VkCommandBuffer CB, vkpipeline_obj_ptr_t pipeline) {
  auto prog = _currentVKPASS->_vk_program;

  // Apply dynamic UBO updates for this draw
  // This allocates per-draw memory and copies shadow buffers
  static uint32_t frame_index = 0; // TODO: Get actual frame index from swapchain
  pipeline->applyPendingUboUpdates(CB, frame_index);

  // Flush uniform blocks BEFORE fetching descriptor set
  // This ensures the GPU buffers have the correct data when bound
  // Note: With dynamic UBOs, this may become unnecessary
  _flushDirtyUniformBlocks();

  if (prog->_tek_name == "FWD_DEPTHPREPASS_RI_NI_MO") {
    // OrkBreak();
  }
  auto desc_set = pipeline->_descriptorSetCache->fetchDescriptorSetForProgram(prog);
  if (desc_set) {
    // Bind descriptor set with dynamic offsets from applyPendingUboUpdates
    if (!pipeline->_dynamic_offsets.empty()) {
      vkCmdBindDescriptorSets(
          CB,
          VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline->_pipelineLayout,
          0, // first set
          1, // set count
          &desc_set->_vkdescset,
          pipeline->_dynamic_offsets.size(),
          pipeline->_dynamic_offsets.data());
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
        cmdbuf,                    // command buffer
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
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
