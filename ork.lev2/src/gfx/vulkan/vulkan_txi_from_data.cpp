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

  bool async = true; //tid._allow_async;

  ptex->_source = ETextureSource::FROM_DATA;
  //ptex->_debugName = "VkTextureInterface::initTextureFromData";
  bool is_brdf = ptex->_debugName.find("brdfIntegrationMap") != std::string::npos;
  /////////////////////////////////////
  // Handle format conversion for macOS
  /////////////////////////////////////
  
  EBufferFormat actual_dst_format = convertFormatForPlatform(tid._dst_format);
  bool needs_conversion = (actual_dst_format != tid._dst_format);

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
      1,               // nummips
      usage);          // usage

  /////////////////////////////////////
  
  bool hash_changed = false;
  
  vktexobj_ptr_t vktex;
  if (auto existing = ptex->_impl.tryAsShared<VulkanTextureObject>()) {
    // Texture already exists - we're updating it
    vktex = existing.value();
    hash_changed = (format_hash != vktex->_format_hash);
    if(hash_changed){
      _texobjs_pending_for_deletion.insert(vktex);
      vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
      vktex->_format_hash = format_hash;
    }
  } else {
    // New texture
    vktex = ptex->_impl.makeShared<VulkanTextureObject>(this);
    vktex->_format_hash = format_hash;
    hash_changed = true;
  }

  /////////////////////////////////////
  // allocate a (cpuside) staging buffer
  // this is used to copy data from the application
  /////////////////////////////////////

  size_t src_size = tid.computeSrcSize();
  size_t transfer_size = src_size;
  
  // Adjust transfer size for format conversion
  if (needs_conversion) {
    if (tid._dst_format == EBufferFormat::BGR8 || tid._dst_format == EBufferFormat::RGB8) {
      // 3 components to 4 components (8-bit)
      transfer_size = tid._w * tid._h * tid._d * 4;
    } else if (tid._dst_format == EBufferFormat::RGB32F) {
      // 3 components to 4 components (32-bit float)
      transfer_size = tid._w * tid._h * tid._d * 4 * sizeof(float);
    }
  }
  
  auto poolForSize     = stagingBufferPoolForSrcOfSize(transfer_size);
  auto staging_buffer  = poolForSize->borrowItem();

  /////////////////////////////////////
  // asynchronously copy data from host to staging buffer
  /////////////////////////////////////

  // Debug logging for RGBA32F textures
  if (tid._dst_format == EBufferFormat::RGBA32F && !ptex->_debugName.empty()) {
    if(is_brdf)logchan_txidata->log("Uploading RGBA32F texture <%p:%s> size<%zux%zu> data<%p>", 
                         (void*) ptex, ptex->_debugName.c_str(), tid._w, tid._h, tid._data);
    // Verify data is not all zeros
    const float* float_data = (const float*)tid._data;
    float sum = 0.0f;
    for (size_t i = 0; i < 16; i++) { // Check first 16 floats
      sum += std::abs(float_data[i]);
    }
    if(is_brdf)logchan_txidata->log("  First 16 floats sum: %f", sum);
  }

  std::atomic<bool> staging_buffer_ready = false;
  auto copy_op = [=,&staging_buffer_ready]() {
    if (needs_conversion) {
      // Perform format conversion
      void* staging_data = staging_buffer->map(0, transfer_size, 0);
      const uint8_t* src_data = (const uint8_t*)tid._data;
      
      if (tid._dst_format == EBufferFormat::BGR8) {
        // BGR8 to BGRA8
        uint8_t* dst = (uint8_t*)staging_data;
        for (size_t i = 0; i < tid._w * tid._h * tid._d; i++) {
          dst[i * 4 + 0] = src_data[i * 3 + 0]; // B
          dst[i * 4 + 1] = src_data[i * 3 + 1]; // G
          dst[i * 4 + 2] = src_data[i * 3 + 2]; // R
          dst[i * 4 + 3] = 255;                 // A
        }
      } else if (tid._dst_format == EBufferFormat::RGB8) {
        // RGB8 to RGBA8
        uint8_t* dst = (uint8_t*)staging_data;
        for (size_t i = 0; i < tid._w * tid._h * tid._d; i++) {
          dst[i * 4 + 0] = src_data[i * 3 + 0]; // R
          dst[i * 4 + 1] = src_data[i * 3 + 1]; // G
          dst[i * 4 + 2] = src_data[i * 3 + 2]; // B
          dst[i * 4 + 3] = 255;                 // A
        }
      } else if (tid._dst_format == EBufferFormat::RGB32F) {
        // RGB32F to RGBA32F
        float* dst = (float*)staging_data;
        const float* src = (const float*)src_data;
        for (size_t i = 0; i < tid._w * tid._h * tid._d; i++) {
          dst[i * 4 + 0] = src[i * 3 + 0]; // R
          dst[i * 4 + 1] = src[i * 3 + 1]; // G
          dst[i * 4 + 2] = src[i * 3 + 2]; // B
          dst[i * 4 + 3] = 1.0f;           // A
        }
      }
      else{
        OrkAssert(false); // Unsupported conversion
      }
      staging_buffer->unmap();
    } else {
      // Direct copy
        if(is_brdf){
          logchan_txidata->log(" _contextVK<%p> hash_changed<%d>  DIRECT COPY size<%zu>", (void*) _contextVK, int(hash_changed), transfer_size);
        }
      staging_buffer->copyFromHost(tid._data, transfer_size);
      // Debug: verify copy for RGBA32F
      if (tid._dst_format == EBufferFormat::RGBA32F && !ptex->_debugName.empty()) {
        void* verify_data = staging_buffer->map(0, 64, 0); // Map first 64 bytes
        const float* staged = (const float*)verify_data;
        float sum = 0.0f;
        for (size_t i = 0; i < 16; i++) {
          sum += std::abs(staged[i]);
        }
        if(is_brdf)logchan_txidata->log("  Staging buffer verification sum: %f", sum);
        staging_buffer->unmap();
      }
    }
    staging_buffer_ready.store(true);
  };
  if(async){
    ork::opq::concurrentQueue()->enqueue(copy_op);
  }
  else{
    copy_op();
  }
 

  secondary_commandbuffer_ptr_t command_buffer;
  VkCommandBuffer vk_cmdbuf = VK_NULL_HANDLE;
  inflighttextrans_ptr_t transfer;

  /////////////////////////////////////////////////////////
  if(async){
  /////////////////////////////////////////////////////////

    /////////////////////////////////////
    // allocate a secondary command buffer
    /////////////////////////////////////

    _seccmdbufpool_xfer.atomicOp([&](sseccmdbufpool_ptr_t& pool) {
      command_buffer = pool->borrowItem();
    });

    /////////////////////////////////////
    // create a transfer object
    /////////////////////////////////////
    vkseccmdbufimpl_ptr_t cmdbuf_impl;

    transfer = std::make_shared<InFlightTextureTransfer>( _contextVK,     //
                                                          staging_buffer, //
                                                          command_buffer );
    vktex->_inflight_transfers.insert(transfer);
    cmdbuf_impl = command_buffer->_impl.getShared<VkSecondaryCommandBufferImpl>();
    vk_cmdbuf = cmdbuf_impl->_vkcmdbuf;

    vktex->_readyForSampling = false;

    /////////////////////////////////////
    // Set up completion callback
    /////////////////////////////////////

    auto tlsema         = std::make_shared<VulkanCompletionSemaphore>(this->_contextVK);
    cmdbuf_impl->_completionSemaphore = tlsema;
    tlsema->_onComplete = [=]() {
      vktex->_inflight_transfers.erase(transfer);
      poolForSize->returnItem(staging_buffer);
      _seccmdbufpool_xfer.atomicOp([&](sseccmdbufpool_ptr_t& pool) {
        pool->returnItem(command_buffer);
      });
      vktex->_readyForSampling = true;
      //printf("free stgbuf<%p>\n", (void*)staging_buffer.get());
    };

  }
  /////////////////////////////////////////////////////////
  else { // synchronous path
  /////////////////////////////////////////////////////////
    vk_cmdbuf = _contextVK->primary_cb()->_vkcmdbuf;
    vktex->_readyForSampling = true;
  }

  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////


  /////////////////////////////////////
  // if the image params have changed
  //   (e.g. size, format, usage)
  //   create a new VkImage and VkImageView
  // otherwise, reuse the existing VkImage and VkImageView
  /////////////////////////////////////

  if (hash_changed) {

    // Check if this is a cube texture
    bool is_cube = tid._initCubeTexture;
    int array_layers = tid._d;

    // For cube textures, we need 6 layers
    if (is_cube) {
      array_layers = 6;
    }

    auto VKICI   = makeVKICI(tid._w, tid._h, 1, actual_dst_format, 1); // depth is always 1 for 2D images
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
    vktex->_imgobj = std::make_shared<VulkanImageObject>(_contextVK, VKICI, debug_name);

    // Create the appropriate image view type
    std::shared_ptr<VkImageViewCreateInfo> IVCI;

    if (is_cube) {
      // Create cube image view
      IVCI = std::make_shared<VkImageViewCreateInfo>();
      initializeVkStruct(*IVCI, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
      IVCI->image = vktex->_imgobj->_vkimage;
      IVCI->viewType = VK_IMAGE_VIEW_TYPE_CUBE;
      IVCI->format = VkFormatConverter::convertBufferFormat(actual_dst_format);
      IVCI->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      IVCI->subresourceRange.baseMipLevel = 0;
      IVCI->subresourceRange.levelCount = 1;
      IVCI->subresourceRange.baseArrayLayer = 0;
      IVCI->subresourceRange.layerCount = 6;
    } else {
      // Keep existing 2D image view creation for non-cube textures
      IVCI = createImageViewInfo2D(
          vktex->_imgobj->_vkimage,
          VkFormatConverter::convertBufferFormat(actual_dst_format),
          VK_IMAGE_ASPECT_COLOR_BIT);
    }

    initializeVkStruct(vktex->_imgobj->_vkimageview);
    VkResult ok = vkCreateImageView(_contextVK->_vkdevice, IVCI.get(), nullptr, &vktex->_imgobj->_vkimageview);
    OrkAssert(VK_SUCCESS == ok);

    // Set debug name for image view
    if (!ptex->_debugName.empty()) {
      std::string view_name = ptex->_debugName + "_view";
      _contextVK->_setObjectDebugName(vktex->_imgobj->_vkimageview, VK_OBJECT_TYPE_IMAGE_VIEW, view_name.c_str());
    }

    vktex->_vkdescriptor_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vktex->_vkdescriptor_info.imageView   = vktex->_imgobj->_vkimageview;
    OrkAssert(vktex->_imgobj->_vkimageview != VK_NULL_HANDLE);

    ptex->_impl  = vktex;
    ptex->_texFormat = actual_dst_format; // Store the actual format
    ptex->_width     = tid._w;
    ptex->_height    = tid._h;
    ptex->_depth     = is_cube ? 6 : tid._d;  // Cube textures always have depth 6
    ptex->_num_mips  = 1;

  }

  /////////////////////////////////////

  vktex->_vksampler = _contextVK->_sampler_base;
  vktex->_vkdescriptor_info.sampler     = vktex->_vksampler->_vksampler;

  vktex->_imgview_hash.init();
  vktex->_imgview_hash.accumulateItem(vktex);
  vktex->_imgview_hash.accumulateItem(vktex->_imgobj);
  vktex->_imgview_hash.accumulateItem(vktex->_imgobj->_vkimageview);
  vktex->_imgview_hash.finish();

  /////////////////////////////////////
  // record transition to transfer destination (for copy)
  /////////////////////////////////////

  auto barrier = createImageBarrier(
      vktex->_imgobj->_vkimage,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VkAccessFlagBits(0),
      VK_ACCESS_TRANSFER_WRITE_BIT);

  // For cube textures, ensure all 6 layers are transitioned
  if (tid._initCubeTexture) {
    barrier->subresourceRange.layerCount = 6;
  }

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
      tid._initCubeTexture ? 6u : uint32_t(tid._d)};  // For cube textures, copy all 6 faces
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
      VK_PIPELINE_STAGE_TRANSFER_BIT,        // srcStageMask
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask
      0, // dependencyFlags
      0, nullptr, // memory barriers
      0, nullptr, // buffer barriers
      1, barrier.get()); // image barriers

  if(async){
    /////////////////////////////////////
    // enqueue recorded texture update cmdbuf
    /////////////////////////////////////

    _contextVK->endRecordCommandBuffer(transfer->_command_buffer);

    /////////////////////////////////////
    // wait for the staging buffer to be ready
    /////////////////////////////////////

    while( not staging_buffer_ready.load()) {
      std::this_thread::yield();
    }

    /////////////////////////////////////
    // enqueue the command buffer for execution
    /////////////////////////////////////

    _contextVK->enqueueDeferredOneShotCommand(transfer->_command_buffer);
  }
  else {
    // synchronous path
    vktex->_readyForSampling = true;
  }

  if(not tid._allow_async){
    // temp hack
    vktex->_readyForSampling = true;
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
