////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkTextureInterface::VkTextureInterface(vkcontext_rawptr_t ctx)
    : TextureInterface(ctx)
    , _contextVK(ctx) {
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::TexManInit() {
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
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::UpdateAnimatedTexture(Texture* ptex, TextureAnimationInst* tai) {
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::generateMipMaps(Texture* ptex) {

  vktexobj_ptr_t vktex;
  if (auto as_vktext = ptex->_impl.tryAsShared<VulkanTextureObject>()) {
    vktex = as_vktext.value();
  } else {
    vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
    OrkAssert(false);
    // vktex->_imgobj = std::make_shared<VulkanImageObject>(_contextVK, imageInfo);
  }

  auto cmdbuf      = _contextVK->beginRecordCommandBuffer();
  auto cmdbuf_impl = cmdbuf->_impl.getShared<VkCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  int32_t mipWidth  = ptex->_width;
  int32_t mipHeight = ptex->_height;

  bool keep_going = true;
  auto image      = vktex->_imgobj;
  auto barrier    = createImageBarrier(
      image->_vkimage,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_TRANSFER_READ_BIT);

  int mip_level = 0;
  while (keep_going) {
    barrier->subresourceRange.baseMipLevel = mip_level;

    /////////////////////////////////////////
    // transition mip level to transfer src
    /////////////////////////////////////////

    vkCmdPipelineBarrier(
        vk_cmdbuf,                      // cmdbuf
        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
        0,                              //  dependencyFlags
        0,
        nullptr, // memoryBarriers
        0,
        nullptr, // bufferMemoryBarriers
        1,
        barrier.get()); // imageMemoryBarriers

    /////////////////////////////////////////
    // blit mip level to next mip level (downsample)
    /////////////////////////////////////////

    VkImageBlit blit{};
    blit.srcOffsets[0]                 = {0, 0, 0};
    blit.srcOffsets[1]                 = {mipWidth, mipHeight, 1};
    blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel       = mip_level;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount     = 1;
    blit.dstOffsets[0]                 = {0, 0, 0};
    blit.dstOffsets[1]                 = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
    blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel       = mip_level + 1;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount     = 1;

    vkCmdBlitImage(
        vk_cmdbuf,
        image->_vkimage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        image->_vkimage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &blit,
        VK_FILTER_LINEAR);

    /////////////////////////////////////////
    // transition mip level to shader read
    /////////////////////////////////////////

    barrier->oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier->newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier->srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        vk_cmdbuf,                             // cmdbuf
        VK_PIPELINE_STAGE_TRANSFER_BIT,        // srcStageMask
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask
        0,                                     // dependencyFlags
        0,
        nullptr, // memoryBarriers
        0,
        nullptr, //
        1,
        barrier.get()); // imageMemoryBarriers

    /////////////////////////////////////////
    // prep for next iteration
    /////////////////////////////////////////

    if (mipWidth > 1)
      mipWidth /= 2;
    if (mipHeight > 1)
      mipHeight /= 2;

    keep_going = (mipWidth > 1) || (mipHeight > 1);
    mip_level++;
  } // while( keep_going ) { // for each mipmap...

  /////////////////////////////////////////
  // transition mip level to shader read
  /////////////////////////////////////////

  barrier->subresourceRange.baseMipLevel = mip_level - 1;
  barrier->oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier->newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier->srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier->dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(
      vk_cmdbuf,                             // cmdbuf
      VK_PIPELINE_STAGE_TRANSFER_BIT,        // srcStageMask
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask
      0,                                     // dependencyFlags
      0,
      nullptr, // memoryBarriers
      0,
      nullptr, // bufferMemoryBarriers
      1,
      barrier.get()); // imageMemoryBarriers

  _contextVK->endRecordCommandBuffer(cmdbuf);
  _contextVK->enqueueDeferredOneShotCommand(cmdbuf);
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::_createFromCompressedLoadReq(texloadreq_ptr_t req) {
  auto ptex       = req->ptex;
  auto vktex      = ptex->_impl.makeShared<VulkanTextureObject>(this);
  auto chain      = req->_cmipchain;
  size_t num_mips = chain->_levels.size();
  auto format     = chain->_format;
  // size_t size = chain->_data->length();
  int iwidth  = chain->_width;
  int iheight = chain->_height;
  for (int ilevel = 0; ilevel < num_mips; ilevel++) {
    auto& level         = chain->_levels[ilevel];
    int level_width     = level._width;
    int level_height    = level._height;
    auto level_data     = level._data->data(0);
    size_t level_length = level._data->length();
    switch (format) {
      case EBufferFormat::S3TC_DXT1:
        printf("  tex<%s> DXT1\n", ptex->_debugName.c_str());
        break;
      default:
        OrkAssert(false);
        break;
    }
    printf("  tex<%s> nummips<%d> w<%d> h<%d> \n", ptex->_debugName.c_str(), num_mips, iwidth, iheight);
    printf("  tex<%s> level<%d> w<%d> h<%d> len<%zu>\n", ptex->_debugName.c_str(), ilevel, level_width, level_height, level_length);
    // load into vulkan
    auto imageInfo   = makeVKICI(level_width, level_height, 1, format, 1);
    imageInfo->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    vktex->_imgobj   = std::make_shared<VulkanImageObject>(_contextVK, imageInfo);
    auto cmdbuf      = _contextVK->beginRecordCommandBuffer();
    auto cmdbuf_impl = cmdbuf->_impl.getShared<VkCommandBufferImpl>();
    auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;
    auto barrier     = createImageBarrier(
        vktex->_imgobj->_vkimage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VkAccessFlagBits(0),
        VK_ACCESS_TRANSFER_WRITE_BIT);
    barrier->subresourceRange.baseMipLevel = ilevel;
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
    auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, level_length, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    staging_buffer->copyFromHost(level_data, level_length);
    vktex->_staging_buffer   = staging_buffer;
    VkBufferImageCopy region = {};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset       = {0, 0, 0};
    region.imageExtent       = {uint32_t(level_width), uint32_t(level_height), 1};
    if (1)
      vkCmdCopyBufferToImage(
          vk_cmdbuf, staging_buffer->_vkbuffer, vktex->_imgobj->_vkimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
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
    _contextVK->endRecordCommandBuffer(cmdbuf);
    _contextVK->enqueueDeferredOneShotCommand(cmdbuf);
    _contextVK->onFenceCrossed([=]() {
      // staging_buffer = nullptr;
       OrkAssert(false);
    });
  }
}

///////////////////////////////////////////////////////////////////////////////

Texture* VkTextureInterface::createFromMipChain(MipChain* from_chain) {
  auto ptex  = new Texture;
  auto vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
  OrkAssert(false);
  // vktex->_imgobj = std::make_shared<VulkanImageObject>(_contextVK, imginfo);
  auto format       = from_chain->_format;
  auto type         = from_chain->_type;
  size_t num_levels = from_chain->_levels.size();

  auto imageInfo = makeVKICI(from_chain->_width, from_chain->_height, 1, format, num_levels);

  auto cmdbuf      = _contextVK->beginRecordCommandBuffer();
  auto cmdbuf_impl = cmdbuf->_impl.getShared<VkCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  auto image = vktex->_imgobj;

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

    auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, level_length, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    staging_buffer->copyFromHost(level_data, level_length);

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

  auto samplerInfo    = makeVKSCI();
  samplerInfo->maxLod = float(num_levels);

  VkSampler vksampler;
  initializeVkStruct(vksampler);
  ok = vkCreateSampler(_contextVK->_vkdevice, samplerInfo.get(), nullptr, &vksampler);
  OrkAssert(VK_SUCCESS == ok);

  // vktex->_vksampler = vksampler;

  /////////////////////////////////////
  // create descriptor image info
  /////////////////////////////////////

  VkDescriptorImageInfo descimageInfo{};
  descimageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  descimageInfo.imageView   = vkimageview;
  descimageInfo.sampler     = vksampler;

  /////////////////////////////////////

  ptex->_texFormat = format;
  ptex->_width     = from_chain->_width;
  ptex->_height    = from_chain->_height;
  ptex->_depth     = 1;
  ptex->_num_mips  = num_levels;
  // ptex->_target    = ETEXTARGET_2D;
  ptex->_debugName = "vulkan_texture";

  _contextVK->endRecordCommandBuffer(cmdbuf);
  _contextVK->enqueueDeferredOneShotCommand(cmdbuf);

  return ptex;
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::initTextureFromData(Texture* ptex, TextureInitData tid) {

  auto vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);

  /////////////////////////////////////
  // map staging memory and copy
  /////////////////////////////////////

  auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, tid.computeDstSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  staging_buffer->copyFromHost(tid._data, tid._truncation_length);
  vktex->_staging_buffer = staging_buffer;

  /////////////////////////////////////

  ptex->_texFormat = tid._dst_format;
  ptex->_width     = tid._w;
  ptex->_height    = tid._h;
  ptex->_depth     = tid._d;
  ptex->_num_mips  = 1;
  ptex->_debugName = "vulkan_texture";

  auto VKICI   = makeVKICI(tid._w, tid._h, tid._d, tid._dst_format, 1);
  VKICI->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  vktex->_imgobj = std::make_shared<VulkanImageObject>(_contextVK, VKICI);

  /////////////////////////////////////

  auto IVCI = createImageViewInfo2D(
      vktex->_imgobj->_vkimage,                                //
      VkFormatConverter::convertBufferFormat(tid._dst_format), //
      VK_IMAGE_ASPECT_COLOR_BIT);

  initializeVkStruct(vktex->_vkimageview);
  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, IVCI.get(), nullptr, &vktex->_imgobj->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  /////////////////////////////////////

  vktex->_sampler_info = makeVKSCI();
  ok                   = vkCreateSampler(_contextVK->_vkdevice, vktex->_sampler_info.get(), nullptr, &vktex->_vksampler);
  OrkAssert(VK_SUCCESS == ok);

  /////////////////////////////////////

  vktex->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vktex->_vkdescriptor_info.imageView   = vktex->_vkimageview;
  vktex->_vkdescriptor_info.sampler     = vktex->_vksampler;

  /////////////////////////////////////
  // transition to transfer dst (for copy)
  /////////////////////////////////////

  auto cmdbuf      = _contextVK->beginRecordCommandBuffer();
  auto cmdbuf_impl = cmdbuf->_impl.getShared<VkCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  auto barrier = createImageBarrier(
      vktex->_imgobj->_vkimage,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VkAccessFlagBits(0),
      VK_ACCESS_TRANSFER_WRITE_BIT);

  vkCmdPipelineBarrier(
      vk_cmdbuf,                         //
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, //
      VK_PIPELINE_STAGE_TRANSFER_BIT,    //
      0,                                 //
      0,
      nullptr, //
      0,
      nullptr, //
      1,
      barrier.get()); //

  /////////////////////////////////////
  // staging mem -> image
  /////////////////////////////////////

  VkBufferImageCopy region{};
  initializeVkStruct(region);
  region.bufferOffset      = 0;
  region.bufferRowLength   = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource  = {
      VK_IMAGE_ASPECT_COLOR_BIT, //
      0,
      0,
      1};
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {uint32_t(tid._w), uint32_t(tid._h), 1};

  vkCmdCopyBufferToImage(
      vk_cmdbuf,                            //
      staging_buffer->_vkbuffer,            //
      vktex->_imgobj->_vkimage,             //
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, //
      1,
      &region); //
  _contextVK->onFenceCrossed([=]() {
    vktex->_staging_buffer = nullptr; // release staging buffer
  });
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

  _contextVK->endRecordCommandBuffer(cmdbuf);
  _contextVK->enqueueDeferredOneShotCommand(cmdbuf);

  /////////////////////////////////////

  // ptex->_target = tid._type;
  ptex->_impl  = vktex;
  ptex->_dirty = false;
}

///////////////////////////////////////////////////////////////////////////////

VkTextureAsyncTask::VkTextureAsyncTask() {
}

///////////////////////////////////////////////////////////////////////////////

VulkanTextureObject::VulkanTextureObject(vktxi_rawptr_t txi) {
  initializeVkStruct(_vksampler);
  initializeVkStruct(_vkdescriptor_info);
}

///////////////////////////////////////////////////////////////////////////////

VulkanTextureObject::~VulkanTextureObject() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
