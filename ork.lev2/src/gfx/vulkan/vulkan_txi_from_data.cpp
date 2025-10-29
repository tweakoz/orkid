////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/math/misc_math.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_txidata = logger()->configureChannel("VKTXIDAT", fvec3(0.8, 0.2, 0.5), false);
///////////////////////////////////////////////////////////////////////////////

std::atomic<int> InFlightTextureTransfer::_xfercount = 0;
std::atomic<size_t> InFlightTextureTransfer::_xferSN = 0;

InFlightTextureTransfer::InFlightTextureTransfer(vkcontext_rawptr_t ctx, //
                                                 vkbuffer_ptr_t stg_buffer, //
                                                 secondary_commandbuffer_ptr_t cmd_buffer) //
  : _staging_buffer(stg_buffer) //
  , _command_buffer(cmd_buffer) { //
  int count = _xfercount.fetch_add(1);
  int SN = _xferSN.fetch_add(1);
  if(1){ //(SN&0xfff)==0){
    logchan_txidata->log("InFlightTextureTransfer ctx<%p> count<%d> SN<%d>", (void*) ctx, count, SN);
  }
  auto cmdbuf_impl = _command_buffer->_impl.getShared<VkSecondaryCommandBufferImpl>();

  VkCommandBufferInheritanceInfo inhinfo = {};
  initializeVkStruct(inhinfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO);

  VkCommandBufferBeginInfo CBBI_GFX = {};
  initializeVkStruct(CBBI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  CBBI_GFX.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CBBI_GFX.pInheritanceInfo = &inhinfo;
  vkResetCommandBuffer(cmdbuf_impl->_vkcmdbuf, 0);
  vkBeginCommandBuffer(cmdbuf_impl->_vkcmdbuf,  &CBBI_GFX); // vkBeginCommandBuffer does an implicit reset


}
InFlightTextureTransfer::~InFlightTextureTransfer(){
  int count = _xfercount.fetch_sub(1);
}

  ///////////////////////////////////////////////////////////////////////////////
  void VkTextureInterface::_beginFrame(){
    // check for textures pending for deletion
    //. to have all transfers completed,
    //.  then they can be deleted
    std::unordered_set<vktexobj_ptr_t> ok_to_delete;
    for (auto vktex : _texobjs_pending_for_deletion) {
      if(vktex->_inflight_transfers.empty()){
        ok_to_delete.insert(vktex);
      }
    }
    for (auto vktex : ok_to_delete) {
      _texobjs_pending_for_deletion.erase(vktex);
    }
  }

  ///////////////////////////////////////////////////////////////////////////////

SecCmdBufPoolAdapter::SecCmdBufPoolAdapter(vkcontext_rawptr_t ctxVK)
    : _contextVK(ctxVK) {
}

secondary_commandbuffer_ptr_t SecCmdBufPoolAdapter::allocFresh() {
  return _contextVK->beginRecordCommandBuffer("SecCmdBufPoolAdapter");
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::initTextureFromData(Texture* ptex, TextureInitData tid) {

  bool async = tid._allow_async;

  ptex->_source = ETextureSource::FROM_DATA;
  //ptex->_debugName = "VkTextureInterface::initTextureFromData";
  bool is_brdf = ptex->_debugName.find("brdfIntegrationMap") != std::string::npos;
  /////////////////////////////////////
  // Handle format conversion for macOS
  /////////////////////////////////////

  EBufferFormat actual_dst_format = convertFormatForPlatform(tid._dst_format);
  bool needs_conversion = (actual_dst_format != tid._dst_format);

  /////////////////////////////////////
  // Calculate number of mip levels
  /////////////////////////////////////

  int num_mips = 1;
  if (tid._autogenmips && tid._w > 1 && tid._h > 1 && !tid._initCubeTexture) {
    num_mips = 1 + int(floor(log2(std::max(tid._w, tid._h))));
  }

  /////////////////////////////////////
  // hash the image creation parameters
  /////////////////////////////////////

  uint64_t usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT //
                 | VK_IMAGE_USAGE_SAMPLED_BIT;

  uint64_t format_hash = hashImageCreationParams(
      tid._w,          //
      tid._h,          //
      tid._d,          //
      actual_dst_format, // Use the actual format for hash
      num_mips,        // nummips
      usage);          // usage

  /////////////////////////////////////

  bool hash_changed = false;

  vktexobj_ptr_t vktex;
  if (auto existing = ptex->_impl.tryAsShared<VulkanTextureObject>()) {
    // Texture already exists - we're updating it
    vktex = existing.value();
    hash_changed = (format_hash != vktex->_format_hash);
    if(hash_changed){
      // Format changed - need to recreate
      _texobjs_pending_for_deletion.insert(vktex);
      vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
      vktex->_format_hash = format_hash;
    }
    // If same format, async, and ready - we'll use double-buffering (_imgobj_pending)
  } else {
    // New texture
    vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
    vktex->_format_hash = format_hash;
    hash_changed = true;
  }

  /////////////////////////////////////
  // Generate mip chain if requested
  /////////////////////////////////////

  struct MipLevelData {
    int width;
    int height;
    std::vector<uint8_t> data;
  };
  std::vector<MipLevelData> mip_levels;

  if (tid._autogenmips && num_mips > 1) {
    // Create base image from input data (after format conversion if needed)
    auto base_image = std::make_shared<Image>();

    if (needs_conversion) {
      // Convert source data to platform format first
      if (tid._dst_format == EBufferFormat::BGR8) {
        base_image->initWithFormat(tid._w, tid._h, EBufferFormat::BGRA8);
        const uint8_t* src = (const uint8_t*)tid._data;
        auto dst = base_image->pixel8(0, 0);
        for (size_t i = 0; i < tid._w * tid._h; i++) {
          dst[i * 4 + 0] = src[i * 3 + 0]; // B
          dst[i * 4 + 1] = src[i * 3 + 1]; // G
          dst[i * 4 + 2] = src[i * 3 + 2]; // R
          dst[i * 4 + 3] = 255;             // A
        }
      } else if (tid._dst_format == EBufferFormat::RGB8) {
        base_image->initWithFormat(tid._w, tid._h, EBufferFormat::RGBA8);
        const uint8_t* src = (const uint8_t*)tid._data;
        auto dst = base_image->pixel8(0, 0);
        for (size_t i = 0; i < tid._w * tid._h; i++) {
          dst[i * 4 + 0] = src[i * 3 + 0]; // R
          dst[i * 4 + 1] = src[i * 3 + 1]; // G
          dst[i * 4 + 2] = src[i * 3 + 2]; // B
          dst[i * 4 + 3] = 255;             // A
        }
      } else if (tid._dst_format == EBufferFormat::RGB32F) {
        base_image->initWithFormat(tid._w, tid._h, EBufferFormat::RGBA32F);
        const float* src = (const float*)tid._data;
        auto dst = (float*)base_image->_data->data();
        for (size_t i = 0; i < tid._w * tid._h; i++) {
          dst[i * 4 + 0] = src[i * 3 + 0]; // R
          dst[i * 4 + 1] = src[i * 3 + 1]; // G
          dst[i * 4 + 2] = src[i * 3 + 2]; // B
          dst[i * 4 + 3] = 1.0f;            // A
        }
      } else {
        // Direct copy for formats that don't need conversion
        base_image->initWithFormat(tid._w, tid._h, actual_dst_format);
        std::memcpy((void*)base_image->_data->data(), tid._data, tid.computeSrcSize());
      }
    } else {
      // Direct copy - no conversion needed
      base_image->initWithFormat(tid._w, tid._h, actual_dst_format);
      std::memcpy((void*)base_image->_data->data(), tid._data, tid.computeSrcSize());
    }

    // Generate mip chain by downsampling
    auto current_image = base_image;
    for (int i = 0; i < num_mips; i++) {
      MipLevelData level;
      level.width = current_image->_width;
      level.height = current_image->_height;
      size_t data_size = current_image->_data->length();
      level.data.resize(data_size);
      memcpy(level.data.data(), current_image->_data->data(), data_size);
      mip_levels.push_back(level);

      // Generate next mip level if not the last
      if (i < num_mips - 1) {
        auto next_image = std::make_shared<Image>();
        current_image->downsample(*next_image);
        current_image = next_image;
      }
    }
  }

  /////////////////////////////////////
  // Setup command buffer
  /////////////////////////////////////

  secondary_commandbuffer_ptr_t command_buffer;
  VkCommandBuffer vk_cmdbuf = VK_NULL_HANDLE;
  inflighttextrans_ptr_t transfer;

  // Storage for staging buffers (need to keep them alive until transfer completes)
  std::vector<vkbuffer_ptr_t> staging_buffers;

  // Track if we suspended a render pass for synchronous uploads
  bool suspended_render_pass = false;

  /////////////////////////////////////////////////////////
  if(async){
  /////////////////////////////////////////////////////////

    /////////////////////////////////////
    // allocate a secondary command buffer
    /////////////////////////////////////

    _seccmdbufpool_xfer.atomicOp([&](sseccmdbufpool_ptr_t& pool) {
      command_buffer = pool->borrowItem();
    });

    // Don't clear _img_sampling - it stays pointing to the old valid image until replaced

  }
  /////////////////////////////////////////////////////////
  else { // synchronous path
  /////////////////////////////////////////////////////////
    // Suspend render pass if active (so we can use barriers)
    if (_contextVK->_renderPassActive) {
      _contextVK->suspendRenderPass();
      suspended_render_pass = true;
    }

    vk_cmdbuf = _contextVK->primary_cb()->_vkcmdbuf;
    // Don't clear _img_sampling - will be replaced after GPU completion
  }

  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////


  /////////////////////////////////////
  // Determine ping-pong buffer indices
  // - write_slot: where we upload the new texture data
  // - sample_slot: what the GPU samples from (stays stable during async upload)
  /////////////////////////////////////

  int write_slot = vktex->_update_index & 1;
  int sample_slot = (vktex->_update_index + 1) & 1;

  // Increment index NOW (not in completion callback) so next upload uses different slot
  vktex->_update_index++;

  /////////////////////////////////////
  // Create new images if format/size changed or never created
  /////////////////////////////////////

  bool creating_new_images = hash_changed || !vktex->_imgobj[0];

  if (creating_new_images) {

    // Check if this is a cube texture
    bool is_cube = tid._initCubeTexture;
    int array_layers = tid._d;

    // For cube textures, we need 6 layers
    if (is_cube) {
      array_layers = 6;
    }

    auto VKICI   = makeVKICI(tid._w, tid._h, 1, actual_dst_format, num_mips); // depth is always 1 for 2D images
    VKICI->usage = usage;

    // Fix the extent.depth and arrayLayers
    VKICI->extent.depth = 1;  // 2D images always have depth = 1
    VKICI->arrayLayers = array_layers;  // Set the correct number of array layers

    // Add cube-compatible flag if this is a cube texture
    if (is_cube) {
      VKICI->flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    std::string debug_name = ptex->_debugName.empty()
                           ? (is_cube?"texture_from_data(cube)":"texture_from_data")
                           : ptex->_debugName;

    // Create both double-buffer slots with fresh images
    auto new_imgobj_a = std::make_shared<VulkanImageObject>(_contextVK, VKICI, debug_name + "_a");
    auto new_imgobj_b = std::make_shared<VulkanImageObject>(_contextVK, VKICI, debug_name + "_b");
    vktex->_imgobj[0] = new_imgobj_a;
    vktex->_imgobj[1] = new_imgobj_b;
    vktex->_update_index = 0;

    // Create image views for both images
    for (int i = 0; i < 2; i++) {
      std::shared_ptr<VkImageViewCreateInfo> IVCI;

      if (is_cube) {
        // Create cube image view
        IVCI = std::make_shared<VkImageViewCreateInfo>();
        initializeVkStruct(*IVCI, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        IVCI->image = vktex->_imgobj[i]->_vkimage;
        IVCI->viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        IVCI->format = VkFormatConverter::convertBufferFormat(actual_dst_format);
        IVCI->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        IVCI->subresourceRange.baseMipLevel = 0;
        IVCI->subresourceRange.levelCount = 1;
        IVCI->subresourceRange.baseArrayLayer = 0;
        IVCI->subresourceRange.layerCount = 6;
      } else {
        // 2D image view
        IVCI = createImageViewInfo2D(
            vktex->_imgobj[i]->_vkimage,
            VkFormatConverter::convertBufferFormat(actual_dst_format),
            VK_IMAGE_ASPECT_COLOR_BIT);
      }

      // Set level count for all mip levels
      IVCI->subresourceRange.levelCount = num_mips;

      initializeVkStruct(vktex->_imgobj[i]->_vkimageview);
      VkResult ok = vkCreateImageView(_contextVK->_vkdevice, IVCI.get(), nullptr, &vktex->_imgobj[i]->_vkimageview);
      OrkAssert(VK_SUCCESS == ok);

      // Set debug name for image view
      if (!ptex->_debugName.empty()) {
        std::string view_name = ptex->_debugName + (i == 0 ? "_view_a" : "_view_b");
        _contextVK->_setObjectDebugName(vktex->_imgobj[i]->_vkimageview, VK_OBJECT_TYPE_IMAGE_VIEW, view_name.c_str());
      }
    }

    // Update descriptor and texture properties
    vktex->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vktex->_vkdescriptor_info.imageView   = vktex->_imgobj[write_slot]->_vkimageview;
    OrkAssert(vktex->_imgobj[write_slot]->_vkimageview != VK_NULL_HANDLE);

    // Set image view hash (only when creating new images - represents format/size identity)
    // Use slot 0's serial number for consistency
    vktex->_imgview_hash.init();
    vktex->_imgview_hash.accumulateItem(vktex->_imgobj[0]->_serial_number);
    vktex->_imgview_hash.finish();

    ptex->_impl  = vktex;
    ptex->_texFormat = actual_dst_format;
    ptex->_width     = tid._w;
    ptex->_height    = tid._h;
    ptex->_depth     = is_cube ? 6 : tid._d;
    ptex->_num_mips  = num_mips;

  }

  /////////////////////////////////////

  vktex->_vksampler = num_mips > 1 ? _contextVK->_sampler_per_maxlod[num_mips] : _contextVK->_sampler_base;
  vktex->_vkdescriptor_info.sampler     = vktex->_vksampler->_vksampler;

  // Determine which image object to upload to (write_slot)
  vkimageobj_ptr_t target_imgobj = vktex->_imgobj[write_slot];

  // NOTE: Don't set _imgview_hash here - it's only set during image creation to avoid
  // invalidating descriptor set cache on every update

  /////////////////////////////////////
  // Upload each mip level
  /////////////////////////////////////

  for (int ilevel = 0; ilevel < num_mips; ilevel++) {
    int level_width = 0;
    int level_height = 0;
    const void* level_data = nullptr;
    size_t level_data_size = 0;

    // Get data for this mip level
    if (tid._autogenmips && num_mips > 1) {
      // Use generated mip data
      auto& mip = mip_levels[ilevel];
      level_width = mip.width;
      level_height = mip.height;
      level_data = mip.data.data();
      level_data_size = mip.data.size();
    } else {
      // Base level only - use original data (with conversion if needed)
      level_width = tid._w;
      level_height = tid._h;

      if (needs_conversion) {
        // For non-mipped textures with conversion, we still need to convert
        // We'll use a temporary buffer
        static thread_local std::vector<uint8_t> conversion_buffer;
        if (tid._dst_format == EBufferFormat::BGR8) {
          size_t pixel_count = tid._w * tid._h * tid._d;
          conversion_buffer.resize(pixel_count * 4);
          const uint8_t* src = (const uint8_t*)tid._data;
          uint8_t* dst = conversion_buffer.data();
          for (size_t i = 0; i < pixel_count; i++) {
            dst[i * 4 + 0] = src[i * 3 + 0]; // B
            dst[i * 4 + 1] = src[i * 3 + 1]; // G
            dst[i * 4 + 2] = src[i * 3 + 2]; // R
            dst[i * 4 + 3] = 255;             // A
          }
          level_data = conversion_buffer.data();
          level_data_size = conversion_buffer.size();
        } else if (tid._dst_format == EBufferFormat::RGB8) {
          size_t pixel_count = tid._w * tid._h * tid._d;
          conversion_buffer.resize(pixel_count * 4);
          const uint8_t* src = (const uint8_t*)tid._data;
          uint8_t* dst = conversion_buffer.data();
          for (size_t i = 0; i < pixel_count; i++) {
            dst[i * 4 + 0] = src[i * 3 + 0]; // R
            dst[i * 4 + 1] = src[i * 3 + 1]; // G
            dst[i * 4 + 2] = src[i * 3 + 2]; // B
            dst[i * 4 + 3] = 255;             // A
          }
          level_data = conversion_buffer.data();
          level_data_size = conversion_buffer.size();
        } else if (tid._dst_format == EBufferFormat::RGB32F) {
          size_t pixel_count = tid._w * tid._h * tid._d;
          conversion_buffer.resize(pixel_count * 4 * sizeof(float));
          const float* src = (const float*)tid._data;
          float* dst = (float*)conversion_buffer.data();
          for (size_t i = 0; i < pixel_count; i++) {
            dst[i * 4 + 0] = src[i * 3 + 0]; // R
            dst[i * 4 + 1] = src[i * 3 + 1]; // G
            dst[i * 4 + 2] = src[i * 3 + 2]; // B
            dst[i * 4 + 3] = 1.0f;            // A
          }
          level_data = conversion_buffer.data();
          level_data_size = conversion_buffer.size();
        } else {
          level_data = tid._data;
          level_data_size = tid.computeSrcSize();
        }
      } else {
        level_data = tid._data;
        level_data_size = tid.computeSrcSize();
      }
    }

    // Allocate staging buffer for this mip level
    auto poolForSize = stagingBufferPoolForSrcOfSize(level_data_size);
    auto staging_buffer = poolForSize->borrowItem();
    staging_buffer->copyFromHost(level_data, level_data_size);
    staging_buffers.push_back(staging_buffer);

    /////////////////////////////////////
    // On first iteration: create transfer object and set up callbacks
    /////////////////////////////////////
    if (ilevel == 0 && async) {
      vkseccmdbufimpl_ptr_t cmdbuf_impl;

      transfer = std::make_shared<InFlightTextureTransfer>(_contextVK, staging_buffer, command_buffer);
      vktex->_inflight_transfers.insert(transfer);
      cmdbuf_impl = command_buffer->_impl.getShared<VkSecondaryCommandBufferImpl>();
      vk_cmdbuf = cmdbuf_impl->_vkcmdbuf;

      /////////////////////////////////////
      // Set up completion and cleanup callbacks
      /////////////////////////////////////

      auto tlsema = std::make_shared<VulkanCompletionSemaphore>(this->_contextVK);
      cmdbuf_impl->_completionSemaphore = tlsema;

      // Pre-enqueue callback: capture primary CB and add to its pending_cleanup
      cmdbuf_impl->_onPreEnqueueCallback = [command_buffer, ctx = this->_contextVK]() {
        auto pricb = ctx->primary_cb();
        pricb->_secondary_cmdbuffers_pending_cleanup.push_back(command_buffer);
      };

      // Cleanup callback: return CB to pool when primary CB is reset
      cmdbuf_impl->_onCleanupCallback = [command_buffer, pool_ref = &_seccmdbufpool_xfer]() {
        pool_ref->atomicOp([&](sseccmdbufpool_ptr_t& pool) { pool->returnItem(command_buffer); });
      };

      // Completion callback: cleanup transfer and staging buffers when GPU completes
      tlsema->_onComplete = [=, this]() {
        vktex->_inflight_transfers.erase(transfer);
        // Return all staging buffers to their pools
        for (auto& buf : staging_buffers) {
          auto poolForSize = this->stagingBufferPoolForSrcOfSize(buf->_length);
          poolForSize->returnItem(buf);
        }

        // Use captured write_slot (not _update_index which has been incremented)
        auto& completed_img = vktex->_imgobj[write_slot];

        // Update sampling image to point to the newly completed image (NOW ready to sample)
        vktex->_img_sampling = completed_img;

        // Update descriptor to point to the newly completed image
        vktex->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vktex->_vkdescriptor_info.imageView = completed_img->_vkimageview;

        // NOTE: Do NOT update _imgview_hash here! It would invalidate descriptor set cache every frame.
        // The hash represents texture format/size identity, not which ping-pong slot is active.

        // Update texture properties
        bool is_cube = tid._initCubeTexture;
        ptex->_texFormat = actual_dst_format;
        ptex->_width = tid._w;
        ptex->_height = tid._h;
        ptex->_depth = is_cube ? 6 : tid._d;
        ptex->_num_mips = num_mips;
      };
    }

    // Transition this mip level to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    auto barrier = createImageBarrier(
        target_imgobj->_vkimage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VkAccessFlagBits(0),
        VK_ACCESS_TRANSFER_WRITE_BIT);
    barrier->subresourceRange.baseMipLevel = ilevel;
    barrier->subresourceRange.levelCount = 1;
    if (tid._initCubeTexture) {
      barrier->subresourceRange.layerCount = 6;
    }
    vkCmdPipelineBarrier(
        vk_cmdbuf,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, barrier.get());

    // Copy staging buffer to this mip level
    VkBufferImageCopy region{};
    initializeVkStruct(region);
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        uint32_t(ilevel),
        0,
        tid._initCubeTexture ? 6u : uint32_t(tid._d)};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {uint32_t(level_width), uint32_t(level_height), 1};

    vkCmdCopyBufferToImage(
        vk_cmdbuf,
        staging_buffer->_vkbuffer,
        target_imgobj->_vkimage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);

    // Transition this mip level to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    barrier->oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier->newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(
        vk_cmdbuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, barrier.get());
  }

  if(async){
    /////////////////////////////////////
    // enqueue recorded texture update cmdbuf
    /////////////////////////////////////

    _contextVK->endRecordCommandBuffer(transfer->_command_buffer);

    /////////////////////////////////////
    // enqueue the command buffer for execution
    /////////////////////////////////////

    _contextVK->enqueueDeferredOneShotCommand(transfer->_command_buffer);
  }
  else {
    // synchronous path - wait for GPU completion

    // Wait for all commands on the graphics queue to complete
    vkQueueWaitIdle(_contextVK->_vkqueue_graphics);

    // Resume render pass if we suspended it
    if (suspended_render_pass) {
      _contextVK->resumeRenderPass();
    }

    // Now texture is guaranteed to be ready for sampling
    vktex->_img_sampling = target_imgobj;
  }

  /////////////////////////////////////
  // Apply sampling mode (default or user-specified)
  /////////////////////////////////////
  ptex->mTexSampleMode = tid._samplingMode;
  this->ApplySamplingMode(ptex);

  /////////////////////////////////////

  ptex->_dirty = false;
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
