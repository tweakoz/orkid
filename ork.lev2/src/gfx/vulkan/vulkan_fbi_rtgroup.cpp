////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include "vulkan_captureasync.h"
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_rtgroup = logger()->configureChannel("VKRTG", fvec3(0.8, 0.2, 0.5), true);
// constexpr uint32_t VK_RENDERING_RESUMING_BIT = 0x00000004;
///////////////////////////////////////////////////////////////////////////////
vkrtgrpimpl_ptr_t VkFrameBufferInterface::_createRtGroupImpl(const VkRtgCreateOptions& options) {
  vkrtgrpimpl_ptr_t RTGIMPL = std::make_shared<VkRtGroupImpl>(_contextVK,options._rtgroup);
  RTGIMPL->_width           = options._width;
  RTGIMPL->_height          = options._height;
  RTGIMPL->_pipeline_bits   = 0;
  int inumtargets           = options._colorOptions.size();
  //////////////////////////////////////////////////
  // color buffers
  //////////////////////////////////////////////////
  switch (options._usage) {
    case "swapchain"_crcu:
    case "user"_crcu: {
      for (int it = 0; it < inumtargets; it++) {
        const auto& color_option = options._colorOptions[it];
        uint64_t buf_usage       = color_option._usage;
        VkFormat vk_fmt          = color_option._format;
        OrkAssert(buf_usage != "depth"_crcu);
        auto bufferimpl = std::make_shared<VklRtBufferImpl>(_contextVK, RTGIMPL.get(), buf_usage, vk_fmt);
        RTGIMPL->_color_buffer_impls.push_back(bufferimpl);
        _vkCreateImageForBuffer(_contextVK, bufferimpl, color_option);
        auto& attachment_ref      = bufferimpl->_attachmentRef;
        attachment_ref.attachment = it;
        attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }
      break;
    }
    case "popup"_crcu:
      break;
    default:
      break;
  }
  //////////////////////////////////////////////////
  // depth buffer
  //////////////////////////////////////////////////
  if (options._depthOptions._format != VK_FORMAT_UNDEFINED) {
    uint64_t USAGE  = "depth"_crcu;
    auto bufferimpl = std::make_shared<VklRtBufferImpl>(_contextVK, RTGIMPL.get(), USAGE, options._depthOptions._format);
    //printf("Creating depth buffer impl <%p> - w<%d> h<%d>\n", bufferimpl.get(), options._width,options._height );
    RTGIMPL->_depth_buffer_impl = bufferimpl;
    _vkCreateImageForBuffer(_contextVK, bufferimpl, options._depthOptions);
    auto& adesc          = bufferimpl->_attachmentDesc;
    adesc.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    adesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    adesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }
  //////////////////////////////////////////////////
  return RTGIMPL;
}

///////////////////////////////////////////////////////////////////////////////

