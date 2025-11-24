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
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;  // DRM needs GENERAL layout for scanout

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

        // Create VulkanImageObject wrapper (like GLFW swapchain does)
        auto imgobj = std::make_shared<VulkanImageObject>(
            _contextVK,
            _images[i],
            _imageViews[i],
            _imageFormat);
        imgobj->_delete_image = false;     // Image managed by DRM
        imgobj->_delete_imageview = false; // ImageView managed by DRM
        _swapChainImages.push_back(imgobj);
        logchan_vkdrm->log("Created VulkanImageObject[%u]: image=%p view=%p", i, _images[i], _imageViews[i]);
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
    // Match drmvk reference: wait for THIS image's fence before using it
    // This ensures the image is not in-flight from a previous use
    size_t fence_index = _currentImage;
    if (fence_index < _frameFences.size()) {
        auto& fence = _frameFences[fence_index];
        // Wait for fence (like drmvk line 92)
        fence->wait();
        fence->reset();
    }

    // Connect the RTG's color buffer to the current image
    auto rtg = _contextVK->_fbi->_ensureMainRtg();
    auto rtg_impl = rtg->_impl.getShared<VkRtGroupImpl>();
    auto rtb_color = rtg->buffer(0);
    auto rtb_impl = rtb_color->_impl.getShared<VklRtBufferImpl>();
    rtb_impl->_is_surface = true;

    auto imgobj = _swapChainImages[_currentImage];
    rtb_impl->_replaceImage(imgobj);

    static int log_count = 0;
    if (log_count < 10) {
        logchan_vkdrm->log("acquireImage[%u]: Using image %u",
                           log_count, _currentImage);
        log_count++;
    }

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
    // Match drmvk reference: submit with fence (like drmvk line 166)
    // Fence was already reset in acquireImage

    size_t fence_index = _currentImage;

    // Submit command buffer
    VkSubmitInfo SI = {};
    SI.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SI.commandBufferCount = 1;
    SI.pCommandBuffers = &ctxVK->_cmdbufcurpri_gfx->_vkcmdbuf;

    // Submit with fence for THIS image
    if (fence_index < _frameFences.size()) {
        auto& fence = _frameFences[fence_index];
        vkQueueSubmit(ctxVK->_vkqueue_graphics, 1, &SI, fence->_vkfence);
    } else {
        vkQueueSubmit(ctxVK->_vkqueue_graphics, 1, &SI, VK_NULL_HANDLE);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Wait for present (page flip) to complete
///////////////////////////////////////////////////////////////////////////////

void VkSwapChainDRM::waitPresentFrame(vkcontext_rawptr_t ctxVK) {
    static int frame_count = 0;
    static ork::Timer fps_timer;
    static bool timer_started = false;

    // Start timer on first frame
    if (!timer_started) {
        fps_timer.Start();
        timer_started = true;
        _lastFrameTime = fps_timer.SecsSinceStart();
    }

    // Timing breakdown for performance analysis
    float time_vblank_wait = 0.0f;
    float time_pageflip = 0.0f;
    auto t_start = std::chrono::high_resolution_clock::now();

    // Match drmvk reference: Wait for previous vblank FIRST (like drmvk main.cpp line 231)
    // This ensures the previous page flip completed before we queue the next one
    if (!_firstFrame) {
        _drmContext->waitForVblank();
    }

    auto t_after_vblank = std::chrono::high_resolution_clock::now();
    time_vblank_wait = std::chrono::duration<float, std::milli>(t_after_vblank - t_start).count();

    // Display via DRM (first frame uses SetCrtc, subsequent use PageFlip)
    if (_firstFrame) {
        // First frame: establish the mode
        logchan_vkdrm->log("FRAME[%d] First frame: SetCrtc with image %u (fb_id=%u)",
                           frame_count, _currentImage, _drmContext->fb_ids[_currentImage]);

        int ret = drmModeSetCrtc(_drmContext->drm_fd,
                                 _drmContext->crtc_id,
                                 _drmContext->fb_ids[_currentImage],
                                 0, 0,  // x, y offset
                                 &_drmContext->connector_id,
                                 1,     // connector count
                                 &_drmContext->mode);
        if (ret < 0) {
            logchan_vkdrm->log("ERROR: drmModeSetCrtc failed (ret=%d)", ret);
            throw std::runtime_error("SetCrtc failed");
        }

        _firstFrame = false;
        _drmContext->displayingImage = _currentImage;
        logchan_vkdrm->log("Initial mode set complete, display active");
    } else {
        // Subsequent frames: page flip (like drmvk line 181)
        if (frame_count < 10) {
            logchan_vkdrm->log("FRAME[%d] Page flip to image %u (fb_id=%u)",
                               frame_count, _currentImage, _drmContext->fb_ids[_currentImage]);
        }

        int ret = drmModePageFlip(_drmContext->drm_fd,
                                  _drmContext->crtc_id,
                                  _drmContext->fb_ids[_currentImage],
                                  DRM_MODE_PAGE_FLIP_EVENT,
                                  _drmContext);
        if (ret < 0) {
            logchan_vkdrm->log("ERROR: drmModePageFlip failed (ret=%d)", ret);
            throw std::runtime_error("Page flip failed");
        }

        _drmContext->flipPending = true;
        _drmContext->displayingImage = _currentImage;
    }

    auto t_after_pageflip = std::chrono::high_resolution_clock::now();
    time_pageflip = std::chrono::duration<float, std::milli>(t_after_pageflip - t_after_vblank).count();

    // Advance to next image (like drmvk line 192)
    _currentImage = (_currentImage + 1) % SWAP_CHAIN_SIZE;

    frame_count++;

    // FPS measurement: Calculate frame time and average FPS
    float current_time = fps_timer.SecsSinceStart();
    float frame_time = current_time - _lastFrameTime;
    _lastFrameTime = current_time;

    // Store frame time in rolling buffer
    _frameTimes[_frameTimeIndex] = frame_time;
    _frameTimeIndex = (_frameTimeIndex + 1) % 10;
    _totalFrameCount++;

    // Print FPS every 10 frames
    if (_totalFrameCount % 10 == 0) {
        // Calculate average frame time over last 10 frames
        float total_time = 0.0f;
        for (int i = 0; i < 10; i++) {
            total_time += _frameTimes[i];
        }
        float avg_frame_time = total_time / 10.0f;
        float avg_fps = (avg_frame_time > 0.0f) ? (1.0f / avg_frame_time) : 0.0f;

        // Print to console with timing breakdown
        float waitPresent_total = time_vblank_wait + time_pageflip;
        float unaccounted = (avg_frame_time * 1000.0f) - waitPresent_total;
        printf("DRM FPS: %.2f | Frame: %.1fms (waitPresent: %.1fms [vblank:%.1fms, flip:%.1fms], app: %.1fms)\n",
               avg_fps, avg_frame_time * 1000.0f,
               waitPresent_total, time_vblank_wait, time_pageflip, unaccounted);
        fflush(stdout);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Get current sub-index (image index, used for fence tracking)
///////////////////////////////////////////////////////////////////////////////

size_t VkSwapChainDRM::subIndex() const {
    return _currentImage;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
