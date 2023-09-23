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
    : _parent(par)
    , _rtb(rtb) { //
}
VkRtGroupImpl::VkRtGroupImpl(RtGroup* rtg)
    : _rtg(rtg) {
}

///////////////////////////////////////////////////////////////////////////////

void _vkCreateImageForBuffer(
    vkcontext_rawptr_t ctxVK, //
    vkrtbufimpl_ptr_t bufferimpl,
    EBufferFormat ork_fmt,
    uint64_t usage) {               //
  auto VKICI = makeVKICI(           //
      bufferimpl->_parent->_width,  // width
      bufferimpl->_parent->_height, // height
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

vkrtgrpimpl_ptr_t VkFrameBufferInterface::_createRtGroupImpl(RtGroup* rtgroup) {
  vkrtgrpimpl_ptr_t RTGIMPL = rtgroup->_impl.makeShared<VkRtGroupImpl>(rtgroup);
  RTGIMPL->_width           = rtgroup->width();
  RTGIMPL->_height          = rtgroup->height();
  int inumtargets           = rtgroup->GetNumTargets();
  int w                     = rtgroup->width();
  int h                     = rtgroup->height();

  RTGIMPL->_pipeline_bits = 0;

  if (rtgroup->_depthBuffer) {
    auto rtbuffer   = rtgroup->_depthBuffer;
    auto bufferimpl = std::make_shared<VklRtBufferImpl>(RTGIMPL.get(), rtbuffer.get());
    rtbuffer->_impl.setShared<VklRtBufferImpl>(bufferimpl);
    bufferimpl->_vkfmt = VkFormatConverter::convertBufferFormat(rtbuffer->mFormat);

    uint64_t USAGE = "depth"_crcu;
    _vkCreateImageForBuffer(_contextVK, bufferimpl, rtbuffer->mFormat, USAGE);

    auto& adesc = bufferimpl->_attachmentDesc;
    initializeVkStruct(adesc);
    adesc.format         = bufferimpl->_vkfmt;
    adesc.samples        = VK_SAMPLE_COUNT_1_BIT;
    adesc.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    adesc.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    adesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    adesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    adesc.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    adesc.finalLayout    = VkFormatConverter::_instance.layoutForUsage(USAGE);
  }
  // other buffers
  for (int it = 0; it < inumtargets; it++) {
    rtbuffer_ptr_t rtbuffer = rtgroup->GetMrt(it);
    auto bufferimpl         = std::make_shared<VklRtBufferImpl>(RTGIMPL.get(), rtbuffer.get());
    rtbuffer->_impl.setShared<VklRtBufferImpl>(bufferimpl);
    bufferimpl->_vkfmt = VkFormatConverter::convertBufferFormat(rtbuffer->mFormat);
    uint64_t USAGE     = "color"_crcu;
    if (rtbuffer->_usage != 0) {
      USAGE = rtbuffer->_usage;
    }
    if (USAGE == "present"_crcu) {

    } else {
      _vkCreateImageForBuffer(_contextVK, bufferimpl, rtbuffer->mFormat, USAGE);
      auto& adesc = bufferimpl->_attachmentDesc;
      initializeVkStruct(adesc);
      adesc.format         = bufferimpl->_vkfmt;
      adesc.samples        = VK_SAMPLE_COUNT_1_BIT;
      adesc.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
      adesc.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      adesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      adesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      adesc.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
      adesc.finalLayout    = VkFormatConverter::_instance.layoutForUsage(USAGE);
    }
    ///////////////////////////////////////////////////
  }

  return RTGIMPL;
}

///////////////////////////////////////////////////////////////////////////////

RtGroup* VkFrameBufferInterface::_popRtGroup() {
  auto rtb0 = _active_rtgroup->mMrt[0];
  if (0)
    printf(
        "poprtg rtb<%s> usage<%08x>\n", //
        rtb0->_debugName.c_str(),       //
        rtb0->_usage);
  if (rtb0->_usage == "present"_crcu) {
    // OrkAssert(false);
  }

  // barrier on all color attachments
  ///////////////////////////////////////////////////

  int num_buf = _active_rtgroup->GetNumTargets();
  std::vector<VkImageMemoryBarrier> barriers(num_buf);

  for (int ib = 0; ib < num_buf; ib++) {
    auto rtb = _active_rtgroup->GetMrt(ib);
    if (rtb->_usage != "color"_crcu) {
      continue;
    }
    auto bufferimpl = rtb->_impl.getShared<VklRtBufferImpl>();
    auto& barrier   = barriers[ib];
    initializeVkStruct(barrier, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);

    barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout           = VkFormatConverter::_instance.layoutForUsage(rtb->_usage);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = bufferimpl->_imgobj->_vkimage; // The image you want to transition.

    barrier.subresourceRange.aspectMask     = VkFormatConverter::_instance.aspectForUsage(rtb->_usage);
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Adjust as needed.
    barrier.dstAccessMask = 0;

    vkCmdPipelineBarrier(
        _contextVK->primary_cb()->_vkcmdbuf,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // Adjust as needed.
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);
  }

  return _active_rtgroup;
}

void VkFrameBufferInterface::_postPushRtGroup(RtGroup* rtgroup) {

  float fx = 0.0f;
  float fy = 0.0f;
  float fw = 0.0f;
  float fh = 0.0f;
  if (rtgroup) {
    fw = rtgroup->width();
    fh = rtgroup->height();
    // glDepthRange(0.0, 1.0f);
  }
  ViewportRect extents(fx, fy, fw, fh);
  pushViewport(extents);
  pushScissor(extents);
  if (rtgroup and rtgroup->_autoclear) {
    rtGroupClear(rtgroup);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_present() {
  OrkAssert(_main_rtb_color->_usage == "present"_crcu);
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_pushRtGroup(RtGroup* rtgroup) {
  // auto prev_rtgroup = _active_rtgroup;
  // if (nullptr == rtgroup) {
  //_setMainAsRenderTarget();
  //} else {
  //_active_rtgroup = rtgroup;
  //}
  /*
  OrkAssert(_active_rtgroup);
  int iw = _active_rtgroup->width();
  int ih = _active_rtgroup->height();
  /////////////////////////////////////////
  if (_active_rtgroup->_pseudoRTG) {
    static const SRasterState defstate;
    //_target.RSI()->BindRasterState(defstate, true);
    //_currentRtGroup = rtgroup;
    // glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // if( rtgroup->_autoclear ){
    // rtGroupClear(rtgroup);
    //}
    return;
  }
  /////////////////////////////////////////
  int inumtargets = _active_rtgroup->GetNumTargets();
  int numsamples  = msaaEnumToInt(_active_rtgroup->_msaa_samples);
  // printf( "inumtargets<%d> numsamples<%d>\n", inumtargets, numsamples );
  // auto texture_target_2D = (numsamples==1) ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
  vkrtgrpimpl_ptr_t RTGIMPL;
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


  /////////////////////////////////////////
  // main (present) rtgroup?
  /////////////////////////////////////////
  if( _active_rtgroup == _main_rtg.get() ){

    VkRenderPassBeginInfo RPI = {};
    initializeVkStruct(RPI, VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
    //RPI.renderPass = renderPass; // Render pass you created earlier.
    //RPI.framebuffer = framebuffer; // Framebuffer you created earlier.
    //RPI.renderArea.offset = {0, 0};
    //RPI.renderArea.extent = extent; // VkExtent2D of your swapchain image, for example.

    VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};  // RGBA black.
    VkClearValue clearDepth = {1.0f, 0};                 // Clear depth 1.0 and stencil 0.
    VkClearValue clearValues[2] = {clearColor, clearDepth};

    //RPI.clearValueCount = 2;
    //RPI.pClearValues = clearValues;

    //vkCmdBeginRenderPass( commandBuffer, //
    //                      &renderPassInfo, //
    //                      VK_SUBPASS_CONTENTS_INLINE);



  }
  */
  /////////////////////////////////////////
  _postPushRtGroup(rtgroup);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_clearColorAndDepth(const fcolor4& rCol, float fdepth) {
  auto cmdbuf     = _contextVK->primary_cb();
  auto rtgimpl    = _active_rtgroup->_impl.getShared<VkRtGroupImpl>();
  int inumtargets = _active_rtgroup->GetNumTargets();
  int w           = _active_rtgroup->width();
  int h           = _active_rtgroup->height();
  printf("clearing rtg<%p> w<%d> h<%d>\n", (void*)rtgimpl.get(), w, h);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_clearDepth(float fdepth) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
