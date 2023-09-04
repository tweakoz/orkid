////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkFrameBufferInterface::VkFrameBufferInterface(vkcontext_rawptr_t ctx)
    : FrameBufferInterface(*ctx)
    , _contextVK(ctx) {

  _main_rtg = std::make_shared<RtGroup>(ctx, 8, 8, MsaaSamples::MSAA_1X);
  _main_rtb_color = _main_rtg->createRenderTarget(EBufferFormat::RGBA8,"present"_crcu);
  //_main_rtb_depth = _main_rtg->createRenderTarget(EBufferFormat::Z32);
}

///////////////////////////////////////////////////////

VkFrameBufferInterface::~VkFrameBufferInterface() {
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_setViewport(int iX, int iY, int iW, int iH) {
  //OrkAssert(false);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_setScissor(int iX, int iY, int iW, int iH) {
  //OrkAssert(false);
}

///////////////////////////////////////////////////////
void VkFrameBufferInterface::_doBeginFrame() {
  RtGroup* rtg = _currentRtGroup;
  _pushRtGroup(rtg);
  if (_main_rtg and (rtg == nullptr)) {
    rtg = _main_rtg.get();
  }
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::_doEndFrame() {
  popViewport();
  popScissor();
  //OrkAssert(false);
}

///////////////////////////////////////////////////////

void VkFrameBufferInterface::capture(const RtBuffer* inpbuf, const file::Path& pth) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////

bool VkFrameBufferInterface::captureToTexture(const CaptureBuffer& capbuf, Texture& tex) {
  OrkAssert(false);
  return false;
}

///////////////////////////////////////////////////////

bool VkFrameBufferInterface::captureAsFormat(const RtBuffer* inpbuf, CaptureBuffer* buffer, EBufferFormat destfmt) {
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