vkrtgrpimpl_ptr_t VkFrameBufferInterface::_createRtGroupImpl(rtgroup_rawptr_t rtgroup) {
  int inumtargets = rtgroup->numImageBuffers();
  //logchan_rtgroup->log("Creating RTG<%p> impl - inumtargets<%d>", rtgroup, inumtargets);
  VkRtgCreateOptions options;
  options._rtgroup = rtgroup;
  options._width  = rtgroup->width();
  options._height = rtgroup->height();
  options._usage       = rtgroup->_usage;
  options._msaaSamples = rtgroup->_msaa_samples;
  ///////////////////////////////////////////////////
  for (int i = 0; i < inumtargets; i++) {
    auto rtb = rtgroup->buffer(i);
    VkRtbCreateOption color_option;
    color_option._usage        = rtb->_usage;
    color_option._format       = VkFormatConverter::convertBufferFormat(rtb->format());
    color_option._with_texture = (rtb->texture() != nullptr);
    //logchan_rtgroup->log("Creating RTB impl - buffer %d usage=0x%zx (%zu)", i, color_option._usage, color_option._usage);
    options._colorOptions.push_back(color_option);
  }
  ///////////////////////////////////////////////////
  auto depth_buffer = rtgroup->_depthBuffer;
  if (depth_buffer) {
    VkRtbCreateOption depth_option;
    depth_option._format       = VkFormatConverter::convertBufferFormat(depth_buffer->format());
    depth_option._with_texture = depth_buffer->texture() != nullptr;
    depth_option._usage        = depth_buffer->_usage;
    options._depthOptions      = depth_option;
  }
  ///////////////////////////////////////////////////
  // set impls in rtgroup and rtbuffers
  ///////////////////////////////////////////////////
  auto rtgimpl    = _createRtGroupImpl(options);
  VkRtGroupImpl::assignToRtGroup(rtgimpl, rtgroup);
  ///////////////////////////////////////////////////
  for (int i = 0; i < inumtargets; i++) {
    auto rtb     = rtgroup->buffer(i);
    auto rtbi    = rtb->_impl.getShared<VklRtBufferImpl>();
    auto texture = rtb->texture();
    if (texture) {
      _contextVK->_txi->_initTextureFromRtBuffer(rtb.get());
      rtbi->_imgobj = texture->_impl.getShared<VulkanTextureObject>()->_imgobj[0];
    }
  }
  ///////////////////////////////////////////////////
  // Handle depth buffer texture if present
  ///////////////////////////////////////////////////
  if (depth_buffer) {
    depth_buffer->_width  = rtgroup->width();
    depth_buffer->_height = rtgroup->height();
    auto depth_impl = rtgimpl->_depth_buffer_impl;
    depth_buffer->_impl.setShared<VklRtBufferImpl>(depth_impl);
    auto texture = depth_buffer->texture();
    if (texture) {
      _contextVK->_txi->_initTextureFromRtBuffer(depth_buffer.get());
      depth_impl->_imgobj = texture->_impl.getShared<VulkanTextureObject>()->_imgobj[0];
    }
  }
  ///////////////////////////////////////////////////
  return rtgimpl;
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_pushRtGroup(rtgroup_rawptr_t rtgroup) {
  if (0)
    logchan_rtgroup->log("VkFrameBufferInterface _pushRtGroup rtgroup<%p>", (void*)rtgroup);

  // Create stack item to track this push operation
  RtgStackItem stack_item;
  stack_item._rtgroup = rtgroup;

  auto stack_impl         = std::make_shared<VkRtgStackItemImpl>();
  stack_impl->_previous_rtgroup = _active_rtgroup;

  bool must_push   = _contextVK->meTargetType != TargetType::WINDOW;
  bool needs_begin = must_push or (_active_rtgroup != rtgroup);

  if (needs_begin) {
    // STEP 1: End any currently active render pass FIRST
    if (_contextVK->_renderPassActive) {
      auto& CB = _contextVK->primary_cb()->_vkcmdbuf;
      _contextVK->_vkCmdEndRenderingKHR(CB);
      _contextVK->_renderPassActive    = false;
      _contextVK->_activeRenderPassRTG = nullptr;
    }

    // STEP 2: Now safe to create/setup RTG implementation (barriers are allowed here)
    vkrtgrpimpl_ptr_t RTGIMPL;
    auto& CB = _contextVK->primary_cb()->_vkcmdbuf;

    switch (rtgroup->_usage) {
      case "swapchain"_crcu:
        if (auto as_impl = rtgroup->_impl.tryAsShared<VkRtGroupImpl>()) {
          RTGIMPL = as_impl.value();
        } else {
          // First time - create the RTG impl
          logchan_rtgroup->log("Creating swapchain RTG impl for first time");
          RTGIMPL = _createRtGroupImpl(rtgroup);
        }
        RTGIMPL->_updateClearParams(rtgroup);
        RTGIMPL->_updateMainSurface(this);
        break;
      case "popup"_crcu:
        OrkAssert(false);
        break;
      case "user"_crcu: {
        OrkAssert(rtgroup);
        int iw = rtgroup->width();
        int ih = rtgroup->height();

        int inumtargets = rtgroup->numImageBuffers();
        int numsamples  = msaaEnumToInt(rtgroup->_msaa_samples);
        if (auto as_impl = rtgroup->_impl.tryAsShared<VkRtGroupImpl>()) {
          RTGIMPL = as_impl.value();
        } else {
          RTGIMPL = _createRtGroupImpl(rtgroup);
        }

        int rtgw       = rtgroup->width();
        int rtgh       = rtgroup->height();
        bool size_diff = (rtgw != RTGIMPL->_width) or (rtgh != RTGIMPL->_height);
        if (size_diff) {
          logchan_rtgroup->log("resize FBO from <%d x %d> to <%d x %d>", RTGIMPL->_width, RTGIMPL->_height, rtgw, rtgh);
          RTGIMPL = _createRtGroupImpl(rtgroup);
          rtgroup->SetSizeDirty(false);
        }

        // Handle cubemap face rendering
        if (rtgroup->_cubeMap) {
          RTGIMPL->_setupCubeFaceRendering(rtgroup->_cubeRenderFace);
        }
        break;
      }
      case "arrayslice"_crcu: {
        if (rtgroup->_impl.isShared<VkRtGroupImpl>()) {
          RTGIMPL = rtgroup->_impl.getShared<VkRtGroupImpl>();
        } else {
          RTGIMPL = _buildRtgImplFromTextureArraySlice(rtgroup);
        }
        break;
      }
      default:
        OrkAssert(false);
        break;
    } // switch (rtgroup->_usage) {

    // STEP 3: Start per-RTG GPU perf block (before render pass begins)
    // TODO this is not the correct place to put this. A certain RTG is only getting pushed once, but popped twice. Why!?
      if (rtgroup->_profiler_series == nullptr) {
        rtgroup->_profiler_name = rtgroup->_name.empty() ? FormatString("rtg:%p", (void*)rtgroup) : std::string("rtg:") + rtgroup->_name;
        printf("Created rtg series! %s\n", rtgroup->_profiler_name.c_str());
        rtgroup->_profiler_series = _contextVK->_gpu_channel->createSeries(CrcString(rtgroup->_profiler_name.c_str()));
      }
      _contextVK->_gpu_channel->beginSample(rtgroup->_profiler_series);

    // STEP 4: Now begin the new render pass
    RTGIMPL->_transitionToRenderTarget(_contextVK->primary_cb());
    auto rinfo = RTGIMPL->renderinfo();
    rinfo->_renderinfo.flags &= (~VK_RENDERING_RESUMING_BIT);
    _contextVK->_vkCmdBeginRenderingKHR(CB, &rinfo->_renderinfo);

    // STEP 5: Update tracking state
    _active_rtgroup                  = rtgroup;
    _contextVK->_renderPassActive    = true;
    _contextVK->_activeRenderPassRTG = RTGIMPL;

    stack_impl->_did_begin_rendering = true;
    stack_impl->_was_redundant       = false;
  } else {
    // Redundant push - same rtgroup already active
    stack_impl->_did_begin_rendering = false;
    stack_impl->_was_redundant       = true;
  }

  stack_item._impl.setShared<VkRtgStackItemImpl>(stack_impl);
  mRtGroupStack.push(stack_item);
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_popRtGroup() {
  // Get the stack item we're popping
  OrkAssert(!mRtGroupStack.empty());
  RtgStackItem popped_item = mRtGroupStack.top();
  mRtGroupStack.pop();

  auto stack_impl   = popped_item._impl.getShared<VkRtgStackItemImpl>();
  auto finished_rtg = popped_item._rtgroup;

  if (0)
    logchan_rtgroup->log(
        "_popRtGroup: RTG %p usage=%llu, did_begin_rendering=%d",
        finished_rtg,
        (unsigned long long)finished_rtg->_usage,
        stack_impl ? stack_impl->_did_begin_rendering : 0);

  // End rendering if there's an active render pass
  // NOTE: We end based on _renderPassActive, not did_begin_rendering, because
  // a redundant push (did_begin_rendering=false) might still have left a render pass active
  if(0)logchan_rtgroup->log("PopRtGroup: BEFORE end, renderPassActive=%d", _contextVK->_renderPassActive);
  if (_contextVK->_renderPassActive) {
    auto& CB = _contextVK->primary_cb()->_vkcmdbuf;

    //////////////////////////////////////////////
    // end dynamic rendering
    //////////////////////////////////////////////
    _contextVK->_vkCmdEndRenderingKHR(CB);

    // Track that render pass has ended
    _contextVK->_renderPassActive    = false;
    _contextVK->_activeRenderPassRTG = nullptr;
  }

  /////////////////////////////////////////////
  // transition finished rtgroup based on its usage
  // This happens regardless of whether rendering occurred
  // since texture might be used even without being rendered to
  /////////////////////////////////////////////
  if (finished_rtg) {

    // End per-RTG GPU perf block (after render pass ends)
    // TODO this is not the correct place to put this. A certain RTG is only getting pushed once, but popped twice. Why!?
    // _contextVK->_gpu_channel->endSample(finished_rtg->_profiler_series);

    auto RTGIMPL = finished_rtg->_impl.getShared<VkRtGroupImpl>();

    switch (finished_rtg->_usage) {
      case "swapchain"_crcu: {
        break;
      }
      case "popup"_crcu: {
        OrkAssert(false);
        break;
      }
      case "user"_crcu: { // we will probably use it as a texture...
        RTGIMPL->_transitionToTexture(_contextVK->primary_cb());
        break;
      }
      case "arrayslice"_crcu: {
        // Array slices are typically used as textures after rendering (e.g., shadow maps)
        RTGIMPL->_transitionToTexture(_contextVK->primary_cb());
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  }

  /////////////////////////////////////////////
  // Restore the previous rtgroup and resume if needed
  /////////////////////////////////////////////

  rtgroup_rawptr_t next_rtg = nullptr;
  bool needs_resume         = false;

  // Check if there's another item on the stack
  if (!mRtGroupStack.empty()) {
    // Get the rtgroup we're returning to
    auto& next_item = mRtGroupStack.top();
    next_rtg        = next_item._rtgroup;
    auto next_impl  = next_item._impl.getShared<VkRtgStackItemImpl>();

    // We need to resume if the next item had begun rendering
    needs_resume = next_impl && next_impl->_did_begin_rendering;
  } else {
    // Stack is empty, return to main RTG
    auto main_rtg = _ensureMainRtg();
    next_rtg      = main_rtg.get();

    // Resume main RTG if we're not already there
    needs_resume = (finished_rtg != next_rtg) && (stack_impl && stack_impl->_did_begin_rendering);
  }

  _active_rtgroup = next_rtg;

  if (needs_resume && next_rtg) {
    auto& CB     = _contextVK->primary_cb()->_vkcmdbuf;
    auto RTGIMPL = next_rtg->_impl.getShared<VkRtGroupImpl>();
    RTGIMPL->_transitionToRenderTarget(_contextVK->primary_cb());
    auto rinfo = RTGIMPL->renderinfo();
    rinfo->_renderinfo.flags |= VK_RENDERING_RESUMING_BIT;
    _contextVK->_vkCmdBeginRenderingKHR(CB, &rinfo->_renderinfo);

    // Track that we've resumed a render pass
    _contextVK->_renderPassActive    = true;
    _contextVK->_activeRenderPassRTG = RTGIMPL;
  }

  if (0)
    logchan_rtgroup->log(
        "PopRtGroup: RTG %p, primary CB %p",
        (void*)_active_rtgroup,
        _contextVK->primary_cb() ? (void*)_contextVK->primary_cb().get() : nullptr);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
