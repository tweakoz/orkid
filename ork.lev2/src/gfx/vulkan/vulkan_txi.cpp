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
static logchannel_ptr_t logchan_txi = logger()->createChannel("VKTXI", fvec3(0.8, 0.2, 0.5), true);

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
  ptex->_debugName = "VkTextureInterface::generateMipMaps";
  vktexobj_ptr_t vktex;
  if (auto as_vktext = ptex->_impl.tryAsShared<VulkanTextureObject>()) {
    vktex = as_vktext.value();
  } else {
    vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
    OrkAssert(false);
    // vktex->_imgobj = std::make_shared<VulkanImageObject>(_contextVK, imageInfo);
  }

  vktex->_loadCB   = _contextVK->beginRecordCommandBuffer(nullptr,"VkTextureInterface::generateMipMaps");

  auto cmdbuf_impl = vktex->_loadCB->_impl.getShared<VkCommandBufferImpl>();
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

  /////////////////////////////////////

  _contextVK->onFenceCrossed([=]() {
    //vktex->_staging_buffers.erase(staging_buffer);
    //vktex->_loadCB = nullptr;
  });

  /////////////////////////////////////

  _contextVK->endRecordCommandBuffer(vktex->_loadCB);
  _contextVK->enqueueDeferredOneShotCommand(vktex->_loadCB);
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::_createFromCompressedLoadReq(texloadreq_ptr_t req) {
  auto ptex = req->ptex;
  printf("xxx _createFromCompressedLoadReq<%p:%s>\n", (void*)ptex.get(), ptex->_debugName.c_str());
  ptex->_debugName = "VkTextureInterface::_createFromCompressedLoadReq";

  auto vktex       = ptex->_impl.makeShared<VulkanTextureObject>(this);
  auto chain       = req->_cmipchain;
  size_t num_mips  = chain->_levels.size();
  auto format      = chain->_format;
  int iwidth       = chain->_width;
  int iheight      = chain->_height;

  // Create a single VkImage with all mip levels
  auto imageInfo   = makeVKICI(iwidth, iheight, 1, format, num_mips);
  imageInfo->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  vktex->_imgobj   = std::make_shared<VulkanImageObject>(_contextVK, imageInfo, "imgmemcfclr");

  vktex->_loadCB   = _contextVK->beginRecordCommandBuffer(nullptr, "VkTextureInterface::_createFromCompressedLoadReq");

  auto cmdbuf_impl = vktex->_loadCB->_impl.getShared<VkCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  for (int ilevel = 0; ilevel < num_mips; ilevel++) {
    auto& level         = chain->_levels[ilevel];
    int level_width     = level._width;
    int level_height    = level._height;
    auto level_data     = level._data->data(0);
    size_t level_length = level._data->length();

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

    // Copy the mip level data from the staging buffer to the image
    auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, level_length, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "staging");
    staging_buffer->copyFromHost(level_data, level_length);
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
  }
  /////////////////////////////////////
  // create image view
  /////////////////////////////////////

  auto IVCI = createImageViewInfo2D(
      vktex->_imgobj->_vkimage,                       //
      VkFormatConverter::convertBufferFormat(format), //
      VK_IMAGE_ASPECT_COLOR_BIT);
  IVCI->subresourceRange.levelCount = num_mips;

  initializeVkStruct(vktex->_imgobj->_vkimageview);
  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, IVCI.get(), nullptr, &vktex->_imgobj->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  /////////////////////////////////////
  // descriptor image info
  /////////////////////////////////////

  vktex->_vksampler                     = _contextVK->_sampler_per_maxlod[num_mips];
  vktex->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vktex->_vkdescriptor_info.imageView   = vktex->_imgobj->_vkimageview;
  vktex->_vkdescriptor_info.sampler     = vktex->_vksampler->_vksampler;

  /////////////////////////////////////

  _contextVK->onFenceCrossed([=]() {
    //vktex->_staging_buffers.clear();
    //vktex->_loadCB = nullptr;
  });

  /////////////////////////////////////

  _contextVK->endRecordCommandBuffer(vktex->_loadCB);
  _contextVK->enqueueDeferredOneShotCommand(vktex->_loadCB);
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

  vktex->_loadCB   = _contextVK->beginRecordCommandBuffer(nullptr,"VkTextureInterface::createFromMipChain");

  auto cmdbuf_impl = vktex->_loadCB->_impl.getShared<VkCommandBufferImpl>();
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

    auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, level_length, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,"staging");
    staging_buffer->copyFromHost(level_data, level_length);
    vktex->_staging_buffers.insert(staging_buffer);
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

  _contextVK->onFenceCrossed([=]() {
    //vktex->_staging_buffers.clear();
    //vktex->_loadCB = nullptr;
  });

  /////////////////////////////////////

  _contextVK->endRecordCommandBuffer(vktex->_loadCB);
  _contextVK->enqueueDeferredOneShotCommand(vktex->_loadCB);

  return ptex;
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::initTextureFromData(Texture* ptex, TextureInitData tid) {
  ptex->_debugName = "VkTextureInterface::initTextureFromData";

  auto vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);

  /////////////////////////////////////
  // map staging memory and copy
  /////////////////////////////////////

  auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, tid.computeDstSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "staging");
  staging_buffer->copyFromHost(tid._data, tid._truncation_length);
  vktex->_staging_buffers.insert(staging_buffer);

  /////////////////////////////////////

  ptex->_texFormat = tid._dst_format;
  ptex->_width     = tid._w;
  ptex->_height    = tid._h;
  ptex->_depth     = tid._d;
  ptex->_num_mips  = 1;

  auto VKICI   = makeVKICI(tid._w, tid._h, tid._d, tid._dst_format, 1);
  VKICI->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  vktex->_imgobj = std::make_shared<VulkanImageObject>(_contextVK, VKICI);

  /////////////////////////////////////

  auto IVCI = createImageViewInfo2D(
      vktex->_imgobj->_vkimage,                                //
      VkFormatConverter::convertBufferFormat(tid._dst_format), //
      VK_IMAGE_ASPECT_COLOR_BIT);

  initializeVkStruct(vktex->_imgobj->_vkimageview);
  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, IVCI.get(), nullptr, &vktex->_imgobj->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  /////////////////////////////////////

  vktex->_vksampler = _contextVK->_sampler_base;

  /////////////////////////////////////

  vktex->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vktex->_vkdescriptor_info.imageView   = vktex->_imgobj->_vkimageview;
  OrkAssert(vktex->_imgobj->_vkimageview != VK_NULL_HANDLE);
  // vktex->_vkdescriptor_info.sampler     = vktex->_vksampler->_vksampler;

  /////////////////////////////////////
  // transition to transfer dst (for copy)
  /////////////////////////////////////

  vktex->_loadCB   = _contextVK->beginRecordCommandBuffer(nullptr,"VkTextureInterface::initTextureFromData");

  auto cmdbuf_impl = vktex->_loadCB->_impl.getShared<VkCommandBufferImpl>();
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

  /////////////////////////////////////

  _contextVK->onFenceCrossed([=]() {
    //vktex->_staging_buffers.erase(staging_buffer);
    //vktex->_loadCB = nullptr;
  });

  /////////////////////////////////////

  _contextVK->endRecordCommandBuffer(vktex->_loadCB);
  _contextVK->enqueueDeferredOneShotCommand(vktex->_loadCB);

  /////////////////////////////////////

  // ptex->_target = tid._type;
  ptex->_impl  = vktex;
  ptex->_dirty = false;
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::_initTextureFromRtBuffer(RtBuffer* rtbuffer) {
  auto ptex = rtbuffer->texture();
  OrkAssert(ptex);
  auto& teximpl = ptex->_impl.makeShared<VulkanTextureObject>(_contextVK->_txi.get());

  auto format  = rtbuffer->format();
  int iwidth   = rtbuffer->_width;
  int iheight  = rtbuffer->_height;
  int num_mips = 1;
  auto fmt_str = EBufferFormatToName(format);

  logchan_txi->log(
      "_initTextureFromRtBuffer ptex<%p:%s> w<%d> h<%d> fmt<%s>",
      (void*)ptex,
      ptex->_debugName.c_str(),
      iwidth,
      iheight,
      fmt_str.c_str());

  /////////////////////////////////////
  // create image object
  /////////////////////////////////////

  auto img_info   = makeVKICI(iwidth, iheight, 1, format, num_mips);
  img_info->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

  teximpl->_imgobj    = std::make_shared<VulkanImageObject>(_contextVK, img_info);
  teximpl->_vksampler = _contextVK->_sampler_base;

  _contextVK->_setObjectDebugName(teximpl->_imgobj->_vkimage, VK_OBJECT_TYPE_IMAGE, (rtbuffer->_debugName + ".vkimg").c_str());

  /////////////////////////////////////
  // create image view
  /////////////////////////////////////

  auto IVCI = createImageViewInfo2D(
      teximpl->_imgobj->_vkimage,                     //
      VkFormatConverter::convertBufferFormat(format), //
      VK_IMAGE_ASPECT_COLOR_BIT);
  IVCI->subresourceRange.levelCount = num_mips;

  initializeVkStruct(teximpl->_imgobj->_vkimageview);
  VkResult ok = vkCreateImageView(_contextVK->_vkdevice, IVCI.get(), nullptr, &teximpl->_imgobj->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  OrkAssert(teximpl->_imgobj->_vkimageview != VK_NULL_HANDLE);

  /////////////////////////////////////
  // create descriptor image info
  /////////////////////////////////////

  teximpl->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  teximpl->_vkdescriptor_info.imageView   = teximpl->_imgobj->_vkimageview;
  teximpl->_vkdescriptor_info.sampler     = teximpl->_vksampler->_vksampler;

  auto rtb_impl        = rtbuffer->_impl.getShared<VklRtBufferImpl>();
  rtb_impl->_vkimgview = teximpl->_imgobj->_vkimageview;
  rtb_impl->_vkimg     = teximpl->_imgobj->_vkimage;
  rtb_impl->setLayout(VK_IMAGE_LAYOUT_UNDEFINED);
  rtb_impl->_teximpl = teximpl;

  /////////////////////////////////////
  // transition to transfer dst (for copy)
  /////////////////////////////////////

  auto cmdbuf      = _contextVK->beginRecordCommandBuffer(nullptr,"VkTextureInterface::_initTextureFromRtBuffer");

  auto cmdbuf_impl = cmdbuf->_impl.getShared<VkCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  auto barrier = createImageBarrier(
      teximpl->_imgobj->_vkimage,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VkAccessFlagBits(0),
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      0,
      0,
      nullptr,
      0,
      nullptr,
      1,
      barrier.get());

  /////////////////////////////////////

  _contextVK->endRecordCommandBuffer(cmdbuf);
  _contextVK->enqueueSecondaryCommandBuffer(cmdbuf);
  //_contextVK->enqueueDeferredOneShotCommand(cmdbuf);
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

VulkanSamplerObject::VulkanSamplerObject(vkcontext_rawptr_t ctx, vksamplercreateinfo_ptr_t cinfo)
    : _cinfo(cinfo) {
  initializeVkStruct(_vksampler);
  VkResult ok = vkCreateSampler(ctx->_vkdevice, _cinfo.get(), nullptr, &_vksampler);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
