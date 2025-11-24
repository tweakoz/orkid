////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include "vulkan_captureasync.h"

#define USE_OIIO
#if defined(USE_OIIO)
#include <OpenImageIO/imageio.h>
OIIO_NAMESPACE_USING
#endif

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_fbi = logger()->configureChannel("VKFBI", fvec3(0.8, 0.2, 0.5), true);


VkMsaaState::VkMsaaState(){
  initializeVkStruct(_VKSTATE, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  _VKSTATE.sampleShadingEnable = VK_FALSE; // Enable/Disable sample shading
  _VKSTATE.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // No multisampling
  _VKSTATE.minSampleShading = 1.0f; // Minimum fraction for sample shading; closer to 1 is smoother
  _VKSTATE.pSampleMask = nullptr; // Optional
  _VKSTATE.alphaToCoverageEnable = VK_FALSE; // Enable/Disable alpha to coverage
  _VKSTATE.alphaToOneEnable = VK_FALSE; // Enable/Disable alpha to one

  _pipeline_bits = 0;
}

VkFrameBufferInterface::VkFrameBufferInterface(vkcontext_rawptr_t ctx)
    : FrameBufferInterface(*ctx)
    , _contextVK(ctx) {
}

///////////////////////////////////////////////////////

VkFrameBufferInterface::~VkFrameBufferInterface() {
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_setViewport(int iX, int iY, int iW, int iH) {

  auto tracker = std::make_shared<VkViewportTracker>();
  tracker->_width = iW;
  tracker->_height = iH;
  tracker->_x = iX;
  tracker->_y = iY;
  _viewportTracker = tracker;
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_setScissor(int iX, int iY, int iW, int iH) {
   auto tracker = std::make_shared<VkViewportTracker>();
  tracker->_width = iW;
  tracker->_height = iH;
  tracker->_x = iX;
  tracker->_y = iY;
   _scissorTracker = tracker;
}

///////////////////////////////////////////////////////
void VkFrameBufferInterface::_doBeginFrame() {
  if (_swapchain) {
    _swapchain->_update();
  }
#if defined(__linux__)
  else if (_swapchain_drm) {
    logchan_fbi->log("DRM: _doBeginFrame - calling acquireImage and enqueueFrame");
    _swapchain_drm->acquireImage(_contextVK);
    _swapchain_drm->enqueueFrame(_contextVK);
  }
#endif
  _ensureMainRtg().get();    // ensure main rtgroup is created
  _active_rtgroup = nullptr; // ensure main rtgroup is pushed on first use
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_doEndFrame() {
  //logchan_fbi->log("_doEndFrame()");
#if defined(__linux__)
  if (_swapchain_drm) {
    logchan_fbi->log("DRM: _doEndFrame - calling waitPresentFrame");
    _swapchain_drm->waitPresentFrame(_contextVK);
  }
#endif
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::GetPixel(const fvec4& rAt, PixelFetchContext& ctx) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::rtGroupClear(RtGroup* rtg) {
  _contextVK->debugPushGroup("VkFBI::rtGroupClear",fvec4(1,0,0,0));
  _pushRtGroup(rtg);
  _popRtGroup();
  _contextVK->debugPopGroup();
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::rtGroupMipGen(RtGroup* rtg) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::msaaBlit(rtgroup_ptr_t src, rtgroup_ptr_t dst) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::blit(rtgroup_ptr_t src, rtgroup_ptr_t dst) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////
void VkFrameBufferInterface::downsample2x2(rtgroup_ptr_t src, rtgroup_ptr_t dst) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_initializeContext(DisplayBuffer* pBuf){
  OrkAssert(false);
}

///////////////////////////////////////////////////////

freestyle_mtl_ptr_t VkFrameBufferInterface::utilshader() {
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
