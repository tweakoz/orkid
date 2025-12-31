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
  static int frame_log_count = 0;

  if (_swapchain) {
    _swapchain->_update();
  }
#if defined(__linux__)
  else if (_swapchain_drm) {
    if (frame_log_count < 10) {
      logchan_fbi->log("DRM: _doBeginFrame[%d] - calling acquireImage", frame_log_count);
    }
    _swapchain_drm->acquireImage(_contextVK);
    // NOTE: enqueueFrame() called later from vulkan_ctx.cpp after rendering
  }
#endif
  _ensureMainRtg().get();    // ensure main rtgroup is created
  _active_rtgroup = nullptr; // ensure main rtgroup is pushed on first use

  if (frame_log_count < 10) {
    frame_log_count++;
  }
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_doEndFrame() {
  // NOTE: DRM's waitPresentFrame is called in _doSubmitPrimaryCommandBuffer,
  // not here, to match GLFW flow
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
  OrkAssert(src != nullptr);
  OrkAssert(dst != nullptr);
  OrkAssert(src->mNumMrts > 0);
  OrkAssert(dst->mNumMrts > 0);

  int src_w = src->width();
  int src_h = src->height();
  int dst_w = dst->width();
  int dst_h = dst->height();

  // Resize destination to half of source if needed
  if (dst_w != src_w / 2 || dst_h != src_h / 2) {
    dst->Resize(src_w / 2, src_h / 2);
    dst_w = dst->width();
    dst_h = dst->height();
  }

  // Ensure source rtgroup has its Vulkan impl initialized
  if (!src->_impl.isShared<VkRtGroupImpl>()) {
    _createRtGroupImpl(src.get());
  }

  // Ensure destination rtgroup has its Vulkan impl initialized
  if (!dst->_impl.isShared<VkRtGroupImpl>()) {
    _createRtGroupImpl(dst.get());
  }

  auto src_buf = src->buffer(0);
  auto dst_buf = dst->buffer(0);
  OrkAssert(src_buf != nullptr);
  OrkAssert(dst_buf != nullptr);

  auto src_impl = src_buf->_impl.getShared<VklRtBufferImpl>();
  auto dst_impl = dst_buf->_impl.getShared<VklRtBufferImpl>();
  OrkAssert(src_impl != nullptr);
  OrkAssert(dst_impl != nullptr);
  OrkAssert(src_impl->_imgobj != nullptr);
  OrkAssert(dst_impl->_imgobj != nullptr);

  VkImage src_image = src_impl->_imgobj->_vkimage;
  VkImage dst_image = dst_impl->_imgobj->_vkimage;

  // Suspend render pass if active
  bool was_active = _contextVK->_renderPassActive;
  if (was_active) {
    _contextVK->suspendRenderPass();
  }

  auto cmdbuf = _contextVK->beginRecordCommandBuffer("VkFBI::downsample2x2");
  auto cmdbuf_impl = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_cmdbuf = cmdbuf_impl->_vkcmdbuf;

  // Transition src to TRANSFER_SRC_OPTIMAL
  auto src_barrier = createImageBarrier(
      src_image,
      src_impl->_currentLayout,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_ACCESS_TRANSFER_READ_BIT);
  src_barrier->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      0, 0, nullptr, 0, nullptr, 1, src_barrier.get());

  // Transition dst to TRANSFER_DST_OPTIMAL
  auto dst_barrier = createImageBarrier(
      dst_image,
      dst_impl->_currentLayout,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VkAccessFlagBits(0),
      VK_ACCESS_TRANSFER_WRITE_BIT);
  dst_barrier->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      0, 0, nullptr, 0, nullptr, 1, dst_barrier.get());

  // Blit from src to dst with linear filtering (downsampling)
  VkImageBlit blit{};
  blit.srcOffsets[0] = {0, 0, 0};
  blit.srcOffsets[1] = {src_w, src_h, 1};
  blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit.srcSubresource.mipLevel = 0;
  blit.srcSubresource.baseArrayLayer = 0;
  blit.srcSubresource.layerCount = 1;
  blit.dstOffsets[0] = {0, 0, 0};
  blit.dstOffsets[1] = {dst_w, dst_h, 1};
  blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit.dstSubresource.mipLevel = 0;
  blit.dstSubresource.baseArrayLayer = 0;
  blit.dstSubresource.layerCount = 1;

  vkCmdBlitImage(
      vk_cmdbuf,
      src_image,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      dst_image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &blit,
      VK_FILTER_LINEAR);

  // Transition src back to SHADER_READ_ONLY_OPTIMAL (for sampling)
  auto src_barrier2 = createImageBarrier(
      src_image,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_ACCESS_SHADER_READ_BIT);
  src_barrier2->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      0, 0, nullptr, 0, nullptr, 1, src_barrier2.get());
  src_impl->_currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  if (src_impl->_imgobj) {
    src_impl->_imgobj->_currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  // Transition dst to SHADER_READ_ONLY_OPTIMAL (ready for sampling)
  auto dst_barrier2 = createImageBarrier(
      dst_image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_SHADER_READ_BIT);
  dst_barrier2->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      0, 0, nullptr, 0, nullptr, 1, dst_barrier2.get());
  dst_impl->_currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  if (dst_impl->_imgobj) {
    dst_impl->_imgobj->_currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  _contextVK->endRecordCommandBuffer(cmdbuf);
  _contextVK->enqueueSecondaryCommandBuffer(cmdbuf);

  // Resume render pass if it was active
  if (was_active) {
    _contextVK->resumeRenderPass();
  }
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
