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

static void _vkCreateImageForBuffer(vkcontext_rawptr_t ctxVK, //
                                    vkrtbufimpl_ptr_t bufferimpl) { //
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
                 | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // Use as texture and allow data transfer to it
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
}

///////////////////////////////////////////////////////////////////////////////

static struct VkFormatConverter{

    VkFormatConverter(){
        _fmtmap[EBufferFormat::RGBA8] = VK_FORMAT_R8G8B8A8_UNORM;
        _fmtmap[EBufferFormat::Z32] = VK_FORMAT_D32_SFLOAT;
    }
    VkFormat convert(EBufferFormat fmt_in) const {
        auto it = _fmtmap.find(fmt_in);
        OrkAssert(it!=_fmtmap.end());
        return it->second;
    }
    std::unordered_map<EBufferFormat,VkFormat> _fmtmap;
};

///////////////////////////////////////////////////////////////////////////////

static const VkFormatConverter _vkFormatConverter;

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
    bufferimpl->_vkfmt = _vkFormatConverter.convert(rtbuffer->mFormat);
    _vkCreateImageForBuffer(_contextVK, bufferimpl);
  }
  // other buffers
  for (int it = 0; it < inumtargets; it++) {
    rtbuffer_ptr_t rtbuffer = rtgroup->GetMrt(it);
    auto bufferimpl = std::make_shared<VklRtBufferImpl>(RTGIMPL.get(), rtbuffer.get());
    rtbuffer->_impl.setShared<VklRtBufferImpl>(bufferimpl);
    bufferimpl->_vkfmt = _vkFormatConverter.convert(rtbuffer->mFormat);
    _vkCreateImageForBuffer(_contextVK, bufferimpl);

    ///////////////////////////////////////////////////
  }

  return RTGIMPL;
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::SetRtGroup(RtGroup* rtgroup) {
  auto prev_rtgroup = _active_rtgroup;
  if (nullptr == rtgroup) {
    _setAsRenderTarget();
  } else {
    _active_rtgroup = rtgroup;
  }
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
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
