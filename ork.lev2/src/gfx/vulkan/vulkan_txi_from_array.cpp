////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/math/misc_math.h>
#include <ork/kernel/memcpy.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_txidata = logger()->configureChannel("VKTXIDAT2", fvec3(0.8, 0.2, 0.5), false);
static logchannel_ptr_t logchan_txia2d  = logger()->configureChannel("VKTEXARRAY", fvec3(0.8, 0.5, 0.2), false);
constexpr bool DEBUG_TEXARRAY2D         = false;
///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::initTextureArray2DFromData(TextureArray* array, TextureArrayInitData tid) {

  if (DEBUG_TEXARRAY2D) {
    logchan_txia2d->log("///////////////////////////////////////////////////////////");
    logchan_txia2d->log("// VkTextureInterface::initTextureArray2DFromData array<%p>", array);
    logchan_txia2d->log("///////////////////////////////////////////////////////////");
  }
  array->_tex->_texType = ETEXTYPE_2D_ARRAY;
  int num_slices        = int(tid._slices.size());
  std::vector<compressedmipchain_ptr_t> subimagedata;
  subimagedata.resize(num_slices);

  ///////////////////////////
  // scan present subimages
  //  extract max width and height
  //  and load mipchains
  ///////////////////////////
  size_t max_w      = 0;
  size_t max_h      = 0;
  size_t max_levels = 0;
  std::unordered_set<EBufferFormat> formats;

  for (int i = 0; i < num_slices; i++) {
    const auto& slice = tid._slices[i];
    auto subimg       = slice._subimg;
    auto mipchain     = slice._cmipchain;
    if (subimg) {
      array->_images[i] = subimg;
      formats.insert(subimg->_format);
      auto subimg_cmipc = subimg->uncompressedMipChain();
      subimagedata[i]   = subimg_cmipc;
      max_levels        = std::max(max_levels, subimg_cmipc->_levels.size());
      max_w             = std::max(max_w, subimg_cmipc->_width);
      max_h             = std::max(max_h, subimg_cmipc->_height);
    } else if (mipchain) {
      subimagedata[i] = mipchain;
      max_levels      = std::max(max_levels, mipchain->_levels.size());
      max_w           = std::max(max_w, mipchain->_width);
      max_h           = std::max(max_h, mipchain->_height);
      formats.insert(mipchain->_format);
    } else {
      OrkAssert(false);
    }
  }

  if (formats.size() > 1) {
    logchan_txia2d->log("TextureArray2D has multiple formats");
    for (auto fmt : formats) {
      auto fmt_str = EBufferFormatToName(fmt);
      logchan_txia2d->log("  format<%s>", fmt_str.c_str());
    }
    OrkAssert(false);
  }

  max_levels -= 1; // levels include base, so subtract 1 for mip count
  auto format = *formats.begin();

  ///////////////////////////
  // Handle RGB8 to RGBA8 conversion on macOS
  ///////////////////////////
  bool needs_conversion = false;
  if (format == EBufferFormat::RGB8) {
    format           = EBufferFormat::RGBA8;
    needs_conversion = true;
    logchan_txia2d->log("Converting RGB8 to RGBA8 for macOS");
  } else if (format == EBufferFormat::BGR8) {
    format           = EBufferFormat::BGRA8;
    needs_conversion = true;
    logchan_txia2d->log("Converting BGR8 to BGRA8 for macOS");
  } else if (format == EBufferFormat::RGB16) {
    format           = EBufferFormat::RGBA16;
    needs_conversion = true;
    logchan_txia2d->log("Converting RGB16 to RGBA16 for macOS");
  } else if (format == EBufferFormat::RGB32F) {
    format           = EBufferFormat::RGBA32F;
    needs_conversion = true;
    logchan_txia2d->log("Converting RGB32F to RGBA32F for macOS");
  }

  array->_tex->_texFormat = format;

  ///////////////////////////
  // Create Vulkan texture object
  ///////////////////////////

  vktexobj_ptr_t vktex = array->_tex->_impl.makeShared<VulkanTextureObject>(this);
  // Don't clear _img_sampling - will be set when upload completes

  ///////////////////////////
  // Setup image creation parameters
  ///////////////////////////

  uint64_t usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  uint64_t image_params_hash = hashImageCreationParams(
      max_w,      // width
      max_h,      // height
      num_slices, // depth/layers
      format,     // format
      max_levels, // mip levels
      usage);     // usage

  vktex->_format_hash = image_params_hash;

  ///////////////////////////
  // Create VkImage for texture array
  ///////////////////////////

  auto VKICI         = makeVKICI(max_w, max_h, 1, format, max_levels); // depth must be 1 for 2D arrays!
  VKICI->usage       = usage;
  VKICI->arrayLayers = num_slices; // array layers specify the number of slices
  VKICI->imageType   = VK_IMAGE_TYPE_2D;
  VKICI->flags       = 0; // VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT only if from 3d image

  std::string debug_name = array->_tex->_debugName.empty() ? "texture_array" : array->_tex->_debugName;
  // Texture arrays use slot [0] only
  vktex->_imgobj[0] = std::make_shared<VulkanImageObject>(_contextVK, VKICI, debug_name);

  if(0)printf("initTextureArray2DFromData: created image %p for array '%s'\n",
         (void*)vktex->_imgobj[0]->_vkimage,
         debug_name.c_str());

  if (0)
    printf(
        "max_levels<%zu> max_w<%zu> max_h<%zu> num_slices<%d> format<%s>\n",
        max_levels,
        max_w,
        max_h,
        num_slices,
        EBufferFormatToName(format).c_str());

  ///////////////////////////
  // Create image view for the entire array
  ///////////////////////////

  VkImageViewCreateInfo viewInfo{};
  initializeVkStruct(viewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
  viewInfo.image                           = vktex->_imgobj[0]->_vkimage;
  viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  viewInfo.format                          = VkFormatConverter::convertBufferFormat(format);
  viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel   = 0;
  viewInfo.subresourceRange.levelCount     = max_levels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = num_slices;

  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, &viewInfo, nullptr, &vktex->_imgobj[0]->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  // Set debug name for image view
  if (!array->_tex->_debugName.empty()) {
    std::string view_name = array->_tex->_debugName + "_array_view";
    _contextVK->_setObjectDebugName(vktex->_imgobj[0]->_vkimageview, VK_OBJECT_TYPE_IMAGE_VIEW, view_name.c_str());
  }

  // Array textures only use slot [0] (no double-buffering)
  vktex->_vkdescriptor_info[0] = std::make_shared<VkDescriptorImageInfo>();
  vktex->_vkdescriptor_info[0]->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vktex->_vkdescriptor_info[0]->imageView = vktex->_imgobj[0]->_vkimageview;
  vktex->_vksampler = _contextVK->_sampler_base;
  vktex->_vkdescriptor_info[0]->sampler = vktex->_vksampler->_vksampler;
  vktex->_descset_sampling = vktex->_vkdescriptor_info[0];

  ///////////////////////////
  // Set texture properties
  ///////////////////////////

  array->_tex->_impl      = vktex;
  array->_tex->_texFormat = format;
  array->_tex->_width     = max_w;
  array->_tex->_height    = max_h;
  array->_tex->_depth     = num_slices;
  array->_tex->_num_mips  = max_levels;

  array->_width     = max_w;
  array->_height    = max_h;
  array->_maxslices = num_slices;

  ///////////////////////////
  // Upload texture data for each slice
  ///////////////////////////

  // Calculate total staging buffer size needed
  size_t total_staging_size = 0;
  for (int level = 0; level < max_levels; level++) {
    size_t level_w         = max_w >> level;
    size_t level_h         = max_h >> level;
    size_t bytes_per_pixel = 0;
    if (format == EBufferFormat::RGBA8 || format == EBufferFormat::BGRA8) {
      bytes_per_pixel = 4;
    } else if (format == EBufferFormat::RGBA16 || format == EBufferFormat::RGBA16F) {
      bytes_per_pixel = 8; // 4 channels * 2 bytes per channel
    } else if (format == EBufferFormat::RGBA32F) {
      bytes_per_pixel = 16; // 4 channels * 4 bytes per channel
    } else {
      OrkAssert(false); // unsupported format for texture array
    }
    total_staging_size += level_w * level_h * bytes_per_pixel * num_slices;
  }

  // Get staging buffer
  auto poolForSize    = stagingBufferPoolForSrcOfSize(total_staging_size);
  auto staging_buffer = poolForSize->borrowItem();
  secondary_commandbuffer_ptr_t command_buffer;
  _seccmdbufpool_xfer.atomicOp([&](sseccmdbufpool_ptr_t& pool) { command_buffer = pool->borrowItem(); });

  // Create transfer object
  auto transfer = std::make_shared<InFlightTextureTransfer>(_contextVK, staging_buffer, command_buffer);
  vktex->_inflight_transfers.insert(transfer);
  auto cmdbuf_impl = transfer->_command_buffer->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  // Set up completion and cleanup callbacks
  auto tlsema                       = std::make_shared<VulkanCompletionSemaphore>(this->_contextVK);
  cmdbuf_impl->_completionSemaphore = tlsema;

  // Pre-enqueue callback: capture primary CB and add to its pending_cleanup
  cmdbuf_impl->_onPreEnqueueCallback = [command_buffer, ctx = this->_contextVK]() {
    auto pricb = ctx->primary_cb();
    pricb->_secondary_cmdbuffers_pending_cleanup.push_back(command_buffer);
  };
  command_buffer->_debugName = FormatString("texupl.array_%s", array->_tex->_debugName.c_str());

  // Cleanup callback: return CB to pool when primary CB is reset
  cmdbuf_impl->_onCleanupCallback = [command_buffer, pool_ref = &_seccmdbufpool_xfer]() {
    auto impl = command_buffer->_impl.getShared<VkSecondaryCommandBufferImpl>();
    impl->_referenced_images.clear();
    pool_ref->atomicOp([&](sseccmdbufpool_ptr_t& pool) { pool->returnItem(command_buffer); });
  };

  // Keep VkImage alive while CB is in use
  cmdbuf_impl->_referenced_images.push_back(vktex->_imgobj[0]);

  // Completion callback: cleanup transfer and staging buffer when GPU completes
  tlsema->_onComplete = [=]() {
    vktex->_inflight_transfers.erase(transfer);
    poolForSize->returnItem(staging_buffer);
    // Texture array is now ready for sampling
    vktex->_img_sampling = vktex->_imgobj[0];
  };

  ///////////////////////////
  // Copy all slice data to staging buffer
  ///////////////////////////

  uint8_t* staging_data = (uint8_t*)staging_buffer->map(0, total_staging_size, 0);
  size_t staging_offset = 0;

  std::vector<VkBufferImageCopy> copy_regions;

  for (int slice = 0; slice < num_slices; slice++) {
    auto subimg_cmipc = subimagedata[slice];
    if (!subimg_cmipc)
      continue;

    for (int level = 0; level < max_levels && level < subimg_cmipc->_levels.size(); level++) {
      auto& mip     = subimg_cmipc->_levels[level];
      size_t mip_w  = mip._width;
      size_t mip_h  = mip._height;
      auto mip_data = mip._data;

      // Handle conversion if needed
      if (needs_conversion && mip_data) {
        // Convert RGB to RGBA (both RGB8 and RGB16)
        size_t bytes_per_pixel = (format == EBufferFormat::RGBA8) ? 1 : 2; // 1 for 8-bit, 2 for 16-bit
        size_t src_size        = mip_w * mip_h * 3 * bytes_per_pixel;
        size_t dst_size        = mip_w * mip_h * 4 * bytes_per_pixel;

        const uint8_t* src = (const uint8_t*)mip_data->data();
        uint8_t* dst       = staging_data + staging_offset;

        if (format == EBufferFormat::RGBA8) {
          // Convert RGB8 to RGBA8
          for (size_t i = 0; i < mip_w * mip_h; i++) {
            dst[i * 4 + 0] = src[i * 3 + 0]; // R
            dst[i * 4 + 1] = src[i * 3 + 1]; // G
            dst[i * 4 + 2] = src[i * 3 + 2]; // B
            dst[i * 4 + 3] = 255;            // A
          }
        } else if (format == EBufferFormat::BGRA8) {
          // Convert BGR8 to BGRA8
          for (size_t i = 0; i < mip_w * mip_h; i++) {
            dst[i * 4 + 0] = src[i * 3 + 0]; // B
            dst[i * 4 + 1] = src[i * 3 + 1]; // G
            dst[i * 4 + 2] = src[i * 3 + 2]; // R
            dst[i * 4 + 3] = 255;            // A
          }
        } else if (format == EBufferFormat::RGBA16) {
          // Convert RGB16 to RGBA16
          const uint16_t* src16 = (const uint16_t*)src;
          uint16_t* dst16       = (uint16_t*)dst;
          for (size_t i = 0; i < mip_w * mip_h; i++) {
            dst16[i * 4 + 0] = src16[i * 3 + 0]; // R
            dst16[i * 4 + 1] = src16[i * 3 + 1]; // G
            dst16[i * 4 + 2] = src16[i * 3 + 2]; // B
            dst16[i * 4 + 3] = 65535;            // A (max value for 16-bit)
          }
        } else if (format == EBufferFormat::RGBA32F) {
          // Convert RGB32F to RGBA32F
          const float* src32 = (const float*)src;
          float* dst32       = (float*)dst;
          for (size_t i = 0; i < mip_w * mip_h; i++) {
            dst32[i * 4 + 0] = src32[i * 3 + 0]; // R
            dst32[i * 4 + 1] = src32[i * 3 + 1]; // G
            dst32[i * 4 + 2] = src32[i * 3 + 2]; // B
            dst32[i * 4 + 3] = 1.0f;             // A
          }
          dst_size = mip_w * mip_h * 4 * sizeof(float);
        } else {
          OrkAssert(false); // Unsupported format for conversion
        }

        // Add copy region
        VkBufferImageCopy region{};
        region.bufferOffset                    = staging_offset;
        region.bufferRowLength                 = 0;
        region.bufferImageHeight               = 0;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = level;
        region.imageSubresource.baseArrayLayer = slice;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset                     = {0, 0, 0};
        region.imageExtent                     = {uint32_t(mip_w), uint32_t(mip_h), 1};
        copy_regions.push_back(region);

        staging_offset += dst_size;
      } else {
        // Direct copy
        memcpy(staging_data + staging_offset, mip_data->data(), mip_data->length());

        // Add copy region
        VkBufferImageCopy region{};
        region.bufferOffset                    = staging_offset;
        region.bufferRowLength                 = 0;
        region.bufferImageHeight               = 0;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = level;
        region.imageSubresource.baseArrayLayer = slice;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset                     = {0, 0, 0};
        region.imageExtent                     = {uint32_t(mip_w), uint32_t(mip_h), 1};
        copy_regions.push_back(region);

        staging_offset += mip_data->length();
      }
    }
  }

  staging_buffer->unmap();

  ///////////////////////////
  // Record transition to transfer destination
  ///////////////////////////

  auto barrier = createImageBarrier(
      vktex->_imgobj[0]->_vkimage,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VkAccessFlagBits(0),
      VK_ACCESS_TRANSFER_WRITE_BIT);

  // Update barrier for all layers
  barrier->subresourceRange.layerCount = num_slices;
  barrier->subresourceRange.levelCount = max_levels;

  vkCmdPipelineBarrier(
      vk_cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, barrier.get());

  ///////////////////////////
  // Record transfer from staging to image
  ///////////////////////////

  vkCmdCopyBufferToImage(
      vk_cmdbuf,
      staging_buffer->_vkbuffer,
      vktex->_imgobj[0]->_vkimage,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      copy_regions.size(),
      copy_regions.data());

  ///////////////////////////
  // Record transition to shader read
  ///////////////////////////

  barrier->oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier->newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      0,
      0,
      nullptr,
      0,
      nullptr,
      1,
      barrier.get());

  ///////////////////////////
  // Enqueue command buffer
  ///////////////////////////

  _contextVK->endRecordCommandBuffer(transfer->_command_buffer);
  _contextVK->enqueueDeferredOneShotCommand(transfer->_command_buffer);

  // Update descriptor to reflect new layout after transition
  if (vktex->_vkdescriptor_info[0]) {
    vktex->_vkdescriptor_info[0]->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  // Apply sampling mode based on mip count
  // Only update filtering mode if mipmaps present, preserve address modes
  if (max_levels > 3) {
    auto& samplingMode           = array->_tex->TexSamplingMode();
    samplingMode._texFiltModeMin = ETextureMinifyFilterMode::LINEAR_MIPMAP_LINEAR;
    samplingMode._texFiltModeMag = ETextureMagnifyFilterMode::LINEAR;
    // Keep existing address modes (CLAMP/WRAP) that were set externally
  }
  this->ApplySamplingMode(array->_tex.get());

  array->_tex->_dirty = false;
  array->_dirty_slices.clear();
  array->_free_slices.clear();

  if (DEBUG_TEXARRAY2D) {
    logchan_txia2d->log(
        "TextureArray created: maxw<%d> maxh<%d> depth<%d> format<%s>",
        max_w,
        max_h,
        num_slices,
        EBufferFormatToName(format).c_str());
    logchan_txia2d->log("///////////////////////////////////////////////////////////");
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::initTextureArray2DAsync(TextureArray* texture_array) { // final

  if (not texture_array->_isDirty) {
    return;
  }

  /////////////////////////////////
  // enqueue texture array initialization on a secondary command buffer
  // and enqueue it for execution
  /////////////////////////////////

  auto cmdbuf = _contextVK->beginRecordCommandBuffer("initTextureArray2D_transition");
  auto cmdbuf_impl = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_cmdbuf = cmdbuf_impl->_vkcmdbuf;
  _enqueueInitTextureArray2DOnCB(texture_array,vk_cmdbuf);
  _contextVK->endRecordCommandBuffer(cmdbuf);
  auto vktex_arr = texture_array->_tex->_impl.getShared<VulkanTextureObject>();
  cmdbuf_impl->_referenced_images.push_back(vktex_arr->_imgobj[0]);
  _contextVK->enqueueDeferredOneShotCommand(cmdbuf);

  /////////////////////////////////
  // Update the image object's tracked layout
  /////////////////////////////////

  texture_array->_isDirty = false;

  /////////////////////////////////

  if (DEBUG_TEXARRAY2D) {
    int w                = texture_array->_width;
    int h                = texture_array->_height;
    int num_slices       = texture_array->_maxslices;
    EBufferFormat format = texture_array->_format;
    logchan_txia2d->log(
        "VkTextureInterface::initTextureArray2DAsync created blank array w<%d> h<%d> slices<%d> format<%s>",
        w,
        h,
        num_slices,
        EBufferFormatToName(format).c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::initTextureArray2D(TextureArray* texture_array) { // final

  /////////////////////////////////////////////////////
  // Initialize texture array on primary command buffer
  /////////////////////////////////////////////////////

  // Suspend render pass if active - barriers cannot be inside dynamic rendering
  bool was_active = _contextVK->_renderPassActive;
  if (was_active) {
    _contextVK->suspendRenderPass();
  }

  auto primary_cb = _contextVK->primary_cb();
  auto vk_cmdbuf = primary_cb->_vkcmdbuf;

  if(0)printf("initTextureArray2D: array='%s' vk_cmdbuf=%p primary_cb=%p\n",
         texture_array->_tex->_debugName.c_str(), (void*)vk_cmdbuf,
         (void*)primary_cb.get());

  _enqueueInitTextureArray2DOnCB(texture_array,vk_cmdbuf);

  // Resume render pass if it was active
  if (was_active) {
    _contextVK->resumeRenderPass();
  }

  /////////////////////////////////
  // Update the image object's tracked layout
  /////////////////////////////////

  texture_array->_isDirty = false;

  /////////////////////////////////

  if (DEBUG_TEXARRAY2D) {
    int w                = texture_array->_width;
    int h                = texture_array->_height;
    int num_slices       = texture_array->_maxslices;
    EBufferFormat format = texture_array->_format;
    logchan_txia2d->log(
        "VkTextureInterface::initTextureArray2D created blank array w<%d> h<%d> slices<%d> format<%s>",
        w,
        h,
        num_slices,
        EBufferFormatToName(format).c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::_enqueueInitTextureArray2DOnCB(TextureArray* texture_array, VkCommandBuffer vk_cmdbuf) {

  bool w_mips          = texture_array->_requires_mips;
  int w                = texture_array->_width;
  int h                = texture_array->_height;
  int num_slices       = texture_array->_maxslices;
  EBufferFormat format = texture_array->_format;

  OrkAssert(num_slices > 0);

  // Handle RGB8 conversion on macOS
  if (format == EBufferFormat::RGB8) {
    format = EBufferFormat::RGBA8;
  } else if (format == EBufferFormat::BGR8) {
    format = EBufferFormat::BGRA8;
  } else if (format == EBufferFormat::RGB32F) {
    format = EBufferFormat::RGBA32F;
  }

  texture_array->_tex->_texType   = ETEXTYPE_2D_ARRAY;
  texture_array->_tex->_texFormat = format;
  texture_array->_tex->_width     = w;
  texture_array->_tex->_height    = h;
  texture_array->_tex->_depth     = num_slices;

  // Calculate mip levels
  int num_levels = 1;
  if (w_mips) {
    int lw = w;
    int lh = h;
    while ((lw >= 8) && (lh >= 8)) {
      lw = lw >> 1;
      lh = lh >> 1;
      num_levels++;
    }
    texture_array->_num_mips = num_levels;
    texture_array->_tex->_num_mips = num_levels;
  }

  // Create texture object
  vktexobj_ptr_t vktex = texture_array->_tex->_impl.makeShared<VulkanTextureObject>(this);

  // Setup usage flags
  uint64_t usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  switch (format) {
    case EBufferFormat::Z16F:
    case EBufferFormat::Z24S8:
    case EBufferFormat::Z32F:
    case EBufferFormat::Z32FS8:
      usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      break;
    default:
      break;
  }

  /////////////////////////////////
  // Apply sampling mode based on mip count
  // Only update filtering mode if mipmaps present, preserve address modes
  /////////////////////////////////

  if (num_levels > 3) {
    auto& samplingMode           = texture_array->_tex->TexSamplingMode();
    samplingMode._texFiltModeMin = ETextureMinifyFilterMode::LINEAR_MIPMAP_LINEAR;
    samplingMode._texFiltModeMag = ETextureMagnifyFilterMode::LINEAR;
    // Keep existing address modes (CLAMP/WRAP) that were set externally
  }
  this->ApplySamplingMode(texture_array->_tex.get());

  /////////////////////////////////
  // Create image
  /////////////////////////////////

  auto VKICI         = makeVKICI(w, h, 1, format, num_levels); // depth must be 1 for 2D arrays!
  VKICI->usage       = usage;
  VKICI->arrayLayers = num_slices; // array layers specify the number of slices
  VKICI->imageType   = VK_IMAGE_TYPE_2D;

  // Texture arrays use slot [0] only
  vktex->_imgobj[0] = std::make_shared<VulkanImageObject>(_contextVK, VKICI);

  if(0)printf("initTextureArray2D: created image %p for array '%s'\n",
         (void*)vktex->_imgobj[0]->_vkimage,
         texture_array->_tex->_debugName.c_str());

  // Create image view
  VkImageViewCreateInfo viewInfo{};
  initializeVkStruct(viewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
  viewInfo.image                         = vktex->_imgobj[0]->_vkimage;
  viewInfo.viewType                      = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  viewInfo.format                        = VkFormatConverter::convertBufferFormat(format);
  viewInfo.subresourceRange.aspectMask   = (format == EBufferFormat::Z32F) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount   = num_levels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = num_slices;

  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, &viewInfo, nullptr, &vktex->_imgobj[0]->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  // Array textures only use slot [0] (no double-buffering)
  vktex->_vkdescriptor_info[0] = std::make_shared<VkDescriptorImageInfo>();
  vktex->_vkdescriptor_info[0]->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vktex->_vkdescriptor_info[0]->imageView = vktex->_imgobj[0]->_vkimageview;
  vktex->_vksampler = _contextVK->_sampler_base;
  vktex->_vkdescriptor_info[0]->sampler = vktex->_vksampler->_vksampler;
  vktex->_descset_sampling = vktex->_vkdescriptor_info[0];

  vktex->_imgview_hash.init();
  vktex->_imgview_hash.accumulateItem(vktex->_imgobj[0]->_serial_number);
  vktex->_imgview_hash.finish();

  texture_array->_tex->_impl = vktex;

  // Transition the blank image to shader read-only layout
  // This is needed for texture arrays that may never get data uploaded

  // Clear the image first (to black/transparent)
  bool is_depth = false;
  switch (format) {
    case EBufferFormat::Z16F:
    case EBufferFormat::Z24S8:
    case EBufferFormat::Z32F:
    case EBufferFormat::Z32FS8:
      is_depth = true;
      break;
    default:
      is_depth = false;
      break;
  }
  auto clear_barrier = createImageBarrier(
      vktex->_imgobj[0]->_vkimage,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VkAccessFlagBits(0),
      VK_ACCESS_TRANSFER_WRITE_BIT);
  clear_barrier->subresourceRange.aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  clear_barrier->subresourceRange.levelCount = num_levels;
  clear_barrier->subresourceRange.layerCount = num_slices;

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      0, 0, nullptr, 0, nullptr, 1, clear_barrier.get());

  // Clear based on format type
  if (is_depth) {
    VkClearDepthStencilValue clear_value = {1.0f, 0};
    VkImageSubresourceRange range = {
        VK_IMAGE_ASPECT_DEPTH_BIT,
        0, static_cast<uint32_t>(num_levels),
        0, static_cast<uint32_t>(num_slices)
    };
    vkCmdClearDepthStencilImage(vk_cmdbuf, vktex->_imgobj[0]->_vkimage,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range);
  } else {
    VkClearColorValue clear_color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    VkImageSubresourceRange range = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, static_cast<uint32_t>(num_levels),
        0, static_cast<uint32_t>(num_slices)
    };
    vkCmdClearColorImage(vk_cmdbuf, vktex->_imgobj[0]->_vkimage,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &range);
  }

  // Now transition to shader read-only
  auto read_barrier = createImageBarrier(
      vktex->_imgobj[0]->_vkimage,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_SHADER_READ_BIT);
  read_barrier->subresourceRange.aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  read_barrier->subresourceRange.levelCount = num_levels;
  read_barrier->subresourceRange.layerCount = num_slices;

  if(0)printf("  Transitioning image %p to SHADER_READ_ONLY_OPTIMAL (is_depth=%d)\n",
         (void*)vktex->_imgobj[0]->_vkimage, is_depth);

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
      0, 0, nullptr, 0, nullptr, 1, read_barrier.get());

  vktex->_imgobj[0]->_currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  if(0)printf("  Transition recorded, _currentLayout set to SHADER_READ_ONLY_OPTIMAL\n");

  // Texture array is now ready for sampling
  vktex->_img_sampling = vktex->_imgobj[0];
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::_updateTextureArraySlice(TextureArraySliceRef* slice_ref, compressedmipchain_ptr_t mipc) {

  auto array      = slice_ref->_array;
  int slice_index = slice_ref->_slice;

  // Check if we need format conversion
  bool needs_conversion = false;
  if (mipc->_format == EBufferFormat::RGB8 && array->_tex->_texFormat == EBufferFormat::RGBA8) {
    needs_conversion = true;
    if (DEBUG_TEXARRAY2D) {
      logchan_txia2d->log("Converting RGB8 to RGBA8 for slice %d update", slice_index);
    }
  } else if (mipc->_format == EBufferFormat::BGR8 && array->_tex->_texFormat == EBufferFormat::BGRA8) {
    needs_conversion = true;
    if (DEBUG_TEXARRAY2D) {
      logchan_txia2d->log("Converting BGR8 to BGRA8 for slice %d update", slice_index);
    }
  } else if (mipc->_format == EBufferFormat::RGB32F && array->_tex->_texFormat == EBufferFormat::RGBA32F) {
    needs_conversion = true;
    if (DEBUG_TEXARRAY2D) {
      logchan_txia2d->log("Converting RGB32F to RGBA32F for slice %d update", slice_index);
    }
  }

  auto vktex = array->_tex->_impl.getShared<VulkanTextureObject>();
  vktex->_dataVersion++;

  int num_levels = int(mipc->_levels.size());

  // IMPORTANT: Clamp to the texture array's actual mip levels
  int array_mip_levels = array->_tex->_num_mips;

    if(0)printf("array_mip_levels<%d>\n", array_mip_levels);
    if(0)printf("num_levels<%d>\n", num_levels);

    num_levels           = std::min(num_levels, array_mip_levels);
    
  // Calculate staging buffer size
  size_t staging_size = 0;
  for (int level = 0; level < num_levels; level++) {
    auto& mip = mipc->_levels[level];
    if (needs_conversion) {
      // 3 to 4 component conversion needs more space
      if (mipc->_format == EBufferFormat::RGB32F) {
        staging_size += mip._width * mip._height * 4 * sizeof(float);
      } else {
        staging_size += mip._width * mip._height * 4;
      }
    } else {
      staging_size += mip._data->length();
    }
  }

  // Get staging buffer and command buffer
    if(0)printf("staging_size<%zu>\n", staging_size);
  auto poolForSize    = stagingBufferPoolForSrcOfSize(staging_size);
    if(0)printf("poolForSize<%p>\n", (void*) poolForSize.get() );
  auto staging_buffer = poolForSize->borrowItem();
  secondary_commandbuffer_ptr_t command_buffer;
  _seccmdbufpool_xfer.atomicOp([&](sseccmdbufpool_ptr_t& pool) { command_buffer = pool->borrowItem(); });

  // Create transfer
  auto transfer = std::make_shared<InFlightTextureTransfer>(_contextVK, staging_buffer, command_buffer);
  vktex->_inflight_transfers.insert(transfer);

  auto cmdbuf_impl   = command_buffer->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_sec_cmdbuf = cmdbuf_impl->_vkcmdbuf;

  if (DEBUG_TEXARRAY2D) {
    logchan_txia2d->log(
        "updateTextureArraySlice vktex<%p> imgobj<%p:%zx:%zx> staging_size<%zu> num_levels<%d>", //
        (void*)vktex.get(),                                                                      //
        vktex->_imgobj[0].get(),                                                                 //
        vktex->_imgobj[0]->_vkimage,                                                             //
        vktex->_imgobj[0]->_vkimageview,                                                         //
        staging_size,                                                                            //
        num_levels);
  }

  // Setup completion and cleanup callbacks
  auto tlsema                       = std::make_shared<VulkanCompletionSemaphore>(_contextVK);
  cmdbuf_impl->_completionSemaphore = tlsema;

  // Pre-enqueue callback: capture primary CB and add to its pending_cleanup
  cmdbuf_impl->_onPreEnqueueCallback = [command_buffer, ctx = this->_contextVK]() {
    auto pricb = ctx->primary_cb();
    pricb->_secondary_cmdbuffers_pending_cleanup.push_back(command_buffer);
  };

  // Cleanup callback: return CB to pool when primary CB is reset
  cmdbuf_impl->_onCleanupCallback = [command_buffer, pool_ref = &_seccmdbufpool_xfer]() {
    auto impl = command_buffer->_impl.getShared<VkSecondaryCommandBufferImpl>();
    impl->_referenced_images.clear();
    pool_ref->atomicOp([&](sseccmdbufpool_ptr_t& pool) { pool->returnItem(command_buffer); });
  };

  // Keep VkImage alive while CB is in use
  cmdbuf_impl->_referenced_images.push_back(vktex->_imgobj[0]);

  // Completion callback: cleanup transfer and staging buffer when GPU completes
  tlsema->_onComplete = [=]() {
    vktex->_inflight_transfers.erase(transfer);
    poolForSize->returnItem(staging_buffer);
  };

  // Copy data to staging buffer
  uint8_t* staging_data = (uint8_t*)staging_buffer->map(0, staging_size, 0);
  size_t offset         = 0;
  std::vector<VkBufferImageCopy> regions;

  for (int level = 0; level < num_levels; level++) {
    auto& mip          = mipc->_levels[level];
    size_t mip_w       = mip._width;
    size_t mip_h       = mip._height;
    auto mip_data      = mip._data;
    const uint8_t* src = (const uint8_t*)mip_data->data();
    uint8_t* dst       = staging_data + offset;
    if (needs_conversion && mip_data) {
      // Convert RGB to RGBA (both RGB8 and RGB16)
      size_t bytes_per_pixel = (array->_tex->_texFormat == EBufferFormat::RGBA8) ? 1 : 2; // 1 for 8-bit, 2 for 16-bit
      size_t src_size        = mip_w * mip_h * 3 * bytes_per_pixel;
      size_t dst_size        = mip_w * mip_h * 4 * bytes_per_pixel;

      if (array->_tex->_texFormat == EBufferFormat::RGBA8) {
        // Convert RGB8 to RGBA8
        for (size_t i = 0; i < mip_w * mip_h; i++) {
          dst[i * 4 + 0] = src[i * 3 + 0]; // R
          dst[i * 4 + 1] = src[i * 3 + 1]; // G
          dst[i * 4 + 2] = src[i * 3 + 2]; // B
          dst[i * 4 + 3] = 255;            // A
        }
      } else if (array->_tex->_texFormat == EBufferFormat::BGRA8) {
        // Convert BGR8 to BGRA8
        for (size_t i = 0; i < mip_w * mip_h; i++) {
          dst[i * 4 + 0] = src[i * 3 + 0]; // B
          dst[i * 4 + 1] = src[i * 3 + 1]; // G
          dst[i * 4 + 2] = src[i * 3 + 2]; // R
          dst[i * 4 + 3] = 255;            // A
        }
      } else if (array->_tex->_texFormat == EBufferFormat::RGBA16) {
        // Convert RGB16 to RGBA16
        const uint16_t* src16 = (const uint16_t*)src;
        uint16_t* dst16       = (uint16_t*)dst;
        for (size_t i = 0; i < mip_w * mip_h; i++) {
          dst16[i * 4 + 0] = src16[i * 3 + 0]; // R
          dst16[i * 4 + 1] = src16[i * 3 + 1]; // G
          dst16[i * 4 + 2] = src16[i * 3 + 2]; // B
          dst16[i * 4 + 3] = 65535;            // A (max value for 16-bit)
        }
      } else if (array->_tex->_texFormat == EBufferFormat::RGBA32F) {
        // Convert RGB32F to RGBA32F
        const float* src32 = (const float*)src;
        float* dst32       = (float*)dst;
        for (size_t i = 0; i < mip_w * mip_h; i++) {
          dst32[i * 4 + 0] = src32[i * 3 + 0]; // R
          dst32[i * 4 + 1] = src32[i * 3 + 1]; // G
          dst32[i * 4 + 2] = src32[i * 3 + 2]; // B
          dst32[i * 4 + 3] = 1.0f;             // A
        }
        dst_size = mip_w * mip_h * 4 * sizeof(float);
      }
    } else {
      // Direct copy
      memcpy_fast(staging_data + offset, src, mip_data->length());
    }

    VkBufferImageCopy region{};
    region.bufferOffset                    = offset;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = level;
    region.imageSubresource.baseArrayLayer = slice_index;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = {0, 0, 0};
    region.imageExtent                     = {uint32_t(mip_w), uint32_t(mip_h), 1};
    regions.push_back(region);
    if (needs_conversion) {
      offset += mip_w * mip_h * 4;
    } else {
      offset += mip._data->length();
    }
  }

  staging_buffer->unmap();

  // Transition image layout to transfer destination.
  // Use UNDEFINED as old layout — we're about to overwrite this slice entirely,
  // and the image may be in SHADER_READ_ONLY or GENERAL depending on init path.
  auto barrier = createImageBarrier(
      vktex->_imgobj[0]->_vkimage,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VkAccessFlagBits(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT),
      VK_ACCESS_TRANSFER_WRITE_BIT);

  // Update barrier for specific slice
  barrier->subresourceRange.baseArrayLayer = slice_index;
  barrier->subresourceRange.layerCount     = 1;
  barrier->subresourceRange.levelCount     = num_levels;

  vkCmdPipelineBarrier(
      vk_sec_cmdbuf,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      0,
      0,
      nullptr,
      0,
      nullptr,
      1,
      barrier.get());

  // Record commands
  vkCmdCopyBufferToImage(
      vk_sec_cmdbuf,
      staging_buffer->_vkbuffer,
      vktex->_imgobj[0]->_vkimage,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      regions.size(),
      regions.data());

  // Transition back to shader read
  barrier->oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier->newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  // barrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT;
  vkCmdPipelineBarrier(
      vk_sec_cmdbuf,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      0,
      0,
      nullptr,
      0,
      nullptr,
      1,
      barrier.get());

  // Submit
  _contextVK->endRecordCommandBuffer(transfer->_command_buffer);
  _contextVK->enqueueDeferredOneShotCommand(transfer->_command_buffer);

  if (DEBUG_TEXARRAY2D) {
    logchan_txia2d->log("Updated texture array slice %d", slice_index);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::updateTextureArraySlice(TextureArraySliceRef* slice_ref, image_ptr_t img) {
  auto array      = slice_ref->_array;
  int slice_index = slice_ref->_slice;

  bool ok = true;
  ok &= (array->_tex->_texType == ETEXTYPE_2D_ARRAY);
  ok &= (slice_index < array->_tex->_depth);
  ok &= (img != nullptr);
  ok &= (img->_width == array->_tex->_width);
  ok &= (img->_height == array->_tex->_height);
  ok &= (img->_depth == 1);

  // Check original format before conversion
  auto expected_format = array->_tex->_texFormat;
  // On macOS, 3-component images are valid if the array format is the 4-component equivalent
  if (expected_format == EBufferFormat::RGBA8 && img->_format == EBufferFormat::RGB8) {
    ok &= true; // This is OK, we'll convert during copy
  } else if (expected_format == EBufferFormat::BGRA8 && img->_format == EBufferFormat::BGR8) {
    ok &= true; // This is OK, we'll convert during copy
  } else if (expected_format == EBufferFormat::RGBA32F && img->_format == EBufferFormat::RGB32F) {
    ok &= true; // This is OK, we'll convert during copy
  } else {
    ok &= (img->_format == expected_format);
  }

  if (!ok) {
    logchan_txia2d->log("ERROR: updateTextureArraySlice validation failed");
    logchan_txia2d->log(
        "slice_index<%d> array_depth<%d> img_format<%s> array_format<%s>",
        slice_index,
        array->_tex->_depth,
        EBufferFormatToName(img->_format).c_str(),
        EBufferFormatToName(array->_tex->_texFormat).c_str());
    logchan_txia2d->log(
        "width<%d> height<%d> img_width<%d> img_height<%d>", array->_tex->_width, array->_tex->_height, img->_width, img->_height);
    OrkAssert(false);
    return;
  }
  // Get mipchain from image
  compressedmipchain_ptr_t mipc;
  if (array->_requires_mips) {
    // If the texture array requires mipmaps, use the mipchain from the image
    mipc = img->uncompressedMipChain();
  } else {
    mipc = img->uncompressedSingleMipChain();
  }
  _updateTextureArraySlice(slice_ref, mipc);
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::updateTextureArray(TextureArray* array) { // final
  // Check if array needs initialization or has no GPU resources
  bool needs_init = !array->_tex->_impl.isShared<VulkanTextureObject>() || array->_isDirty;

  // If we need to initialize but have images, we should use initTextureArray2DFromData instead
  if (needs_init && !array->_images.empty()) {
    logchan_txia2d->log("updateTextureArray: initializing from data with %zu images", array->_images.size());

    // Build TextureArrayInitData from current images
    // Need to ensure we have the right number of slices with proper ordering
    TextureArrayInitData init_data;

    // Create slices for all indices up to maxslices
    for (size_t i = 0; i < array->_maxslices; i++) {
      TextureArrayInitSubItem slice_item;

      // Check if we have an image for this slice
      auto img_iter = array->_images.find(i);
      if (img_iter != array->_images.end() && img_iter->second) {
        slice_item._subimg = img_iter->second;
      } else {
        // Broadcast: fill empty slots with a copy of the first loaded image
        image_ptr_t fill_img;
        for (auto& [idx, img] : array->_images) {
          if (img) { fill_img = img; break; }
        }
        if (fill_img) {
          slice_item._subimg = std::make_shared<Image>(fill_img->clone());
        } else {
          auto blank_img = std::make_shared<Image>();
          blank_img->initWithFormat(array->_width, array->_height, array->_format);
          slice_item._subimg = blank_img;
        }
      }

      init_data._slices.push_back(slice_item);
    }

    // Initialize with the data (this will handle mip levels correctly)
    initTextureArray2DFromData(array, init_data);

    // Clear dirty flags since initTextureArray2DFromData uploads everything
    array->_dirty_slices.clear();
    array->_isDirty = false;
    return;
  }

  // If already initialized but just needs basic structure
  if (needs_init) {
    initTextureArray2D(array);
  }

  // Early exit if no dirty slices
  if (array->_dirty_slices.empty()) {
    return;
  }

  logchan_txia2d->log("updateTextureArray: processing %zu dirty slices", array->_dirty_slices.size());

  // Process each dirty slice
  for (size_t slice_index : array->_dirty_slices) {
    auto img_iter = array->_images.find(slice_index);
    if (img_iter != array->_images.end()) {
      auto img = img_iter->second;

      if (img) {
        logchan_txia2d->log("Updating dirty slice %zu", slice_index);

        // Create temporary slice ref for this index
        TextureArraySliceRef slice_ref(array, slice_index);

        // Use existing slice update mechanism
        updateTextureArraySlice(&slice_ref, img);
      } else {
        logchan_txia2d->log("WARNING: Slice %zu marked dirty but has null image", slice_index);
      }
    } else {
      logchan_txia2d->log("WARNING: Slice %zu marked dirty but not found in _images", slice_index);
    }
  }

  // Clear dirty flags after successful upload
  array->_dirty_slices.clear();
  array->_isDirty = false;

  logchan_txia2d->log("updateTextureArray: completed, all slices clean");
}

  ///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
