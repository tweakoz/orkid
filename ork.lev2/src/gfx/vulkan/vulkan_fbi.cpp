////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"

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
  // Use captureAsFormat to get the data
  CaptureBuffer capbuf;
  if (!captureAsFormat(inpbuf, &capbuf, EBufferFormat::RGBA8)) {
    return nullptr;
  }

  int iw = capbuf.width();
  int ih = capbuf.height();
  
  printf("VkFrameBufferInterface::capture pth<%s> BUFW<%d> BUFH<%d>\n", pth.c_str(), iw, ih);

  // Flip the image vertically (Vulkan has Y pointing down, while most image formats have Y pointing up)
  auto outbuf = (uint8_t*)malloc(iw * ih * 4);
  auto srcdata = (uint8_t*)capbuf._data;
  
  for (int iy = 0; iy < ih; iy++) {
    for (int ix = 0; ix < iw; ix++) {
      int src_pixel = iy * iw + ix;
      int dst_pixel = (ih - 1 - iy) * iw + ix;
      
      int src_byte = src_pixel * 4;
      int dst_byte = dst_pixel * 4;
      
      outbuf[dst_byte + 0] = srcdata[src_byte + 0]; // R
      outbuf[dst_byte + 1] = srcdata[src_byte + 1]; // G
      outbuf[dst_byte + 2] = srcdata[src_byte + 2]; // B
      outbuf[dst_byte + 3] = srcdata[src_byte + 3]; // A
    }
  }

#if defined(USE_OIIO)
  auto out = ImageOutput::create(pth.c_str());
  if (!out) {
    free(outbuf);
    return nullptr;
  }
  ImageSpec spec(iw, ih, 4, TypeDesc::UINT8);
  out->open(pth.c_str(), spec);
  out->write_image(TypeDesc::UINT8, outbuf);
  out->close();
#endif

  free(outbuf);
  
  // TODO: Return a CaptureAsync future instead
  return nullptr;
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
