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
  //logchan_fbi->log("_doBeginFrame()");
  OrkAssert(_contextVK->_is_visual_frame);
  if (_swapchain) {
    _swapchain->_update();
  }
  _active_rtgroup = _main_rtg.get();
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_doEndFrame() {
  //logchan_fbi->log("_doEndFrame()");
}

///////////////////////////////////////////////////////

captureasync_ptr_t VkFrameBufferInterface::capture(const RtBuffer* inpbuf, const file::Path& pth) {
  // For now, return a simple future that completes after one frame
  // The actual GPU transfer happens in captureAsFormat which records the commands
  // After endFrame submits the command buffer, the data will be available
  
  auto future = std::make_shared<CaptureAsync>();
  future->_width = inpbuf->_width;
  future->_height = inpbuf->_height;
  future->_format = EBufferFormat::RGBA8;
  
  // Capture to a buffer (this records the GPU commands)
  auto capbuf = std::make_shared<CaptureBuffer>();
  if (!captureAsFormat(inpbuf, capbuf.get(), EBufferFormat::RGBA8)) {
    future->_failed = true;
    return future;
  }
  
  // Verify staging buffer was created
  if (!capbuf->_impl.isSet()) {
    future->_failed = true;
    return future;
  }
  
  // Store capture data in the future's implementation
  struct VulkanCaptureData {
    capturebuffer_ptr_t capture_buffer;
    file::Path path;
    int width;
    int height;
    bool frame_submitted = false;
  };
  
  auto capture_data = std::make_shared<VulkanCaptureData>();
  capture_data->capture_buffer = capbuf;
  capture_data->path = pth;
  capture_data->width = inpbuf->_width;
  capture_data->height = inpbuf->_height;
  
  future->_impl.setShared<VulkanCaptureData>(capture_data);
  
  // After one frame iteration, the command buffer will be submitted and executed
  // So we'll mark as ready after that
  _contextVK->_pending_capture = future;
  
  return future;
}

///////////////////////////////////////////////////////

bool VkFrameBufferInterface::captureToTexture(const CaptureBuffer& capbuf, Texture& tex) {
  OrkAssert(false);
  return false;
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::GetPixel(const fvec4& rAt, PixelFetchContext& ctx) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::rtGroupClear(RtGroup* rtg) {
  //OrkAssert(false);
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
