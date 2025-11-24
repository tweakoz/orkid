////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if defined(__linux__)

#include "headers/vulkan_ctx.h"
#include "headers/vk_swapchain_drm.h"
#include <ork/util/logger.h>

extern "C" {
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_vkdrm = logger()->configureChannel("VKDRM", fvec3(0.2, 0.8, 0.6), true);

///////////////////////////////////////////////////////////////////////////////
// Constructor
///////////////////////////////////////////////////////////////////////////////

VkSwapChainDRM::VkSwapChainDRM(vkcontext_rawptr_t ctxVK, drm::drm_context_rawptr_t drmctx)
    : _contextVK(ctxVK)
    , _drmContext(drmctx) {

    logchan_vkdrm->log("Creating DRM swapchain");

    _width = drmctx->imageExtent.width;
    _height = drmctx->imageExtent.height;

    // Create fences for frame synchronization
    _frameFences.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        _frameFences[i] = std::make_shared<VulkanFenceObject>(ctxVK);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Destructor
///////////////////////////////////////////////////////////////////////////////

VkSwapChainDRM::~VkSwapChainDRM() {
    logchan_vkdrm->log("Destroying DRM swapchain");
    _teardown();
}

///////////////////////////////////////////////////////////////////////////////
// Buildup - Create all Vulkan/DRM resources
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_buildup() {
    logchan_vkdrm->log("Building DRM swapchain resources");

    _createExportableImages();
    _exportImagesToDRM();
    _createRenderPass();
    _createImageViews();
    _createFramebuffers();

    logchan_vkdrm->log("DRM swapchain ready: %dx%d, %u images", _width, _height, SWAP_CHAIN_SIZE);
}

///////////////////////////////////////////////////////////////////////////////
// Teardown - Destroy all resources
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_teardown() {
    if (!_contextVK || !_contextVK->_vkdevice) {
        return;
    }

    VkDevice device = _contextVK->_vkdevice;
    vkDeviceWaitIdle(device);

    // Cleanup swap chain resources
    for (uint32_t i = 0; i < SWAP_CHAIN_SIZE; i++) {
        if (_framebuffers[i]) {
            vkDestroyFramebuffer(device, _framebuffers[i], nullptr);
            _framebuffers[i] = VK_NULL_HANDLE;
        }
        if (_imageViews[i]) {
            vkDestroyImageView(device, _imageViews[i], nullptr);
            _imageViews[i] = VK_NULL_HANDLE;
        }
        if (_images[i]) {
            vkDestroyImage(device, _images[i], nullptr);
            _images[i] = VK_NULL_HANDLE;
        }
        if (_imageMemories[i]) {
            vkFreeMemory(device, _imageMemories[i], nullptr);
            _imageMemories[i] = VK_NULL_HANDLE;
        }
    }

    if (_renderPass) {
        vkDestroyRenderPass(device, _renderPass, nullptr);
        _renderPass = VK_NULL_HANDLE;
    }

    _frameFences.clear();
}

///////////////////////////////////////////////////////////////////////////////
// Create exportable images with DRM modifiers
// Ported from ~/drmvk/vk.inl:189-470
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_createExportableImages() {
    logchan_vkdrm->log("Creating %u exportable images with DRM modifiers", SWAP_CHAIN_SIZE);

    VkDevice device = _contextVK->_vkdevice;
    VkPhysicalDevice physicalDevice = _contextVK->_vkphysicaldevice;
    VkInstance instance = _GVI->_instance;

    // Step 1: Query supported DRM modifiers
    VkDrmFormatModifierPropertiesListEXT modifierPropsList = {};
    modifierPropsList.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;

    VkFormatProperties2 formatProps = {};
    formatProps.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    formatProps.pNext = &modifierPropsList;

    auto vkGetPhysicalDeviceFormatProperties2 =
        (PFN_vkGetPhysicalDeviceFormatProperties2)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties2");

    if (!vkGetPhysicalDeviceFormatProperties2) {
        logchan_vkdrm->log("ERROR: vkGetPhysicalDeviceFormatProperties2 not available");
        throw std::runtime_error("Required Vulkan function not available");
    }

    vkGetPhysicalDeviceFormatProperties2(physicalDevice, _imageFormat, &formatProps);

    logchan_vkdrm->log("Found %u DRM modifiers for format", modifierPropsList.drmFormatModifierCount);

    if (modifierPropsList.drmFormatModifierCount == 0) {
        logchan_vkdrm->log("ERROR: No DRM modifiers supported for this format");
        throw std::runtime_error("No DRM modifiers available");
    }

    // Allocate and query modifier properties
    std::vector<VkDrmFormatModifierPropertiesEXT> modifierProps(modifierPropsList.drmFormatModifierCount);
    modifierPropsList.pDrmFormatModifierProperties = modifierProps.data();
    vkGetPhysicalDeviceFormatProperties2(physicalDevice, _imageFormat, &formatProps);

    // Step 2: Find a modifier that supports COLOR_ATTACHMENT
    uint64_t selectedModifier = DRM_FORMAT_MOD_INVALID;
    uint64_t fallbackModifier = DRM_FORMAT_MOD_INVALID;

    logchan_vkdrm->log("Available DRM modifiers:");
    for (const auto& prop : modifierProps) {
        bool supportsColorAttachment = (prop.drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
        logchan_vkdrm->log("  0x%016lx - COLOR_ATTACHMENT: %s",
                           prop.drmFormatModifier,
                           supportsColorAttachment ? "YES" : "NO");

        if (supportsColorAttachment) {
            if (prop.drmFormatModifier == DRM_FORMAT_MOD_LINEAR) {
                selectedModifier = prop.drmFormatModifier;
                logchan_vkdrm->log("  -> SELECTED (LINEAR modifier)");
            } else if (fallbackModifier == DRM_FORMAT_MOD_INVALID) {
                fallbackModifier = prop.drmFormatModifier;
            }
        }
    }

    if (selectedModifier == DRM_FORMAT_MOD_INVALID) {
        selectedModifier = fallbackModifier;
        if (selectedModifier != DRM_FORMAT_MOD_INVALID) {
            logchan_vkdrm->log("LINEAR modifier not available, using fallback: 0x%lx", selectedModifier);
        }
    }

    if (selectedModifier == DRM_FORMAT_MOD_INVALID) {
        logchan_vkdrm->log("ERROR: No modifier supports COLOR_ATTACHMENT");
        throw std::runtime_error("No suitable DRM modifier found");
    }

    _drmModifier = selectedModifier;

    // Get function pointers
    auto vkGetImageDrmFormatModifierPropertiesEXT =
        (PFN_vkGetImageDrmFormatModifierPropertiesEXT)vkGetDeviceProcAddr(device, "vkGetImageDrmFormatModifierPropertiesEXT");

    auto vkGetImageMemoryRequirements2 =
        (PFN_vkGetImageMemoryRequirements2)vkGetDeviceProcAddr(device, "vkGetImageMemoryRequirements2KHR");
    if (!vkGetImageMemoryRequirements2) {
        vkGetImageMemoryRequirements2 =
            (PFN_vkGetImageMemoryRequirements2)vkGetDeviceProcAddr(device, "vkGetImageMemoryRequirements2");
    }
    if (!vkGetImageMemoryRequirements2) {
        logchan_vkdrm->log("ERROR: vkGetImageMemoryRequirements2 not available");
        throw std::runtime_error("Required Vulkan function not available");
    }

    auto vkBindImageMemory2 = (PFN_vkBindImageMemory2)vkGetDeviceProcAddr(device, "vkBindImageMemory2KHR");
    if (!vkBindImageMemory2) {
        vkBindImageMemory2 = (PFN_vkBindImageMemory2)vkGetDeviceProcAddr(device, "vkBindImageMemory2");
    }

    auto vkGetImageSubresourceLayout2EXT =
        (PFN_vkGetImageSubresourceLayout2EXT)vkGetDeviceProcAddr(device, "vkGetImageSubresourceLayout2EXT");

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    // Step 3: Create all swap chain images
    for (uint32_t i = 0; i < SWAP_CHAIN_SIZE; i++) {
        logchan_vkdrm->log("Creating swap chain image %u/%u", i + 1, SWAP_CHAIN_SIZE);

        // Create image with DRM modifier
        VkImageDrmFormatModifierListCreateInfoEXT modifierListInfo = {};
        modifierListInfo.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
        modifierListInfo.drmFormatModifierCount = 1;
        modifierListInfo.pDrmFormatModifiers = &selectedModifier;

        VkExternalMemoryImageCreateInfo externalInfo = {};
        externalInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        externalInfo.pNext = &modifierListInfo;
        externalInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.pNext = &externalInfo;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = _imageFormat;
        imageInfo.extent.width = _width;
        imageInfo.extent.height = _height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkResult result = vkCreateImage(device, &imageInfo, nullptr, &_images[i]);
        if (result != VK_SUCCESS) {
            logchan_vkdrm->log("ERROR: Failed to create image %u with DRM modifier (result=%d)", i, result);
            throw std::runtime_error("Failed to create Vulkan image");
        }

        // Query which modifier was actually used
        if (vkGetImageDrmFormatModifierPropertiesEXT && i == 0) {
            VkImageDrmFormatModifierPropertiesEXT modifierInfo = {};
            modifierInfo.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT;
            if (vkGetImageDrmFormatModifierPropertiesEXT(device, _images[i], &modifierInfo) == VK_SUCCESS) {
                logchan_vkdrm->log("Image created with modifier: 0x%lx", modifierInfo.drmFormatModifier);
                _drmModifier = modifierInfo.drmFormatModifier;
            }
        }

        // Get memory requirements
        VkMemoryRequirements2 memRequirements2 = {};
        memRequirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

        VkImageMemoryRequirementsInfo2 memReqInfo = {};
        memReqInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
        memReqInfo.image = _images[i];

        vkGetImageMemoryRequirements2(device, &memReqInfo, &memRequirements2);
        VkMemoryRequirements& memRequirements = memRequirements2.memoryRequirements;

        // Allocate exportable memory
        VkExportMemoryAllocateInfo exportAllocInfo = {};
        exportAllocInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        exportAllocInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = &exportAllocInfo;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = UINT32_MAX;

        for (uint32_t j = 0; j < memProperties.memoryTypeCount; j++) {
            if ((memRequirements.memoryTypeBits & (1 << j)) &&
                (memProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                allocInfo.memoryTypeIndex = j;
                break;
            }
        }

        if (allocInfo.memoryTypeIndex == UINT32_MAX) {
            logchan_vkdrm->log("ERROR: Failed to find suitable exportable memory type");
            throw std::runtime_error("No suitable memory type found");
        }

        result = vkAllocateMemory(device, &allocInfo, nullptr, &_imageMemories[i]);
        if (result != VK_SUCCESS) {
            logchan_vkdrm->log("ERROR: Failed to allocate memory for image %u (result=%d)", i, result);
            throw std::runtime_error("Failed to allocate Vulkan memory");
        }

        // Bind memory
        if (!vkBindImageMemory2) {
            vkBindImageMemory(device, _images[i], _imageMemories[i], 0);
        } else {
            VkBindImageMemoryInfo bindInfo = {};
            bindInfo.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
            bindInfo.image = _images[i];
            bindInfo.memory = _imageMemories[i];
            bindInfo.memoryOffset = 0;
            vkBindImageMemory2(device, 1, &bindInfo);
        }

        // Query layout for DRM
        if (!vkGetImageSubresourceLayout2EXT) {
            VkImageSubresource subresource = {};
            subresource.aspectMask = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;
            subresource.mipLevel = 0;
            subresource.arrayLayer = 0;
            vkGetImageSubresourceLayout(device, _images[i], &subresource, &_imageLayouts[i]);
        } else {
            VkImageSubresource2EXT subresource2 = {};
            subresource2.sType = VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2_EXT;
            subresource2.imageSubresource.aspectMask = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;
            subresource2.imageSubresource.mipLevel = 0;
            subresource2.imageSubresource.arrayLayer = 0;

            VkSubresourceLayout2EXT layout2 = {};
            layout2.sType = VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2_EXT;

            vkGetImageSubresourceLayout2EXT(device, _images[i], &subresource2, &layout2);
            _imageLayouts[i] = layout2.subresourceLayout;
        }

        if (i == 0) {
            logchan_vkdrm->log("DRM modifier image layout: offset=%lu, size=%lu, rowPitch=%lu",
                               _imageLayouts[i].offset, _imageLayouts[i].size, _imageLayouts[i].rowPitch);
        }
    }

    logchan_vkdrm->log("Successfully created %u exportable images", SWAP_CHAIN_SIZE);
}

///////////////////////////////////////////////////////////////////////////////
// Export images to DRM framebuffers
// Ported from ~/drmvk/vk.inl:412-470
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_exportImagesToDRM() {
    logchan_vkdrm->log("Exporting images to DRM framebuffers");

    VkDevice device = _contextVK->_vkdevice;
    int drm_fd = _drmContext->drm_fd;

    auto vkGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR");
    if (!vkGetMemoryFdKHR) {
        logchan_vkdrm->log("ERROR: vkGetMemoryFdKHR not available");
        throw std::runtime_error("Required Vulkan function not available");
    }

    for (uint32_t i = 0; i < SWAP_CHAIN_SIZE; i++) {
        // Export as dmabuf fd
        VkMemoryGetFdInfoKHR getFdInfo = {};
        getFdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
        getFdInfo.memory = _imageMemories[i];
        getFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

        VkResult result = vkGetMemoryFdKHR(device, &getFdInfo, &_drmContext->dmabuf_fds[i]);
        if (result != VK_SUCCESS) {
            logchan_vkdrm->log("ERROR: Failed to export dmabuf for image %u (result=%d)", i, result);
            throw std::runtime_error("Failed to export DMABUF");
        }

        // Import dmabuf to DRM with modifier
        uint32_t handles[4] = {0};
        uint32_t pitches[4] = {(uint32_t)_imageLayouts[i].rowPitch, 0, 0, 0};
        uint32_t offsets[4] = {(uint32_t)_imageLayouts[i].offset, 0, 0, 0};
        uint64_t modifiers[4] = {_drmModifier, 0, 0, 0};

        int ret = drmPrimeFDToHandle(drm_fd, _drmContext->dmabuf_fds[i], &handles[0]);
        if (ret < 0) {
            logchan_vkdrm->log("ERROR: Failed to import dmabuf to DRM for image %u (ret=%d)", i, ret);
            throw std::runtime_error("Failed to import DMABUF to DRM");
        }

        ret = drmModeAddFB2WithModifiers(drm_fd, _width, _height,
                                         DRM_FORMAT_ARGB8888,
                                         handles, pitches, offsets, modifiers,
                                         &_drmContext->fb_ids[i], DRM_MODE_FB_MODIFIERS);
        if (ret < 0) {
            logchan_vkdrm->log("ERROR: Failed to create DRM framebuffer %u (ret=%d)", i, ret);
            throw std::runtime_error("Failed to create DRM framebuffer");
        }

        logchan_vkdrm->log("Created DRM framebuffer %u: fb_id=%u, dmabuf_fd=%d",
                           i, _drmContext->fb_ids[i], _drmContext->dmabuf_fds[i]);
    }

    logchan_vkdrm->log("Successfully exported all images to DRM");
}

///////////////////////////////////////////////////////////////////////////////
// Create render pass
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_createRenderPass() {
    logchan_vkdrm->log("Creating DRM render pass");

    VkDevice device = _contextVK->_vkdevice;

    // Single color attachment
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = _imageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // Subpass dependency for layout transition
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &_renderPass);
    if (result != VK_SUCCESS) {
        logchan_vkdrm->log("ERROR: Failed to create render pass (result=%d)", result);
        throw std::runtime_error("Failed to create DRM render pass");
    }

    logchan_vkdrm->log("DRM render pass created successfully");
}

///////////////////////////////////////////////////////////////////////////////
// Create image views
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_createImageViews() {
    logchan_vkdrm->log("Creating image views for %u swap images", SWAP_CHAIN_SIZE);

    VkDevice device = _contextVK->_vkdevice;

    for (uint32_t i = 0; i < SWAP_CHAIN_SIZE; i++) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = _images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = _imageFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &_imageViews[i]);
        if (result != VK_SUCCESS) {
            logchan_vkdrm->log("ERROR: Failed to create image view %u (result=%d)", i, result);
            throw std::runtime_error("Failed to create DRM image view");
        }
    }

    logchan_vkdrm->log("Successfully created %u image views", SWAP_CHAIN_SIZE);
}

///////////////////////////////////////////////////////////////////////////////
// Create framebuffers
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_createFramebuffers() {
    logchan_vkdrm->log("Creating framebuffers for %u swap images", SWAP_CHAIN_SIZE);

    VkDevice device = _contextVK->_vkdevice;

    for (uint32_t i = 0; i < SWAP_CHAIN_SIZE; i++) {
        VkImageView attachments[] = {_imageViews[i]};

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = _renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = _width;
        framebufferInfo.height = _height;
        framebufferInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &_framebuffers[i]);
        if (result != VK_SUCCESS) {
            logchan_vkdrm->log("ERROR: Failed to create framebuffer %u (result=%d)", i, result);
            throw std::runtime_error("Failed to create DRM framebuffer");
        }
    }

    logchan_vkdrm->log("Successfully created %u framebuffers", SWAP_CHAIN_SIZE);
}

