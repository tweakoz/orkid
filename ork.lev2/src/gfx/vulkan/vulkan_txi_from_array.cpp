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
static logchannel_ptr_t logchan_txidata = logger()->createChannel("VKTXIDAT", fvec3(0.8, 0.2, 0.5), true);
static logchannel_ptr_t logchan_txia2d  = logger()->createChannel("VKTEXARRAY", fvec3(0.8, 0.5, 0.2), true);
constexpr bool DEBUG_TEXARRAY2D = true;
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
#if defined(__APPLE__)
  if (format == EBufferFormat::RGB8) {
    format           = EBufferFormat::RGBA8;
    needs_conversion = true;
    logchan_txia2d->log("Converting RGB8 to RGBA8 for macOS");
  }
#endif

  array->_tex->_texFormat = format;

  ///////////////////////////
  // Create Vulkan texture object
  ///////////////////////////

  vktexobj_ptr_t vktex = array->_tex->_impl.makeShared<VulkanTextureObject>(this);

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

  vktex->_image_params_hash = image_params_hash;

  ///////////////////////////
  // Create VkImage for texture array
  ///////////////////////////

  auto VKICI         = makeVKICI(max_w, max_h, 1, format, max_levels); // depth must be 1 for 2D arrays!
  VKICI->usage       = usage;
  VKICI->arrayLayers = num_slices; // array layers specify the number of slices
  VKICI->imageType   = VK_IMAGE_TYPE_2D;
  VKICI->flags       = 0; // VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT only if from 3d image

  vktex->_imgobj = std::make_shared<VulkanImageObject>(_contextVK, VKICI);

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
  viewInfo.image                           = vktex->_imgobj->_vkimage;
  viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  viewInfo.format                          = VkFormatConverter::convertBufferFormat(format);
  viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel   = 0;
  viewInfo.subresourceRange.levelCount     = max_levels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = num_slices;

  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, &viewInfo, nullptr, &vktex->_imgobj->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  vktex->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vktex->_vkdescriptor_info.imageView   = vktex->_imgobj->_vkimageview;
  vktex->_vksampler                     = _contextVK->_sampler_base;

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
    size_t bytes_per_pixel = (format == EBufferFormat::RGBA8) ? 4 : 0; // extend for other formats
    total_staging_size += level_w * level_h * bytes_per_pixel * num_slices;
  }

  // Get staging buffer
  auto poolForSize    = stagingBufferPoolForSrcOfSize(total_staging_size);
  auto staging_buffer = poolForSize->borrowItem();
  auto command_buffer = _seccmdbufpool_xfer->borrowItem();

  // Create transfer object
  auto transfer = std::make_shared<InFlightTextureTransfer>(_contextVK, staging_buffer, command_buffer);
  vktex->_inflight_transfers.insert(transfer);
  auto cmdbuf_impl = transfer->_command_buffer->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  // Set up completion callback
  auto tlsema                       = std::make_shared<VulkanCompletionSemaphore>(this->_contextVK);
  cmdbuf_impl->_completionSemaphore = tlsema;
  tlsema->_onComplete               = [=]() {
    vktex->_inflight_transfers.erase(transfer);
    poolForSize->returnItem(staging_buffer);
    _seccmdbufpool_xfer->returnItem(transfer->_command_buffer);
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
        // Convert RGB8 to RGBA8
        size_t src_size = mip_w * mip_h * 3;
        size_t dst_size = mip_w * mip_h * 4;

        const uint8_t* src = (const uint8_t*)mip_data->data();
        uint8_t* dst       = staging_data + staging_offset;

        for (size_t i = 0; i < mip_w * mip_h; i++) {
          dst[i * 4 + 0] = src[i * 3 + 0]; // R
          dst[i * 4 + 1] = src[i * 3 + 1]; // G
          dst[i * 4 + 2] = src[i * 3 + 2]; // B
          dst[i * 4 + 3] = 255;            // A
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
      vktex->_imgobj->_vkimage,
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
      vktex->_imgobj->_vkimage,
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

void VkTextureInterface::initTextureArray2D(TextureArray* texture_array) {
  if (!texture_array->_isDirty) {
    return;
  }

  bool w_mips          = texture_array->_requires_mips;
  int w                = texture_array->_width;
  int h                = texture_array->_height;
  int num_slices       = texture_array->_maxslices;
  EBufferFormat format = texture_array->_format;

  OrkAssert(num_slices > 0);

  // Handle RGB8 conversion on macOS
#if defined(__APPLE__)
  if (format == EBufferFormat::RGB8) {
    format = EBufferFormat::RGBA8;
  }
#endif

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
  }

  // Create texture object
  vktexobj_ptr_t vktex = texture_array->_tex->_impl.makeShared<VulkanTextureObject>(this);

  // Setup usage flags
  uint64_t usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  if (format == EBufferFormat::Z32F) {
    usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  }

  // Create image
  auto VKICI         = makeVKICI(w, h, 1, format, num_levels); // depth must be 1 for 2D arrays!
  VKICI->usage       = usage;
  VKICI->arrayLayers = num_slices; // array layers specify the number of slices
  VKICI->imageType   = VK_IMAGE_TYPE_2D;

  vktex->_imgobj = std::make_shared<VulkanImageObject>(_contextVK, VKICI);

  // Create image view
  VkImageViewCreateInfo viewInfo{};
  initializeVkStruct(viewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
  viewInfo.image                         = vktex->_imgobj->_vkimage;
  viewInfo.viewType                      = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  viewInfo.format                        = VkFormatConverter::convertBufferFormat(format);
  viewInfo.subresourceRange.aspectMask   = (format == EBufferFormat::Z32F) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount   = num_levels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = num_slices;

  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, &viewInfo, nullptr, &vktex->_imgobj->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  vktex->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vktex->_vkdescriptor_info.imageView   = vktex->_imgobj->_vkimageview;
  vktex->_vksampler                     = _contextVK->_sampler_base;

  texture_array->_tex->_impl = vktex;
  texture_array->_isDirty    = false;

  if(DEBUG_TEXARRAY2D) {
  logchan_txia2d->log(
      "VkTextureInterface::initTextureArray2D created blank array w<%d> h<%d> slices<%d> format<%s>",
      w,
      h,
      num_slices,
      EBufferFormatToName(format).c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::_updateTextureArraySlice(TextureArraySliceRef* slice_ref, compressedmipchain_ptr_t mipc) {

  auto array      = slice_ref->_array;
  int slice_index = slice_ref->_slice;

  // Check if we need format conversion
  bool needs_conversion = false;
#if defined(__APPLE__)
  if (mipc->_format == EBufferFormat::RGB8 && array->_tex->_texFormat == EBufferFormat::RGBA8) {
    needs_conversion = true;
    if(DEBUG_TEXARRAY2D){
      logchan_txia2d->log("Converting RGB8 to RGBA8 for slice %d update", slice_index);
    }
  }
#endif

  auto vktex = array->_tex->_impl.getShared<VulkanTextureObject>();
  vktex->_dataVersion++;

  int num_levels = int(mipc->_levels.size());

  // IMPORTANT: Clamp to the texture array's actual mip levels
  int array_mip_levels = array->_tex->_num_mips;
  num_levels           = std::min(num_levels, array_mip_levels);

  // Calculate staging buffer size
  size_t staging_size = 0;
  for (int level = 0; level < num_levels; level++) {
    auto& mip = mipc->_levels[level];
    if (needs_conversion) {
      // RGB8 to RGBA8 conversion needs more space
      staging_size += mip._width * mip._height * 4;
    } else {
      staging_size += mip._data->length();
    }
  }

  // Get staging buffer and command buffer
  auto poolForSize    = stagingBufferPoolForSrcOfSize(staging_size);
  auto staging_buffer = poolForSize->borrowItem();
  auto command_buffer = _seccmdbufpool_xfer->borrowItem();

  // Create transfer
  auto transfer = std::make_shared<InFlightTextureTransfer>(_contextVK, staging_buffer, command_buffer);
  vktex->_inflight_transfers.insert(transfer);

  auto cmdbuf_impl = transfer->_command_buffer->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  if(DEBUG_TEXARRAY2D) {
    logchan_txia2d->log(
      "updateTextureArraySlice vktex<%p> imgobj<%p:%zx:%zx> staging_size<%zu> num_levels<%d>", //
      (void*)vktex.get(),                                                                      //
      vktex->_imgobj.get(),                                                                    //
      vktex->_imgobj->_vkimage,                                                                //
      vktex->_imgobj->_vkimageview,                                                            //
      staging_size,                                                                            //
      num_levels);
  }

  // Setup completion
  auto tlsema                       = std::make_shared<VulkanCompletionSemaphore>(_contextVK);
  cmdbuf_impl->_completionSemaphore = tlsema;
  tlsema->_onComplete               = [=]() {
    vktex->_inflight_transfers.erase(transfer);
    poolForSize->returnItem(staging_buffer);
    _seccmdbufpool_xfer->returnItem(transfer->_command_buffer);
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
      // Convert RGB8 to RGBA8

      for (size_t i = 0; i < mip_w * mip_h; i++) {
        dst[i * 4 + 0] = src[i * 3 + 0];   // src[i * 3 + 0]; // R
        dst[i * 4 + 1] = src[i * 3 + 1]; // G
        dst[i * 4 + 2] = src[i * 3 + 2]; // B
        dst[i * 4 + 3] = 255;            // A
      }
    } else {
      // Direct copy
      memcpy(staging_data + offset, src, mip_data->length());
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

  // Transition image layout from shader read to transfer destination
  auto barrier = createImageBarrier(
      vktex->_imgobj->_vkimage,
      VK_IMAGE_LAYOUT_GENERAL,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VkAccessFlagBits(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT),
      VK_ACCESS_TRANSFER_WRITE_BIT);

  // Update barrier for specific slice
  barrier->subresourceRange.baseArrayLayer = slice_index;
  barrier->subresourceRange.layerCount     = 1;
  barrier->subresourceRange.levelCount     = num_levels;

  vkCmdPipelineBarrier(
      vk_cmdbuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, barrier.get());

  // Record commands
  vkCmdCopyBufferToImage(
      vk_cmdbuf,
      staging_buffer->_vkbuffer,
      vktex->_imgobj->_vkimage,
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

  // Submit
  _contextVK->endRecordCommandBuffer(transfer->_command_buffer);
  _contextVK->enqueueDeferredOneShotCommand(transfer->_command_buffer);

  logchan_txia2d->log("Updated texture array slice %d", slice_index);
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
#if defined(__APPLE__)
  // On macOS, RGB8 images are valid if the array format is RGBA8
  if (expected_format == EBufferFormat::RGBA8 && img->_format == EBufferFormat::RGB8) {
    ok &= true; // This is OK, we'll convert during copy
  } else
#endif
  {
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
  auto mipc = img->uncompressedMipChain();
  _updateTextureArraySlice(slice_ref, mipc);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan