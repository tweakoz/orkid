////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_rtgroup = logger()->createChannel("VKRTG", fvec3(0.8, 0.2, 0.5), true);

///////////////////////////////////////////////////////////////////////////////

VklRtBufferImpl::VklRtBufferImpl(VkRtGroupImpl* par, RtBuffer* rtb) //
    : _rtg_impl(par)
    , _rtb(rtb) { //

  initializeVkStruct(_attachmentDesc);
  initializeVkStruct(_vkimgview);

  _attachmentDesc.samples       = VK_SAMPLE_COUNT_1_BIT;        // No multisampling for this example.
  _attachmentDesc.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;  // Clear the color/depth buffer before rendering.
  _attachmentDesc.storeOp       = VK_ATTACHMENT_STORE_OP_STORE; // Store the rendered color/depth for presentation.
  _attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  _attachmentDesc.finalLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
  switch (rtb->format()) {
    case EBufferFormat::DEPTH:
      _attachmentDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      _attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      break;
    default:
      _attachmentDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // We don't care about stencil.
      _attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      break;
  }

  _vkfmt                 = VkFormatConverter::convertBufferFormat(rtb->format());
  _attachmentDesc.format = _vkfmt;
}
void VklRtBufferImpl::setLayout(VkImageLayout layout) {
  auto previousLayout           = _attachmentDesc.finalLayout;
  _currentLayout                = layout;
  _attachmentDesc.initialLayout = previousLayout;
  _attachmentDesc.finalLayout   = layout;
  OrkAssert(_rtg_impl);
  _rtg_impl->__attachments = nullptr;
}

VkRtGroupImpl::VkRtGroupImpl(RtGroup* rtg)
    : _rtg(rtg) {
}

///////////////////////////////////////////////////////////////////////////////

rtgroup_attachments_ptr_t VkRtGroupImpl::attachments() {
  if (__attachments) {
    return __attachments;
  }
  __attachments = std::make_shared<RtGroupAttachments>();
  auto at       = std::make_shared<RtGroupAttachments>();
  int numrt     = _rtg->GetNumTargets();
  for (int i = 0; i < numrt; i++) {
    auto rtbuffer   = _rtg->GetMrt(i);
    auto bufferimpl = rtbuffer->_impl.getShared<VklRtBufferImpl>();
    __attachments->_descriptions.push_back(bufferimpl->_attachmentDesc);
    __attachments->_references.push_back(bufferimpl->_attachmentRef);
    __attachments->_imageviews.push_back(bufferimpl->_vkimgview);
    __attachments->descimginfos.push_back(bufferimpl->_descriptorInfo);

    if (bufferimpl->_vkimgview == VK_NULL_HANDLE) {
      printf("rtg<%s> has null imageview\n", _rtg->_name.c_str());
      OrkAssert(false);
    }
  }
  if (_rtg->_depthBuffer) {
    auto rtbuffer   = _rtg->_depthBuffer;
    auto bufferimpl = rtbuffer->_impl.getShared<VklRtBufferImpl>();
    __attachments->_descriptions.push_back(bufferimpl->_attachmentDesc);
    __attachments->_references.push_back(bufferimpl->_attachmentRef);
    __attachments->_imageviews.push_back(bufferimpl->_vkimgview);
    __attachments->descimginfos.push_back(bufferimpl->_descriptorInfo);
    OrkAssert(bufferimpl->_vkimgview != VK_NULL_HANDLE);
  }
  return __attachments;
}

///////////////////////////////////////////////////////////////////////////////

