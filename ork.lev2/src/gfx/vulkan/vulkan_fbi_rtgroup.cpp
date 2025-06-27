////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_rtgroup = logger()->createChannel("VKRTG", fvec3(0.8, 0.2, 0.5), true);
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
      /////////////////////////////////////////
      int inumtargets = rtgroup->numImageBuffers();
      int numsamples  = msaaEnumToInt(rtgroup->_msaa_samples);
      if (auto as_impl = rtgroup->_impl.tryAsShared<VkRtGroupImpl>()) {
        RTGIMPL = as_impl.value();
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
  logchan_rtgroup->log("__setRtGroup: Recording transition to primary CB %p", (void*)_contextVK->primary_cb()->_vkcmdbuf);

  // Move the transition here, before resetting and beginning the secondary command buffer
  RTGIMPL->_transitionToRenderTarget(_contextVK->primary_cb());

  // DEBUG: Log that transition is complete
  logchan_rtgroup->log("__setRtGroup: Transition recorded, now resetting secondary CB %p", (void*)vkcmdbuf->_vkcmdbuf);

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
  __setRtGroup(rtgroup);
  logchan_rtgroup->log("PushRtGroup: RTG %p, primary CB %p", (void*)rtgroup, _contextVK->primary_cb() ? (void*)_contextVK->primary_cb().get() : nullptr);
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
  
  _contextVK->enqueueSecondaryCommandBuffer(RTGIMPL->_cmdbufRTG);

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

  bool back_to_main = (finished_rtg!=_main_rtg.get());

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

bool VkFrameBufferInterface::captureAsFormat(const RtBuffer* inpbuf, CaptureBuffer* capbuf, EBufferFormat destfmt) {
  auto rtbi = inpbuf->_impl.getShared<VklRtBufferImpl>();
  if (nullptr == capbuf) {
    OrkAssert(false);
    return false;
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

  rtbi->_transitionToHostRead(_contextVK->primary_cb());

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
  region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent       = {uint32_t(w), uint32_t(h), 1};

  // GL_ERRORCHECK();
  static size_t yo       = 0;
  constexpr float inv256 = 1.0f / 255.0f;
  switch (destfmt) {
    case EBufferFormat::NV12: {
      size_t rgbasize = w * h * 4;
      if (capbuf->_tempbuffer.size() != rgbasize) {
        capbuf->_tempbuffer.resize(rgbasize);
      }

      // glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capbuf->_tempbuffer.data());
      // GL_ERRORCHECK();
      //  todo convert RGBA8 to NV12 (on GPU)

      // grab RGBA8 vkimg to staging buffer
      OrkAssert(vkfmt == VK_FORMAT_R8G8B8A8_UNORM);
      auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, rgbasize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
      vkCmdCopyImageToBuffer(
          _contextVK->primary_cb()->_vkcmdbuf, vkimg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, staging_buffer->_vkbuffer, 1, &region);

      staging_buffer->copyToHost(capbuf->_tempbuffer.data(), rgbasize);

      // convert to NV12
      auto outptr      = (uint8_t*)capbuf->_data;
      size_t numpixels = w * h;
      fvec3 avgcol;
      for (size_t yin = 0; yin < h; yin++) {
        int yout = (h - 1) - yin;
        for (size_t x = 0; x < w; x++) {
          int i_in       = (yin * w) + x;
          int i_out      = (yout * w) + x;
          size_t srcbase = i_in * 4;
          int R          = capbuf->_tempbuffer[srcbase + 0];
          int G          = capbuf->_tempbuffer[srcbase + 1];
          int B          = capbuf->_tempbuffer[srcbase + 2];
          // printf("RGB<%d %d %d>\n", R, G, B);
          auto rgb = fvec3(R, G, B) * inv256;
          avgcol += rgb;
          auto yuv      = rgb.YUV();
          outptr[i_out] = uint8_t(yuv.x * 255.0f);
        }
      }
      avgcol *= 1.0f / float(numpixels);
      for (size_t yin = 0; yin < h / 2; yin++) {
        int yout = ((h / 2) - 1) - yin;
        for (size_t x = 0; x < w / 2; x++) {
          size_t ybase    = yin * 2;
          size_t xbase    = x * 2;
          size_t srcbase1 = (((ybase + 0) * w) + (xbase + 0)) * 4;
          size_t srcbase2 = (((ybase + 0) * w) + (xbase + 1)) * 4;
          size_t srcbase3 = (((ybase + 1) * w) + (xbase + 0)) * 4;
          size_t srcbase4 = (((ybase + 1) * w) + (xbase + 1)) * 4;
          int R1          = capbuf->_tempbuffer[srcbase1 + 0];
          int G1          = capbuf->_tempbuffer[srcbase1 + 1];
          int B1          = capbuf->_tempbuffer[srcbase1 + 2];
          int R2          = capbuf->_tempbuffer[srcbase2 + 0];
          int G2          = capbuf->_tempbuffer[srcbase2 + 1];
          int B2          = capbuf->_tempbuffer[srcbase2 + 2];
          int R3          = capbuf->_tempbuffer[srcbase3 + 0];
          int G3          = capbuf->_tempbuffer[srcbase3 + 1];
          int B3          = capbuf->_tempbuffer[srcbase3 + 2];
          int R4          = capbuf->_tempbuffer[srcbase4 + 0];
          int G4          = capbuf->_tempbuffer[srcbase4 + 1];
          int B4          = capbuf->_tempbuffer[srcbase4 + 2];
          auto rgb1       = fvec3(R1, G1, B1) * inv256;
          auto rgb2       = fvec3(R2, G2, B2) * inv256;
          auto rgb3       = fvec3(R3, G3, B3) * inv256;
          auto rgb4       = fvec3(R4, G4, B4) * inv256;
          auto yuv1       = rgb1.YUV();
          auto yuv2       = rgb2.YUV();
          auto yuv3       = rgb3.YUV();
          auto yuv4       = rgb4.YUV();
          auto yuv        = (yuv1 + yuv2 + yuv3 + yuv4) * 0.125;
          yuv += fvec3(0.5, 0.5, 0.5);
          int u                            = int(yuv.y * 255.0f);
          int v                            = int(yuv.z * 255.0f);
          int outindex                     = (yout * (w / 2) + x) * 2;
          outptr[numpixels + outindex + 0] = u;
          outptr[numpixels + outindex + 1] = v;
        }
      }
      break;
    }
    case EBufferFormat::RGBA8: {
      // glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capbuf->_data);
      OrkAssert(false);
      break;
    }
    case EBufferFormat::RGB8: {
      OrkAssert(false);
      //////////////////////////////////////
      // read RGBA
      //////////////////////////////////////
      size_t rgbasize = w * h * 4;
      if (capbuf->_tempbuffer.size() != rgbasize) {
        capbuf->_tempbuffer.resize(rgbasize);
      }
      // glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capbuf->_tempbuffer.data());
      //////////////////////////////////////
      // discard alpha
      //////////////////////////////////////
      auto SRC = (const uint32_t*)capbuf->_tempbuffer.data();
      auto DST = (uint8_t*)capbuf->_data;
      for (size_t ipix = 0; ipix < (w * h); ipix++) {
        int idi    = ipix * 3;
        DST[idi++] = (SRC[ipix] & 0x00ff0000) >> 16;
        DST[idi++] = (SRC[ipix] & 0x0000ff00) >> 8;
        DST[idi++] = (SRC[ipix] & 0x000000ff);
      }
      //////////////////////////////////////
      break;
    }
    case EBufferFormat::RGBA16F:
      OrkAssert(false);
      // glReadPixels(x, y, w, h, GL_RGBA, GL_HALF_FLOAT, capbuf->_data);
      break;
    ///////////////////////////////////////////////////////
    case EBufferFormat::RGBA32F: {
      OrkAssert(vkfmt == VK_FORMAT_R32G32B32A32_SFLOAT);
      size_t bufsize = w * h * 16;
      if (capbuf->_tempbuffer.size() != bufsize) {
        capbuf->_tempbuffer.resize(bufsize);
      }
      auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, bufsize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
      vkCmdCopyImageToBuffer(
          _contextVK->primary_cb()->_vkcmdbuf, vkimg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, staging_buffer->_vkbuffer, 1, &region);
      staging_buffer->copyToHost(capbuf->_tempbuffer.data(), bufsize);
      capbuf->_impl.setShared<VulkanBuffer>(staging_buffer);
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
  return true;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////