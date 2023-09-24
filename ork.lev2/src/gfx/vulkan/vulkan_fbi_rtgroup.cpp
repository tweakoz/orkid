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

  bool is_surface = rtgroup->_name.find("ui::Surface") != std::string::npos;
  if(is_surface){
  }
  ////////////////////////////////////////
  // depth buffer
  ////////////////////////////////////////
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
  ////////////////////////////////////////
  // other buffers
  ////////////////////////////////////////
  bool is_swapchain = false;
  for (int it = 0; it < inumtargets; it++) {
    rtbuffer_ptr_t rtbuffer = rtgroup->GetMrt(it);
    auto bufferimpl         = std::make_shared<VklRtBufferImpl>(RTGIMPL.get(), rtbuffer.get());
    rtbuffer->_impl.setShared<VklRtBufferImpl>(bufferimpl);
    bufferimpl->_vkfmt = VkFormatConverter::convertBufferFormat(rtbuffer->mFormat);
    ////////////////////////////////////////////
    uint64_t USAGE     = "color"_crcu;
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
  if(rtgroup->_pseudoRTG or is_swapchain){
    //OrkAssert(false);
  }
  ////////////////////////////////////////////////////////////////////
  // setup renderpass for rtgroup
  ////////////////////////////////////////////////////////////////////
  else{

    for (int it = 0; it < inumtargets; it++) {
      rtbuffer_ptr_t rtbuffer = rtgroup->GetMrt(it);
      OrkAssert(rtbuffer->_usage != "depth"_crcu);
      auto bufferimpl         = rtbuffer->_impl.getShared<VklRtBufferImpl>();
      auto texture = rtbuffer->texture();
      OrkAssert(texture!=nullptr);
      printf( "texture<%p:%s> _usage<0x%zx>\n", (void*) texture, texture->_debugName.c_str(), rtbuffer->_usage );
      OrkAssert(rtbuffer->_usage=="color"_crcu);
      auto teximpl = texture->_impl.getShared<VulkanTextureObject>();
      auto format = bufferimpl->_vkfmt;

      // Define the color attachment
      auto& attachment = RTGIMPL->_vkattach_descriptions.emplace_back();

      attachment.format = format; // This should match the format of your image
      attachment.samples = VK_SAMPLE_COUNT_1_BIT;
      attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      auto& attachment_ref = RTGIMPL->_vkattach_references.emplace_back();

      attachment_ref.attachment = it;
      attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      auto& image_descriptor_info = RTGIMPL->_vkattach_descimginfos.emplace_back();
      OrkAssert(teximpl->_imgobj->_vkimageview != VK_NULL_HANDLE);
      image_descriptor_info.imageView = teximpl->_imgobj->_vkimageview;
      image_descriptor_info.sampler = teximpl->_vksampler->_vksampler;

      RTGIMPL->_vkattach_imageviews.push_back(teximpl->_imgobj->_vkimageview);

    }

    // Define the subpass
    RTGIMPL->_vksubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    RTGIMPL->_vksubpass.colorAttachmentCount = RTGIMPL->_vkattach_references.size();
    RTGIMPL->_vksubpass.pColorAttachments = RTGIMPL->_vkattach_references.data();

    // Define the render pass
    VkRenderPassCreateInfo RPI{};
    initializeVkStruct(RPI, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    RPI.attachmentCount = RTGIMPL->_vkattach_descriptions.size();
    RPI.pAttachments = RTGIMPL->_vkattach_descriptions.data();
    RPI.subpassCount = 1;
    RPI.pSubpasses = &RTGIMPL->_vksubpass;

    // Optionally, you can also define subpass dependencies for layout transitions
    //VkSubpassDependency dependency{};
    //dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    //dependency.dstSubpass = 0;
    //dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    //dependency.srcAccessMask = 0;
    //dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    //dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    //RPI.dependencyCount = 1;
    //RPI.pDependencies = &dependency;

    vkCreateRenderPass(_contextVK->_vkdevice, &RPI, nullptr, &RTGIMPL->_vkrp);

    // Create Framebuffer
    initializeVkStruct(RTGIMPL->_vkfbinfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
    RTGIMPL->_vkfbinfo.renderPass = RTGIMPL->_vkrp;
    RTGIMPL->_vkfbinfo.attachmentCount = RTGIMPL->_vkattach_imageviews.size();
    RTGIMPL->_vkfbinfo.pAttachments = RTGIMPL->_vkattach_imageviews.data();
    RTGIMPL->_vkfbinfo.width = w;
    RTGIMPL->_vkfbinfo.height = h;
    RTGIMPL->_vkfbinfo.layers = 1;

    vkCreateFramebuffer(_contextVK->_vkdevice, &RTGIMPL->_vkfbinfo, nullptr, &RTGIMPL->_vkfb);

    //VkWriteDescriptorSet DWRITE{};
    //initializeVkStruct(DWRITE, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
    //DWRITE.dstSet = /* Your Descriptor Set */;
    //DWRITE.dstBinding = /* Your Binding */;
    //DWRITE.dstArrayElement = 0;
    //DWRITE.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    //DWRITE.descriptorCount = 1;
    //DWRITE.pImageInfo = &IMGINFO;
    // Don't forget to destroy the framebuffer and render pass when they are no longer needed
    // vkDestroyFramebuffer(_contextVK->_device, framebuffer, nullptr);
    // vkDestroyRenderPass(_contextVK->_device, renderPass, nullptr);

    //vkUpdateDescriptorSets(_contextVK->_device, 1, &descriptorWrite, 0, nullptr);
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
  _active_rtgroup = rtgroup;

  bool is_surface = rtgroup->_name.find("ui::Surface") != std::string::npos;
  if(is_surface){
    //OrkAssert(false);
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
  //printf( "inumtargets<%d> numsamples<%d>\n", inumtargets, numsamples );
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
  _postPushRtGroup(rtgroup);
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
