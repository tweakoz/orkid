////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#if defined(__linux__)

#include <vulkan/vulkan.h>
#include "vk_protos.h"
#include <ork/lev2/drm/drm_types.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

struct VkSwapChainDRM : public VkFramebufferOutput {

    VkSwapChainDRM(vkcontext_rawptr_t ctxVK, drm::drm_context_rawptr_t drmctx);
    ~VkSwapChainDRM();

    // VkFramebufferOutput interface
    void beginFrame(vkcontext_rawptr_t ctxVK) override final;
    void endFrame(vkcontext_rawptr_t ctxVK) override final;
    void submit(vkcontext_rawptr_t ctxVK) override final;

    void _buildup();
    void _teardown();
    void _acquireImage(vkcontext_rawptr_t ctxVK);
    void _enqueueFrame(vkcontext_rawptr_t ctxVK);
    void _waitPresentFrame(vkcontext_rawptr_t ctxVK);

    void _waitFrame(vkcontext_rawptr_t ctxVK);
    void _createExportableImages();
    void _createScanoutImages();
    void _exportImagesToDRM();

    vkcontext_rawptr_t        _contextVK  = nullptr;
    drm::drm_context_rawptr_t _drmContext = nullptr;

    // Vulkan images (exportable with DRM modifiers)
    VkImage             _images[MAX_FRAMES_IN_FLIGHT]        = {VK_NULL_HANDLE};
    VkDeviceMemory      _imageMemories[MAX_FRAMES_IN_FLIGHT] = {VK_NULL_HANDLE};
    VkImageView         _imageViews[MAX_FRAMES_IN_FLIGHT]    = {VK_NULL_HANDLE};
    VkSubresourceLayout _imageLayouts[MAX_FRAMES_IN_FLIGHT]  = {};
    vkimageobj_ptr_t    _imageObjects[MAX_FRAMES_IN_FLIGHT]  = {};

    VkFormat _imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    uint64_t _drmModifier = 0;

    // When the render images use a non-LINEAR modifier (e.g. NVIDIA block-linear),
    // scanout goes through LINEAR copy-target images (render -> copy -> flip) so
    // the display engine's decode of the buffer is unambiguous.
    bool                _useLinearScanout = false;
    VkImage             _scanoutImages[MAX_FRAMES_IN_FLIGHT]   = {VK_NULL_HANDLE};
    VkDeviceMemory      _scanoutMemories[MAX_FRAMES_IN_FLIGHT] = {VK_NULL_HANDLE};
    VkSubresourceLayout _scanoutLayouts[MAX_FRAMES_IN_FLIGHT]  = {};
    VkImageLayout       _scanoutVkLayouts[MAX_FRAMES_IN_FLIGHT] = {VK_IMAGE_LAYOUT_UNDEFINED};
    
    // First frame uses SetCrtc, rest use PageFlip
    bool _firstFrame = true;      
};

using vkswapchaindrm_ptr_t = std::shared_ptr<VkSwapChainDRM>;
using vkswapchaindrm_rawptr_t = VkSwapChainDRM*;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
