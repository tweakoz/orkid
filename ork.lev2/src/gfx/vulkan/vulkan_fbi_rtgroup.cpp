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
// TEMPORARY (aug11) rtg/depth-image IDENTITY trace — ORKID_DEBUG_RTGID=1.
///////////////////////////////////////////////////////////////////////////////
static bool rtgidTrace() {
  static int v = -1;
  if (v < 0) {
    auto e = getenv("ORKID_DEBUG_RTGID");
    v      = (e and atoi(e)) ? 1 : 0;
  }
  return v == 1;
}
static void rtgidDump(const char* tag, vkcontext_rawptr_t ctxVK, vkrtgrpimpl_ptr_t impl, const char* extra) {
  if (not rtgidTrace())
    return;
  static int s_count = 0;
  int        frame   = ctxVK ? ctxVK->GetTargetFrame() : -1;
  if (s_count > 400 and (frame % 500) != 0)
    return;
  s_count++;
  auto rtg   = impl ? impl->_rtgroup : nullptr;
  auto dbuf  = impl ? impl->_depth_buffer_impl : nullptr;
  void* ss   = (dbuf and dbuf->_imgobj) ? (void*)dbuf->_imgobj->_vkimage : nullptr;
  void* ms   = (dbuf and dbuf->_msaa_imgobj) ? (void*)dbuf->_msaa_imgobj->_vkimage : nullptr;
  void* tex  = (rtg and rtg->_depthBuffer) ? (void*)rtg->_depthBuffer->_texture.get() : nullptr;
  // the image the SAMPLER side actually reads: rtbuffer _teximpl -> VulkanTextureObject
  void* tex_img = nullptr;
  if (dbuf) {
    if (auto tt = dbuf->_teximpl.tryAsShared<VulkanTextureObject>())
      tex_img = tt.value()->_imgobj[0] ? (void*)tt.value()->_imgobj[0]->_vkimage : nullptr;
  }
  printf(
      "[RTGID] %-8s f<%d> rtg<%p:%s> impl<%p> wh<%dx%d> lay<%d> mv<%d> msaa<%d> dbuf<%p> ss_img<%p> ms_img<%p> tex<%p> teximg<%p> lyt<%d> ro<%d> %s\n",
      tag,
      frame,
      (void*)rtg,
      (rtg and rtg->_name.length()) ? rtg->_name.c_str() : "?",
      (void*)impl.get(),
      impl ? impl->_width : -1,
      impl ? impl->_height : -1,
      rtg ? rtg->_numLayers : -1,
      rtg ? int(rtg->_multiview) : -1,
      rtg ? msaaEnumToInt(rtg->_msaa_samples) : -1,
      (void*)dbuf.get(),
      ss,
      ms,
      tex,
      tex_img,
      dbuf ? int(dbuf->_currentLayout) : -1,
      impl ? int(impl->_depthReadOnlyMode) : -1,
      extra ? extra : "");
  fflush(stdout);
}
///////////////////////////////////////////////////////////////////////////////
vkrtgrpimpl_ptr_t VkFrameBufferInterface::_createRtGroupImpl(const VkRtgCreateOptions& options) {
  vkrtgrpimpl_ptr_t RTGIMPL = std::make_shared<VkRtGroupImpl>(_contextVK,options._rtgroup);
  RTGIMPL->_width           = options._width;
  RTGIMPL->_height          = options._height;
  // _pipeline_bits stays -1: assigned lazily by layoutBits() from the
  // attachment layout once the buffer impls exist (pipeline-leak fix).
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

void VkFrameBufferInterface::_ensureDepth(rtgroup_ptr_t rtg, int w, int h, const VkRtbCreateOption& depth_opt) {
  vkrtgrpimpl_ptr_t rtg_impl;
  if (auto existing = rtg->_impl.tryAsShared<VkRtGroupImpl>()) {
    // Resize existing impl.
    rtg_impl = existing.value();
    logchan_rtgroup->log("_ensureDepth: resize %dx%d -> %dx%d", rtg_impl->_width, rtg_impl->_height, w, h);
    rtg_impl->_width  = w;
    rtg_impl->_height = h;
    rtg->miW          = w;
    rtg->miH          = h;
    if (rtg->_depthBuffer) {
      rtg->_depthBuffer->_width  = w;
      rtg->_depthBuffer->_height = h;
      // this path recreates the depth image via _vkCreateImageForBuffer (not _initTextureFromRtBuffer),
      // so the depth Texture metadata must be updated here too — it is the HZB's sole extent source.
      if (auto dtex = rtg->_depthBuffer->_texture) {
        dtex->_width  = w;
        dtex->_height = h;
      }
    }
    if (rtg_impl->_depth_buffer_impl) {
      logchan_rtgroup->log("_ensureDepth: recreating depth buffer %dx%d", w, h);
      _vkCreateImageForBuffer(_contextVK, rtg_impl->_depth_buffer_impl, depth_opt);
    }
    rtg_impl->_invalidateAttachments(); // transitions handled by FBI
  } else {
    // First-time creation.
    logchan_rtgroup->log("_ensureDepth: creating impl %dx%d", w, h);
    rtg_impl          = _createRtGroupImpl(rtg.get());
    rtg_impl->_width  = w;
    rtg_impl->_height = h;
    VkRtGroupImpl::assignToRtGroup(rtg_impl, rtg.get());
    _vkCreateImageForBuffer(_contextVK, rtg_impl->_depth_buffer_impl, depth_opt);
  }
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
      _contextVK->_gpuSliceClosePass(); // GPU timing: after the end, never inside (multiview)
      _endedDepthWritePass(_contextVK->_activeRenderPassRTG);
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

        RTGIMPL->_updateClearParams(rtgroup);

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

    // Unpaired-setter guard: a read-only-depth flag that outlived the frame
    // that set it is always a bug — every draw in the pass we are about to
    // begin would silently lose its depth write (S3 turned that leak from a
    // dormant flag into painter-order rendering). Loud in every build, fatal
    // where asserts are compiled in.
    if (RTGIMPL->_depthReadOnlyMode and RTGIMPL->_depthReadOnlySetFrame != _contextVK->GetTargetFrame()) {
      logchan_rtgroup->log(
          "FATAL: rtg<%s> begins a render pass with a STALE read-only-depth flag (set on frame %d, now frame %d) — "
          "the setter never paired its transitionDepthForSampling with a transitionDepthForWriting",
          rtgroup->_name.c_str(),
          RTGIMPL->_depthReadOnlySetFrame,
          _contextVK->GetTargetFrame());
      OrkAssert(false);
    }

    // STEP 4: Now begin the new render pass
    RTGIMPL->_transitionToRenderTarget(_contextVK->primary_cb());
    auto rinfo = RTGIMPL->renderinfo();
    rinfo->_renderinfo.flags &= (~VK_RENDERING_RESUMING_BIT);
    // GPU timing: open this pass instance's slice BEFORE the begin (a timestamp
    // inside a multiview instance consumes one query per view). The name is the
    // rtgroup's; an unnamed rtgroup lands under one stable bucket rather than a
    // pointer that changes every run.
    _contextVK->_gpuSliceOpenPass(rtgroup->_name.empty() ? std::string("rtg:unnamed") : "rtg:" + rtgroup->_name);
    _contextVK->_vkCmdBeginRenderingKHR(CB, &rinfo->_renderinfo);

    // STEP 5: Update tracking state
    _active_rtgroup                  = rtgroup;
    _contextVK->_renderPassActive    = true;
    _contextVK->_activeRenderPassRTG = RTGIMPL;

    stack_impl->_did_begin_rendering = true;
    stack_impl->_was_redundant       = false;
    rtgidDump("PUSH", _contextVK, RTGIMPL, nullptr);
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
        "_popRtGroup: RTG %p usage=%llu, did_begin_rendering=%d mRtGroupStack=%d",
        finished_rtg,
        (unsigned long long)finished_rtg->_usage,
        stack_impl ? stack_impl->_did_begin_rendering : 0,
        mRtGroupStack.size());

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
    _contextVK->_gpuSliceClosePass(); // GPU timing: after the end, never inside (multiview)
    _endedDepthWritePass(_contextVK->_activeRenderPassRTG);

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
        // One-shot: the read-only-depth mode lasts for a single push/pop
        // cycle. Reset it here so the next push on this RTG goes back to
        // the default read/write depth attachment transition.
        RTGIMPL->_depthReadOnlyMode     = false;
        RTGIMPL->_depthReadOnlySetFrame = -1;
        // The cached renderinfo BAKES the depth loadOp from this flag
        // (_autoclear and not _depthReadOnlyMode, vulkan_ctx_renderinfo.cpp),
        // so a reset that leaves the cache in place hands the next push the
        // read-only pass's LOAD_OP_LOAD — the depth prepass then loads stale
        // depth instead of clearing it. UNCONDITIONAL: this site resets the
        // flag directly rather than going through transitionDepthForWriting,
        // whose invalidation is gated on the flag having been set.
        RTGIMPL->_rinfo_retain        = nullptr;
        RTGIMPL->_rinfo_resume_retain = nullptr;
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
    // GPU timing: the resumed instance is a NEW segment under the rtgroup it
    // belongs to; GpuPassStats sums the segments of one name.
    _contextVK->_gpuSliceOpenPass(next_rtg->_name.empty() ? std::string("rtg:unnamed") : "rtg:" + next_rtg->_name);
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

///////////////////////////////////////////////////////////////////////////////
// Called wherever a dynamic-rendering pass ENDS, with the rtg it was rendering.
// A multiview MSAA depth attachment leaves its render pass unresolved by design
// (VulkanRenderInfo forces resolveMode=NONE there — one pass cannot resolve several
// views), so the single-sample copy every sampler reads is filled here instead, per
// layer. Only after a pass that could WRITE depth: a read-only-depth segment has
// nothing new to copy down.
///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_endedDepthWritePass(vkrtgrpimpl_ptr_t impl) {
  if (not impl)
    return;
  if (impl->_depthReadOnlyMode) {
    rtgidDump("ENDDPW", _contextVK, impl, "SKIP:readonly");
    return;
  }
  if (not impl->_needsManualDepthResolve()) {
    rtgidDump("ENDDPW", _contextVK, impl, "SKIP:no-manual-resolve");
    return;
  }
  rtgidDump("ENDDPW", _contextVK, impl, "RESOLVE");
  impl->_resolveMultiviewDepth(_contextVK->primary_cb());
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::transitionDepthForSampling(rtgroup_ptr_t rtg) {
  if (not rtg) return;
  auto try_impl = rtg->_impl.tryAsShared<VkRtGroupImpl>();
  if (not try_impl) return;
  auto impl = try_impl.value();
  if (not impl->_depth_buffer_impl) return;

  // This method only mutates CPU-side state (mode flag + cached renderinfo)
  // — no command-buffer work, no barriers. The actual layout transition
  // happens inside the next _pushRtGroup() → _transitionToRenderTarget()
  // call on this rtg, which already handles ending the active pass first.
  // So it's safe to call while another RTG's render pass is active.
  impl->_depthReadOnlyMode     = true;
  impl->_depthReadOnlySetFrame = _contextVK->GetTargetFrame();
  rtgidDump("SAMPLE", _contextVK, impl, "hzb-seed");

  // Invalidate cached renderinfo so the next renderinfo() call rebuilds
  // with _rainfo_depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
  // (copied from _depth_buffer_impl->_currentLayout after the transition).
  impl->_rinfo_retain = nullptr;
  impl->_rinfo_resume_retain = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// Counterpart to transitionDepthForSampling: force the rtg back into normal
// depth-WRITE mode. transitionDepthForSampling's read-only flag is one-shot
// "reset by the next matching PopRtGroup" — but a compute-side depth sample
// (e.g. the HZB build) transitions WITHOUT any push/pop pair, so the flag
// leaks into the following depth-prepass. With _depthReadOnlyMode==true the
// renderinfo sets resolveMode=NONE (vulkan_ctx_renderinfo.cpp), so the MSAA
// depth is never resolved into the single-sample _imgobj that everything
// samples — it stays at the clear value. A depth-WRITE pass calls this
// before its push to guarantee the resolve fires. We clear ONLY the flag +
// cached renderinfo; the actual layout transition back to an attachment
// layout is handled by the next _pushRtGroup() → _transitionToRenderTarget().
///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::transitionDepthForWriting(rtgroup_ptr_t rtg) {
  if (not rtg) return;
  auto try_impl = rtg->_impl.tryAsShared<VkRtGroupImpl>();
  if (not try_impl) return;
  auto impl = try_impl.value();
  if (not impl->_depth_buffer_impl) return;

  if (not impl->_depthReadOnlyMode) return; // already write-mode; nothing to do

  impl->_depthReadOnlyMode     = false;
  impl->_depthReadOnlySetFrame = -1;

  // Drop cached renderinfo so the next renderinfo() rebuilds with the depth
  // attachment in write mode and resolveMode = SAMPLE_ZERO.
  impl->_rinfo_retain = nullptr;
  impl->_rinfo_resume_retain = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
