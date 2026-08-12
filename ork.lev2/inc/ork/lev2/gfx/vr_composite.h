////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <memory>

////////////////////////////////////////////////////////////////////////////////
// VR composite blit seam (OPENXR X3).
//
//  The XR device owns a foreign (runtime-created) swapchain VkImage; the engine's
//  wide two-eye output lives in a lev2::Texture. This free function records a
//  ONE-SHOT sync command buffer (submit + WAIT) that copies the two-eye texture
//  into the swapchain image and returns the CB wall time (seconds) so the caller
//  can MEASURE the composite cost (X3 demands measurement before any v2).
//
//  Colorspace contract (adjudicated 2026-07-14 — encode the sRGB OETF EXACTLY ONCE):
//    - SRGB swapchain (needsGammaEncode=false): a straight vkCmdBlitImage — the
//      hardware applies the OETF on store into the sRGB image (the "pure blit").
//    - UNORM swapchain (needsGammaEncode=true): the runtime offers no sRGB view, so
//      the encode is done with TRANSFER ops only (no graphics pipeline): blit the
//      linear source into a cached SRGB intermediate (hardware OETF on store), then
//      vkCmdCopyImage the encoded bytes verbatim into the UNORM swapchain image
//      (copy-compatible formats — a raw byte copy, no second conversion).
//
//  Handles are opaque uint64 (VkImage) / int64 (VkFormat) so this header, and the
//  XR device that includes it, stay free of the Vulkan backend headers. Zero == none.
////////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
struct Context;
struct Texture;
} // namespace ork::lev2

namespace ork::lev2::vulkan {

// Records + submits + waits the composite CB. dstImage/dstVkFormat are the XR
// swapchain image + its VkFormat; w/h are the SOURCE (== blit-region) extent — a 1:1
// copy of the source texture into a w x h region of the destination. When
// needsGammaEncode is set the sRGB OETF is applied exactly once via the SRGB-
// intermediate route (the intermediate image is cached internally across frames,
// recreated on size/context change). Returns the measured wall-clock seconds of the
// submit+wait, or a negative value on a hard failure (no source image, bad context...).
//
//  The wide two-eye path passes w = 2*eyeW, dstX/dstY = 0, dstDiscard = true (one
//  whole-image copy). The per-eye stereo path calls this TWICE into ONE wide swapchain
//  image: left with w = eyeW, dstX = 0, dstDiscard = true (the fresh image is fully
//  overwritten across the two calls, so discarding prior contents is safe); right with
//  w = eyeW, dstX = eyeW, dstDiscard = false (transition FROM the release layout so the
//  just-written left half is PRESERVED). dstX/dstY default to 0 / dstDiscard to true so
//  the existing wide call is byte-identical.
//
//  flipV inverts the SOURCE V axis during the blit (srcOffsets[0].y = h,
//  srcOffsets[1].y = 0). The engine's offscreen RtGroup textures are BOTTOM-UP in
//  memory (the on-screen mirror samples them with a V-flipped uvrect to present them
//  upright); the runtime's swapchain contract is TOP-DOWN. flipV=true supplies the ONE
//  Y-flip that must exist on the projection→swapchain path so submitted rows are
//  top-down. The flip is always applied in the BLIT stage — in the gamma-encode route
//  the second stage is a vkCmdCopyImage that CANNOT flip, so the intermediate must
//  already be flipped. Defaults false so any non-XR caller is byte-identical.
double vrCompositeBlitToXrImage(
    Context* ctx,
    Texture* src,
    uint64_t dstImage,
    int64_t dstVkFormat,
    uint32_t w,
    uint32_t h,
    bool needsGammaEncode,
    uint32_t dstX     = 0,
    uint32_t dstY     = 0,
    bool dstDiscard   = true,
    bool flipV        = false);

////////////////////////////////////////////////////////////////////////////////
// Depth-layer copy (XR_KHR_composition_layer_depth). Converts one per-eye orkid DEPTH
// texture (standard-Z window depth, Z32F/Z24) into a w×h region at (dstX,dstY) of the
// runtime's wide D16_UNORM depth swapchain image, applying the runtime's contract:
//   revZ = clamp(1 - stdZ, 1/65535, 1)   — standard-Z → reverse-Z, background stays
//                                          OPAQUE (packed 0 = "empty" composites through)
//   + the SAME V-flip the color path uses (flipV), so depth rows match color rows as the
//     runtime receives them.
// nearZ/farZ are the device planes (passed through for symmetry / future validation; the
// per-view XrCompositionLayerDepthInfoKHR the device chains carries them to the runtime).
//
// The depth swapchain image carries DEPTH_STENCIL_ATTACHMENT usage ONLY (no transfer), so
// this MUST write via a depth-only render pass (fullscreen quad sampling the source depth,
// gl_FragDepth = revZ), viewport = the eye half — NOT a blit/copy. Returns the measured
// submit+wait seconds, or a negative value on failure (device treats <0 as "drop the
// depth layer this session, color-only"). Reachable only from OpenXrDevice on a real
// runtime — UNVERIFIED-BY-DESIGN on mac.
double vrCompositeDepthToXrImage(
    Context* ctx,
    Texture* srcDepth,
    uint64_t dstDepthImage,
    int64_t dstDepthVkFormat,
    uint32_t w,
    uint32_t h,
    float nearZ,
    float farZ,
    uint32_t dstX = 0,
    uint32_t dstY = 0,
    bool flipV    = false);

} // namespace ork::lev2::vulkan
