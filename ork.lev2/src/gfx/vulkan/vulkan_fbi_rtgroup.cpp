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

  initializeVkStruct(_attachmentDesc);
  initializeVkStruct(_vkimgview);


  _attachmentDesc.samples       = VK_SAMPLE_COUNT_1_BIT;        // No multisampling for this example.
  _attachmentDesc.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;  // Clear the color buffer before rendering.
  _attachmentDesc.storeOp       = VK_ATTACHMENT_STORE_OP_STORE; // Store the rendered content for presentation.
  _attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  _attachmentDesc.finalLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
  switch (rtb->format()) {
    case EBufferFormat::DEPTH:
      _attachmentDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
      _attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
      break;
    default:
      _attachmentDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // We don't care about stencil.
      _attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      break;
  }

   _vkfmt = VkFormatConverter::convertBufferFormat(rtb->format());
   _attachmentDesc.format      = _vkfmt;

}
void VklRtBufferImpl::setLayout(VkImageLayout layout){
  _currentLayout              = layout;
  _attachmentDesc.finalLayout = layout;
  OrkAssert(_parent);
  _parent->__attachments = nullptr;
}

VkRtGroupImpl::VkRtGroupImpl(RtGroup* rtg)
    : _rtg(rtg) {
}

///////////////////////////////////////////////////////////////////////////////

rtgroup_attachments_ptr_t VkRtGroupImpl::attachments() {
  if( __attachments ){
    return __attachments;
  }
  __attachments = std::make_shared<RtGroupAttachments>();
  auto at = std::make_shared<RtGroupAttachments>();
  int numrt = _rtg->GetNumTargets();
  for (int i = 0; i < numrt; i++) {
    auto rtbuffer = _rtg->GetMrt(i);
    auto bufferimpl = rtbuffer->_impl.getShared<VklRtBufferImpl>();
    __attachments->_descriptions.push_back(bufferimpl->_attachmentDesc);
    __attachments->_references.push_back(bufferimpl->_attachmentRef);
    __attachments->_imageviews.push_back(bufferimpl->_vkimgview);
    __attachments->descimginfos.push_back(bufferimpl->_descriptorInfo);
  }
  if(_rtg->_depthBuffer){
    auto rtbuffer = _rtg->_depthBuffer;
    auto bufferimpl = rtbuffer->_impl.getShared<VklRtBufferImpl>();
    __attachments->_descriptions.push_back(bufferimpl->_attachmentDesc);
    __attachments->_references.push_back(bufferimpl->_attachmentRef);
    __attachments->_imageviews.push_back(bufferimpl->_vkimgview);
    __attachments->descimginfos.push_back(bufferimpl->_descriptorInfo);
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

void VklRtBufferImpl::_replaceImage(
    VkFormat new_fmt, //
    VkImageView new_view, //
    VkImage new_img) { //

  auto old_img = _imgobj->_vkimage;
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
    uint64_t USAGE = "depth"_crcu;
    _vkCreateImageForBuffer(_contextVK, bufferimpl, rtbuffer->mFormat, USAGE);
    bufferimpl->setLayout(VkFormatConverter::_instance.layoutForUsage(USAGE));
    auto& adesc = bufferimpl->_attachmentDesc;
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
    auto bufferimpl = rtbuffer->_impl.makeShared<VklRtBufferImpl>(RTGIMPL.get(), rtbuffer.get());
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
      auto bufferimpl = rtbuffer->_impl.makeShared<VklRtBufferImpl>(RTGIMPL.get(),rtbuffer.get());
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
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_pushRtGroup(RtGroup* rtgroup) {
  // auto prev_rtgroup = _active_rtgroup;
  // if (nullptr == rtgroup) {
  //_setMainAsRenderTarget();
  //} else {
  //_active_rtgroup = rtgroup;
  //}

  if( rtgroup == _main_rtg.get() ){
    auto rpass = createRenderPassForRtGroup(_contextVK, _main_rtg);
    _contextVK->_renderpasses.push_back(rpass);
    _contextVK->beginRenderPass(rpass);
    _postPushRtGroup(_main_rtg.get() );
    return;
  }

  if(nullptr==_active_rtgroup){
    _active_rtgroup = rtgroup;
    return;
    //OrkAssert(false);
  }
  _active_rtgroup = rtgroup;

  bool is_surface = rtgroup->_name.find("ui::Surface") != std::string::npos;
  if (is_surface) {
    // OrkAssert(false);
  }

  OrkAssert(_active_rtgroup);
  int iw = _active_rtgroup->width();
  int ih = _active_rtgroup->height();
  /////////////////////////////////////////
  // if we are a psuedp rtgroup (eg. swapchain), NO_OP
  /////////////////////////////////////////
  if (_active_rtgroup->_pseudoRTG) {
    return;
  }
  /////////////////////////////////////////
  int inumtargets = _active_rtgroup->GetNumTargets();
  int numsamples  = msaaEnumToInt(_active_rtgroup->_msaa_samples);
  // printf( "inumtargets<%d> numsamples<%d>\n", inumtargets, numsamples );
  //  auto texture_target_2D = (numsamples==1) ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
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
  _postPushRtGroup(rtgroup);
}

///////////////////////////////////////////////////////////////////////////////

RtGroup* VkFrameBufferInterface::_popRtGroup() {
  OrkAssert(_active_rtgroup);
  if( _active_rtgroup == _main_rtg.get() ){
    auto rpass = _contextVK->_renderpasses.back();
    _contextVK->endRenderPass(rpass);
    return _active_rtgroup;
  }
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
