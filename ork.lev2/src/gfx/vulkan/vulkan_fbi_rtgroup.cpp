
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
vkrtgrpimpl_ptr_t VkFrameBufferInterface::_createRtGroupImpl(const VkRtgCrOpts& options) {
  int inumtargets           = options._colorFormats.size();
  vkrtgrpimpl_ptr_t RTGIMPL = std::make_shared<VkRtGroupImpl>(_contextVK);
  RTGIMPL->_width = options._width;
  RTGIMPL->_height = options._height;
  RTGIMPL->_pipeline_bits = 0;
  if(options._depthFormat!=VK_FORMAT_UNDEFINED) {
    uint64_t USAGE  = "depth"_crcu;
    auto bufferimpl = std::make_shared<VklRtBufferImpl>(RTGIMPL.get(),USAGE, options._depthFormat);
    RTGIMPL->_depth_buffer_impl = bufferimpl;
    _vkCreateImageForBuffer(_contextVK, bufferimpl, options._depthFormat, USAGE);
    auto& adesc          = bufferimpl->_attachmentDesc;
    adesc.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    adesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    adesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }
  ////////////////////////////////////////
  // other buffers
  ////////////////////////////////////////
  bool is_swapchain = false;
  for (int it = 0; it < inumtargets; it++) {
    ////////////////////////////////////////////
    uint64_t USAGE = "color"_crcu;
    if (options._colorUsages[it] != 0) {
      USAGE = options._colorUsages[it];
    }
    auto vkfmt    = options._colorFormats[it];
    auto bufferimpl         = std::make_shared<VklRtBufferImpl>(RTGIMPL.get(),USAGE, vkfmt);
    RTGIMPL->_color_buffer_impls.push_back(bufferimpl);
    ////////////////////////////////////////////
    if (USAGE == "swapchain"_crcu) {
      is_swapchain = true;
    }
    ////////////////////////////////////////////
    else { // not present...
      //OrkAssert(rtgroup->_msaa_samples == MsaaSamples::MSAA_1X);
      //_contextVK->_txi->_initTextureFromRtBuffer(rtbuffer.get());
    }
    ///////////////////////////////////////////////////
  }

  switch (options._usage) {
    case "user"_crcu: {
      for (int it = 0; it < inumtargets; it++) {
        uint64_t buf_usage = options._colorUsages[it];
        VkFormat vk_fmt = options._colorFormats[it];
        //rtbuffer_ptr_t rtbuffer = rtgroup->buffer(it);
        OrkAssert(buf_usage != "depth"_crcu);
        auto usage = buf_usage;
        auto bufferimpl = std::make_shared<VklRtBufferImpl>(RTGIMPL.get(),usage, vk_fmt);
        //auto texture    = rtbuffer->texture();
        //OrkAssert(texture != nullptr);
        //printf("texture<%p:%s> _usage<0x%llx>\n", (void*)texture, texture->_debugName.c_str(), buf_usage);
        OrkAssert(buf_usage == "color"_crcu);
        //auto teximpl = texture->_impl.getShared<VulkanTextureObject>();

        //bufferimpl->setLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        auto& attachment_ref = bufferimpl->_attachmentRef;

        attachment_ref.attachment = it;
        attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        //OrkAssert(teximpl->_imgobj->_vkimageview != VK_NULL_HANDLE);
        //bufferimpl->_descriptorInfo.imageView = teximpl->_imgobj->_vkimageview;
        //bufferimpl->_descriptorInfo.sampler   = teximpl->_vksampler->_vksampler;
        //bufferimpl->_imgobj                   = teximpl->_imgobj;
        //bufferimpl->_vkimgview                = teximpl->_imgobj->_vkimageview;
        //bufferimpl->_vkimg                    = teximpl->_imgobj->_vkimage;
        //OrkAssert(bufferimpl->_vkimgview != VK_NULL_HANDLE);
      }
      break;
    }
    case "swapchain"_crcu: 
      break;
    case "popup"_crcu: 
      break;
    default:
      break;
  }
  return RTGIMPL;
}

///////////////////////////////////////////////////////////////////////////////

vkrtgrpimpl_ptr_t VkFrameBufferInterface::_createRtGroupImpl(rtgroup_rawptr_t rtgroup) {
  int inumtargets           = rtgroup->numImageBuffers();
  VkRtgCrOpts options;
  options._usage = rtgroup->_usage;
  options._msaaSamples = rtgroup->_msaa_samples;
  options._depthFormat = VkFormatConverter::convertBufferFormat(rtgroup->_depthBuffer->format());
  for(int i=0; i < inumtargets; i++) {
    auto rtb = rtgroup->buffer(i);
    options._colorFormats.push_back(VkFormatConverter::convertBufferFormat(rtb->format()));
    options._colorUsages.push_back(rtb->_usage);
  }
  options._width = rtgroup->width();
  options._height = rtgroup->height();
  auto rtgimpl = _createRtGroupImpl(options);
  ///////////////////////////////////////////////////
  // set impls in rtgroup and rtbuffers
  ///////////////////////////////////////////////////
  VkRtGroupImpl::assignToRtGroup(rtgimpl, rtgroup);
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
        OrkAssert(rtb_impl->_vkimgview != VK_NULL_HANDLE);
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
  // transition rtgroup to RTT mode
  /////////////////////////////////////////

  RTGIMPL->_transitionToRenderTarget(_contextVK->primary_cb());

  /////////////////////////////////////////
  // Begin dynamic rendering
  /////////////////////////////////////////

  auto vkcmdbuf = RTGIMPL->_cmdbufRTG->_impl.getShared<VkSecondaryCommandBufferImpl>();
  vkResetCommandBuffer(vkcmdbuf->_vkcmdbuf, 0);                         // vkBeginCommandBuffer does an implicit reset
  vkBeginCommandBuffer(vkcmdbuf->_vkcmdbuf, &RTGIMPL->_cmdBufCBBI_GFX); // vkBeginCommandBuffer does an implicit reset
  auto rinfo = RTGIMPL->renderinfo();

  _contextVK->_vkCmdBeginRenderingKHR(vkcmdbuf->_vkcmdbuf, &rinfo->_renderinfo);

  _contextVK->_vkcmdbuffer_current = RTGIMPL->_cmdbufRTG->_impl.getShared<VkSecondaryCommandBufferImpl>()->_vkcmdbuf;

}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_pushRtGroup(rtgroup_rawptr_t rtgroup) {
  __setRtGroup(rtgroup);
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_popRtGroup(bool continue_render) {

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

  _contextVK->_vkCmdEndRenderingKHR(cbufimpl->_vkcmdbuf);
  vkEndCommandBuffer(cbufimpl->_vkcmdbuf);
  cbufimpl->_recorded = true;
  _contextVK->enqueueSecondaryCommandBuffer(RTGIMPL->_cmdbufRTG);

  _contextVK->_vkcmdbuffer_current = _contextVK->primary_cb()->_vkcmdbuf;

  /////////////////////////////////////////////
  // transition rtgroup ?
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
  auto vkimgview = rtbi->_vkimgview;

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
