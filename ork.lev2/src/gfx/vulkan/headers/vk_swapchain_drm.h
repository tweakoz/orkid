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

struct VkSwapChainDRM {

    VkSwapChainDRM(vkcontext_rawptr_t ctxVK, drm::drm_context_rawptr_t drmctx);
    ~VkSwapChainDRM();

    void _buildup();
    void _teardown();

    VkResult acquireImage(vkcontext_rawptr_t ctxVK);
    void enqueueFrame(vkcontext_rawptr_t ctxVK);
    void waitPresentFrame(vkcontext_rawptr_t ctxVK);
    size_t subIndex() const;

    void _submitFrameWithSemaphores(vkcontext_rawptr_t ctxVK);
    void _createExportableImages();
    void _exportImagesToDRM();
    void _createRenderPass();
    void _createImageViews();
    void _createFramebuffers();

    vkcontext_rawptr_t _contextVK = nullptr;
    drm::drm_context_rawptr_t _drmContext = nullptr;

    // Swap chain configuration
    static constexpr uint32_t SWAP_CHAIN_SIZE = 3;  // Triple buffering

    // Vulkan images (exportable with DRM modifiers)
    VkImage _images[SWAP_CHAIN_SIZE] = {VK_NULL_HANDLE};
    VkDeviceMemory _imageMemories[SWAP_CHAIN_SIZE] = {VK_NULL_HANDLE};
    VkImageView _imageViews[SWAP_CHAIN_SIZE] = {VK_NULL_HANDLE};
    VkFramebuffer _framebuffers[SWAP_CHAIN_SIZE] = {VK_NULL_HANDLE};
    std::vector<vkimageobj_ptr_t> _swapChainImages;  // Wrapped image objects for RTG

    VkFormat _imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkSubresourceLayout _imageLayouts[SWAP_CHAIN_SIZE];
    uint64_t _drmModifier = 0;

    // Render pass (shared across all images)
    VkRenderPass _renderPass = VK_NULL_HANDLE;

    // Synchronization (one per image to prevent reuse while in-flight)
    // CRITICAL: Must match SWAP_CHAIN_SIZE to track each image's fence
    static constexpr size_t MAX_FRAMES_IN_FLIGHT = SWAP_CHAIN_SIZE;
    std::vector<vkfence_obj_ptr_t> _frameFences;  // Fences for GPU work completion (one per image)

    // Frame management
    uint32_t _currentImage = 0;        // Current swap image index (0, 1, or 2)
    bool _firstFrame = true;           // First frame uses SetCrtc, rest use PageFlip
    int _width = 0;
    int _height = 0;

    // FPS measurement (for debugging performance)
    float _frameTimes[10] = {0};       // Rolling buffer of last 10 frame times
    int _frameTimeIndex = 0;           // Current position in buffer
    int _totalFrameCount = 0;          // Total frames rendered
    float _lastFrameTime = 0.0f;       // Time of last frame
};

using vkswapchaindrm_ptr_t = std::shared_ptr<VkSwapChainDRM>;
using vkswapchaindrm_rawptr_t = VkSwapChainDRM*;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