void _vkCreateImageForBuffer(
    vkcontext_rawptr_t ctxVK, //
    vkrtbufimpl_ptr_t bufferimpl,
    EBufferFormat ork_fmt,
    uint64_t usage) {               //
  auto VKICI = makeVKICI(           //
      bufferimpl->_rtg_impl->_width,  // width
      bufferimpl->_rtg_impl->_height, // height
      1,                            // depth
      ork_fmt,                      // format
      1);                           // miplevels
  switch (usage) {
    case "depth"_crcu:
      VKICI->usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      break;
    case "color"_crcu:
      VKICI->usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      // Use as texture and allow data transfer to it
      VKICI->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
      VKICI->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      break;
    case "present"_crcu:
      VKICI->usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      break;
    default:
      OrkAssert(false);
      break;
  }
  ///////////////////////////////////////////////////
  bufferimpl->_imgobj = std::make_shared<VulkanImageObject>(ctxVK, VKICI);
  auto& vkimage       = bufferimpl->_imgobj->_vkimage;
  bufferimpl->_vkimg  = vkimage;
  ///////////////////////////////////////////////////
  auto IVCI = createImageViewInfo2D(
      vkimage,            //
      bufferimpl->_vkfmt, //
      VkFormatConverter::_instance.aspectForUsage(usage));
  VkResult OK = vkCreateImageView(ctxVK->_vkdevice, IVCI.get(), nullptr, &bufferimpl->_vkimgview);
  OrkAssert(OK == VK_SUCCESS);
  ///////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::_replaceImage(
    VkFormat new_fmt,     //
    VkImageView new_view, //
    VkImage new_img) {    //

  auto old_img  = _imgobj->_vkimage;
  auto old_view = _vkimgview;

  ////////////////////
  // delete old image
  ////////////////////

  // todo

  ////////////////////
  // assign new image
  ////////////////////

  _init      = false;
  _vkimg     = new_img;
  _vkimgview = new_view;
  _vkfmt     = new_fmt;
}

///////////////////////////////////////////////////////////////////////////////

