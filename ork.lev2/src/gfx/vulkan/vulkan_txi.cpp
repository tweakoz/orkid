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
std::atomic<size_t> VulkanTextureObject::_vkto_count = 0;
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_txi = logger()->configureChannel("VKTXI", fvec3(0.8, 0.2, 0.5), true);

VkTextureInterface::VkTextureInterface(vkcontext_rawptr_t ctx)
    : TextureInterface(ctx)
    , _contextVK(ctx) {
    _seccmdbufpool_xfer.atomicOp([ctx](sseccmdbufpool_ptr_t& pool) {
      pool = std::make_shared<SecCmdBufPool>(SecCmdBufPoolAdapter(ctx));
    });
}

///////////////////////////////////////////////////////////////////////////////

bool VkTextureInterface::destroyTexture(texture_ptr_t ptex) {
  /*
  auto glto = tex->_impl.get<gltexobj_ptr_t>();
tex->_impl.set<void*>(nullptr);

void_lambda_t lamb = [=]() {
  if (glto) {
    if (glto->mObject != 0)
      glDeleteTextures(1, &glto->mObject);
  }
};
// opq::mainSerialQueue()->push(lamb,get_backtrace());
opq::mainSerialQueue()->enqueue(lamb);
*/
  return false;
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::ApplySamplingMode(Texture* ptex) {
  if (!ptex) return;
  
  // Get or create VulkanTextureObject
  vktexobj_ptr_t vktex;
  if (auto as_vktext = ptex->_impl.tryAsShared<VulkanTextureObject>()) {
    vktex = as_vktext.value();
  } else {
    // Texture not initialized yet - store sampling mode for later
    // This will be applied when texture is actually created
    return;
  }
  
  // Get the sampling mode
  const auto& sampling_mode = ptex->TexSamplingMode();
  
  // Get or create appropriate sampler
  auto new_sampler = _contextVK->_getOrCreateSampler(sampling_mode);
  
  // Update texture object
  vktex->_vksampler = new_sampler;

  // Update sampler on both descriptor infos (if they exist)
  if (vktex->_vkdescriptor_info[0]) {
    vktex->_vkdescriptor_info[0]->sampler = new_sampler->_vksampler;
  }
  if (vktex->_vkdescriptor_info[1]) {
    vktex->_vkdescriptor_info[1]->sampler = new_sampler->_vksampler;
  }

  // NOTE: Don't update imageView here - each descriptor permanently points to its slot's imageView
  // _descset_sampling already points to the correct descriptor for the active sampling slot
  
  // Special handling for depth textures
  if (ptex->_isDepthTexture) {
    // Might need to enable compare mode for shadow sampling
    // This would require creating a different sampler with compareEnable = VK_TRUE
  }
  
  // Debug logging
  if (0) {
    printf("VkTXI::ApplySamplingMode tex<%s> sampler<%p>\n", 
           ptex->_debugName.c_str(), 
           new_sampler.get());
  }
}

///////////////////////////////////////////////////////////////////////////////

SbsPoolAdapter::SbsPoolAdapter(vkcontext_rawptr_t ctxVK, size_t size, uint64_t usage)
  : _contextVK(ctxVK)
  , _size(size)
  , _usage(usage) {

}

///////////////////////////////////////////////////////////////////////////////

vkbuffer_ptr_t SbsPoolAdapter::allocFresh() {
  return std::make_shared<VulkanBuffer>(_contextVK, _size, _usage,"stagingBufferSet");
}

///////////////////////////////////////////////////////////////////////////////

stagingbufferpool_ptr_t VkTextureInterface::stagingBufferPoolForSrcOfSize(size_t size) {
  // round up to next power of two
  stagingbufferpool_ptr_t rval;
  size_t rounded_size = nextPowerOfTwo(size);
  _stagingSrcBuffers.atomicOp([&](sbpoolmap_t& unlocked) {
    auto it             = unlocked.find(rounded_size);
    if (it != unlocked.end()) {
      rval = it->second;
    } else {
      SbsPoolAdapter adapter(_contextVK, rounded_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
      auto new_pool = std::make_shared<StagingBufferPool>(adapter);
      unlocked[rounded_size] = new_pool;
      rval = new_pool;
    }
  });
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::generateMipMaps(Texture* ptex) {
  // Guard: nothing to do if image has <= 1 mip level
  if (ptex->_num_mips <= 1) {
    return;
  }

  ptex->_debugName = "VkTextureInterface::generateMipMaps";
  vktexobj_ptr_t vktex;
  if (auto as_vktext = ptex->_impl.tryAsShared<VulkanTextureObject>()) {
    vktex = as_vktext.value();
  } else {
    vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
    OrkAssert(false);
  }

  // Suspend render pass if active — we need to execute transfer barriers inline
  bool was_active = _contextVK->_renderPassActive;
  if (was_active) {
    _contextVK->suspendRenderPass();
  }

  auto cmdbuf = _contextVK->beginRecordCommandBuffer("VkTextureInterface::generateMipMaps");

  auto cmdbuf_impl = cmdbuf->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  int32_t mipWidth   = ptex->_width;
  int32_t mipHeight  = ptex->_height;
  int32_t num_mips   = ptex->_num_mips;

  // Determine layer count (6 for cube maps, 1 for 2D)
  uint32_t layer_count = (ptex->_texType == ETEXTYPE_CUBE) ? 6 : 1;

  auto image   = vktex->samplingImage();
  auto barrier = createImageBarrier(
      image->_vkimage,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VkAccessFlagBits(0),
      VK_ACCESS_TRANSFER_WRITE_BIT);
  barrier->subresourceRange.layerCount = layer_count;

  /////////////////////////////////////////
  // Step 1: Transition ALL mip levels to TRANSFER_DST_OPTIMAL
  //   The image may be in any layout (SHADER_READ_ONLY, COLOR_ATTACHMENT, etc.)
  //   Use UNDEFINED as oldLayout to discard contents of mips 1..N-1 (they have no valid data yet)
  //   For mip 0, the data was rendered by the caller and we want to preserve it,
  //   but UNDEFINED→TRANSFER_DST discards. So transition mip 0 separately.
  /////////////////////////////////////////

  // Mip 0: preserve contents — transition from current layout to TRANSFER_DST
  // Use SHADER_READ_ONLY as the expected layout after RTG rendering + _transitionToTexture
  barrier->subresourceRange.baseMipLevel = 0;
  barrier->subresourceRange.levelCount   = 1;
  barrier->oldLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier->newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier->srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier->dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      0, 0, nullptr, 0, nullptr, 1, barrier.get());

  // Mips 1..N-1: no valid data, use UNDEFINED to discard
  if (num_mips > 2) {
    barrier->subresourceRange.baseMipLevel = 1;
    barrier->subresourceRange.levelCount   = num_mips - 1;
    barrier->oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier->newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier->srcAccessMask = VkAccessFlagBits(0);
    barrier->dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(
        vk_cmdbuf,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, barrier.get());
  }

  // Reset levelCount to 1 for per-mip barriers in the loop
  barrier->subresourceRange.levelCount = 1;

  /////////////////////////////////////////
  // Step 2: Generate mip chain
  //   For each level i from 0 to num_mips-2:
  //     - Transition mip[i]: TRANSFER_DST → TRANSFER_SRC
  //     - Blit mip[i] → mip[i+1]
  //     - Transition mip[i]: TRANSFER_SRC → SHADER_READ_ONLY
  /////////////////////////////////////////

  for (int i = 0; i < num_mips - 1; i++) {

    // Transition mip[i] from TRANSFER_DST to TRANSFER_SRC
    barrier->subresourceRange.baseMipLevel = i;
    barrier->oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier->newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier->dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(
        vk_cmdbuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, barrier.get());

    // Blit mip[i] → mip[i+1]
    int32_t nextWidth  = mipWidth  > 1 ? mipWidth  / 2 : 1;
    int32_t nextHeight = mipHeight > 1 ? mipHeight / 2 : 1;

    VkImageBlit blit{};
    blit.srcOffsets[0]                 = {0, 0, 0};
    blit.srcOffsets[1]                 = {mipWidth, mipHeight, 1};
    blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel       = i;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount     = layer_count;
    blit.dstOffsets[0]                 = {0, 0, 0};
    blit.dstOffsets[1]                 = {nextWidth, nextHeight, 1};
    blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel       = i + 1;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount     = layer_count;

    vkCmdBlitImage(
        vk_cmdbuf,
        image->_vkimage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        image->_vkimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blit, VK_FILTER_LINEAR);

    // Transition mip[i] from TRANSFER_SRC to SHADER_READ_ONLY
    barrier->subresourceRange.baseMipLevel = i;
    barrier->oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier->newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier->srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        vk_cmdbuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, barrier.get());

    mipWidth  = nextWidth;
    mipHeight = nextHeight;
  }

  /////////////////////////////////////////
  // Step 3: Transition the last mip level from TRANSFER_DST to SHADER_READ_ONLY
  //   It was the destination of the final blit but never used as a source
  /////////////////////////////////////////

  barrier->subresourceRange.baseMipLevel = num_mips - 1;
  barrier->oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier->newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      0, 0, nullptr, 0, nullptr, 1, barrier.get());

  // Update tracked layout — all mip levels are now SHADER_READ_ONLY
  image->_currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  /////////////////////////////////////

  _contextVK->endRecordCommandBuffer(cmdbuf);
  _contextVK->enqueueSecondaryCommandBuffer(cmdbuf);

  // Resume render pass if it was active
  if (was_active) {
    _contextVK->resumeRenderPass();
  }
}

