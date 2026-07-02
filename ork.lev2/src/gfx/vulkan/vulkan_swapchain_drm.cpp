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
#include <ork/kernel/timer.h>

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
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        _frame_fences[i] = std::make_shared<VulkanFenceObject>(ctxVK);
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
    if (_useLinearScanout)
        _createScanoutImages();
    _exportImagesToDRM();

    ///////////////////////////////////////////////////
    // Create/Update render target group impl for swapchain
    ///////////////////////////////////////////////////

    VkRtbCreateOption depth_option;
    depth_option._format       = VK_FORMAT_D32_SFLOAT;
    depth_option._usage        = "depth"_crcu;
    depth_option._with_texture = false;

    auto rtg = _contextVK->_fbi->_ensureMainRtg();

    vkrtgrpimpl_ptr_t rtg_impl;
    if (auto existing = rtg->_impl.tryAsShared<VkRtGroupImpl>()) {
        rtg_impl = existing.value();
        logchan_vkdrm->log("DRM: Using existing RTG impl");
    } else {
        // First time creation
        logchan_vkdrm->log("DRM: Creating RTG impl for main rtg");
        rtg_impl = _contextVK->_fbi->_createRtGroupImpl(rtg.get());
        rtg_impl->_width = _width;
        rtg_impl->_height = _height;
        VkRtGroupImpl::assignToRtGroup(rtg_impl, rtg.get());
        _vkCreateImageForBuffer(_contextVK, rtg_impl->_depth_buffer_impl, depth_option);
    }

    logchan_vkdrm->log("DRM swapchain ready: %dx%d, %u images", _width, _height, MAX_FRAMES_IN_FLIGHT);
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
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        _imageObjects[i] = nullptr; // release VulkanImageObject wrappers first
        if (_imageViews[i] != VK_NULL_HANDLE) {
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
        if (_scanoutImages[i]) {
            vkDestroyImage(device, _scanoutImages[i], nullptr);
            _scanoutImages[i] = VK_NULL_HANDLE;
        }
        if (_scanoutMemories[i]) {
            vkFreeMemory(device, _scanoutMemories[i], nullptr);
            _scanoutMemories[i] = VK_NULL_HANDLE;
        }
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        _frame_fences[i] = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// Create exportable images with DRM modifiers
// Ported from ~/drmvk/vk.inl:189-470
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_createExportableImages() {
    logchan_vkdrm->log("Creating %u exportable images with DRM modifiers", MAX_FRAMES_IN_FLIGHT);

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
    bool linear_can_transfer_dst = false;

    logchan_vkdrm->log("Available DRM modifiers:");
    for (const auto& prop : modifierProps) {
        bool supportsColorAttachment = (prop.drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
        logchan_vkdrm->log("  0x%016lx - COLOR_ATTACHMENT: %s",
                           prop.drmFormatModifier,
                           supportsColorAttachment ? "YES" : "NO");

        if (prop.drmFormatModifier == DRM_FORMAT_MOD_LINEAR) {
            linear_can_transfer_dst = (prop.drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT);
        }

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

    // When rendering must use a vendor tiled modifier, scan out via LINEAR
    // copy-target images so the display decode is unambiguous.
    // ORKID_DRM_DIRECT_SCANOUT=1 forces the old direct (tiled) scanout for A/B testing.
    bool force_direct = (std::getenv("ORKID_DRM_DIRECT_SCANOUT") != nullptr);
    _useLinearScanout = (selectedModifier != DRM_FORMAT_MOD_LINEAR) && linear_can_transfer_dst && !force_direct;
    logchan_vkdrm->log("scanout mode: %s", _useLinearScanout ? "LINEAR-copy" : "direct");

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
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        logchan_vkdrm->log("Creating swap chain image %u/%u", i + 1, MAX_FRAMES_IN_FLIGHT);

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
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (_useLinearScanout)
            imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
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
        // dma-buf export requires dedicated allocation on some drivers (e.g. NVIDIA);
        // always use it — scanout images are one-image-per-allocation anyway
        VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {};
        dedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicatedAllocInfo.image = _images[i];

        VkExportMemoryAllocateInfo exportAllocInfo = {};
        exportAllocInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        exportAllocInfo.pNext = &dedicatedAllocInfo;
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

        // Create image view for use as color attachment / _replaceImage injection
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = _images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = _imageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        result = vkCreateImageView(device, &viewInfo, nullptr, &_imageViews[i]);
        OrkAssert(result == VK_SUCCESS);
        _imageObjects[i] = std::make_shared<VulkanImageObject>(_contextVK, _images[i], _imageViews[i], _imageFormat);
    }

    logchan_vkdrm->log("Successfully created %u exportable images", MAX_FRAMES_IN_FLIGHT);
}

///////////////////////////////////////////////////////////////////////////////
// Create LINEAR copy-target images for scanout (non-LINEAR render modifier)
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_createScanoutImages() {
    logchan_vkdrm->log("Creating %u LINEAR scanout images", MAX_FRAMES_IN_FLIGHT);

    VkDevice device = _contextVK->_vkdevice;
    VkPhysicalDevice physicalDevice = _contextVK->_vkphysicaldevice;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    uint64_t linearModifier = DRM_FORMAT_MOD_LINEAR;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkImageDrmFormatModifierListCreateInfoEXT modifierListInfo = {};
        modifierListInfo.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
        modifierListInfo.drmFormatModifierCount = 1;
        modifierListInfo.pDrmFormatModifiers = &linearModifier;

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
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkResult result = vkCreateImage(device, &imageInfo, nullptr, &_scanoutImages[i]);
        if (result != VK_SUCCESS) {
            logchan_vkdrm->log("ERROR: Failed to create LINEAR scanout image %u (result=%d)", i, result);
            throw std::runtime_error("Failed to create LINEAR scanout image");
        }

        VkMemoryRequirements2 memRequirements2 = {};
        memRequirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

        VkImageMemoryRequirementsInfo2 memReqInfo = {};
        memReqInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
        memReqInfo.image = _scanoutImages[i];
        vkGetImageMemoryRequirements2(device, &memReqInfo, &memRequirements2);
        VkMemoryRequirements& memRequirements = memRequirements2.memoryRequirements;

        VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {};
        dedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicatedAllocInfo.image = _scanoutImages[i];

        VkExportMemoryAllocateInfo exportAllocInfo = {};
        exportAllocInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        exportAllocInfo.pNext = &dedicatedAllocInfo;
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
            logchan_vkdrm->log("ERROR: no exportable memory type for scanout image");
            throw std::runtime_error("No suitable memory type found");
        }

        result = vkAllocateMemory(device, &allocInfo, nullptr, &_scanoutMemories[i]);
        if (result != VK_SUCCESS) {
            logchan_vkdrm->log("ERROR: Failed to allocate scanout memory %u (result=%d)", i, result);
            throw std::runtime_error("Failed to allocate Vulkan memory");
        }

        VkBindImageMemoryInfo bindInfo = {};
        bindInfo.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
        bindInfo.image = _scanoutImages[i];
        bindInfo.memory = _scanoutMemories[i];
        bindInfo.memoryOffset = 0;
        vkBindImageMemory2(device, 1, &bindInfo);

        VkImageSubresource subresource = {};
        subresource.aspectMask = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;
        vkGetImageSubresourceLayout(device, _scanoutImages[i], &subresource, &_scanoutLayouts[i]);

        _scanoutVkLayouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;

        if (i == 0) {
            logchan_vkdrm->log("LINEAR scanout layout: offset=%lu, size=%lu, rowPitch=%lu",
                               _scanoutLayouts[i].offset, _scanoutLayouts[i].size, _scanoutLayouts[i].rowPitch);
        }
    }

    logchan_vkdrm->log("Successfully created %u LINEAR scanout images", MAX_FRAMES_IN_FLIGHT);
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

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // scanout source: LINEAR copy-target images when active, else the render images
        VkDeviceMemory scan_mem            = _useLinearScanout ? _scanoutMemories[i] : _imageMemories[i];
        const VkSubresourceLayout& scan_lo = _useLinearScanout ? _scanoutLayouts[i] : _imageLayouts[i];
        uint64_t scan_modifier             = _useLinearScanout ? DRM_FORMAT_MOD_LINEAR : _drmModifier;

        // Export as dmabuf fd
        VkMemoryGetFdInfoKHR getFdInfo = {};
        getFdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
        getFdInfo.memory = scan_mem;
        getFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

        VkResult result = vkGetMemoryFdKHR(device, &getFdInfo, &_drmContext->dmabuf_fds[i]);
        if (result != VK_SUCCESS) {
            logchan_vkdrm->log("ERROR: Failed to export dmabuf for image %u (result=%d)", i, result);
            throw std::runtime_error("Failed to export DMABUF");
        }

        // Import dmabuf to DRM with modifier
        uint32_t handles[4] = {0};
        uint32_t pitches[4] = {(uint32_t)scan_lo.rowPitch, 0, 0, 0};
        uint32_t offsets[4] = {(uint32_t)scan_lo.offset, 0, 0, 0};
        uint64_t modifiers[4] = {scan_modifier, 0, 0, 0};

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

///////////////////////////////////////////////////////////////////////////////
// Acquire next image
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_acquireImage(vkcontext_rawptr_t ctxVK) {
    // Waits for display to show (flip) the last frame which had drmModePageFlip called on it.
    // We assume once the flip has occured for the last frame then the current frame is available.
    // We wait here, instead of immediately after drmModePageFlip, so the CPU can keep going and
    // does not wait until until the frame is truly needed.
    _drmContext->waitForVblank();

    // Wait on this frames fence for good measure and reset.
    // The frame should always be finished by this point so the wait should be a no-op.
    _frame_fences[_sub_index]->wait();
    _frame_fences[_sub_index]->reset();
}

///////////////////////////////////////////////////////////////////////////////
// Enqueue frame for rendering
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_enqueueFrame(vkcontext_rawptr_t ctxVK) {
    // Match drmvk reference: submit with fence (like drmvk line 166)
    // Fence was already reset in acquireImage

    // Submit command buffer
    VkSubmitInfo SI = {};
    SI.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SI.commandBufferCount = 1;
    SI.pCommandBuffers = &ctxVK->_cmdbufcurpri_gfx->_vkcmdbuf;

    VkTimelineSemaphoreSubmitInfo timelineInfo{};
    initializeVkStruct(timelineInfo, VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO);
    timelineInfo.signalSemaphoreValueCount = ctxVK->_oneShotSignalValues.size();
    timelineInfo.pSignalSemaphoreValues    = ctxVK->_oneShotSignalValues.data();
    SI.pNext                = &timelineInfo;
    SI.signalSemaphoreCount = ctxVK->_oneShotSignalSemaphores.size();
    SI.pSignalSemaphores    = ctxVK->_oneShotSignalSemaphores.data();

    ctxVK->_gfxqueue->queueSubmit(&SI, _frame_fences[_sub_index]->_vkfence);
}

///////////////////////////////////////////////////////////////////////////////
// Wait for present (page flip) to complete
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::_waitPresentFrame(vkcontext_rawptr_t ctxVK) {
    // CRITICAL: Wait for GPU to finish rendering THIS image before we flip it
    // The fence was signaled by enqueueFrame() when GPU completes
    _frame_fences[_sub_index]->wait();

    // Display via DRM (first frame uses SetCrtc, subsequent use PageFlip)
    if (_firstFrame) {
        logchan_vkdrm->log("First frame: SetCrtc with image %zu (fb_id=%u)",
                           _sub_index, _drmContext->fb_ids[_sub_index]);

        int ret = drmModeSetCrtc(_drmContext->drm_fd,
                                 _drmContext->crtc_id,
                                 _drmContext->fb_ids[_sub_index],
                                 0, 0,  // x, y offset
                                 &_drmContext->connector_id,
                                 1,     // connector count
                                 &_drmContext->mode);
        if (ret < 0) {
            logchan_vkdrm->log("ERROR: drmModeSetCrtc failed (ret=%d)", ret);
            throw std::runtime_error("SetCrtc failed");
        }

        _firstFrame = false;
        _drmContext->displayingImage = _sub_index;
        logchan_vkdrm->log("Initial mode set complete, display active");
    } else {
        // ORKID_DRM_NOVSYNC=1: async (tearing) flips — don't wait for vblank.
        // Falls back to vsynced flips if the driver rejects ASYNC for this plane.
        static int s_novsync = (getenv("ORKID_DRM_NOVSYNC") != nullptr) ? 1 : 0;
        uint32_t flip_flags = DRM_MODE_PAGE_FLIP_EVENT;
        if (s_novsync == 1)
            flip_flags |= DRM_MODE_PAGE_FLIP_ASYNC;
        int ret = drmModePageFlip(_drmContext->drm_fd,
                                  _drmContext->crtc_id,
                                  _drmContext->fb_ids[_sub_index],
                                  flip_flags,
                                  _drmContext);
        if (ret < 0 && s_novsync == 1) {
            logchan_vkdrm->log("async page flip rejected (ret=%d) — falling back to vsync flips", ret);
            s_novsync = -1; // don't retry async
            ret = drmModePageFlip(_drmContext->drm_fd,
                                  _drmContext->crtc_id,
                                  _drmContext->fb_ids[_sub_index],
                                  DRM_MODE_PAGE_FLIP_EVENT,
                                  _drmContext);
        }
        if (ret < 0) {
            logchan_vkdrm->log("ERROR: drmModePageFlip failed (ret=%d)", ret);
            throw std::runtime_error("Page flip failed");
        }

        _drmContext->flipPending = true;
        _drmContext->displayingImage = _sub_index;
    }
}


///////////////////////////////////////////////////////////////////////////////
// VkSwapChainDRM - VkFramebufferOutput implementation (DRM direct-rendering, Linux only)
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::beginFrame(vkcontext_rawptr_t ctxVK) {
  OrkAssertI(!_acquired, "beginFrame called twice without a submit in between");
  _acquireImage(ctxVK);
  // Inject the acquired DRM image directly into the main RTG color buffer.
  // _replaceImage resets the layout to UNDEFINED, so the subsequent _transitionToRenderTarget
  // in _pushRtGroup correctly barriers UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL.
  auto main_rtg  = ctxVK->_fbi->_ensureMainRtg();
  auto main_rtb  = main_rtg->buffer(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();
  main_rtbi->_is_surface = true;
  main_rtbi->_replaceImage(_imageObjects[_sub_index]);
  _acquired = true;
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::endFrame(vkcontext_rawptr_t ctxVK) {
  auto main_rtg  = ctxVK->_fbi->_ensureMainRtg();
  auto main_rtb  = main_rtg->buffer(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();
  auto cmdbuf    = ctxVK->primary_cb()->_vkcmdbuf;

  if (_useLinearScanout) {
    // Copy the tiled render image into this frame's LINEAR scanout image

    auto src_bar = createImageBarrier(
        main_rtbi->_imgobj->_vkimage,
        main_rtbi->_currentLayout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);
    src_bar->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    auto dst_bar = createImageBarrier(
        _scanoutImages[_sub_index],
        VK_IMAGE_LAYOUT_UNDEFINED, // discard previous contents
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        (VkAccessFlagBits)0,
        VK_ACCESS_TRANSFER_WRITE_BIT);
    dst_bar->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageMemoryBarrier pre_bars[2] = {*src_bar, *dst_bar};
    vkCmdPipelineBarrier(
        cmdbuf,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr,
        2, pre_bars);

    VkImageCopy region = {};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.extent = {uint32_t(_width), uint32_t(_height), 1};
    vkCmdCopyImage(
        cmdbuf,
        main_rtbi->_imgobj->_vkimage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        _scanoutImages[_sub_index],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region);

    auto scan_bar = createImageBarrier(
        _scanoutImages[_sub_index],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        (VkAccessFlagBits)0);
    scan_bar->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdPipelineBarrier(
        cmdbuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr,
        1, scan_bar.get());

    _scanoutVkLayouts[_sub_index] = VK_IMAGE_LAYOUT_GENERAL;
    main_rtbi->_currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    main_rtbi->_imgobj->_currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    return;
  }

  // Direct scanout: transition DRM image COLOR_ATTACHMENT_OPTIMAL -> GENERAL
  auto imgbar = createImageBarrier(
      main_rtbi->_imgobj->_vkimage,
      main_rtbi->_currentLayout,
      VK_IMAGE_LAYOUT_GENERAL,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      (VkAccessFlagBits)0);
  imgbar->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  vkCmdPipelineBarrier(
      ctxVK->primary_cb()->_vkcmdbuf,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      0, 0, nullptr, 0, nullptr,
      1, imgbar.get());

  main_rtbi->_currentLayout = VK_IMAGE_LAYOUT_GENERAL;
  main_rtbi->_imgobj->_currentLayout = VK_IMAGE_LAYOUT_GENERAL;
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::submit(vkcontext_rawptr_t ctxVK) {
  _enqueueFrame(ctxVK);
  _waitPresentFrame(ctxVK);
  _incrementFrame();
  _acquired = false;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
