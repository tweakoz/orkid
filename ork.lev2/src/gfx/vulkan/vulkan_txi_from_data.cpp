
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
static logchannel_ptr_t logchan_txidata = logger()->createChannel("VKTXIDAT", fvec3(0.8, 0.2, 0.5), true);
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
  if((count&0xff)==0){
    logchan_txidata->log("InFlightTextureTransfer count<%d> SN<%d>", count, SN);
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

  ptex->_debugName = "VkTextureInterface::initTextureFromData";

  /////////////////////////////////////
  // hash the image creation parameters
  /////////////////////////////////////

  uint64_t usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT //
                 | VK_IMAGE_USAGE_SAMPLED_BIT;

  uint64_t image_params_hash = hashImageCreationParams(
      tid._w,          //
      tid._h,          //
      tid._d,          //
      tid._dst_format, //
      1,               // nummips
      usage);          // usage

  /////////////////////////////////////
  
  bool hash_changed = false;
  
  vktexobj_ptr_t vktex;
  if (auto existing = ptex->_impl.tryAsShared<VulkanTextureObject>()) {
    // Texture already exists - we're updating it
    vktex = existing.value();
    hash_changed = (image_params_hash != vktex->_image_params_hash);
    if(hash_changed){
      _texobjs_pending_for_deletion.insert(vktex);
      vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
      vktex->_image_params_hash = image_params_hash;
    }
  } else {
    // New texture
    vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
    vktex->_image_params_hash = image_params_hash;
    hash_changed = true;
  }

  /////////////////////////////////////
  // allocate a (cpuside) staging buffer
  // this is used to copy data from the application
  /////////////////////////////////////

  size_t transfer_size = tid.computeDstSize();
  auto poolForSize     = stagingBufferPoolForSrcOfSize(transfer_size);
  auto staging_buffer  = poolForSize->borrowItem();
  auto command_buffer = _seccmdbufpool_xfer->borrowItem();
  /////////////////////////////////////
  // create a transfer object
  /////////////////////////////////////

  auto transfer = std::make_shared<InFlightTextureTransfer>( _contextVK,     //
                                                             staging_buffer, //
                                                             command_buffer );
  vktex->_inflight_transfers.insert(transfer);

  auto cmdbuf_impl = transfer->_command_buffer->_impl.getShared<VkSecondaryCommandBufferImpl>();
  auto vk_cmdbuf   = cmdbuf_impl->_vkcmdbuf;

  /////////////////////////////////////
  // Set up completion callback
  /////////////////////////////////////

  auto tlsema         = std::make_shared<VulkanCompletionSemaphore>(this->_contextVK);
  cmdbuf_impl->_completionSemaphore = tlsema;
  tlsema->_onComplete = [=]() {
    vktex->_inflight_transfers.erase(transfer);
    poolForSize->returnItem(staging_buffer);
    _seccmdbufpool_xfer->returnItem(transfer->_command_buffer);
    //printf("free stgbuf<%p>\n", (void*)staging_buffer.get());
  };
  /////////////////////////////////////
  // copy data from application to staging buffer (synchronously)
  //  after this is complete,
  //  the application buffer can be released
  /////////////////////////////////////

  staging_buffer->copyFromHost(tid._data, transfer_size);


  /////////////////////////////////////
  // if the image params have changed
  //   (e.g. size, format, usage)
  //   create a new VkImage and VkImageView
  // otherwise, reuse the existing VkImage and VkImageView
  /////////////////////////////////////

  if (hash_changed) {

    auto VKICI   = makeVKICI(tid._w, tid._h, tid._d, tid._dst_format, 1);
    VKICI->usage = usage;

    vktex->_imgobj = std::make_shared<VulkanImageObject>(_contextVK, VKICI);


    auto IVCI = createImageViewInfo2D(
        vktex->_imgobj->_vkimage,                                //
        VkFormatConverter::convertBufferFormat(tid._dst_format), //
        VK_IMAGE_ASPECT_COLOR_BIT);

    initializeVkStruct(vktex->_imgobj->_vkimageview);
    VkResult ok = vkCreateImageView(_contextVK->_vkdevice, IVCI.get(), nullptr, &vktex->_imgobj->_vkimageview);
    OrkAssert(VK_SUCCESS == ok);

    vktex->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vktex->_vkdescriptor_info.imageView   = vktex->_imgobj->_vkimageview;
    OrkAssert(vktex->_imgobj->_vkimageview != VK_NULL_HANDLE);

    ptex->_impl  = vktex;
    ptex->_texFormat = tid._dst_format;
    ptex->_width     = tid._w;
    ptex->_height    = tid._h;
    ptex->_depth     = tid._d;
    ptex->_num_mips  = 1;

  }

  /////////////////////////////////////

  vktex->_vksampler = _contextVK->_sampler_base;
  // vktex->_vkdescriptor_info.sampler     = vktex->_vksampler->_vksampler;

  /////////////////////////////////////
  // record transition to transfer destination (for copy)
  /////////////////////////////////////

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
  // record transfer from staging mem to image
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
  // record transition to sampleable texture
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
  // enqueue recorded texture update cmdbuf
  /////////////////////////////////////

  _contextVK->endRecordCommandBuffer(transfer->_command_buffer);
  _contextVK->enqueueDeferredOneShotCommand(transfer->_command_buffer);

  /////////////////////////////////////

  ptex->_dirty = false;
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