///////////////////////////////////////////////////////////////////////////////

Texture* VkTextureInterface::createFromMipChain(MipChain* from_chain) {
  auto ptex        = new Texture;
  ptex->_debugName = "VkTextureInterface::createFromMipChain";
  auto vktex       = ptex->_impl.makeShared<VulkanTextureObject>(this);
  OrkAssert(false);
  // vktex->_imgobj = std::make_shared<VulkanImageObject>(_contextVK, imginfo);
  auto format       = from_chain->_format;
  auto type         = from_chain->_type;
  size_t num_levels = from_chain->_levels.size();

  auto imageInfo = makeVKICI(from_chain->_width, from_chain->_height, 1, format, num_levels);

  vktex->_loadCB = _contextVK->beginRecordCommandBuffer("VkTextureInterface::createFromMipChain");

  auto cmdbuf_impl = vktex->_loadCB->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  // Use slot [0] for mipchain loading
  auto image = vktex->_imgobj[0];

  for (size_t l = 0; l < num_levels; l++) {

    auto level          = from_chain->_levels[l];
    int level_width     = level->_width;
    int level_height    = level->_height;
    void* level_data    = level->_data;
    size_t level_length = level->_length;

    /////////////////////////////////////
    // transition to transfer dst (for copy)
    /////////////////////////////////////
    auto barrier = createImageBarrier(
        image->_vkimage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VkAccessFlagBits(0),
        VK_ACCESS_TRANSFER_WRITE_BIT);

    barrier->subresourceRange.baseMipLevel = l;

    vkCmdPipelineBarrier(
        vk_cmdbuf,                         // cmdbuf
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT,    // dstStageMask
        0,                                 // dependencyFlags
        0,
        nullptr, // memoryBarriers
        0,
        nullptr, // bufferMemoryBarriers
        1,
        barrier.get()); // imageMemoryBarriers

    /////////////////////////////////////
    // map staging memory and copy
    /////////////////////////////////////

    auto set            = stagingBufferPoolForSrcOfSize(level_length);
    auto staging_buffer = set->borrowItem();
    staging_buffer->copyFromHost(level_data, level_length);
    // Keep staging buffer alive until command buffer completes execution
    cmdbuf_impl->_referenced_buffers.push_back(staging_buffer);
    VkBufferImageCopy region = {};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset       = {0, 0, 0};
    region.imageExtent       = {uint32_t(level_width), uint32_t(level_height), 1};

    vkCmdCopyBufferToImage(vk_cmdbuf, staging_buffer->_vkbuffer, image->_vkimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    /////////////////////////////////////
    // transition to sampleable texture
    /////////////////////////////////////

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
  }

  /////////////////////////////////////
  // create image view
  /////////////////////////////////////

  auto IVCI = createImageViewInfo2D(
      image->_vkimage,                                //
      VkFormatConverter::convertBufferFormat(format), //
      VK_IMAGE_ASPECT_COLOR_BIT);
  IVCI->subresourceRange.levelCount = num_levels;

  VkImageView vkimageview;
  initializeVkStruct(vkimageview);
  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, IVCI.get(), nullptr, &vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  // vktex->_vkimageview = vkimageview;

  /////////////////////////////////////
  // create sampler
  /////////////////////////////////////

  vktex->_vksampler = _contextVK->_sampler_per_maxlod[num_levels];

  /////////////////////////////////////
  // create descriptor image info
  /////////////////////////////////////

  VkDescriptorImageInfo descimageInfo{};
  descimageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  descimageInfo.imageView   = vkimageview;
  descimageInfo.sampler     = vktex->_vksampler->_vksampler;

  /////////////////////////////////////

  ptex->_texFormat = format;
  ptex->_width     = from_chain->_width;
  ptex->_height    = from_chain->_height;
  ptex->_depth     = 1;
  ptex->_num_mips  = num_levels;
  // ptex->_target    = ETEXTARGET_2D;

  /////////////////////////////////////
  // Set default sampling mode and apply it
  /////////////////////////////////////
  
  if (num_levels > 3) {
    ptex->TexSamplingMode().presetTrilinearWrap();
  }
  this->ApplySamplingMode(ptex);

  /////////////////////////////////////

  _contextVK->endRecordCommandBuffer(vktex->_loadCB);
  cmdbuf_impl->_referenced_images.push_back(vktex->_imgobj[0]);
  _contextVK->enqueueDeferredOneShotCommand(vktex->_loadCB);

  return ptex;
}

///////////////////////////////////////////////////////////////////////////////

VulkanTextureObject::VulkanTextureObject(vktxi_rawptr_t txi) {

  initializeVkStruct(_vksampler);
  initializeVkStruct(_vkdescriptor_info);

  int count = _vkto_count.fetch_add(1);
  if (0) {
    logchan_txi->log("VulkanTextureObject count<%d>", count);
  }
}

///////////////////////////////////////////////////////////////////////////////

VulkanTextureObject::~VulkanTextureObject() {
  _vkto_count.fetch_sub(1);
  _imgobj[0] = nullptr;
  _imgobj[1] = nullptr;
  _img_sampling = nullptr;
  _loadCB = nullptr;
}

VulkanSamplerObject::VulkanSamplerObject(vkcontext_rawptr_t ctx, vksamplercreateinfo_ptr_t cinfo)
    : _cinfo(cinfo) {
  initializeVkStruct(_vksampler);
  VkResult ok = vkCreateSampler(ctx->_vkdevice, _cinfo.get(), nullptr, &_vksampler);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