///////////////////////////////////////////////////////////////////////////////
// Acquire next image
///////////////////////////////////////////////////////////////////////////////

VkResult VkSwapChainDRM::acquireImage(vkcontext_rawptr_t ctxVK) {
    // For DRM, we manually manage the image index
    // Just advance to next image in the triple buffer
    // Actual synchronization happens in waitPresentFrame()
    return VK_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// Submit frame with semaphores
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_submitFrameWithSemaphores(vkcontext_rawptr_t ctxVK) {
    // Submit command buffer with fence
    // DRM doesn't use semaphores the same way as GLFW swapchain
    // We use fences for CPU/GPU synchronization
}

///////////////////////////////////////////////////////////////////////////////
// Enqueue frame for rendering
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::enqueueFrame(vkcontext_rawptr_t ctxVK) {
    // TODO: Wait for the fence from the previous use of this frame slot
    // Skipping for now since we don't submit GPU work yet
    // _frameFences[_currentFrame]->wait();
    // _frameFences[_currentFrame]->reset();
}

///////////////////////////////////////////////////////////////////////////////
// Wait for present (page flip) to complete
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::waitPresentFrame(vkcontext_rawptr_t ctxVK) {
    // Page flip to the current image
    logchan_vkdrm->log("Page flip to image %u (fb_id=%u)", _currentImage, _drmContext->fb_ids[_currentImage]);

    int ret = drmModePageFlip(_drmContext->drm_fd, _drmContext->crtc_id,
                              _drmContext->fb_ids[_currentImage],
                              DRM_MODE_PAGE_FLIP_EVENT, _drmContext);
    if (ret < 0) {
        logchan_vkdrm->log("ERROR: drmModePageFlip failed (ret=%d)", ret);
        throw std::runtime_error("Page flip failed");
    }

    // Wait for vblank
    _drmContext->waitForVblank();

    // Advance to next image
    _currentImage = (_currentImage + 1) % SWAP_CHAIN_SIZE;

    // Advance to next frame
    _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

///////////////////////////////////////////////////////////////////////////////
// Get current sub-index (frame-in-flight)
///////////////////////////////////////////////////////////////////////////////

size_t VkSwapChainDRM::subIndex() const {
    return _currentFrame;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