vkrtgrpimpl_ptr_t VkFrameBufferInterface::_createRtGroupImpl(RtGroup* rtgroup) {
  vkrtgrpimpl_ptr_t RTGIMPL = rtgroup->_impl.makeShared<VkRtGroupImpl>(rtgroup);
  RTGIMPL->_width           = rtgroup->width();
  RTGIMPL->_height          = rtgroup->height();
  int inumtargets           = rtgroup->GetNumTargets();
  int w                     = rtgroup->width();
  int h                     = rtgroup->height();

  RTGIMPL->_pipeline_bits = 0;

  bool is_surface = rtgroup->_name.find("ui::Surface") != std::string::npos;
  if (is_surface) {
  }
  ////////////////////////////////////////
  // depth buffer
  ////////////////////////////////////////
  if (rtgroup->_depthBuffer) {
    auto rtbuffer   = rtgroup->_depthBuffer;
    auto bufferimpl = rtbuffer->_impl.makeShared<VklRtBufferImpl>(RTGIMPL.get(), rtbuffer.get());
    uint64_t USAGE  = "depth"_crcu;
    _vkCreateImageForBuffer(_contextVK, bufferimpl, rtbuffer->mFormat, USAGE);
    bufferimpl->setLayout(VkFormatConverter::_instance.layoutForUsage(USAGE));
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
    rtbuffer_ptr_t rtbuffer = rtgroup->GetMrt(it);
    auto bufferimpl         = rtbuffer->_impl.makeShared<VklRtBufferImpl>(RTGIMPL.get(), rtbuffer.get());
    ////////////////////////////////////////////
    uint64_t USAGE = "color"_crcu;
    if (rtbuffer->_usage != 0) {
      USAGE = rtbuffer->_usage;
    }
    ////////////////////////////////////////////
    if (USAGE == "present"_crcu) {
      is_swapchain = true;
    }
    ////////////////////////////////////////////
    else { // not present...
      OrkAssert(rtgroup->_msaa_samples == MsaaSamples::MSAA_1X);
      _contextVK->_txi->_initTextureFromRtBuffer(rtbuffer.get());
    }
    ///////////////////////////////////////////////////
  }

  ////////////////////////////////////////////////////////////////////
  if (rtgroup->_pseudoRTG or is_swapchain) {
    // OrkAssert(false);
  }
  ////////////////////////////////////////////////////////////////////
  // setup renderpass for rtgroup
  ////////////////////////////////////////////////////////////////////
  else {

    for (int it = 0; it < inumtargets; it++) {
      rtbuffer_ptr_t rtbuffer = rtgroup->GetMrt(it);
      OrkAssert(rtbuffer->_usage != "depth"_crcu);
      auto bufferimpl = rtbuffer->_impl.makeShared<VklRtBufferImpl>(RTGIMPL.get(), rtbuffer.get());
      auto texture    = rtbuffer->texture();
      OrkAssert(texture != nullptr);
      printf("texture<%p:%s> _usage<0x%zx>\n", (void*)texture, texture->_debugName.c_str(), rtbuffer->_usage);
      OrkAssert(rtbuffer->_usage == "color"_crcu);
      auto teximpl = texture->_impl.getShared<VulkanTextureObject>();
      auto format  = bufferimpl->_vkfmt;

      bufferimpl->setLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

      auto& attachment_ref = bufferimpl->_attachmentRef;

      attachment_ref.attachment = it;
      attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      OrkAssert(teximpl->_imgobj->_vkimageview != VK_NULL_HANDLE);
      bufferimpl->_descriptorInfo.imageView = teximpl->_imgobj->_vkimageview;
      bufferimpl->_descriptorInfo.sampler   = teximpl->_vksampler->_vksampler;
      bufferimpl->_imgobj                   = teximpl->_imgobj;
      bufferimpl->_vkimgview                = teximpl->_imgobj->_vkimageview;
      bufferimpl->_vkimg                    = teximpl->_imgobj->_vkimage;
      OrkAssert(bufferimpl->_vkimgview != VK_NULL_HANDLE);
    }

    // VkWriteDescriptorSet DWRITE{};
    // initializeVkStruct(DWRITE, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
    // DWRITE.dstSet = /* Your Descriptor Set */;
    // DWRITE.dstBinding = /* Your Binding */;
    // DWRITE.dstArrayElement = 0;
    // DWRITE.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // DWRITE.descriptorCount = 1;
    // DWRITE.pImageInfo = &IMGINFO;
    //  Don't forget to destroy the framebuffer and render pass when they are no longer needed
    //  vkDestroyFramebuffer(_contextVK->_device, framebuffer, nullptr);
    //  vkDestroyRenderPass(_contextVK->_device, renderPass, nullptr);

    // vkUpdateDescriptorSets(_contextVK->_device, 1, &descriptorWrite, 0, nullptr);
  }

  return RTGIMPL;
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_present() {
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_pushRtGroup(RtGroup* rtgroup) {

  _active_rtgroup = rtgroup;
  vkrtgrpimpl_ptr_t RTGIMPL;

  /////////////////////////////////
  // end previous renderpass ?
  /////////////////////////////////

  size_t prev_rpass_count = _contextVK->_renderpasses.size();
  if (prev_rpass_count > 0) {
    auto prev_rpass = _contextVK->_renderpasses.back();
      _contextVK->endRenderPass(prev_rpass);
  }

  /////////////////////////////////
  // main_rtg ?
  //  (images managed by swapchain)
  /////////////////////////////////
  if (rtgroup == _main_rtg.get()) {
    RTGIMPL = _main_rtg->_impl.getShared<VkRtGroupImpl>();
  }
  /////////////////////////////////
  // auxillary rtg ?
  /////////////////////////////////
  else {
    OrkAssert(_active_rtgroup);
    /////////////////////////////////////////
    // if we are a psuedp rtgroup (eg. swapchain), NO_OP
    /////////////////////////////////////////
    if (_active_rtgroup->_pseudoRTG) {
      return;
    }
    int iw = _active_rtgroup->width();
    int ih = _active_rtgroup->height();
    /////////////////////////////////////////
    int inumtargets = _active_rtgroup->GetNumTargets();
    int numsamples  = msaaEnumToInt(_active_rtgroup->_msaa_samples);
    // printf( "inumtargets<%d> numsamples<%d>\n", inumtargets, numsamples );
    //  auto texture_target_2D = (numsamples==1) ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
    if (auto as_impl = _active_rtgroup->_impl.tryAsShared<VkRtGroupImpl>()) {
      RTGIMPL = as_impl.value();
    } else {
      RTGIMPL = _createRtGroupImpl(_active_rtgroup);
      _active_rtgroup->_impl.setShared<VkRtGroupImpl>(RTGIMPL);
    }
    /////////////////////////////////////////
    int implw      = RTGIMPL->_width;
    int implh      = RTGIMPL->_height;
    int rtgw       = _active_rtgroup->width();
    int rtgh       = _active_rtgroup->height();
    bool size_diff = (rtgw != implw) || (rtgh != implh);
    if (size_diff) {
      logchan_rtgroup->log("resize FBO iw<%d> ih<%d>", iw, ih);
      RTGIMPL = _createRtGroupImpl(_active_rtgroup);
      _active_rtgroup->_impl.setShared<VkRtGroupImpl>(RTGIMPL);
      _active_rtgroup->SetSizeDirty(false);
    }
    for (int i = 0; i < inumtargets; i++) {
      auto rtb      = _active_rtgroup->GetMrt(i);
      auto rtb_impl = rtb->_impl.getShared<VklRtBufferImpl>();
      OrkAssert(rtb_impl->_vkimgview != VK_NULL_HANDLE);
    }
  }
  /////////////////////////////////////////
  // transition rtgroup to RTT mode
  /////////////////////////////////////////

  int inumtargets = _active_rtgroup->GetNumTargets();
  //auto vkcmdbuf   = rpass_impl->_seccmdbuffer->_impl.getShared<VkCommandBufferImpl>();
  for (int i = 0; i < inumtargets; i++) {
    auto rtb      = _active_rtgroup->GetMrt(i);
    auto rtb_impl = rtb->_impl.getShared<VklRtBufferImpl>();
    rtb_impl->transitionToRenderTarget(_contextVK,_contextVK->primary_cb());
  }
  /////////////////////////////////////////
  // begin new renderpass
  /////////////////////////////////////////
  auto rpass_name = FormatString("rpass<%s>", rtgroup->_name.c_str());
  auto rpass = _contextVK->createRenderPassForRtGroup(rtgroup, true, rpass_name);
  _contextVK->_renderpasses.push_back(rpass);
  _contextVK->beginRenderPass(rpass);
  /////////////////////////////////////////
  auto rpass_impl = rpass->_impl.getShared<VulkanRenderPass>();
  _contextVK->pushCommandBuffer(rpass_impl->_seccmdbuffer);
  rpass_impl->_seccmdbuffer->_no_draw = (rtgroup != _main_rtg.get());
  /////////////////////////////////////////
  // push vp/scissor rect
  /////////////////////////////////////////
  float fx = 0.0f;
  float fy = 0.0f;
  float fw = 0.0f;
  float fh = 0.0f;
  if (rtgroup) {
    fw = rtgroup->width();
    fh = rtgroup->height();
  }
  ViewportRect extents(fx, fy, fw, fh);
  pushViewport(extents);
  pushScissor(extents);
}

///////////////////////////////////////////////////////////////////////////////

RtGroup* VkFrameBufferInterface::_popRtGroup(bool continue_render) {

  auto popped_rtg = _active_rtgroup;

  _active_rtgroup = mRtGroupStack.top();
  mRtGroupStack.pop();

  OrkAssert(_active_rtgroup);
  auto rtg_renpass    = _contextVK->_cur_renderpass;
  auto rtg_rpass_impl = rtg_renpass->_impl.getShared<VulkanRenderPass>();
  auto rtg_cmdbuf     = rtg_rpass_impl->_seccmdbuffer;
  auto vk_cmdbuf      = rtg_cmdbuf->_impl.getShared<VkCommandBufferImpl>();

  ///////////////////////////////////////////////////

  int num_buf = popped_rtg->GetNumTargets();

  //////////////////////////////////////////////
  // RTG commandbuffer complete, pop and execute
  //////////////////////////////////////////////

  _contextVK->popCommandBuffer(); // rtg
  _contextVK->enqueueSecondaryCommandBuffer(rtg_cmdbuf);

  //////////////////////////////////////////////
  // finish the renderpass (on primary cb)
  //////////////////////////////////////////////

  _contextVK->endRenderPass(_contextVK->_cur_renderpass);

  /////////////////////////////////////////////
  // transition rtgroup to texture sampling
  /////////////////////////////////////////////

  for (int ib = 0; ib < num_buf; ib++) {
    auto rtb      = popped_rtg->GetMrt(ib);
    auto rtb_impl = rtb->_impl.getShared<VklRtBufferImpl>();
    rtb_impl->transitionToTexture(_contextVK,_contextVK->primary_cb());
  }

  /////////////////////////////////////////
  // begin new renderpass ?
  /////////////////////////////////////////

  if (continue_render) {
    auto rpass_name = FormatString("rpass<%s>.rtg_continue", _active_rtgroup->_name.c_str());
    auto rpass = _contextVK->createRenderPassForRtGroup(_active_rtgroup, false, rpass_name);
    _contextVK->_renderpasses.push_back(rpass);
    _contextVK->beginRenderPass(rpass);
    auto rpass_impl = rpass->_impl.getShared<VulkanRenderPass>();
    _contextVK->pushCommandBuffer(rpass_impl->_seccmdbuffer);
  }

  return _active_rtgroup;
}

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::transitionToRenderTarget(vkcontext_rawptr_t ctxVK, vkcmdbufimpl_ptr_t cb) {

  //OrkAssert(ctxVK->_cur_renderpass);

  if (_imgobj) {

    auto new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    auto barrier = createImageBarrier(
        _imgobj->_vkimage,                        // VkImage image
        _currentLayout, // VkImageLayout oldLayout
        new_layout, // VkImageLayout newLayout
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,     // VkAccessFlags srcAccessMask
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);    // VkAccessFlags dstAccessMask

    if (1)
      vkCmdPipelineBarrier(
          cb->_vkcmdbuf,                                 // command buffer
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
          VK_DEPENDENCY_BY_REGION_BIT,                   // dependencyFlags
          0,
          nullptr, // memoryBarriers
          0,
          nullptr, // bufferMemoryBarriers
          1,
          barrier.get()); // imageMemoryBarriers

      setLayout(new_layout);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::transitionToTexture(vkcontext_rawptr_t ctxVK, vkcmdbufimpl_ptr_t cb) {

  //OrkAssert(ctxVK->_cur_renderpass);

  if (_imgobj) {

    if (_rtb->_usage != "color"_crcu) {
      return;
    }

    auto new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    auto barrier = createImageBarrier(
        _imgobj->_vkimage,                        // VkImage image
        _currentLayout, // VkImageLayout oldLayout
        new_layout, // VkImageLayout newLayout
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,     // VkAccessFlags srcAccessMask
        VK_ACCESS_SHADER_READ_BIT);               // VkAccessFlags dstAccessMask

    barrier->subresourceRange.aspectMask = VkFormatConverter::_instance.aspectForUsage(_rtb->_usage);

    if (1)
      vkCmdPipelineBarrier(
          cb->_vkcmdbuf,                                 // command buffer
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // dstStageMask
          VK_DEPENDENCY_BY_REGION_BIT,                   // dependencyFlags
          0,
          nullptr, // memoryBarriers
          0,
          nullptr, // bufferMemoryBarriers
          1,
          barrier.get()); // imageMemoryBarriers

    setLayout(new_layout);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VklRtBufferImpl::transitionToHostRead(vkcontext_rawptr_t ctxVK, vkcmdbufimpl_ptr_t cb) {

  //OrkAssert(ctxVK->_cur_renderpass);

  if (_imgobj) {

    if (_rtb->_usage != "color"_crcu) {
      return;
    }

    auto new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    auto barrier = createImageBarrier(
        _imgobj->_vkimage,                        // VkImage image
        _currentLayout, // VkImageLayout oldLayout
        new_layout, // VkImageLayout newLayout
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,     // VkAccessFlags srcAccessMask
        VK_ACCESS_TRANSFER_READ_BIT);               // VkAccessFlags dstAccessMask

    barrier->subresourceRange.aspectMask = VkFormatConverter::_instance.aspectForUsage(_rtb->_usage);

    if (1)
      vkCmdPipelineBarrier(
          cb->_vkcmdbuf,                                 // command buffer
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
          VK_PIPELINE_STAGE_TRANSFER_BIT,         // dstStageMask
          VK_DEPENDENCY_BY_REGION_BIT,                   // dependencyFlags
          0,
          nullptr, // memoryBarriers
          0,
          nullptr, // bufferMemoryBarriers
          1,
          barrier.get()); // imageMemoryBarriers

    setLayout(new_layout);
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

  if(capbuf->_captureW!=0){
    x = capbuf->_captureX;
    y = capbuf->_captureY;
    w = capbuf->_captureW;
    h = capbuf->_captureH;
  }

  rtbi->transitionToHostRead(_contextVK, _contextVK->primary_cb() );

  //printf("captureAsFormat w<%d> h<%d>\n", w, h);

  bool fmtmatch = (capbuf->format() == destfmt);
  bool sizmatch = (capbuf->width() == w);
  sizmatch &= (capbuf->height() == h);

  if (not(fmtmatch and sizmatch))
    capbuf->setFormatAndSize(destfmt, w, h);


  auto vkimg = rtbi->_imgobj->_vkimage;
  auto vkfmt = rtbi->_vkfmt;
  auto vkimgview = rtbi->_vkimgview;

  VkBufferImageCopy region = {};
  region.bufferOffset      = 0;
  region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent       = {uint32_t(w), uint32_t(h), 1};

  //GL_ERRORCHECK();
  static size_t yo       = 0;
  constexpr float inv256 = 1.0f / 255.0f;
  switch (destfmt) {
    case EBufferFormat::NV12: {
      size_t rgbasize = w * h * 4;
      if (capbuf->_tempbuffer.size() != rgbasize) {
        capbuf->_tempbuffer.resize(rgbasize);
      }

      //glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capbuf->_tempbuffer.data());
      //GL_ERRORCHECK();
      // todo convert RGBA8 to NV12 (on GPU)

      // grab RGBA8 vkimg to staging buffer
      OrkAssert(vkfmt==VK_FORMAT_R8G8B8A8_UNORM);
      auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, rgbasize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
      vkCmdCopyImageToBuffer( _contextVK->primary_cb()->_vkcmdbuf, 
                              vkimg, 
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                              staging_buffer->_vkbuffer, 
                              1, 
                              &region);


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
      //glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capbuf->_data);
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
      //glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capbuf->_tempbuffer.data());
      //////////////////////////////////////
      // discard alpha
      //////////////////////////////////////
      auto SRC = (const uint32_t*) capbuf->_tempbuffer.data();
      auto DST = (uint8_t*) capbuf->_data;
      for( size_t ipix=0; ipix<(w*h); ipix++ ){
        int idi = ipix*3;
        DST[idi++] = (SRC[ipix]&0x00ff0000)>>16;
        DST[idi++] = (SRC[ipix]&0x0000ff00)>>8;
        DST[idi++] = (SRC[ipix]&0x000000ff);
      }
      //////////////////////////////////////
      break;
    }
    case EBufferFormat::RGBA16F:
      OrkAssert(false);
      //glReadPixels(x, y, w, h, GL_RGBA, GL_HALF_FLOAT, capbuf->_data);
      break;
    ///////////////////////////////////////////////////////
    case EBufferFormat::RGBA32F:{
      OrkAssert(vkfmt==VK_FORMAT_R32G32B32A32_SFLOAT);
      size_t bufsize = w * h * 16;
      if (capbuf->_tempbuffer.size() != bufsize) {
        capbuf->_tempbuffer.resize(bufsize);
      }
      auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, bufsize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
      vkCmdCopyImageToBuffer( _contextVK->primary_cb()->_vkcmdbuf, 
                              vkimg, 
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                              staging_buffer->_vkbuffer, 
                              1, 
                              &region);
      staging_buffer->copyToHost(capbuf->_tempbuffer.data(), bufsize);
      capbuf->_impl.setShared<VulkanBuffer>(staging_buffer);
      break;
    }
    ///////////////////////////////////////////////////////
    case EBufferFormat::R32F:
      OrkAssert(false);
      //glReadPixels(x, y, w, h, GL_RED, GL_FLOAT, capbuf->_data);
      break;
    case EBufferFormat::R32UI:
      OrkAssert(false);
      //glReadPixels(x, y, w, h, GL_RED_INTEGER, GL_UNSIGNED_INT, capbuf->_data);
      break;
    case EBufferFormat::RG32F:
      OrkAssert(false);
      //glReadPixels(x, y, w, h, GL_RG, GL_FLOAT, capbuf->_data);
      break;
    default:
      OrkAssert(false);
      break;
  }
  //GL_ERRORCHECK();

  //glBindFramebuffer(GL_FRAMEBUFFER, 0);
  //  glReadBuffer( readbuffer ); // restore read buffer
  //GL_ERRORCHECK();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
