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
VkRtGroupImpl::VkRtGroupImpl(RtGroup* rtg) : _rtg(rtg) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_setAsRenderTarget() { // _main_rtg
  _currentRtGroup = _main_rtg.get();
  _active_rtgroup = _main_rtg.get();
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doResizeMainSurface(int iw, int ih) {
  _fbi->_main_rtg->Resize(iw, ih);
}

///////////////////////////////////////////////////////////////////////////////

static struct VkFormatConverter{

    VkFormatConverter(){
        _fmtmap[EBufferFormat::RGBA8] = VK_FORMAT_R8G8B8A8_UNORM;
        _fmtmap[EBufferFormat::Z32] = VK_FORMAT_D32_SFLOAT;

        _layoutmap["depth"_crcu] = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        _layoutmap["color"_crcu] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        _layoutmap["present"_crcu] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        // VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        // VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        // VK_IMAGE_LAYOUT_PREINITIALIZED
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR

        _aspectmap["depth"_crcu] = VK_IMAGE_ASPECT_DEPTH_BIT;
        _aspectmap["color"_crcu] = VK_IMAGE_ASPECT_COLOR_BIT;
        _aspectmap["present"_crcu] = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    VkFormat convertBufferFormat(EBufferFormat fmt_in) const {
        auto it = _fmtmap.find(fmt_in);
        OrkAssert(it!=_fmtmap.end());
        return it->second;
    }
    VkImageLayout layoutForUsage(uint64_t usage) const {
        auto it = _layoutmap.find(usage);
        OrkAssert(it!=_layoutmap.end());
        return it->second;
    }
    VkImageAspectFlagBits aspectForUsage(uint64_t usage) const {
        auto it = _aspectmap.find(usage);
        OrkAssert(it!=_aspectmap.end());
        return it->second;
    }
    std::unordered_map<EBufferFormat,VkFormat> _fmtmap;
    std::unordered_map<uint64_t,VkImageLayout> _layoutmap;
    std::unordered_map<uint64_t,VkImageAspectFlagBits> _aspectmap;
};

static const VkFormatConverter _vkFormatConverter;

///////////////////////////////////////////////////////////////////////////////

static void _vkCreateImageForBuffer(vkcontext_rawptr_t ctxVK, //
                                    vkrtbufimpl_ptr_t bufferimpl,
                                    uint64_t usage ) { //
    VkImageCreateInfo imginf = {};
    initializeVkStruct(imginf, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
    imginf.imageType = VK_IMAGE_TYPE_2D; // Regular 2D texture
    imginf.format = bufferimpl->_vkfmt; 
    imginf.extent.width = bufferimpl->_parent->_width;
    imginf.extent.height = bufferimpl->_parent->_height;
    imginf.extent.depth = 1;
    imginf.mipLevels = 1; 
    imginf.arrayLayers = 1; 
    imginf.samples = VK_SAMPLE_COUNT_1_BIT; 
    imginf.tiling = VK_IMAGE_TILING_OPTIMAL; 
    imginf.usage = VK_IMAGE_USAGE_SAMPLED_BIT //
                 | VK_IMAGE_USAGE_TRANSFER_DST_BIT // Use as texture and allow data transfer to it
                 ;
    imginf.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imginf.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ///////////////////////////////////////////////////
    VkResult OK = vkCreateImage(ctxVK->_vkdevice, 
                                &imginf, 
                                nullptr, 
                                &bufferimpl->_vkimg);
    OrkAssert(OK == VK_SUCCESS);
    ///////////////////////////////////////////////////
    bufferimpl->_vkmemflags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                            //| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // do not need flush...
    ///////////////////////////////////////////////////
    VkMemoryRequirements memRequirements;
    initializeVkStruct(memRequirements);
    vkGetImageMemoryRequirements(ctxVK->_vkdevice, 
                                 bufferimpl->_vkimg, 
                                 &memRequirements);
    ///////////////////////////////////////////////////
    VkMemoryAllocateInfo allocInfo = {};
    initializeVkStruct(allocInfo, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = ctxVK->_findMemoryType(memRequirements.memoryTypeBits, //
                                                            bufferimpl->_vkmemflags);
    ///////////////////////////////////////////////////
    VkDeviceMemory imageMemory;
    initializeVkStruct(bufferimpl->_vkmem);
    OK = vkAllocateMemory(ctxVK->_vkdevice, //
                          &allocInfo, //
                          nullptr, //
                          &bufferimpl->_vkmem);
    OrkAssert(OK == VK_SUCCESS);
    ///////////////////////////////////////////////////
    vkBindImageMemory( ctxVK->_vkdevice, //
                       bufferimpl->_vkimg, //
                       bufferimpl->_vkmem, 0);
    ///////////////////////////////////////////////////
    VkImageViewCreateInfo viewInfo = {};
    initializeVkStruct(viewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    viewInfo.image = bufferimpl->_vkimg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // This is typical for a color buffer. Other types include 3D, cube maps, etc.
    viewInfo.format = bufferimpl->_vkfmt; // This is a typical format. Adjust as needed.
    viewInfo.subresourceRange.aspectMask = _vkFormatConverter.aspectForUsage(usage);
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    OK = vkCreateImageView(ctxVK->_vkdevice, 
                           &viewInfo, 
                           nullptr, 
                           &bufferimpl->_vkimgview);
    OrkAssert(OK == VK_SUCCESS);
    ///////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

vkrtgrpimpl_ptr_t VkFrameBufferInterface::_createRtGroupImpl(RtGroup* rtgroup) {
  vkrtgrpimpl_ptr_t RTGIMPL = std::make_shared<VkRtGroupImpl>(rtgroup);
  RTGIMPL->_width           = rtgroup->width();
  RTGIMPL->_height          = rtgroup->height();
  int inumtargets = rtgroup->GetNumTargets();
  int w = rtgroup->width();
  int h = rtgroup->height();
  if (rtgroup->_depthBuffer) {
    auto rtbuffer = rtgroup->_depthBuffer;
    auto bufferimpl = std::make_shared<VklRtBufferImpl>(RTGIMPL.get(), rtbuffer.get());
    rtbuffer->_impl.setShared<VklRtBufferImpl>(bufferimpl);
    bufferimpl->_vkfmt = _vkFormatConverter.convertBufferFormat(rtbuffer->mFormat);

    uint64_t USAGE = "depth"_crcu;
    _vkCreateImageForBuffer(_contextVK, bufferimpl, USAGE);

    auto& adesc = bufferimpl->_attachmentDesc;
    initializeVkStruct(adesc);
    adesc.format = bufferimpl->_vkfmt;
    adesc.samples = VK_SAMPLE_COUNT_1_BIT;
    adesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    adesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    adesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    adesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    adesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    adesc.finalLayout = _vkFormatConverter.layoutForUsage(USAGE);

  }
  // other buffers
  for (int it = 0; it < inumtargets; it++) {
    rtbuffer_ptr_t rtbuffer = rtgroup->GetMrt(it);
    auto bufferimpl = std::make_shared<VklRtBufferImpl>(RTGIMPL.get(), rtbuffer.get());
    rtbuffer->_impl.setShared<VklRtBufferImpl>(bufferimpl);
    bufferimpl->_vkfmt = _vkFormatConverter.convertBufferFormat(rtbuffer->mFormat);
    uint64_t USAGE = "color"_crcu;
    if(rtbuffer->_usage != 0){
      USAGE = rtbuffer->_usage;
    }
    _vkCreateImageForBuffer(_contextVK, bufferimpl, USAGE);
    auto& adesc = bufferimpl->_attachmentDesc;
    initializeVkStruct(adesc);
    adesc.format = bufferimpl->_vkfmt;
    adesc.samples = VK_SAMPLE_COUNT_1_BIT;
    adesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    adesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    adesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    adesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    adesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    adesc.finalLayout = _vkFormatConverter.layoutForUsage(USAGE);
    ///////////////////////////////////////////////////
  }

  return RTGIMPL;
}

///////////////////////////////////////////////////////////////////////////////

RtGroup* VkFrameBufferInterface::_popRtGroup() {
  auto rtb0 = _active_rtgroup->mMrt[0];
  if(0)printf( "poprtg rtb<%s> usage<%08x>\n", //
          rtb0->_debugName.c_str(), //
          rtb0->_usage);
  if(rtb0->_usage == "present"_crcu){
    //OrkAssert(false);
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
    //glDepthRange(0.0, 1.0f);
  }
  ViewportRect extents(fx, fy, fw, fh);
  pushViewport(extents);
  pushScissor(extents);
  if(rtgroup and rtgroup->_autoclear) {
    rtGroupClear(rtgroup);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_present(){
  OrkAssert(_main_rtb_color->_usage == "present"_crcu);

}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_pushRtGroup(RtGroup* rtgroup) {
  auto prev_rtgroup = _active_rtgroup;
  if (nullptr == rtgroup) {
    _setAsRenderTarget();
  } else {
    _active_rtgroup = rtgroup;
  }
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
  auto cmdbuf = _contextVK->_cmdbufcurframe_gfx_pri;
  auto rtgimpl = _active_rtgroup->_impl.getShared<VkRtGroupImpl>();
  int inumtargets = _active_rtgroup->GetNumTargets();
  int w = _active_rtgroup->width();
  int h = _active_rtgroup->height();
  printf( "clearing rtg<%p> w<%d> h<%d>\n", (void*) rtgimpl.get(), w, h );
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_clearDepth(float fdepth) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
