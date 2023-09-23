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
  if( auto as_vktext = ptex->_impl.tryAsShared<VulkanTextureObject>() ){
    vktex = as_vktext.value();
  } else {
    vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
  }

    auto cmdbuf = _contextVK->beginRecordCommandBuffer();
    auto cmdbuf_impl = cmdbuf->_impl.getShared<VkCommandBufferImpl>();
    auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;


  int32_t mipWidth  = ptex->_width;
  int32_t mipHeight = ptex->_height;

  bool keep_going = true;

  auto barrier = createImageBarrier(vktex->_vkimage,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_READ_BIT
                                    );

    int mip_level = 0;
    while( keep_going ) {
        barrier->subresourceRange.baseMipLevel = mip_level;

        /////////////////////////////////////////
        // transition mip level to transfer src
        /////////////////////////////////////////

        vkCmdPipelineBarrier( vk_cmdbuf, // cmdbuf
                              VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
                              VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
                              0, //  dependencyFlags
                              0, nullptr, // memoryBarriers
                              0, nullptr, // bufferMemoryBarriers
                              1, barrier.get()); // imageMemoryBarriers

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
        blit.dstSubresource.mipLevel       = mip_level+1;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = 1;

        vkCmdBlitImage(
            vk_cmdbuf,
            vktex->_vkimage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            vktex->_vkimage,
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

        vkCmdPipelineBarrier( vk_cmdbuf, // cmdbuf
                              VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask
                              0, // dependencyFlags
                              0, nullptr, // memoryBarriers
                              0, nullptr,  // 
                              1, barrier.get()); // imageMemoryBarriers


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

  vkCmdPipelineBarrier( vk_cmdbuf, // cmdbuf
                        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask
                        0, // dependencyFlags
                        0, nullptr,  // memoryBarriers
                        0, nullptr,  // bufferMemoryBarriers
                        1, barrier.get()); // imageMemoryBarriers

  _contextVK->endRecordCommandBuffer(cmdbuf);
  _contextVK->enqueueDeferredOneShotCommand(cmdbuf);
  
}

///////////////////////////////////////////////////////////////////////////////

using vkimagecreateinfo_ptr_t = std::shared_ptr<VkImageCreateInfo>;

vkimagecreateinfo_ptr_t makeVKICI(int w, int h, int d, //
                                  EBufferFormat fmt, int nummips) { //
  auto ret = std::make_shared<VkImageCreateInfo>();
  initializeVkStruct(*ret, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
  ret->imageType     = VK_IMAGE_TYPE_2D;
  ret->format        = VkFormatConverter::convertBufferFormat(fmt);
  ret->extent.width  = w;
  ret->extent.height = h;
  ret->extent.depth  = d;
  ret->mipLevels     = nummips;
  //ret->arrayLayers   = 1;
  //ret->samples       = VK_SAMPLE_COUNT_1_BIT;
  //ret->tiling        = VK_IMAGE_TILING_OPTIMAL;
  //ret->usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  //ret->sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  //ret->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  return ret;
}

///////////////////////////////////////////////////////////////////////////////
using vksamplercreateinfo_ptr_t = std::shared_ptr<VkSamplerCreateInfo>;
///////////////////////////////////////////////////////////////////////////////

vksamplercreateinfo_ptr_t makeVKSCI() { //
  auto ret = std::make_shared<VkSamplerCreateInfo>();
  initializeVkStruct(*ret, VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
  ret->magFilter               = VK_FILTER_LINEAR;
  ret->minFilter               = VK_FILTER_LINEAR;
  ret->addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  ret->addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  ret->addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  ret->anisotropyEnable        = VK_TRUE;
  ret->maxAnisotropy           = 16;
  ret->borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  ret->unnormalizedCoordinates = VK_FALSE;
  ret->compareEnable           = VK_FALSE;
  ret->compareOp               = VK_COMPARE_OP_ALWAYS;
  ret->mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  ret->mipLodBias              = 0.0f;
  ret->minLod                  = 0.0f;
  ret->maxLod                  = 1.0f;
  return ret;
}

///////////////////////////////////////////////////////////////////////////////

Texture* VkTextureInterface::createFromMipChain(MipChain* from_chain) {
    auto ptex = new Texture;
    auto vktex  = ptex->_impl.makeShared<VulkanTextureObject>(this);

    auto format = from_chain->_format;
    auto type = from_chain->_type;
    size_t num_levels = from_chain->_levels.size();

    auto imageInfo = makeVKICI(from_chain->_width, from_chain->_height, 1, format, num_levels);

    auto cmdbuf = _contextVK->beginRecordCommandBuffer();
    auto cmdbuf_impl = cmdbuf->_impl.getShared<VkCommandBufferImpl>();
    auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

    for( size_t l=0; l<num_levels; l++ ){

        auto level = from_chain->_levels[l];
        int level_width = level->_width;
        int level_height = level->_height;
        void* level_data = level->_data;
        size_t level_length = level->_length;

        /////////////////////////////////////
        // transition to transfer dst (for copy)
        /////////////////////////////////////

        auto barrier = createImageBarrier(vktex->_vkimage,
                                          VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          VkAccessFlagBits(0),
                                          VK_ACCESS_TRANSFER_WRITE_BIT
                                          );

        barrier->subresourceRange.baseMipLevel   = l;

        vkCmdPipelineBarrier(
            vk_cmdbuf, // cmdbuf
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
            0, // dependencyFlags
            0, nullptr, // memoryBarriers
            0, nullptr, // bufferMemoryBarriers
            1, barrier.get()); // imageMemoryBarriers
        
        /////////////////////////////////////
        // map staging memory and copy
        /////////////////////////////////////

        auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, level_length, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        staging_buffer->copyFromHost(level_data,level_length);

        VkBufferImageCopy region = {};
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageOffset       = {0, 0, 0};
        region.imageExtent       = {uint32_t(level_width), uint32_t(level_height), 1};

        vkCmdCopyBufferToImage(
            vk_cmdbuf,
            staging_buffer->_vkbuffer,
            vktex->_vkimage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);
        
        /////////////////////////////////////
        // transition to sampleable texture
        /////////////////////////////////////

        barrier->oldLayout         = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier->newLayout         = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier->srcAccessMask     = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier->dstAccessMask     = VK_ACCESS_SHADER_READ_BIT;

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

    VkImageViewCreateInfo viewInfo{};
    initializeVkStruct(viewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    viewInfo.image                           = vktex->_vkimage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VkFormatConverter::convertBufferFormat(format);
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = num_levels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    
    VkImageView vkimageview;
    initializeVkStruct(vkimageview);
    VkResult ok = vkCreateImageView(_contextVK->_vkdevice, &viewInfo, nullptr, &vkimageview);
    OrkAssert(VK_SUCCESS == ok);

    //vktex->_vkimageview = vkimageview;

    /////////////////////////////////////
    // create sampler
    /////////////////////////////////////

    auto samplerInfo = makeVKSCI();
    samplerInfo->maxLod                  = float(num_levels);

    VkSampler vksampler;
    initializeVkStruct(vksampler);
    ok = vkCreateSampler(_contextVK->_vkdevice, samplerInfo.get(), nullptr, &vksampler);
    OrkAssert(VK_SUCCESS == ok);

    //vktex->_vksampler = vksampler;

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
   //ptex->_target    = ETEXTARGET_2D;
    ptex->_debugName = "vulkan_texture";

    _contextVK->endRecordCommandBuffer(cmdbuf);
    _contextVK->enqueueDeferredOneShotCommand(cmdbuf);

    return ptex;
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::initTextureFromData(Texture* ptex, TextureInitData tid) {
  auto vktex  = ptex->_impl.makeShared<VulkanTextureObject>(this);
  ptex->_texFormat = tid._dst_format;
  ptex->_width     = tid._w;
  ptex->_height    = tid._h;
  ptex->_depth     = tid._d;
  ptex->_num_mips  = 1;
  ptex->_debugName = "vulkan_texture";

  auto imageInfo = makeVKICI(tid._w, tid._h, tid._d, tid._dst_format, 1);
  imageInfo->arrayLayers   = 1;
  imageInfo->samples       = VK_SAMPLE_COUNT_1_BIT;
  imageInfo->tiling        = VK_IMAGE_TILING_OPTIMAL;
  imageInfo->usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo->sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  initializeVkStruct(vktex->_vkimage);
  VkResult ok = vkCreateImage(_contextVK->_vkdevice, imageInfo.get(), nullptr, &vktex->_vkimage);
  OrkAssert(VK_SUCCESS == ok);

  /////////////////////////////////////

  vktex->_imgmem = std::make_shared<VulkanMemoryForImage>(_contextVK, vktex->_vkimage,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vkBindImageMemory(_contextVK->_vkdevice, vktex->_vkimage, *vktex->_imgmem->_vkmem, 0);

  /////////////////////////////////////

  VkImageViewCreateInfo viewInfo{};
  initializeVkStruct(viewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
  viewInfo.image                           = vktex->_vkimage;
  viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format                          = VkFormatConverter::convertBufferFormat(tid._dst_format);
  viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel   = 0;
  viewInfo.subresourceRange.levelCount     = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = 1;

  initializeVkStruct(vktex->_vkimageview);
  ok = vkCreateImageView(_contextVK->_vkdevice, &viewInfo, nullptr, &vktex->_vkimageview);
  OrkAssert(VK_SUCCESS == ok);

  /////////////////////////////////////

  auto samplerInfo = makeVKSCI();

  VkSampler vksampler;
  initializeVkStruct(vksampler);
  ok = vkCreateSampler(_contextVK->_vkdevice, samplerInfo.get(), nullptr, &vksampler);
  OrkAssert(VK_SUCCESS == ok);

  /////////////////////////////////////

  VkDescriptorImageInfo descimageInfo{};
  descimageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  descimageInfo.imageView   = vktex->_vkimageview;
  descimageInfo.sampler     = vksampler;

  /////////////////////////////////////

  auto cmdbuf = _contextVK->beginRecordCommandBuffer();
  auto cmdbuf_impl = cmdbuf->_impl.getShared<VkCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  auto barrier = createImageBarrier(vktex->_vkimage,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VkAccessFlagBits(0),
                                    VK_ACCESS_TRANSFER_WRITE_BIT
                                    );
  barrier->subresourceRange  = {
    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags     aspectMask;
    0,                         // uint32_t               baseMipLevel;
    1,                         // uint32_t               levelCount;
    0,                         // uint32_t               baseArrayLayer;
    1};                        // uint32_t               layerCount;

  vkCmdPipelineBarrier(
      vk_cmdbuf,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      0,
      0,
      nullptr,
      0,
      nullptr,
      1,
      barrier.get());

  /////////////////////////////////////
  // map staging memory and copy
  /////////////////////////////////////

  auto staging_buffer = std::make_shared<VulkanBuffer>(_contextVK, tid.computeDstSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  staging_buffer->copyFromHost(tid._data,tid._truncation_length);

  /////////////////////////////////////
  // GPU mem -> image
  /////////////////////////////////////

  VkBufferImageCopy region{};
  region.bufferOffset      = 0;
  region.bufferRowLength   = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageOffset       = {0, 0, 0};
  region.imageExtent       = {uint32_t(tid._w), uint32_t(tid._h), 1};

  vkCmdCopyBufferToImage(
      vk_cmdbuf,
      staging_buffer->_vkbuffer,
      vktex->_vkimage,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &region);

  /////////////////////////////////////
  // transition to sampleable texture
  /////////////////////////////////////

  barrier->oldLayout         = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier->newLayout         = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier->srcAccessMask     = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier->dstAccessMask     = VK_ACCESS_SHADER_READ_BIT;

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

  //ptex->_target = tid._type;
  ptex->_impl = vktex;
  ptex->_dirty = false;


}

///////////////////////////////////////////////////////////////////////////////

VkTextureAsyncTask::VkTextureAsyncTask() {
}

///////////////////////////////////////////////////////////////////////////////

VulkanTextureObject::VulkanTextureObject(vktxi_rawptr_t txi) {
}

///////////////////////////////////////////////////////////////////////////////

VulkanTextureObject::~VulkanTextureObject() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
