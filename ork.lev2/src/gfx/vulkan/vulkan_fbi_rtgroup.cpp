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
static logchannel_ptr_t logchan_rtgroup = logger()->configureChannel("VKRTG", fvec3(0.8, 0.2, 0.5), false);

///////////////////////////////////////////////////////////////////////////////
vkrtgrpimpl_ptr_t VkFrameBufferInterface::_createRtGroupImpl(const VkRtgCreateOptions& options) {
  vkrtgrpimpl_ptr_t RTGIMPL = std::make_shared<VkRtGroupImpl>(_contextVK);
  RTGIMPL->_width = options._width;
  RTGIMPL->_height = options._height;
  RTGIMPL->_pipeline_bits = 0;
  //////////////////////////////////////////////////
  // color buffers
  //////////////////////////////////////////////////
  switch (options._usage) {
    case "swapchain"_crcu:
    case "user"_crcu: {
      int inumtargets = options._colorOptions.size();
      for (int it = 0; it < inumtargets; it++) {
        const auto& color_option = options._colorOptions[it];
        uint64_t buf_usage = color_option._usage;
        VkFormat vk_fmt = color_option._format;
        OrkAssert(buf_usage != "depth"_crcu);
        auto bufferimpl = std::make_shared<VklRtBufferImpl>(_contextVK, RTGIMPL.get(), buf_usage, vk_fmt);
        RTGIMPL->_color_buffer_impls.push_back(bufferimpl);
        _vkCreateImageForBuffer(_contextVK, bufferimpl, color_option);        
        auto& attachment_ref = bufferimpl->_attachmentRef;
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
  if(options._depthOptions._format!=VK_FORMAT_UNDEFINED) {
    uint64_t USAGE  = "depth"_crcu;
    auto bufferimpl = std::make_shared<VklRtBufferImpl>(_contextVK, RTGIMPL.get(),USAGE, options._depthOptions._format);
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
  int inumtargets           = rtgroup->numImageBuffers();
  VkRtgCreateOptions options;
  options._usage = rtgroup->_usage;
  options._msaaSamples = rtgroup->_msaa_samples;
  bool as_texture = false;
  for(int i=0; i < inumtargets; i++) {
    auto rtb = rtgroup->buffer(i);
    VkRtbCreateOption color_option;
    color_option._usage = rtb->_usage;
    color_option._format = VkFormatConverter::convertBufferFormat(rtb->format());
    color_option._with_texture = (rtb->texture() != nullptr);
    logchan_rtgroup->log("Creating RTB impl - buffer %d usage=0x%zx (%zu)", i, color_option._usage, color_option._usage);
    options._colorOptions.push_back(color_option);
  }
  auto depth_buffer = rtgroup->_depthBuffer;
  if(depth_buffer) {
    VkRtbCreateOption depth_option;
    depth_option._format = VkFormatConverter::convertBufferFormat(depth_buffer->format());
    depth_option._with_texture = depth_buffer->texture() != nullptr;
    depth_option._usage = depth_buffer->_usage;
    options._depthOptions = depth_option;
  }
  options._width = rtgroup->width();
  options._height = rtgroup->height();
  auto rtgimpl = _createRtGroupImpl(options);
  ///////////////////////////////////////////////////
  // set impls in rtgroup and rtbuffers
  ///////////////////////////////////////////////////
  VkRtGroupImpl::assignToRtGroup(rtgimpl, rtgroup);
  ///////////////////////////////////////////////////
  for(int i=0; i < inumtargets; i++) {
    auto rtbuffer = rtgroup->buffer(i);
    auto bufferimpl = rtbuffer->_impl.getShared<VklRtBufferImpl>();
    auto texture  = rtbuffer->texture();
    if(texture) {
      _contextVK->_txi->_initTextureFromRtBuffer(rtbuffer.get());
      bufferimpl->_imgobj = texture->_impl.getShared<VulkanTextureObject>()->_imgobj;
    }
  }
  ///////////////////////////////////////////////////
  return rtgimpl;
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::__setRtGroup(rtgroup_rawptr_t rtgroup) {
  if(0)printf("VkFrameBufferInterface __setRtGroup rtgroup<%p>\n", (void*) rtgroup);
  _active_rtgroup = rtgroup;
  vkrtgrpimpl_ptr_t RTGIMPL;

  /////////////////////////////////
  // main_rtg ?
  //  (images managed by swapchain)
  /////////////////////////////////
  switch(rtgroup->_usage) {
    case "swapchain"_crcu:
      RTGIMPL = rtgroup->_impl.getShared<VkRtGroupImpl>();
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
      
      //printf("VkFrameBufferInterface::__setRtGroup rtgroup<%p> w<%d> h<%d> usage=user\n", rtgroup, iw, ih);
      
      /////////////////////////////////////////
      int inumtargets = rtgroup->numImageBuffers();
      int numsamples  = msaaEnumToInt(rtgroup->_msaa_samples);
      if (auto as_impl = rtgroup->_impl.tryAsShared<VkRtGroupImpl>()) {
        RTGIMPL = as_impl.value();
        //printf("  rtgroup already has impl\n");
      } else {
        RTGIMPL = _createRtGroupImpl(rtgroup);
        rtgroup->_impl.setShared<VkRtGroupImpl>(RTGIMPL);
      }
      /////////////////////////////////////////
      int implw      = RTGIMPL->_width;
      int implh      = RTGIMPL->_height;
      int rtgw       = rtgroup->width();
      int rtgh       = rtgroup->height();
      bool size_diff = (rtgw != implw) || (rtgh != implh);
      if (size_diff) {
        logchan_rtgroup->log("resize FBO iw<%d> ih<%d>", iw, ih);
        RTGIMPL = _createRtGroupImpl(rtgroup);
        rtgroup->_impl.setShared<VkRtGroupImpl>(RTGIMPL);
        rtgroup->SetSizeDirty(false);
      }
      for (int i = 0; i < inumtargets; i++) {
        auto rtb      = rtgroup->buffer(i);
        //printf("  checking rtbuffer<%p> has_impl<%d>\n", rtb.get(), rtb->_impl.isSet());
        auto rtb_impl = rtb->_impl.getShared<VklRtBufferImpl>();
        auto rtb_imgobj = rtb_impl->_imgobj;
        OrkAssert(rtb_imgobj->_vkimageview != VK_NULL_HANDLE);
      }
      break;
    }
    case "arrayslice"_crcu:
      OrkAssert(false);
      break;
    default:
      OrkAssert(false);
      break;
  }

  /////////////////////////////////////////
  // Begin dynamic rendering
  /////////////////////////////////////////

  auto vkcmdbuf = RTGIMPL->_cmdbufRTG->_impl.getShared<VkSecondaryCommandBufferImpl>();

  // DEBUG: Log the primary command buffer we're recording to
  logchan_rtgroup->log("__setRtGroup: transitioning rtgroup<%p> to RenderTarget on primary CB %p", (void*) this, (void*)_contextVK->primary_cb()->_vkcmdbuf);

  // Move the transition here, before resetting and beginning the secondary command buffer
  RTGIMPL->_transitionToRenderTarget(_contextVK->primary_cb());

  // DEBUG: Log that transition is complete
  logchan_rtgroup->log("__setRtGroup: Transition recorded, now resetting secondary CB %p for new commands", (void*)vkcmdbuf->_vkcmdbuf);

  vkResetCommandBuffer(vkcmdbuf->_vkcmdbuf, 0);                         // vkBeginCommandBuffer does an implicit reset
  vkcmdbuf->_recorded = false;                                          // Reset the recorded flag
  vkBeginCommandBuffer(vkcmdbuf->_vkcmdbuf, &RTGIMPL->_cmdBufCBBI_GFX); // vkBeginCommandBuffer does an implicit reset
  auto rinfo = RTGIMPL->renderinfo();

  // DEBUG: Log that secondary command buffer is beginning rendering
  logchan_rtgroup->log("__setRtGroup: Secondary CB %p beginning rendering", (void*)vkcmdbuf->_vkcmdbuf);

  _contextVK->_vkCmdBeginRenderingKHR(vkcmdbuf->_vkcmdbuf, &rinfo->_renderinfo);

  _contextVK->_vkcmdbuffer_current = RTGIMPL->_cmdbufRTG->_impl.getShared<VkSecondaryCommandBufferImpl>()->_vkcmdbuf;

  // At start and end of __setRtGroup, log RTG pointer and CB pointers
  logchan_rtgroup->log("__setRtGroup: RTG %p, primary CB %p, secondary CB %p", (void*)rtgroup, (void*)_contextVK->primary_cb()->_vkcmdbuf, (void*)vkcmdbuf->_vkcmdbuf);
  // When vkBeginCommandBuffer and vkEndCommandBuffer are called, log CB pointer
  logchan_rtgroup->log("vkBeginCommandBuffer: CB %p", (void*)vkcmdbuf->_vkcmdbuf);
  logchan_rtgroup->log("vkEndCommandBuffer: CB %p", (void*)vkcmdbuf->_vkcmdbuf);
  // When _vkCmdBeginRenderingKHR and _vkCmdEndRenderingKHR are called, log CB pointer
  logchan_rtgroup->log("vkCmdBeginRenderingKHR: CB %p", (void*)vkcmdbuf->_vkcmdbuf);
  logchan_rtgroup->log("vkCmdEndRenderingKHR: CB %p", (void*)vkcmdbuf->_vkcmdbuf);
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_pushRtGroup(rtgroup_rawptr_t rtgroup) {
  if(0)printf("VkFrameBufferInterface _pushRtGroup rtgroup<%p>\n", (void*) rtgroup);
  bool must_push = _contextVK->meTargetType != TargetType::WINDOW;

  if(must_push or (_active_rtgroup!=rtgroup)){
    __setRtGroup(rtgroup);
    //logchan_rtgroup->log("PushRtGroup: RTG %p, primary CB %p", (void*)rtgroup, _contextVK->primary_cb() ? (void*)_contextVK->primary_cb().get() : nullptr);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_popRtGroup() {

  auto finished_rtg         = _active_rtgroup;
  rtgroup_rawptr_t next_rtg = mRtGroupStack.top();
  _active_rtgroup           = next_rtg;
  auto RTGIMPL              = finished_rtg->_impl.getShared<VkRtGroupImpl>();
  auto cbufimpl             = RTGIMPL->_cmdbufRTG->_impl.getShared<VkSecondaryCommandBufferImpl>();

  ///////////////////////////////////////////////////

  int num_buf = finished_rtg->numImageBuffers();

  //////////////////////////////////////////////
  // end dynamic rendering
  // RTG commandbuffer complete, pop and execute
  //////////////////////////////////////////////

  // Check if command buffer is in recording state before ending rendering
  if (cbufimpl->_recorded == false) {
  _contextVK->_vkCmdEndRenderingKHR(cbufimpl->_vkcmdbuf);
  vkEndCommandBuffer(cbufimpl->_vkcmdbuf);
  cbufimpl->_recorded = true;
  }
  
  // Switch back to primary command buffer before enqueuing
  if (_contextVK->primary_cb() == nullptr) {
    logchan_rtgroup->log("ERROR: primary_cb() is nullptr in _popRtGroup! This will crash.");
    OrkAssert(_contextVK->primary_cb() != nullptr);
  }
  logchan_rtgroup->log("Switching back to primary command buffer %p before enqueuing", (void*)_contextVK->primary_cb()->_vkcmdbuf);
  _contextVK->_vkcmdbuffer_current = _contextVK->primary_cb()->_vkcmdbuf;
  
  std::string group_name = "ExecuteSecondary_" + finished_rtg->_name;
  _contextVK->debugPushGroup(group_name, fvec4(1,0,1,1));
  _contextVK->enqueueSecondaryCommandBuffer(RTGIMPL->_cmdbufRTG);
  _contextVK->debugPopGroup();

  /////////////////////////////////////////////
  // transition finished rtgroup based on its usage
  /////////////////////////////////////////////

  switch(finished_rtg->_usage) {
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
    case "arrayslice"_crcu:
      OrkAssert(false);
      break;
    default:
      OrkAssert(false);
      break;
  }

  /////////////////////////////////////////////
  // Resume rendering on the next rtgroup if needed
  /////////////////////////////////////////////

    auto main_rtg = _ensureMainRtg();
  bool back_to_main = (finished_rtg!=main_rtg.get());

  if(back_to_main) {
    // TODO:
    //  probably need to start a new command buffer here    
    //   instead of restarting the current one
    _active_rtgroup = next_rtg;
    auto RTGIMPL = next_rtg->_impl.getShared<VkRtGroupImpl>();
    RTGIMPL->_transitionToRenderTarget(_contextVK->primary_cb());
    auto vkcmdbuf = RTGIMPL->_cmdbufRTG->_impl.getShared<VkSecondaryCommandBufferImpl>();
    auto rinfo = RTGIMPL->renderinfo();
    vkResetCommandBuffer(vkcmdbuf->_vkcmdbuf, 0);                         // vkBeginCommandBuffer does an implicit reset
    vkcmdbuf->_recorded = false;                                          // Reset the recorded flag
    vkBeginCommandBuffer(vkcmdbuf->_vkcmdbuf, &RTGIMPL->_cmdBufCBBI_GFX); 
    _contextVK->_vkCmdBeginRenderingKHR(vkcmdbuf->_vkcmdbuf, &rinfo->_renderinfo);
    _contextVK->_vkcmdbuffer_current = RTGIMPL->_cmdbufRTG->_impl.getShared<VkSecondaryCommandBufferImpl>()->_vkcmdbuf;
  
  }

  logchan_rtgroup->log("PopRtGroup: RTG %p, primary CB %p", (void*)_active_rtgroup, _contextVK->primary_cb() ? (void*)_contextVK->primary_cb().get() : nullptr);
}

///////////////////////////////////////////////////////

captureasync_ptr_t VkFrameBufferInterface::captureAsFormat(const RtBuffer* inpbuf, capturebuffer_ptr_t capbuf, EBufferFormat destfmt, void_lambda_t on_capture_complete) {

  auto future = std::make_shared<CaptureAsync>();
  future->_width = inpbuf->_width;
  future->_height = inpbuf->_height;
  future->_format = destfmt;
  
  OrkAssert(inpbuf->_impl.isShared<VklRtBufferImpl>()); // must have been implemented for vulkan already
  auto rtbi = inpbuf->_impl.getShared<VklRtBufferImpl>();
  if (nullptr == capbuf) {
    future->_failed = true;
    return future;
  }
  int x = 0;
  int y = 0;
  int w = inpbuf->_width;
  int h = inpbuf->_height;

  if (capbuf->_captureW != 0) {
    x = capbuf->_captureX;
    y = capbuf->_captureY;
    w = capbuf->_captureW;
    h = capbuf->_captureH;
  }

  // Capture must be called during a frame when command buffer is active
  auto cb = _contextVK->primary_cb();
  OrkAssert(cb != nullptr); // capture must be called during frame recording
  
  /*
  printf("VkFrameBufferInterface::captureAsFormat rtb<%p> w<%d> h<%d> has_impl<%d>\n", 
         inpbuf, w, h, inpbuf->_impl.isSet());
  printf("  rtb->_impl.isShared<VklRtBufferImpl>() = %d\n", 
         inpbuf->_impl.isShared<VklRtBufferImpl>());
  */

  rtbi->_transitionToHostRead(cb);

  // printf("captureAsFormat w<%d> h<%d>\n", w, h);

  bool fmtmatch = (capbuf->format() == destfmt);
  bool sizmatch = (capbuf->width() == w);
  sizmatch &= (capbuf->height() == h);

  if (not(fmtmatch and sizmatch))
    capbuf->setFormatAndSize(destfmt, w, h);

  auto vkimg     = rtbi->_imgobj->_vkimage;
  auto vkfmt     = rtbi->_vkfmt;
  auto imgobj   = rtbi->_imgobj;
  auto vkimgview = imgobj->_vkimageview;

  VkBufferImageCopy region = {};
  region.bufferOffset      = 0;
  region.bufferRowLength   = 0;  // 0 means tightly packed
  region.bufferImageHeight = 0;  // 0 means tightly packed
  region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent       = {uint32_t(w), uint32_t(h), 1};

  // GL_ERRORCHECK();
  static size_t yo       = 0;
  constexpr float inv256 = 1.0f / 255.0f;
  switch (destfmt) {
    case EBufferFormat::NV12: {
      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);
      break;
    }
    case EBufferFormat::RGBA8: {
      // Handle both 8-bit and 32-bit float formats
      bool is_float_format = (vkfmt == VK_FORMAT_R32G32B32A32_SFLOAT);
      bool is_8bit_format = (vkfmt == VK_FORMAT_R8G8B8A8_UNORM || vkfmt == VK_FORMAT_B8G8R8A8_UNORM);
      
      OrkAssert(is_float_format || is_8bit_format);
      
      size_t staging_bufsize = is_float_format ? (w * h * 16) : (w * h * 4); // 16 bytes per pixel for RGBA32F
      
      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);
      
      // Create staging buffer for GPU to CPU transfer
      auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, staging_bufsize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, "capture_staging");
      
      // Copy image to staging buffer (image is already in TRANSFER_SRC_OPTIMAL from transition)
      vkCmdCopyImageToBuffer(
          cb->_vkcmdbuf, 
          vkimg, 
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
          staging_buffer->_vkbuffer, 
          1, 
          &region);
      
      // Transition back to render target for continued rendering
      rtbi->_transitionToRenderTarget(cb);
      
      // Store staging buffer with metadata about conversion requirements
      auto capbuf_impl = capbuf->_impl.makeShared<VkCaptureBufferImpl>();
      capbuf_impl->staging_buffer = staging_buffer;
      capbuf_impl->_actual_format = vkfmt;
      capbuf_impl->_desired_format = destfmt;
      
      auto async_impl = std::make_shared<VkCaptureAsyncImpl>(_contextVK);
      async_impl->capture_buffer = capbuf;
      async_impl->width = w;
      async_impl->height = h;
      async_impl->format = destfmt;
      async_impl->_stagingBuffer = staging_buffer;
      async_impl->_copySubmitted = true;  // Will be submitted with this command buffer
      // Note: fence will be set when frame is submitted
      
      future->_impl.setShared<VkCaptureAsyncImpl>(async_impl);
      future->_captureBuffer = async_impl->capture_buffer;
      future->_on_capture_complete = on_capture_complete;
      
      // Register with context for processing after frame
      _contextVK->_pending_captures.push_back(future);
      
      //printf("VkFrameBufferInterface::captureAsFormat - copy command recorded, data will be available after frame submit\n");
      break;
    }
    case EBufferFormat::RGB8: {
      //////////////////////////////////////
      // RGB8 not implemented for Vulkan yet
      //////////////////////////////////////
      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);
      // TODO: Implement RGB8 capture for Vulkan
      OrkAssert(false);
      //////////////////////////////////////
      break;
    }
    case EBufferFormat::RGBA16F:{
      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);
      // TODO: Implement RGBA16F capture for Vulkan
      OrkAssert(false);
      break;
    }
    ///////////////////////////////////////////////////////
    case EBufferFormat::RGBA32F: {
      OrkAssert(vkfmt == VK_FORMAT_R32G32B32A32_SFLOAT);
      // Set up image with format and preallocated data
      capbuf->_image->initWithFormat(w, h, destfmt);
      
      // Create staging buffer for GPU to CPU transfer
      size_t bufsize = w * h * 16; // 16 bytes per pixel for RGBA32F
      auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, bufsize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, "capture_staging_f32");
      
      // Copy image to staging buffer
      vkCmdCopyImageToBuffer(
          cb->_vkcmdbuf, 
          vkimg, 
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
          staging_buffer->_vkbuffer, 
          1, 
          &region);
      
      // Transition back to render target
      rtbi->_transitionToRenderTarget(cb);
      
      // Store staging buffer with metadata (use same struct for consistency)
      auto capbuf_impl = capbuf->_impl.makeShared<VkCaptureBufferImpl>();
      capbuf_impl->staging_buffer = staging_buffer;
      capbuf_impl->_actual_format = vkfmt;
      capbuf_impl->_desired_format = destfmt;
      
      // Store capture data in the future
      auto async_impl = std::make_shared<VkCaptureAsyncImpl>(_contextVK);
      async_impl->capture_buffer = capbuf;
      async_impl->width = w;
      async_impl->height = h;
      async_impl->format = destfmt;
      async_impl->_stagingBuffer = staging_buffer;
      async_impl->_copySubmitted = true;  // Will be submitted with this command buffer
      // Note: fence will be set when frame is submitted
      
      future->_impl.setShared<VkCaptureAsyncImpl>(async_impl);
      future->_captureBuffer = async_impl->capture_buffer;
      future->_on_capture_complete = on_capture_complete;
      
      // Register with context for processing after frame
      _contextVK->_pending_captures.push_back(future);
      break;
    }
    ///////////////////////////////////////////////////////
    case EBufferFormat::R32F:
      OrkAssert(false);
      // glReadPixels(x, y, w, h, GL_RED, GL_FLOAT, capbuf->_data);
      break;
    case EBufferFormat::R32UI:
      OrkAssert(false);
      // glReadPixels(x, y, w, h, GL_RED_INTEGER, GL_UNSIGNED_INT, capbuf->_data);
      break;
    case EBufferFormat::RG32F:
      OrkAssert(false);
      // glReadPixels(x, y, w, h, GL_RG, GL_FLOAT, capbuf->_data);
      break;
    default:
      OrkAssert(false);
      break;
  }
  // GL_ERRORCHECK();

  // glBindFramebuffer(GL_FRAMEBUFFER, 0);
  //   glReadBuffer( readbuffer ); // restore read buffer
  // GL_ERRORCHECK();
  return future;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
