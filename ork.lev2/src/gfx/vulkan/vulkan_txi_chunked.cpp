////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// Chunked-upload texture API. Layers underneath the existing
// _createFromLoadReq / initTextureArray2DFromData paths.
//
// Lifecycle:
//   reserveTexture / reserveTextureArray
//     - create VkImage + memory + image view + descriptor info
//     - enqueue initial barrier: UNDEFINED -> TRANSFER_DST_OPTIMAL (whole image)
//   uploadTextureRegion (1..N times, in any order)
//     - per call: allocate staging buffer, copy host data in,
//       enqueue vkCmdCopyBufferToImage for the sub-region
//   finalizeUpload
//     - enqueue final barrier: TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
//     - on GPU completion, sets _img_sampling = _imgobj[0], texture is bindable
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"

namespace ork::lev2::vulkan {

static logchannel_ptr_t logchan_txi_chunked =
    logger()->configureChannel("VKTXICHUNK", fvec3(0.5, 0.2, 0.8), false);

///////////////////////////////////////////////////////////////////////////////
// Helper: bytes-per-pixel for the EBufferFormats that the chunked API
// supports as destination formats. Caller must convert RGB->RGBA etc.
// before uploadTextureRegion (see VKMT.md §10.7 design notes).
///////////////////////////////////////////////////////////////////////////////
static size_t _chunkedBytesPerPixel(EBufferFormat fmt) {
  switch (fmt) {
    case EBufferFormat::R8:
      return 1;
    case EBufferFormat::R16UI:
    case EBufferFormat::Y16UI:
      return 2;
    case EBufferFormat::RGBA8:
    case EBufferFormat::BGRA8:
    case EBufferFormat::SRGB_BGRA8:
    case EBufferFormat::R32F:
    case EBufferFormat::RG16F:
    case EBufferFormat::RGB10A2:
      return 4;
    case EBufferFormat::RGBA16:
    case EBufferFormat::RGBA16F:
    case EBufferFormat::RGBA16UI:
    case EBufferFormat::RG32F:
      return 8;
    case EBufferFormat::RGBA32F:
    case EBufferFormat::RGBA32UI:
      return 16;
    default:
      // Caller must use a supported destination format. RGB8/BGR8/RGB16/
      // RGB32F should already have been expanded to their *A* variants.
      OrkAssert(false);
      return 0;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Helper: allocate one VkImage + view + descriptor info into vktex->_imgobj[slot].
// Returns nothing; populates the slot in-place.
///////////////////////////////////////////////////////////////////////////////
static void _allocImageSlot(
    vkcontext_rawptr_t ctxVK,
    Texture* tex,
    vktexobj_ptr_t vktex,
    int slot,
    int w,
    int h,
    int num_mips,
    int layers,
    bool is_array,
    EBufferFormat fmt) {

  auto VKICI = makeVKICI(w, h, 1, fmt, num_mips);
  VKICI->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
               | VK_IMAGE_USAGE_TRANSFER_DST_BIT
               | VK_IMAGE_USAGE_SAMPLED_BIT;
  VKICI->arrayLayers = layers;
  VKICI->imageType   = VK_IMAGE_TYPE_2D;
  VKICI->flags       = 0;

  std::string base_dbg = tex->_debugName.empty()
                  ? (is_array ? std::string("chunked_array") : std::string("chunked_tex"))
                  : tex->_debugName;
  std::string slot_dbg = tex->_streaming
                       ? (base_dbg + (slot == 0 ? "_A" : "_B"))
                       : base_dbg;
  vktex->_imgobj[slot] = std::make_shared<VulkanImageObject>(ctxVK, VKICI, slot_dbg);

  // Image view
  VkImageViewCreateInfo IVCI{};
  initializeVkStruct(IVCI, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
  IVCI.image    = vktex->_imgobj[slot]->_vkimage;
  IVCI.viewType = is_array ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
  IVCI.format   = VkFormatConverter::convertBufferFormat(fmt);
  IVCI.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  IVCI.subresourceRange.baseMipLevel   = 0;
  IVCI.subresourceRange.levelCount     = num_mips;
  IVCI.subresourceRange.baseArrayLayer = 0;
  IVCI.subresourceRange.layerCount     = layers;
  initializeVkStruct(vktex->_imgobj[slot]->_vkimageview);
  VkResult ok = vkCreateImageView(ctxVK->_vkdevice, &IVCI, nullptr, &vktex->_imgobj[slot]->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);
  if (not tex->_debugName.empty()) {
    std::string view_name = slot_dbg + (is_array ? "_array_view" : "_view");
    ctxVK->_setObjectDebugName(vktex->_imgobj[slot]->_vkimageview, VK_OBJECT_TYPE_IMAGE_VIEW, view_name.c_str());
  }

  // Descriptor info
  vktex->_vkdescriptor_info[slot] = std::make_shared<VkDescriptorImageInfo>();
  vktex->_vkdescriptor_info[slot]->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vktex->_vkdescriptor_info[slot]->imageView   = vktex->_imgobj[slot]->_vkimageview;
  vktex->_vkdescriptor_info[slot]->sampler     = vktex->_vksampler->_vksampler;
}

///////////////////////////////////////////////////////////////////////////////
// Helper: build the VulkanImageObject(s) + view(s) + descriptor(s) for a 2D
// or 2D-array texture. If tex->_streaming, allocates BOTH _imgobj[0] and
// _imgobj[1]; otherwise just _imgobj[0]. Records the initial layout barrier
// on a secondary CB.
///////////////////////////////////////////////////////////////////////////////
static void _reserveCommon(
    VkTextureInterface* txi,
    vkcontext_rawptr_t ctxVK,
    Texture* tex,
    int w,
    int h,
    int num_slices,  // 0 = plain 2D, >0 = 2D array
    int num_mips,
    EBufferFormat fmt) {

  OrkAssert(tex);
  OrkAssert(w > 0 and h > 0 and num_mips > 0);

  const bool is_array  = num_slices > 0;
  const int  layers    = is_array ? num_slices : 1;
  const bool streaming = tex->_streaming;

  auto vktex = tex->_impl.makeShared<VulkanTextureObject>(txi);

  // Sampler — shared across both slots when streaming.
  vktex->_vksampler = is_array
                    ? ctxVK->_sampler_base
                    : ctxVK->_sampler_per_maxlod[num_mips];

  ///////////////////////////
  // Allocate image slot(s)
  ///////////////////////////
  _allocImageSlot(ctxVK, tex, vktex, /*slot*/0, w, h, num_mips, layers, is_array, fmt);
  if (streaming) {
    _allocImageSlot(ctxVK, tex, vktex, /*slot*/1, w, h, num_mips, layers, is_array, fmt);
  }

  // Initial descriptor pointer + hash from slot 0 (initial "front").
  vktex->_front_idx.store(0);
  vktex->_descset_sampling = vktex->_vkdescriptor_info[0];
  vktex->_imgview_hash.init();
  vktex->_imgview_hash.accumulateItem(vktex->_imgobj[0]->_serial_number);
  vktex->_imgview_hash.finish();

  // NOTE: _img_sampling is intentionally left null here — it gates
  // "texture is ready to sample" and gets set in finalizeUpload's
  // completion callback.

  ///////////////////////////
  // Update Texture metadata
  ///////////////////////////

  tex->_width     = w;
  tex->_height    = h;
  tex->_depth     = is_array ? num_slices : 1;
  tex->_texFormat = fmt;
  tex->_num_mips  = num_mips;
  tex->_dirty     = false;
  if (is_array) {
    tex->_texType = ETEXTYPE_2D_ARRAY;
  }

  ///////////////////////////
  // Initial layout barrier(s).
  //  - Non-streaming: slot 0 UNDEFINED -> TRANSFER_DST_OPTIMAL.
  //      uploadTextureRegion writes into TRANSFER_DST; finalize transitions
  //      the whole image to SHADER_READ_ONLY (one-shot).
  //  - Streaming: slot 0 UNDEFINED -> SHADER_READ_ONLY_OPTIMAL (this is the
  //      initial "front" — render samples it; before first cycle finalizes
  //      it's all zeros, gated by _img_sampling).
  //      slot 1 UNDEFINED -> SHADER_READ_ONLY_OPTIMAL too (the initial
  //      "back"; uploadTextureRegion barrier-sandwiches its own writes).
  ///////////////////////////

  auto cb = ctxVK->beginRecordCommandBuffer("VkTXI::reserveTexture::initBarrier");
  auto cb_impl = cb->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vkcb    = cb_impl->_vkcmdbuf;

  auto emit_barrier = [&](VulkanImageObject* img, VkImageLayout new_layout,
                          VkAccessFlagBits dstAccess, VkPipelineStageFlagBits dstStage) {
    auto barrier = createImageBarrier(
        img->_vkimage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        new_layout,
        VkAccessFlagBits(0),
        dstAccess);
    barrier->subresourceRange.layerCount = layers;
    barrier->subresourceRange.levelCount = num_mips;
    vkCmdPipelineBarrier(
        vkcb,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        dstStage,
        0, 0, nullptr, 0, nullptr,
        1, barrier.get());
  };
  if (streaming) {
    // Both slots start in SHADER_READ_ONLY. Uploads do per-call barrier
    // sandwich on the back; finalize is pure metadata swap.
    emit_barrier(vktex->_imgobj[0].get(),
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_SHADER_READ_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    emit_barrier(vktex->_imgobj[1].get(),
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_SHADER_READ_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    cb_impl->_referenced_images.push_back(vktex->_imgobj[0]);
    cb_impl->_referenced_images.push_back(vktex->_imgobj[1]);
  } else {
    // Non-streaming: slot 0 stays in TRANSFER_DST until finalize.
    emit_barrier(vktex->_imgobj[0].get(),
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT);
    cb_impl->_referenced_images.push_back(vktex->_imgobj[0]);
  }

  ctxVK->endRecordCommandBuffer(cb);
  ctxVK->enqueueDeferredOneShotCommand(cb);

  logchan_txi_chunked->log(
      "reserve%s tex<%p:%s> %dx%d mips=%d slices=%d fmt=%s streaming=%d",
      is_array ? "TextureArray" : "Texture",
      (void*)tex, tex->_debugName.c_str(),
      w, h, num_mips, layers,
      EBufferFormatToName(fmt).c_str(),
      streaming ? 1 : 0);
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::reserveTexture(
    Texture* tex,
    int width,
    int height,
    int num_mips,
    EBufferFormat fmt) {
  _reserveCommon(this, _contextVK, tex, width, height, /*num_slices*/ 0, num_mips, fmt);
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::reserveTextureArray(
    TextureArray* tarr,
    int width,
    int height,
    int num_slices,
    int num_mips,
    EBufferFormat fmt) {
  OrkAssert(tarr and tarr->_tex);
  OrkAssert(num_slices > 0);
  _reserveCommon(this, _contextVK, tarr->_tex.get(), width, height, num_slices, num_mips, fmt);
  tarr->_width     = width;
  tarr->_height    = height;
  tarr->_maxslices = num_slices;
  tarr->_num_mips  = num_mips;
  tarr->_format    = fmt;
  tarr->_isDirty   = false;
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::uploadTextureRegion(
    Texture* tex,
    const TextureRegionUpload& up,
    ::ork::void_lambda_t on_complete) {

  OrkAssert(tex);
  OrkAssert(up._data and up._data_size > 0);
  OrkAssert(up._extent_w > 0 and up._extent_h > 0 and up._extent_d > 0);

  // Caller must supply destination-format data — assert size matches.
  const size_t bpp = _chunkedBytesPerPixel(tex->_texFormat);
  const size_t expected = size_t(up._extent_w) * size_t(up._extent_h) * size_t(up._extent_d) * bpp;
  OrkAssert(up._data_size == expected);

  auto vktex = tex->_impl.getShared<VulkanTextureObject>();
  OrkAssert(vktex and vktex->_imgobj[0]);

  // For streaming, writes go to the BACK slot (the slot that isn't
  // currently being sampled). The back is in TRANSFER_DST_OPTIMAL between
  // cycles (set up by reserve initially, restored by finalize's swap).
  const int slot = tex->_streaming
                 ? (1 - vktex->_front_idx.load(std::memory_order_acquire))
                 : 0;
  OrkAssert(vktex->_imgobj[slot]);

  ///////////////////////////
  // Staging buffer — owned ephemerally; alive until GPU copy completes.
  ///////////////////////////

  auto poolForSize    = stagingBufferPoolForSrcOfSize(up._data_size);
  auto staging_buffer = poolForSize->borrowItem();
  staging_buffer->copyFromHost(up._data, up._data_size);

  ///////////////////////////
  // Record copy on a secondary CB.
  ///////////////////////////

  auto cb = _contextVK->beginRecordCommandBuffer("VkTXI::uploadTextureRegion");
  auto cb_impl = cb->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vkcb    = cb_impl->_vkcmdbuf;

  // For streaming uploads we barrier-sandwich the back image around the
  // copy. The back is at rest in SHADER_READ_ONLY (between cycles) and
  // render NEVER samples it — render only ever samples the front — so
  // these transitions can't race a render draw. (Non-streaming: image is
  // in TRANSFER_DST from reserve until finalize, no barrier needed here.)
  const uint32_t mip_count   = uint32_t(tex->_num_mips > 0 ? tex->_num_mips : 1);
  const uint32_t layer_count = (tex->_texType == ETEXTYPE_2D_ARRAY)
                             ? uint32_t(tex->_depth > 0 ? tex->_depth : 1)
                             : 1;

  if (tex->_streaming) {
    auto acquire = createImageBarrier(
        vktex->_imgobj[slot]->_vkimage,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_ACCESS_SHADER_READ_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT);
    acquire->subresourceRange.layerCount = layer_count;
    acquire->subresourceRange.levelCount = mip_count;
    vkCmdPipelineBarrier(
        vkcb,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, acquire.get());
  }

  VkBufferImageCopy region{};
  region.bufferOffset                    = 0;
  region.bufferRowLength                 = 0;
  region.bufferImageHeight               = 0;
  region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel       = uint32_t(up._mip_level);
  region.imageSubresource.baseArrayLayer = uint32_t(up._array_layer);
  region.imageSubresource.layerCount     = 1;
  region.imageOffset                     = {up._offset_x, up._offset_y, up._offset_z};
  region.imageExtent                     = {uint32_t(up._extent_w), uint32_t(up._extent_h), uint32_t(up._extent_d)};

  vkCmdCopyBufferToImage(
      vkcb,
      staging_buffer->_vkbuffer,
      vktex->_imgobj[slot]->_vkimage,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &region);

  if (tex->_streaming) {
    auto release = createImageBarrier(
        vktex->_imgobj[slot]->_vkimage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    release->subresourceRange.layerCount = layer_count;
    release->subresourceRange.levelCount = mip_count;
    vkCmdPipelineBarrier(
        vkcb,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, release.get());
  }

  ///////////////////////////
  // Completion semaphore — returns staging buffer to its pool when the
  // GPU finishes; fires user callback. Capture the pool too so it can't
  // outlive the buffer.
  ///////////////////////////

  auto sema = std::make_shared<VulkanCompletionSemaphore>(_contextVK);
  cb_impl->_completionSemaphore = sema;
  sema->_onComplete = [staging_buffer, poolForSize, on_complete]() {
    poolForSize->returnItem(staging_buffer);
    if (on_complete) on_complete();
  };

  // Keep image + staging alive for the duration of GPU execution.
  cb_impl->_referenced_images.push_back(vktex->_imgobj[slot]);
  cb_impl->_referenced_buffers.push_back(staging_buffer);

  _contextVK->endRecordCommandBuffer(cb);
  _contextVK->enqueueDeferredOneShotCommand(cb);

  logchan_txi_chunked->log(
      "uploadTextureRegion tex<%p:%s> slot=%d mip=%d slice=%d off=(%d,%d,%d) ext=(%d,%d,%d) bytes=%zu",
      (void*)tex, tex->_debugName.c_str(), slot,
      up._mip_level, up._array_layer,
      up._offset_x, up._offset_y, up._offset_z,
      up._extent_w, up._extent_h, up._extent_d,
      up._data_size);
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::finalizeUpload(
    Texture* tex,
    ::ork::void_lambda_t on_complete) {

  OrkAssert(tex);
  auto vktex = tex->_impl.getShared<VulkanTextureObject>();
  OrkAssert(vktex and vktex->_imgobj[0]);

  const uint32_t layers   = tex->_depth > 0 ? uint32_t(tex->_depth) : 1;
  const bool is_array = (tex->_texType == ETEXTYPE_2D_ARRAY);
  const uint32_t layer_count = is_array ? layers : 1;
  const uint32_t mip_count   = uint32_t(tex->_num_mips > 0 ? tex->_num_mips : 1);

  auto cb = _contextVK->beginRecordCommandBuffer("VkTXI::finalizeUpload");
  auto cb_impl = cb->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vkcb    = cb_impl->_vkcmdbuf;

  auto emit_barrier_full = [&](VkImage img, VkImageLayout old_layout, VkImageLayout new_layout,
                               VkAccessFlagBits srcAccess, VkAccessFlagBits dstAccess,
                               VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage) {
    auto barrier = createImageBarrier(img, old_layout, new_layout, srcAccess, dstAccess);
    barrier->subresourceRange.layerCount = layer_count;
    barrier->subresourceRange.levelCount = mip_count;
    vkCmdPipelineBarrier(vkcb, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, barrier.get());
  };

  if (not tex->_streaming) {
    ///////////////////////////
    // Non-streaming (one-shot): single image, transition TRANSFER_DST -> SHADER_READ.
    // After this completes the texture is sampleable and immutable.
    ///////////////////////////
    emit_barrier_full(
        vktex->_imgobj[0]->_vkimage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    auto sema = std::make_shared<VulkanCompletionSemaphore>(_contextVK);
    cb_impl->_completionSemaphore = sema;
    sema->_onComplete = [vktex, on_complete]() {
      vktex->_img_sampling = vktex->_imgobj[0];
      if (on_complete) on_complete();
    };

    cb_impl->_referenced_images.push_back(vktex->_imgobj[0]);
    _contextVK->endRecordCommandBuffer(cb);
    _contextVK->enqueueDeferredOneShotCommand(cb);
  }
  else {
    ///////////////////////////
    // Streaming cycle. Both slots live in SHADER_READ_ONLY at rest;
    // uploadTextureRegion barrier-sandwiches its writes on the back.
    // Finalize itself does NO GPU work — it's a pure metadata swap
    // sequenced behind the loader's already-submitted upload CBs.
    //
    // We attach a completion sema to a tiny no-op CB that exists only
    // so the sema gets signaled in-order with the prior upload CBs.
    // When it fires we flip _front_idx, repoint _descset_sampling, and
    // bump _imgview_hash so the descriptor cache misses → MoltenVK
    // re-encodes the Metal argbuffer with the new front image.
    //
    // CRITICAL: we never transition either image's layout in finalize.
    // The current front is being sampled by in-flight render frames;
    // touching its layout would race against those samples.
    ///////////////////////////
    const int  cur_front = vktex->_front_idx.load(std::memory_order_acquire);
    const int  cur_back  = 1 - cur_front;
    // Stamp this finalize. `new_front` is the absolute slot we want as
    // the visible front when this swap fires (= the back we just wrote).
    // We pass it as an absolute index (not derived via XOR) so out-of-order
    // swap firing doesn't toggle to the wrong slot.
    const uint64_t my_seq    = vktex->_swap_seq_next.fetch_add(1, std::memory_order_acq_rel) + 1;
    const int      new_front = cur_back;
    (void)layer_count;
    (void)mip_count;

    auto sema = std::make_shared<VulkanCompletionSemaphore>(_contextVK);
    cb_impl->_completionSemaphore = sema;
    sema->_onComplete = [vktex, my_seq, new_front, on_complete]() {
      // Only apply if this swap is the newest the texture has seen.
      // Stale completions (older finalize firing after a newer one) get
      // dropped — _pendingOneShotSemas is an unordered_set so callback
      // order is hash-based, not FIFO.
      uint64_t applied = vktex->_swap_seq_applied.load(std::memory_order_acquire);
      while (applied < my_seq) {
        if (vktex->_swap_seq_applied.compare_exchange_weak(
              applied, my_seq,
              std::memory_order_acq_rel, std::memory_order_acquire)) {
          vktex->_descset_sampling = vktex->_vkdescriptor_info[new_front];
          vktex->_img_sampling     = vktex->_imgobj[new_front];
          vktex->_imgview_hash.init();
          vktex->_imgview_hash.accumulateItem(vktex->_imgobj[new_front]->_serial_number);
          vktex->_imgview_hash.finish();
          vktex->_front_idx.store(new_front, std::memory_order_release);
          break;
        }
      }
      // applied >= my_seq → another (newer) swap already won; this one is stale.
      if (on_complete) on_complete();
    };

    cb_impl->_referenced_images.push_back(vktex->_imgobj[cur_back]);
    _contextVK->endRecordCommandBuffer(cb);
    _contextVK->enqueueDeferredOneShotCommand(cb);
  }

  ///////////////////////////
  // Apply sampling mode now (CPU-side); fully takes effect once
  // _img_sampling becomes non-null.
  ///////////////////////////

  if (tex->_num_mips > 3) {
    tex->TexSamplingMode().presetTrilinearWrap();
  }
  this->ApplySamplingMode(tex);

  tex->_residenceState.fetch_or(1);

  logchan_txi_chunked->log(
      "finalizeUpload tex<%p:%s> streaming=%d layers=%u mips=%u",
      (void*)tex, tex->_debugName.c_str(),
      tex->_streaming ? 1 : 0, layer_count, mip_count);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
