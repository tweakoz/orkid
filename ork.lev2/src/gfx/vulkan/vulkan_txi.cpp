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

void VkTextureInterface::initTextureFromData(Texture* ptex, TextureInitData tid) {
}

///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::generateMipMaps(Texture* ptex) {

  vktexobj_ptr_t vktex;
  if( auto as_vktext = ptex->_impl.tryAsShared<VulkanTextureObject>() ){
    vktex = as_vktext.value();
  } else {
    vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
  }

  auto cmdbuf = std::make_shared<CommandBuffer>();
  _contextVK->pushCommandBuffer(cmdbuf);
  auto cmdbuf_impl = _contextVK->_cmdbufcur_gfx;
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  VkImageMemoryBarrier barrier{};
  barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image                           = vktex->_vkimage;
  barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount     = 1;
  barrier.subresourceRange.levelCount     = 1;

  int32_t mipWidth  = ptex->_width;
  int32_t mipHeight = ptex->_height;

  bool keep_going = true;

    int mip_level = 0;
    while( keep_going ) {
        barrier.subresourceRange.baseMipLevel = mip_level;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(
            vk_cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

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

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            vk_cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1)
        mipWidth /= 2;
        if (mipHeight > 1)
        mipHeight /= 2;

        keep_going = (mipWidth > 1) || (mipHeight > 1);
        mip_level++;
    }

  barrier.subresourceRange.baseMipLevel = mip_level - 1;
  barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(
      vk_cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

  _contextVK->popCommandBuffer();
  _contextVK->enqueueSecondaryCommandBuffer(cmdbuf);
  
}

///////////////////////////////////////////////////////////////////////////////

Texture* VkTextureInterface::createFromMipChain(MipChain* from_chain) {
    auto ptex = new Texture;
    auto vktex  = ptex->_impl.makeShared<VulkanTextureObject>(this);

    auto format = from_chain->_format;
    auto type = from_chain->_type;
    size_t num_levels = from_chain->_levels.size();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = VkFormat(format);
    imageInfo.extent.width  = from_chain->_width;
    imageInfo.extent.height = from_chain->_height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = num_levels;

    for( size_t l=0; l<num_levels; l++ ){

        auto level = from_chain->_levels[l];
        int level_width = level->_width;
        int level_height = level->_height;
        void* level_data = level->_data;
        size_t level_length = level->_length;

        


    }


    return ptex;
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
