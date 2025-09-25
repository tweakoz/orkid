////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_txi_loadreq = logger()->configureChannel("VKTXILOAD", fvec3(0.8, 0.2, 0.5), false);
///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::_createFromLoadReq(texloadreq_ptr_t req) {
  auto ptex = req->ptex;
  auto assreq = req->_assetloadreq;
  
  // Fire beginLoadMainThread event
  if (assreq and assreq->_on_event) {
    assreq->_on_event("beginLoadMainThread"_crcu, nullptr);
  }
  
  logchan_txi_loadreq->log("=== BEGIN _createFromLoadReq<%p:%s> ===", (void*)ptex.get(), ptex->_debugName.c_str());
  //ptex->_debugName = "VkTextureInterface::_createFromLoadReq";

  auto vktex       = ptex->_impl.makeShared<VulkanTextureObject>(this);
  auto chain       = req->_cmipchain;
  size_t num_mips  = chain->_levels.size();
  auto src_format  = chain->_format;
  int iwidth       = chain->_width;
  int iheight      = chain->_height;
  
  logchan_txi_loadreq->log("  Base dimensions: %dx%d", iwidth, iheight);
  logchan_txi_loadreq->log("  Number of mip levels: %zu", num_mips);
  logchan_txi_loadreq->log("  Source format: %s", EBufferFormatToName(src_format).c_str());
  
  // Log all mip level details
  for (int i = 0; i < num_mips; i++) {
    auto& level = chain->_levels[i];
    logchan_txi_loadreq->log("    Mip[%d]: %dx%d, data_length=%zu bytes",
                             i, level._width, level._height, 
                             level._data ? level._data->length() : 0);
  }

  // Convert format for platform if needed (e.g., BGR8->BGRA8 on macOS)
  auto dst_format = convertFormatForPlatform(src_format);
  bool needs_conversion = (dst_format != src_format);
  
  // Log format conversion
  if (needs_conversion) {
    auto src_format_name = EBufferFormatToName(src_format);
    auto dst_format_name = EBufferFormatToName(dst_format);
    logchan_txi_loadreq->log("FORMAT CONVERSION REQUIRED: %s -> %s for texture<%p:%s> (macOS/Metal compatibility)",
                             src_format_name.c_str(), dst_format_name.c_str(),
                             (void*)ptex.get(), ptex->_debugName.c_str());
  } else {
    auto format_name = EBufferFormatToName(src_format);
    logchan_txi_loadreq->log("No format conversion needed: %s for texture<%p:%s>",
                             format_name.c_str(),
                             (void*)ptex.get(), ptex->_debugName.c_str());
  }

  // Create a single VkImage with all mip levels using the converted format
  auto imageInfo   = makeVKICI(iwidth, iheight, 1, dst_format, num_mips);
  imageInfo->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  std::string debug_name = ptex->_debugName.empty() ? "texture_loadreq" : ptex->_debugName;
  vktex->_imgobj   = std::make_shared<VulkanImageObject>(_contextVK, imageInfo, debug_name);

  vktex->_loadCB   = _contextVK->beginRecordCommandBuffer("VkTextureInterface::_createFromLoadReq");


  /////////////////////////////////////
  // Set up completion callback
  /////////////////////////////////////

  vktex->_readyForSampling = false;

  auto cmdbuf_impl = vktex->_loadCB->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  auto tlsema         = std::make_shared<VulkanCompletionSemaphore>(this->_contextVK);
  cmdbuf_impl->_completionSemaphore = tlsema;
  tlsema->_onComplete = [=]() {
    vktex->_readyForSampling = true;
  };

  /////////////////////////////////////

  for (int ilevel = 0; ilevel < num_mips; ilevel++) {
    auto& level         = chain->_levels[ilevel];
    int level_width     = level._width;
    int level_height    = level._height;
    auto level_data     = level._data->data(0);
    size_t level_length = level._data->length();
    
    // Skip empty mip levels
    if (level_length == 0 || level_width == 0 || level_height == 0) {
      logchan_txi_loadreq->log("  Skipping empty mip level %d (width=%d, height=%d, length=%zu)",
                               ilevel, level_width, level_height, level_length);
      continue;
    }
    
    logchan_txi_loadreq->log("  Processing mip level %d: %dx%d, data_length=%zu bytes",
                             ilevel, level_width, level_height, level_length);

    // Transition the mip level to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    auto barrier = createImageBarrier(
        vktex->_imgobj->_vkimage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VkAccessFlagBits(0),
        VK_ACCESS_TRANSFER_WRITE_BIT);
    barrier->subresourceRange.baseMipLevel = ilevel;
    barrier->subresourceRange.levelCount   = 1;
    vkCmdPipelineBarrier(
        vk_cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, barrier.get());

    // Handle format conversion if needed
    size_t staging_length = level_length;
    const void* staging_data = level_data;
    std::vector<uint8_t> converted_data;
    
    if (needs_conversion && level_length > 0) {
      // Convert data based on format
      if (src_format == EBufferFormat::BGR8 && dst_format == EBufferFormat::BGRA8) {
        // Convert BGR8 to BGRA8
        size_t pixel_count = level_width * level_height;
        if (pixel_count > 0) {
          converted_data.resize(pixel_count * 4);
          const uint8_t* src = static_cast<const uint8_t*>(level_data);
          uint8_t* dst = converted_data.data();
          for (size_t i = 0; i < pixel_count; i++) {
            dst[i * 4 + 0] = src[i * 3 + 0]; // B
            dst[i * 4 + 1] = src[i * 3 + 1]; // G
            dst[i * 4 + 2] = src[i * 3 + 2]; // R
            dst[i * 4 + 3] = 255;             // A
          }
          staging_length = converted_data.size();
          staging_data = converted_data.data();
          logchan_txi_loadreq->log("    Converted mip[%d] BGR8->BGRA8: %zu pixels, %zu->%zu bytes",
                                   ilevel, pixel_count, level_length, staging_length);
        }
      } else if (src_format == EBufferFormat::RGB8 && dst_format == EBufferFormat::RGBA8) {
        // Convert RGB8 to RGBA8
        size_t pixel_count = level_width * level_height;
        if (pixel_count > 0) {
          converted_data.resize(pixel_count * 4);
          const uint8_t* src = static_cast<const uint8_t*>(level_data);
          uint8_t* dst = converted_data.data();
          for (size_t i = 0; i < pixel_count; i++) {
            dst[i * 4 + 0] = src[i * 3 + 0]; // R
            dst[i * 4 + 1] = src[i * 3 + 1]; // G
            dst[i * 4 + 2] = src[i * 3 + 2]; // B
            dst[i * 4 + 3] = 255;             // A
          }
          staging_length = converted_data.size();
          staging_data = converted_data.data();
          logchan_txi_loadreq->log("    Converted mip[%d] RGB8->RGBA8: %zu pixels, %zu->%zu bytes",
                                   ilevel, pixel_count, level_length, staging_length);
        }
      }
      // Add other conversions as needed
    }
    
    // Ensure we have valid data before creating staging buffer
    if (staging_length == 0) {
      continue;
    }
    
    // Copy the (possibly converted) mip level data to the staging buffer
    auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, staging_length, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "_createFromLoadReq");
    staging_buffer->copyFromHost(staging_data, staging_length);
    vktex->_staging_buffers.insert(staging_buffer);
    VkBufferImageCopy region = {};
    region.bufferOffset      = 0;
    region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, uint32_t(ilevel), 0, 1};
    region.imageExtent       = {uint32_t(level_width), uint32_t(level_height), 1};
    vkCmdCopyBufferToImage(
        vk_cmdbuf, staging_buffer->_vkbuffer, vktex->_imgobj->_vkimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    // Transition the mip level to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
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

    // Fire onMipLoad event after staging buffer filled
    if (assreq and assreq->_on_event) {
      auto data = std::make_shared<varmap::VarMap>();
      data->makeValueForKey<int>("level") = ilevel;
      data->makeValueForKey<int>("width") = level._width;
      data->makeValueForKey<int>("height") = level._height;
      data->makeValueForKey<datablock_ptr_t>("data") = level._data;
      data->makeValueForKey<uint32_t>("format") = int(dst_format);  // Use converted format
      data->makeValueForKey<std::string>("format_string") = EBufferFormatToName(dst_format);
      assreq->_on_event("onMipLoad"_crcu, data);
    }
  }
  /////////////////////////////////////
  // create image view
  /////////////////////////////////////

  auto IVCI = createImageViewInfo2D(
      vktex->_imgobj->_vkimage,                           //
      VkFormatConverter::convertBufferFormat(dst_format), // Use converted format
      VK_IMAGE_ASPECT_COLOR_BIT);
  IVCI->subresourceRange.levelCount = num_mips;

  initializeVkStruct(vktex->_imgobj->_vkimageview);
  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, IVCI.get(), nullptr, &vktex->_imgobj->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);
  
  // Set debug name for image view
  if (!ptex->_debugName.empty()) {
    std::string view_name = ptex->_debugName + "_view";
    _contextVK->_setObjectDebugName(vktex->_imgobj->_vkimageview, VK_OBJECT_TYPE_IMAGE_VIEW, view_name.c_str());
  }

  /////////////////////////////////////
  // descriptor image info
  /////////////////////////////////////

  // Temporarily set a default sampler - will be updated by ApplySamplingMode
  vktex->_vksampler                     = _contextVK->_sampler_per_maxlod[num_mips];
  vktex->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vktex->_vkdescriptor_info.imageView   = vktex->_imgobj->_vkimageview;
  vktex->_vkdescriptor_info.sampler     = vktex->_vksampler->_vksampler;

  vktex->_imgview_hash.init();
  vktex->_imgview_hash.accumulateItem(vktex);
  vktex->_imgview_hash.accumulateItem(vktex->_imgobj);
  vktex->_imgview_hash.accumulateItem(vktex->_imgobj->_vkimageview);
  vktex->_imgview_hash.finish();

  /////////////////////////////////////

  _contextVK->endRecordCommandBuffer(vktex->_loadCB);
  _contextVK->enqueueDeferredOneShotCommand(vktex->_loadCB);

  /////////////////////////////////////
  // Update texture properties
  /////////////////////////////////////

  ptex->_width = iwidth;
  ptex->_height = iheight;
  ptex->_depth = 1;
  ptex->_texFormat = dst_format;  // Use converted format
  ptex->_num_mips = num_mips;
  ptex->_dirty = false;
  
  /////////////////////////////////////
  // Set default sampling mode and apply it (matching GL behavior)
  /////////////////////////////////////
  
  // Set default sampling mode based on mip count (same as GL)
  if (num_mips > 3) {
    ptex->TexSamplingMode().presetTrilinearWrap();
  }
  // Apply the sampling mode to create/update the sampler
  this->ApplySamplingMode(ptex.get());

  logchan_txi_loadreq->log("=== END _createFromLoadReq<%p:%s> - texture loaded successfully ===", 
                           (void*)ptex.get(), ptex->_debugName.c_str());
  
  // Fire endLoadMainThread - CPU work complete
  if (assreq and assreq->_on_event) {
    assreq->_on_event("endLoadMainThread"_crcu, nullptr);
  }

  // Fire loadComplete - from asset system perspective, loading is done
  if (assreq and assreq->_on_event) {
    auto data = std::make_shared<varmap::VarMap>();
    data->makeValueForKey<std::string>("infname") = assreq->_asset_path.c_str();
    data->makeValueForKey<std::string>("loader") = "_loadDDSTexture";
    assreq->_on_event("loadComplete"_crcu, data);
  }

  ptex->_residenceState.fetch_or(1);
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
