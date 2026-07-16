////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// VR composite blit (OPENXR X3): copy the engine's wide two-eye texture into the
// XR runtime's swapchain VkImage via a ONE-SHOT sync CB (submit + WAIT). The XR
// device orchestrates the frame (acquire/wait/release/EndFrame); this backend
// function owns the GPU copy + the sRGB-OETF-exactly-once colorspace contract.
//
// Verification: mac has NO XR runtime, so this is UNVERIFIED-BY-DESIGN here — it is
// reachable only from OpenXrDevice::__composite on a real runtime. Correctness is by
// inspection + the fleet gate (Monado / the owner's private box).
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/vr_composite.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/kernel/timer.h>
#include <ork/util/logger.h>
#include "headers/vulkan_ctx.h"
#include "headers/vk_image.h"
#include "headers/vk_protos.h"
#include "../shadlang/shadlang_backend_spirv.h"
#include <cstdlib>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
////////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_vrcomp = logger()->configureChannel("VRCOMPOSITE", fvec3(0.9, 0.5, 0.2), false);

// Reverse-Z opaque floor pushed into the depth-conversion FS. Default 1/255 (~0.4% of far):
//  large enough to clear the runtime's TESSELATION vertex-stage coverage/anchor quantization
//  (distant terrain was composited BLACK at the 1/65535 floor) yet negligible parallax on sky.
//  Override via ORKID_XR_DEPTH_FLOOR (must mirror openxr_device.cpp's CPU kRevZFloor).
static float vrRevZFloor() {
  static float s_floor = [] {
    if (const char* e = getenv("ORKID_XR_DEPTH_FLOOR"); e and e[0]) {
      float v = float(atof(e));
      if (v > 0.0f and v < 1.0f)
        return v;
    }
    return 1.0f / 255.0f;
  }();
  return s_floor;
}

// Cached SRGB intermediate for the UNORM gamma-encode route. Single XR session per
// process → a file-scope cache keyed on extent+context is sufficient; the
// VulkanImageObject dtor funnels through the context's destroy path and no-ops past
// gpu shutdown, so process-exit teardown is safe.
static vkimageobj_ptr_t s_gammaIntermediate;
static uint32_t s_gi_w = 0, s_gi_h = 0;
static VkContext* s_gi_ctx = nullptr;

////////////////////////////////////////////////////////////////////////////////

double vrCompositeBlitToXrImage(
    Context* ctx_base,
    Texture* src,
    uint64_t dstImage64,
    int64_t dstFmt64,
    uint32_t w,
    uint32_t h,
    bool needsGammaEncode,
    uint32_t dstX,
    uint32_t dstY,
    bool dstDiscard,
    bool flipV) {

  auto ctxVK = static_cast<VkContext*>(ctx_base);
  if (not ctxVK or not src)
    return -1.0;
  VkImage dstImage = (VkImage)dstImage64;
  if (dstImage == VK_NULL_HANDLE)
    return -1.0;

  // Source VkImage + tracked layout (the wide two-eye RTG, sampled → SHADER_READ).
  auto try_vktex = src->_impl.tryAsShared<VulkanTextureObject>();
  if (not try_vktex)
    return -1.0;
  auto vktex     = try_vktex.value();
  auto srcImgObj = vktex->samplingImage();
  if (not srcImgObj or srcImgObj->_vkimage == VK_NULL_HANDLE)
    return -1.0;
  VkImage srcImage        = srcImgObj->_vkimage;
  VkImageLayout srcLayout = srcImgObj->_currentLayout;
  VkImageLayout srcRestore =
      (srcLayout == VK_IMAGE_LAYOUT_UNDEFINED) ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : srcLayout;

  // One-shot primary CB from the graphics pool (same thread as the frame recorder,
  // so no pool concurrency; independent of the frame CB and its render-pass state).
  VkCommandBufferAllocateInfo cbai{};
  initializeVkStruct(cbai, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
  cbai.commandPool        = ctxVK->_vkcmdpool_graphics;
  cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbai.commandBufferCount = 1;
  VkCommandBuffer cb      = VK_NULL_HANDLE;
  if (VK_SUCCESS != vkAllocateCommandBuffers(ctxVK->_vkdevice, &cbai, &cb))
    return -1.0;

  VkCommandBufferBeginInfo bi{};
  initializeVkStruct(bi, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cb, &bi);

  auto barrier = [&](VkImage img,
                     VkImageLayout oldL,
                     VkImageLayout newL,
                     VkAccessFlags srcA,
                     VkAccessFlags dstA,
                     VkPipelineStageFlags srcS,
                     VkPipelineStageFlags dstS) {
    VkImageMemoryBarrier b{};
    initializeVkStruct(b, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
    b.srcAccessMask       = srcA;
    b.dstAccessMask       = dstA;
    b.oldLayout           = oldL;
    b.newLayout           = newL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image               = img;
    b.subresourceRange    = vkColorSubresource();
    vkCmdPipelineBarrier(cb, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
  };

  // Blit the full w×h source into the destination at (dx,dy). The wide path passes
  //  (0,0) (whole-image copy); the per-eye stereo path passes (0,0) then (eyeW,0) to
  //  place each eye into its half of the ONE wide swapchain image.
  //  flipV inverts the SOURCE V axis (top row ↔ bottom row) at 1:1 scale — this is the
  //  ONLY stage that can flip in the gamma-encode route (its second stage is a
  //  vkCmdCopyImage that cannot), so both routes flip HERE.
  auto blitRegion = [&](VkImage from, VkImage to, int32_t dx, int32_t dy) {
    VkImageBlit blit{};
    blit.srcSubresource = VkColorSubresourceLayers{};
    blit.srcOffsets[0]  = {0, flipV ? (int32_t)h : 0, 0};
    blit.srcOffsets[1]  = {(int32_t)w, flipV ? 0 : (int32_t)h, 1};
    blit.dstSubresource = VkColorSubresourceLayers{};
    blit.dstOffsets[0]  = {dx, dy, 0};
    blit.dstOffsets[1]  = {dx + (int32_t)w, dy + (int32_t)h, 1};
    // 1:1 extent → NEAREST (no resample); the OETF is a format-store conversion,
    // not a filter, so filtering is irrelevant here.
    vkCmdBlitImage(
        cb, from, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, to, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
  };

  // Destination start-state: a whole-image write DISCARDS prior contents (oldLayout
  //  UNDEFINED); a partial write that must PRESERVE the other half (the second per-eye
  //  stereo blit) transitions FROM the release layout (COLOR_ATTACHMENT_OPTIMAL) so the
  //  already-written half survives.
  const VkImageLayout dstOldLayout =
      dstDiscard ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  const VkAccessFlags dstOldAccess = dstDiscard ? 0 : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  const VkPipelineStageFlags dstOldStage =
      dstDiscard ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  // src → TRANSFER_SRC
  barrier(
      srcImage,
      srcLayout,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_ACCESS_SHADER_READ_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT);

  if (not needsGammaEncode) {
    // SRGB swapchain: the pure blit — the hardware applies the sRGB OETF on store
    // into the SRGB image (encode exactly once).
    barrier(
        dstImage,
        dstOldLayout, // whole-image overwrite discards; per-eye 2nd blit preserves
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dstOldAccess,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        dstOldStage,
        VK_PIPELINE_STAGE_TRANSFER_BIT);
    blitRegion(srcImage, dstImage, (int32_t)dstX, (int32_t)dstY);
    barrier(
        dstImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // XR_KHR_vulkan_enable release layout
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  } else {
    // UNORM swapchain: the runtime offers no sRGB view, so encode with TRANSFER ops
    // only (no graphics pipeline): blit linear src → SRGB intermediate (hardware
    // OETF on store), then raw-copy the encoded bytes into the UNORM swapchain
    // (copy-compatible formats → byte copy, no second conversion). Encode once.
    // The intermediate's SRGB channel order MUST match the swapchain's UNORM order,
    //  because the second step is a RAW byte copy (vkCmdCopyImage, no swizzle): a
    //  BGRA swapchain needs a BGRA_SRGB intermediate or R/B end up swapped. The
    //  linear->SRGB blit into it is value-based (R->R), so the OETF is applied
    //  correctly regardless of the source's channel order.
    VkFormat interFmt = VK_FORMAT_R8G8B8A8_SRGB;
    if ((VkFormat)dstFmt64 == VK_FORMAT_B8G8R8A8_UNORM)
      interFmt = VK_FORMAT_B8G8R8A8_SRGB;
    if ((not s_gammaIntermediate) or s_gi_w != w or s_gi_h != h or s_gi_ctx != ctxVK) {
      auto cinfo   = makeVKICI((int)w, (int)h, 1, interFmt, 1);
      cinfo->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      s_gammaIntermediate = std::make_shared<VulkanImageObject>(ctxVK, cinfo, "vr_gamma_intermediate");
      s_gammaIntermediate->_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      s_gi_w   = w;
      s_gi_h   = h;
      s_gi_ctx = ctxVK;
      logchan_vrcomp->log("created SRGB gamma-encode intermediate <%u x %u>", w, h);
    }
    VkImage interImage = s_gammaIntermediate->_vkimage;

    barrier(
        interImage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);
    // linear src → SRGB intermediate (origin): applies OETF once. The intermediate is
    //  sized to the source (w×h) so per-eye calls reuse one eyeW×eyeH intermediate.
    blitRegion(srcImage, interImage, 0, 0);
    barrier(
        interImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);
    barrier(
        dstImage,
        dstOldLayout, // whole-image overwrite discards; per-eye 2nd blit preserves
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dstOldAccess,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        dstOldStage,
        VK_PIPELINE_STAGE_TRANSFER_BIT);

    // raw byte copy: SRGB-encoded bytes → UNORM swapchain region (no conversion).
    VkImageCopy copy{};
    copy.srcSubresource = VkColorSubresourceLayers{};
    copy.srcOffset      = {0, 0, 0};
    copy.dstSubresource = VkColorSubresourceLayers{};
    copy.dstOffset      = {(int32_t)dstX, (int32_t)dstY, 0};
    copy.extent         = {w, h, 1};
    vkCmdCopyImage(
        cb,
        interImage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copy);

    barrier(
        dstImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    s_gammaIntermediate->_currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  }

  // restore src to its sampling layout so the engine keeps sampling it next frame.
  barrier(
      srcImage,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      srcRestore,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  srcImgObj->_currentLayout = srcRestore;

  vkEndCommandBuffer(cb);

  // submit + WAIT (one-shot sync — X3 v1). Submit via the thread-safe queue wrapper
  // (the exact queue XR is bound to) and block on a fence, then MEASURE.
  VkFenceCreateInfo fci{};
  initializeVkStruct(fci, VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
  VkFence fence = VK_NULL_HANDLE;
  vkCreateFence(ctxVK->_vkdevice, &fci, nullptr, &fence);

  VkSubmitInfo si{};
  initializeVkStruct(si, VK_STRUCTURE_TYPE_SUBMIT_INFO);
  si.commandBufferCount = 1;
  si.pCommandBuffers    = &cb;

  ork::Timer timer;
  timer.Start();
  ctxVK->_gfxqueue->queueSubmit(&si, fence);
  vkWaitForFences(ctxVK->_vkdevice, 1, &fence, VK_TRUE, UINT64_MAX);
  double elapsed = timer.SecsSinceStart();

  vkDestroyFence(ctxVK->_vkdevice, fence, nullptr);
  vkFreeCommandBuffers(ctxVK->_vkdevice, ctxVK->_vkcmdpool_graphics, 1, &cb);

  return elapsed;
}

////////////////////////////////////////////////////////////////////////////////
// vrCaptureEyeDepth (OPENXR X3, depth) — PRESERVE one eye's scene depth before the sibling
// eye overwrites the shared forward-render depth. Both DualMonoVr eyes render through ONE
// forward RTG (per-viewport _bufferKey), so at composite time only the last-rendered eye's
// depth survives. This captures each eye's depth (a straight same-format vkCmdCopyImage) into
// a caller-owned samplable device-local depth texture, recorded IN-FRAME (secondary CB,
// executed within this frame's primary CB — ordered AFTER the eye's forward render and BEFORE
// the sibling eye's, so it reads the correct per-eye depth; a separate submit+wait would race
// the not-yet-submitted render). dstDepth is lazily (re)built to match the source
// extent+format and left in SHADER_READ so the depth-conversion pass below can sample it.
// Returns false on any unavailable precondition (caller skips the depth layer for that frame).
////////////////////////////////////////////////////////////////////////////////

static void _ensureSamplableDepthTexture(
    VkContext* ctxVK, texture_ptr_t& dst, uint32_t w, uint32_t h, VkFormat vkfmt) {
  if (dst) {
    if (auto ex = dst->_impl.tryAsShared<VulkanTextureObject>()) {
      auto so = ex.value()->samplingImage();
      if (so and so->_format == vkfmt and uint32_t(dst->_width) == w and uint32_t(dst->_height) == h)
        return; // still valid — reuse
    }
  }
  auto tex        = std::make_shared<Texture>();
  tex->_width     = int(w);
  tex->_height    = int(h);
  tex->_texType   = ETEXTYPE_2D;
  tex->_debugName = "vr_depth_capture";
  tex->_source    = ETextureSource::FROM_RTG;
  auto vk_tex     = tex->_impl.makeShared<VulkanTextureObject>(ctxVK->_txi.get());

  auto ici = makeVKICI(int(w), int(h), 1, vkfmt, 1);
  // TRANSFER_SRC so the ORKID_XR_DEPTH_DEBUG readback can copy the captured depth to a host
  //  buffer (the D16 swapchain image itself is DEPTH_STENCIL_ATTACHMENT-only — not readable).
  ici->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  vk_tex->_imgobj[0]                 = std::make_shared<VulkanImageObject>(ctxVK, ici, "vr_depth_capture");
  vk_tex->_imgobj[0]->_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vk_tex->_vksampler                 = ctxVK->_sampler_base;

  auto IVCI   = createImageViewInfo2D(vk_tex->_imgobj[0]->_vkimage, vkfmt, VK_IMAGE_ASPECT_DEPTH_BIT);
  VkResult ok = vkCreateImageView(ctxVK->_vkdevice, IVCI.get(), nullptr, &vk_tex->_imgobj[0]->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  vk_tex->_vkdescriptor_info[0]              = std::make_shared<VkDescriptorImageInfo>();
  vk_tex->_vkdescriptor_info[0]->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vk_tex->_vkdescriptor_info[0]->imageView   = vk_tex->_imgobj[0]->_vkimageview;
  vk_tex->_vkdescriptor_info[0]->sampler     = vk_tex->_vksampler->_vksampler;
  vk_tex->_descset_sampling                  = vk_tex->_vkdescriptor_info[0];
  vk_tex->_img_sampling                      = vk_tex->_imgobj[0];
  dst = tex;
}

bool vrCaptureEyeDepth(Context* ctx_base, Texture* srcDepth, texture_ptr_t& dstDepth) {
  auto ctxVK = static_cast<VkContext*>(ctx_base);
  if (not ctxVK or not srcDepth)
    return false;
  auto try_vktex = srcDepth->_impl.tryAsShared<VulkanTextureObject>();
  if (not try_vktex)
    return false;
  auto srcImgObj = try_vktex.value()->samplingImage();
  if (not srcImgObj or srcImgObj->_vkimage == VK_NULL_HANDLE)
    return false;
  VkImage srcImage        = srcImgObj->_vkimage;
  VkFormat srcFmt         = srcImgObj->_format;
  VkImageLayout srcLayout = srcImgObj->_currentLayout;
  uint32_t w              = uint32_t(srcDepth->_width);
  uint32_t h              = uint32_t(srcDepth->_height);
  if (w == 0 or h == 0 or srcFmt == VK_FORMAT_UNDEFINED)
    return false;

  _ensureSamplableDepthTexture(ctxVK, dstDepth, w, h, srcFmt);
  auto dstImgObj   = dstDepth->_impl.getShared<VulkanTextureObject>()->samplingImage();
  VkImage dstImage = dstImgObj->_vkimage;

  // In-frame secondary CB (barriers/copy are illegal inside a render pass — suspend/resume,
  //  mirroring _initTextureFromRtBuffer's in-frame RTG-texture GPU-op idiom).
  bool was_active = ctxVK->_renderPassActive;
  if (was_active)
    ctxVK->suspendRenderPass();
  auto cmdbuf      = ctxVK->beginRecordCommandBuffer("vrCaptureEyeDepth");
  auto cmdbuf_impl = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto cb          = cmdbuf_impl->_vkcmdbuf;

  const VkImageSubresourceRange depthRange{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
  auto depthBarrier = [&](VkImage img, VkImageLayout oldL, VkImageLayout newL, //
                          VkAccessFlags srcA, VkAccessFlags dstA,               //
                          VkPipelineStageFlags srcS, VkPipelineStageFlags dstS) {
    VkImageMemoryBarrier b{};
    initializeVkStruct(b, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
    b.srcAccessMask       = srcA;
    b.dstAccessMask       = dstA;
    b.oldLayout           = oldL;
    b.newLayout           = newL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image               = img;
    b.subresourceRange    = depthRange;
    vkCmdPipelineBarrier(cb, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
  };

  // Src depth was last a depth attachment or a sampled texture; restore it to the SAME layout
  //  the engine tracks so the RTG's next-frame transition sees no discontinuity.
  VkImageLayout srcRestore = (srcLayout == VK_IMAGE_LAYOUT_UNDEFINED) //
                                 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                 : srcLayout;
  bool restoreIsSampled    = (srcRestore == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  depthBarrier(
      srcImage, srcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
  depthBarrier(
      dstImage, dstImgObj->_currentLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, //
      0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkImageCopy copy{};
  copy.srcSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
  copy.srcOffset      = {0, 0, 0};
  copy.dstSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
  copy.dstOffset      = {0, 0, 0};
  copy.extent         = {w, h, 1};
  vkCmdCopyImage(
      cb, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  depthBarrier(
      dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, //
      VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  depthBarrier(
      srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcRestore, VK_ACCESS_TRANSFER_READ_BIT,
      restoreIsSampled ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      restoreIsSampled ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

  srcImgObj->_currentLayout = srcRestore;
  dstImgObj->_currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  ctxVK->endRecordCommandBuffer(cmdbuf);
  ctxVK->enqueueSecondaryCommandBuffer(cmdbuf);
  if (was_active)
    ctxVK->resumeRenderPass();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// vrCompositeDepthToXrImage — depth-only RENDER PASS converting one captured per-eye
// standard-Z depth texture into a w×h region at (dstX,dstY) of the runtime's wide D16 depth
// swapchain image (which carries DEPTH_STENCIL_ATTACHMENT usage ONLY — no transfer). A
// fullscreen triangle whose FS samples srcDepth (normalized UV, V-flipped per flipV — the
// SAME flipV the color path uses) and writes gl_FragDepth = clamp(1 - stdZ, 1/65535, 1)
// (reverse-Z with the opaque floor; mirrors stdZtoRevZ in openxr_device). Left eye (dstX==0)
// DISCARDS the fresh image; right eye (dstX==eyeW) transitions FROM the attachment layout so
// the just-written left half is PRESERVED (mirrors the color path's dstDiscard). Lazily-built
// pipeline (GLSL→SPIR-V via the engine's shaderc path — least machinery; the color path has no
// shaders). flipV is a PUSH CONSTANT (A8: parametric, never baked). Runs inside the caller's
// externalSubmitMutex region (one-shot submit+wait). Returns the submit+wait seconds, or <0 on
// any unavailable precondition — device then drops to color-only (one-shot [VRCOMPOSITE] warn).
////////////////////////////////////////////////////////////////////////////////

struct VrDepthConvPipeline {
  VkContext* _ctx            = nullptr;
  VkFormat _depthFmt         = VK_FORMAT_UNDEFINED;
  VkShaderModule _vs         = VK_NULL_HANDLE;
  VkShaderModule _fs         = VK_NULL_HANDLE;
  VkDescriptorSetLayout _dsl = VK_NULL_HANDLE;
  VkPipelineLayout _playout  = VK_NULL_HANDLE;
  VkPipeline _pipeline       = VK_NULL_HANDLE;
  bool _ready                = false;
};
static VrDepthConvPipeline s_depthconv;

static const char* kVrDepthVS = R"GLSL(
#version 450
layout(location=0) out vec2 vUV;
void main(){
  // fullscreen triangle: verts (0,0)->(2,0)->(0,2) in UV, clip = uv*2-1.
  vec2 uv = vec2((gl_VertexIndex == 1) ? 2.0 : 0.0,
                 (gl_VertexIndex == 2) ? 2.0 : 0.0);
  vUV = uv;
  gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

static const char* kVrDepthFS = R"GLSL(
#version 450
layout(location=0) in vec2 vUV;
layout(set=0, binding=0) uniform sampler2D SrcDepth;
// A8: flipV AND the opaque floor are parametric (host-tunable) — pushed, never baked.
layout(push_constant) uniform PC { int flipV; float revzFloor; } pc;
void main(){
  vec2 uv = vUV;
  if(pc.flipV != 0)
    uv.y = 1.0 - uv.y;
  float stdz = texture(SrcDepth, uv).r;                    // standard-Z window depth
  gl_FragDepth = clamp(1.0 - stdz, pc.revzFloor, 1.0);     // reverse-Z + opaque floor
}
)GLSL";

static bool _ensureDepthConvPipeline(VkContext* ctxVK, VkFormat depthFmt) {
  if (s_depthconv._ready and s_depthconv._ctx == ctxVK and s_depthconv._depthFmt == depthFmt)
    return true;
  if (s_depthconv._ready) {
    logchan_vrcomp->log("depth-conv pipeline ctx/format changed unexpectedly — refusing rebuild");
    return false;
  }
  namespace slspirv = ::ork::lev2::shadlang::spirv;
  auto vs_bin = slspirv::SpirvCompiler::compileGlslToSpirv("vr_depth_vs", kVrDepthVS, shaderc_glsl_vertex_shader);
  auto fs_bin = slspirv::SpirvCompiler::compileGlslToSpirv("vr_depth_fs", kVrDepthFS, shaderc_glsl_fragment_shader);
  if (vs_bin.empty() or fs_bin.empty())
    return false;

  auto mkmodule = [&](const std::vector<uint32_t>& bin) -> VkShaderModule {
    VkShaderModuleCreateInfo smci{};
    initializeVkStruct(smci, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    smci.codeSize    = bin.size() * sizeof(uint32_t);
    smci.pCode       = bin.data();
    VkShaderModule m = VK_NULL_HANDLE;
    if (VK_SUCCESS != vkCreateShaderModule(ctxVK->_vkdevice, &smci, nullptr, &m))
      return VK_NULL_HANDLE;
    return m;
  };
  s_depthconv._vs = mkmodule(vs_bin);
  s_depthconv._fs = mkmodule(fs_bin);
  if (s_depthconv._vs == VK_NULL_HANDLE or s_depthconv._fs == VK_NULL_HANDLE)
    return false;

  VkDescriptorSetLayoutBinding dslb{};
  dslb.binding         = 0;
  dslb.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  dslb.descriptorCount = 1;
  dslb.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo dslci{};
  initializeVkStruct(dslci, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
  dslci.bindingCount = 1;
  dslci.pBindings    = &dslb;
  if (VK_SUCCESS != vkCreateDescriptorSetLayout(ctxVK->_vkdevice, &dslci, nullptr, &s_depthconv._dsl))
    return false;

  VkPushConstantRange pcr{};
  pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  pcr.offset     = 0;
  pcr.size       = sizeof(int32_t) + sizeof(float); // { int flipV; float revzFloor; }
  VkPipelineLayoutCreateInfo plci{};
  initializeVkStruct(plci, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
  plci.setLayoutCount         = 1;
  plci.pSetLayouts            = &s_depthconv._dsl;
  plci.pushConstantRangeCount = 1;
  plci.pPushConstantRanges    = &pcr;
  if (VK_SUCCESS != vkCreatePipelineLayout(ctxVK->_vkdevice, &plci, nullptr, &s_depthconv._playout))
    return false;

  VkPipelineShaderStageCreateInfo stages[2];
  initializeVkStruct(stages[0], VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
  stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = s_depthconv._vs;
  stages[0].pName  = "main";
  initializeVkStruct(stages[1], VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
  stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = s_depthconv._fs;
  stages[1].pName  = "main";

  VkPipelineVertexInputStateCreateInfo vin{};
  initializeVkStruct(vin, VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);

  VkPipelineInputAssemblyStateCreateInfo ia{};
  initializeVkStruct(ia, VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo vp{};
  initializeVkStruct(vp, VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
  vp.viewportCount = 1;
  vp.scissorCount  = 1;

  VkPipelineRasterizationStateCreateInfo rs{};
  initializeVkStruct(rs, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode    = VK_CULL_MODE_NONE;
  rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.lineWidth   = 1.0f;

  VkPipelineMultisampleStateCreateInfo ms{};
  initializeVkStruct(ms, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo ds{};
  initializeVkStruct(ds, VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
  ds.depthTestEnable  = VK_TRUE;
  ds.depthWriteEnable = VK_TRUE;
  ds.depthCompareOp   = VK_COMPARE_OP_ALWAYS;

  VkPipelineColorBlendStateCreateInfo cbs{};
  initializeVkStruct(cbs, VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
  cbs.attachmentCount = 0;

  VkDynamicState dyns[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dyn{};
  initializeVkStruct(dyn, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
  dyn.dynamicStateCount = 2;
  dyn.pDynamicStates    = dyns;

  VkPipelineRenderingCreateInfo prci{};
  initializeVkStruct(prci, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);
  prci.colorAttachmentCount  = 0;
  prci.depthAttachmentFormat = depthFmt;

  VkGraphicsPipelineCreateInfo gpci{};
  initializeVkStruct(gpci, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
  gpci.pNext               = &prci;
  gpci.stageCount          = 2;
  gpci.pStages             = stages;
  gpci.pVertexInputState   = &vin;
  gpci.pInputAssemblyState = &ia;
  gpci.pViewportState      = &vp;
  gpci.pRasterizationState = &rs;
  gpci.pMultisampleState   = &ms;
  gpci.pDepthStencilState  = &ds;
  gpci.pColorBlendState    = &cbs;
  gpci.pDynamicState       = &dyn;
  gpci.layout              = s_depthconv._playout;
  if (VK_SUCCESS != vkCreateGraphicsPipelines(ctxVK->_vkdevice, VK_NULL_HANDLE, 1, &gpci, nullptr, &s_depthconv._pipeline))
    return false;

  s_depthconv._ctx      = ctxVK;
  s_depthconv._depthFmt = depthFmt;
  s_depthconv._ready    = true;
  logchan_vrcomp->log("depth-conv pipeline ready (depthFmt=%d)", int(depthFmt));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ORKID_XR_DEPTH_DEBUG=1 discriminator (one-shot). The D16 swapchain image is not readable
// (DEPTH_STENCIL_ATTACHMENT-only), so read back the CAPTURED source depth (standard-Z, made
// TRANSFER_SRC-capable in _ensureSamplableDepthTexture), then apply the SAME conversion the FS
// does to report the PUBLISHED reverse-Z distribution. Prints rawStdZ min/max/mean + an 8-bucket
// histogram of published revZ (bucket0 = background/far@floor, bucket7 = near geometry). Reading:
//   degenerate histogram (all one bucket / noise)  => CAPTURE garbage (layout/aspect/uninit).
//   near content high + background exactly at floor => conversion CORRECT (black => runtime
//                                                       coverage SEMANTICS; the raised floor helps).
////////////////////////////////////////////////////////////////////////////////

static void _debugReadbackDepth(VkContext* ctxVK, VulkanTextureObject* vktex, uint32_t w, uint32_t h) {
  auto imgobj = vktex ? vktex->samplingImage() : nullptr;
  if (not imgobj or imgobj->_vkimage == VK_NULL_HANDLE or w == 0 or h == 0)
    return;
  VkFormat fmt = imgobj->_format;
  bool isF32 = (fmt == VK_FORMAT_D32_SFLOAT) or (fmt == VK_FORMAT_D32_SFLOAT_S8_UINT);
  bool isU16 = (fmt == VK_FORMAT_D16_UNORM);
  if (not isF32 and not isU16) {
    logchan_vrcomp->log("[XR_DEPTH_DEBUG] captured depth format %d not float/unorm16 — readback skipped", int(fmt));
    return;
  }
  size_t bpt     = isF32 ? 4 : 2;
  size_t nTexels = size_t(w) * size_t(h);
  size_t bufsize = nTexels * bpt;
  auto staging   = std::make_shared<VulkanBuffer>(ctxVK, bufsize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, "vr_depth_debug");

  VkCommandBufferAllocateInfo cbai{};
  initializeVkStruct(cbai, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
  cbai.commandPool        = ctxVK->_vkcmdpool_graphics;
  cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbai.commandBufferCount = 1;
  VkCommandBuffer cb = VK_NULL_HANDLE;
  if (VK_SUCCESS != vkAllocateCommandBuffers(ctxVK->_vkdevice, &cbai, &cb))
    return;
  VkCommandBufferBeginInfo bi{};
  initializeVkStruct(bi, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cb, &bi);

  VkImageLayout cur = imgobj->_currentLayout;
  auto bar = [&](VkImageLayout oldL, VkImageLayout newL, VkAccessFlags sa, VkAccessFlags da, //
                 VkPipelineStageFlags ss, VkPipelineStageFlags ds) {
    VkImageMemoryBarrier b{};
    initializeVkStruct(b, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
    b.srcAccessMask       = sa;
    b.dstAccessMask       = da;
    b.oldLayout           = oldL;
    b.newLayout           = newL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image               = imgobj->_vkimage;
    b.subresourceRange    = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cb, ss, ds, 0, 0, nullptr, 0, nullptr, 1, &b);
  };
  bar(cur, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkBufferImageCopy region{};
  region.imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
  region.imageOffset      = {0, 0, 0};
  region.imageExtent      = {w, h, 1};
  vkCmdCopyImageToBuffer(cb, imgobj->_vkimage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging->_vkbuffer, 1, &region);
  bar(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cur, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  vkEndCommandBuffer(cb);

  VkFenceCreateInfo fci{};
  initializeVkStruct(fci, VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
  VkFence fence = VK_NULL_HANDLE;
  vkCreateFence(ctxVK->_vkdevice, &fci, nullptr, &fence);
  VkSubmitInfo si{};
  initializeVkStruct(si, VK_STRUCTURE_TYPE_SUBMIT_INFO);
  si.commandBufferCount = 1;
  si.pCommandBuffers    = &cb;
  ctxVK->_gfxqueue->queueSubmit(&si, fence);
  vkWaitForFences(ctxVK->_vkdevice, 1, &fence, VK_TRUE, UINT64_MAX);
  vkDestroyFence(ctxVK->_vkdevice, fence, nullptr);
  vkFreeCommandBuffers(ctxVK->_vkdevice, ctxVK->_vkcmdpool_graphics, 1, &cb);

  void* mapped = staging->map(0, bufsize, 0);
  if (not mapped) {
    logchan_vrcomp->log("[XR_DEPTH_DEBUG] staging map failed");
    return;
  }
  const float floor = vrRevZFloor();
  double rawmin = 1e9, rawmax = -1e9, rawsum = 0.0;
  int hist[8]   = {0};
  for (size_t i = 0; i < nTexels; i++) {
    float stdz = isF32 ? ((const float*)mapped)[i] : (float(((const uint16_t*)mapped)[i]) / 65535.0f);
    if (stdz < rawmin) rawmin = stdz;
    if (stdz > rawmax) rawmax = stdz;
    rawsum += stdz;
    float revz = 1.0f - stdz;
    if (revz < floor) revz = floor;
    if (revz > 1.0f) revz = 1.0f;
    int b = int(revz * 8.0f);
    if (b < 0) b = 0;
    if (b > 7) b = 7;
    hist[b]++;
  }
  staging->unmap();
  printf("[XR_DEPTH_DEBUG] captured %ux%u fmt=%d floor=%g | rawStdZ min=%.5f max=%.5f mean=%.5f\n",
         w, h, int(fmt), double(floor), rawmin, rawmax, rawsum / double(nTexels));
  printf("[XR_DEPTH_DEBUG] published revZ 8-bucket histogram [0..1] (n=%zu): "
         "[%d %d %d %d %d %d %d %d]  (b0=far/floor/background, b7=near geometry)\n",
         nTexels, hist[0], hist[1], hist[2], hist[3], hist[4], hist[5], hist[6], hist[7]);
  fflush(stdout);
}

double vrCompositeDepthToXrImage(
    Context* ctx_base,
    Texture* srcDepth,
    uint64_t dstDepthImage64,
    int64_t dstDepthFmt64,
    uint32_t w,
    uint32_t h,
    float nearZ,
    float farZ,
    uint32_t dstX,
    uint32_t dstY,
    bool flipV) {

  auto warnOnce = [](const char* why) {
    static bool s_warned = false;
    if (not s_warned) {
      s_warned = true;
      logchan_vrcomp->log("vrCompositeDepthToXrImage: %s — dropping depth layer to color-only.", why);
    }
  };

  auto ctxVK = static_cast<VkContext*>(ctx_base);
  if (not ctxVK or not srcDepth) {
    warnOnce("null context/source");
    return -1.0;
  }
  VkImage dstImage = (VkImage)dstDepthImage64;
  if (dstImage == VK_NULL_HANDLE) {
    warnOnce("null dst image");
    return -1.0;
  }
  VkFormat depthFmt = (VkFormat)dstDepthFmt64;

  auto try_vktex = srcDepth->_impl.tryAsShared<VulkanTextureObject>();
  if (not try_vktex) {
    warnOnce("source not a vulkan texture");
    return -1.0;
  }
  auto srcVkTex  = try_vktex.value();
  auto srcImgObj = srcVkTex->samplingImage();
  if (not srcImgObj or srcImgObj->_vkimage == VK_NULL_HANDLE or srcImgObj->_vkimageview == VK_NULL_HANDLE) {
    warnOnce("source has no sampleable image");
    return -1.0;
  }
  if (srcVkTex->_vksampler == nullptr or srcVkTex->_vksampler->_vksampler == VK_NULL_HANDLE) {
    warnOnce("source has no sampler");
    return -1.0;
  }
  if (not _ensureDepthConvPipeline(ctxVK, depthFmt)) {
    warnOnce("pipeline build failed");
    return -1.0;
  }

  const bool discard = (dstX == 0); // left eye discards the fresh image; right preserves left half

  // dst depth image view (depth aspect) for dynamic rendering — created + destroyed per call.
  auto dIVCI          = createImageViewInfo2D(dstImage, depthFmt, VK_IMAGE_ASPECT_DEPTH_BIT);
  VkImageView dstView = VK_NULL_HANDLE;
  if (VK_SUCCESS != vkCreateImageView(ctxVK->_vkdevice, dIVCI.get(), nullptr, &dstView)) {
    warnOnce("dst view create failed");
    return -1.0;
  }

  // per-call descriptor pool + set binding the source depth sampler.
  VkDescriptorPoolSize dps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
  VkDescriptorPoolCreateInfo dpci{};
  initializeVkStruct(dpci, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
  dpci.maxSets       = 1;
  dpci.poolSizeCount = 1;
  dpci.pPoolSizes    = &dps;
  VkDescriptorPool dpool = VK_NULL_HANDLE;
  if (VK_SUCCESS != vkCreateDescriptorPool(ctxVK->_vkdevice, &dpci, nullptr, &dpool)) {
    vkDestroyImageView(ctxVK->_vkdevice, dstView, nullptr);
    warnOnce("descriptor pool create failed");
    return -1.0;
  }
  VkDescriptorSetAllocateInfo dsai{};
  initializeVkStruct(dsai, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
  dsai.descriptorPool     = dpool;
  dsai.descriptorSetCount = 1;
  dsai.pSetLayouts        = &s_depthconv._dsl;
  VkDescriptorSet dset = VK_NULL_HANDLE;
  if (VK_SUCCESS != vkAllocateDescriptorSets(ctxVK->_vkdevice, &dsai, &dset)) {
    vkDestroyDescriptorPool(ctxVK->_vkdevice, dpool, nullptr);
    vkDestroyImageView(ctxVK->_vkdevice, dstView, nullptr);
    warnOnce("descriptor set alloc failed");
    return -1.0;
  }
  VkDescriptorImageInfo dii{};
  dii.sampler     = srcVkTex->_vksampler->_vksampler;
  dii.imageView   = srcImgObj->_vkimageview;
  dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkWriteDescriptorSet wds{};
  initializeVkStruct(wds, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
  wds.dstSet          = dset;
  wds.dstBinding      = 0;
  wds.descriptorCount = 1;
  wds.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  wds.pImageInfo      = &dii;
  vkUpdateDescriptorSets(ctxVK->_vkdevice, 1, &wds, 0, nullptr);

  // one-shot primary CB (independent of the frame CB; submit + WAIT, then MEASURE).
  VkCommandBufferAllocateInfo cbai{};
  initializeVkStruct(cbai, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
  cbai.commandPool        = ctxVK->_vkcmdpool_graphics;
  cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbai.commandBufferCount = 1;
  VkCommandBuffer cb = VK_NULL_HANDLE;
  if (VK_SUCCESS != vkAllocateCommandBuffers(ctxVK->_vkdevice, &cbai, &cb)) {
    vkDestroyDescriptorPool(ctxVK->_vkdevice, dpool, nullptr);
    vkDestroyImageView(ctxVK->_vkdevice, dstView, nullptr);
    warnOnce("command buffer alloc failed");
    return -1.0;
  }
  VkCommandBufferBeginInfo bi{};
  initializeVkStruct(bi, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cb, &bi);

  auto imgBarrier = [&](VkImage img, VkImageLayout oldL, VkImageLayout newL, //
                        VkAccessFlags srcA, VkAccessFlags dstA,              //
                        VkPipelineStageFlags srcS, VkPipelineStageFlags dstS) {
    VkImageMemoryBarrier b{};
    initializeVkStruct(b, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
    b.srcAccessMask       = srcA;
    b.dstAccessMask       = dstA;
    b.oldLayout           = oldL;
    b.newLayout           = newL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image               = img;
    b.subresourceRange    = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cb, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
  };

  // Src depth -> SHADER_READ (the capture pass already left it there; defensive if not).
  VkImageLayout srcLayout = srcImgObj->_currentLayout;
  if (srcLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    imgBarrier(
        srcImgObj->_vkimage, srcLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, //
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    srcImgObj->_currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  // dst D16 -> DEPTH_ATTACHMENT_OPTIMAL (whole image discarded for left; preserved for right).
  VkImageLayout dstOld = discard ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  imgBarrier(
      dstImage, dstOld, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, //
      discard ? 0 : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

  VkRenderingAttachmentInfo depthAtt{};
  initializeVkStruct(depthAtt, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
  depthAtt.imageView   = dstView;
  depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depthAtt.resolveMode = VK_RESOLVE_MODE_NONE;
  depthAtt.loadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // FS writes every pixel of the half
  depthAtt.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

  VkRenderingInfo rinfo{};
  initializeVkStruct(rinfo, VK_STRUCTURE_TYPE_RENDERING_INFO);
  rinfo.renderArea           = {{int32_t(dstX), int32_t(dstY)}, {w, h}};
  rinfo.layerCount           = 1;
  rinfo.colorAttachmentCount = 0;
  rinfo.pDepthAttachment     = &depthAtt;
  ctxVK->_vkCmdBeginRenderingKHR(cb, &rinfo);

  VkViewport vpr{float(dstX), float(dstY), float(w), float(h), 0.0f, 1.0f};
  vkCmdSetViewport(cb, 0, 1, &vpr);
  VkRect2D scis{{int32_t(dstX), int32_t(dstY)}, {w, h}};
  vkCmdSetScissor(cb, 0, 1, &scis);
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_depthconv._pipeline);
  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_depthconv._playout, 0, 1, &dset, 0, nullptr);
  struct { int32_t flipV; float revzFloor; } pc{flipV ? 1 : 0, vrRevZFloor()};
  vkCmdPushConstants(cb, s_depthconv._playout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
  vkCmdDraw(cb, 3, 1, 0, 0);
  ctxVK->_vkCmdEndRenderingKHR(cb);

  // release layout for the runtime (DEPTH_STENCIL_ATTACHMENT_OPTIMAL per XR_KHR_vulkan_enable).
  imgBarrier(
      dstImage, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

  vkEndCommandBuffer(cb);

  VkFenceCreateInfo fci{};
  initializeVkStruct(fci, VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
  VkFence fence = VK_NULL_HANDLE;
  vkCreateFence(ctxVK->_vkdevice, &fci, nullptr, &fence);
  VkSubmitInfo si{};
  initializeVkStruct(si, VK_STRUCTURE_TYPE_SUBMIT_INFO);
  si.commandBufferCount = 1;
  si.pCommandBuffers    = &cb;

  ork::Timer timer;
  timer.Start();
  ctxVK->_gfxqueue->queueSubmit(&si, fence);
  vkWaitForFences(ctxVK->_vkdevice, 1, &fence, VK_TRUE, UINT64_MAX);
  double elapsed = timer.SecsSinceStart();

  vkDestroyFence(ctxVK->_vkdevice, fence, nullptr);
  vkFreeCommandBuffers(ctxVK->_vkdevice, ctxVK->_vkcmdpool_graphics, 1, &cb);
  vkDestroyDescriptorPool(ctxVK->_vkdevice, dpool, nullptr);
  vkDestroyImageView(ctxVK->_vkdevice, dstView, nullptr);

  // ORKID_XR_DEPTH_DEBUG=1 discriminator — one-shot readback of the captured source depth
  //  (reflects the published revZ distribution). Runs after the conversion so src is settled.
  static bool s_depthdbg_done = false;
  if (not s_depthdbg_done) {
    if (const char* e = getenv("ORKID_XR_DEPTH_DEBUG"); e and e[0] == '1') {
      s_depthdbg_done = true;
      _debugReadbackDepth(ctxVK, srcVkTex.get(), uint32_t(srcDepth->_width), uint32_t(srcDepth->_height));
    }
  }
  return elapsed;
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
////////////////////////////////////////////////////////////////////////////////
