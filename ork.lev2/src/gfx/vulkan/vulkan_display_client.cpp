#include "headers/vulkan_ctx.h"
#include <ork/util/logger.h>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
////////////////////////////////////////////////////////////////////////////////

static auto logchan_presentout = logger()->configureChannel("DisplayClientOut", fvec3(0.2, 0.8, 0.5), true);

////////////////////////////////////////////////////////////////////////////////

VkDisplayClientOutput::VkDisplayClientOutput(vkcontext_rawptr_t vk_ctx, int width, int height, vkdisplayclient_ptr_t client)
    : _gfx_ctx(vk_ctx)
    , _display_client(client) {

  _width  = width;
  _height = height;

  OrkAssertI(_display_client->_shared->state != OrkDisplayClientState::Uninitialized,
    "VkDisplayClientOutput: Trying to create VkDisplayClientOutput with uninitialized DisplayClient!");

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    auto imgobj = std::make_shared<VulkanImageObject>(vk_ctx, _display_client->_local.images[i], _display_client->_local.views[i], VK_FORMAT_B8G8R8A8_UNORM);
    imgobj->_delete_image     = false;
    imgobj->_delete_imageview = false;
    _imgobjs[i] = imgobj;
  }

  auto main_rtg = vk_ctx->_fbi->_ensureMainRtg();
  vk_ctx->_fbi->_createRtGroupImpl(main_rtg.get());

  // Incremement to 1 at start as a timeline cannot be signalled/waited at 0.
  _incrementFrame();


  logchan_presentout->log("created: size=%dx%d client=%p", width, height, client.get());
}

////////////////////////////////////////////////////////////////////////////////

VkDisplayClientOutput::~VkDisplayClientOutput() {
  vkDeviceWaitIdle(_gfx_ctx->_vkdevice);
  logchan_presentout->log("destroyed");
}

///////////////////////////////////////////////////////

void VkDisplayClientOutput::beginFrame(vkcontext_rawptr_t vk_ctx) {
  OrkAssertI(!_acquired, "beginFrame called twice without a submit in between");

  if(0) logchan_presentout->log("beginFrame: frame=%lu acquiring", _current_frame);
  _acquired_index = _display_client->acquireImage(vk_ctx->_vkdevice);

  auto main_rtg  = vk_ctx->_fbi->_ensureMainRtg();
  auto main_rtb  = main_rtg->buffer(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();
  main_rtbi->_is_surface = false;
  main_rtbi->_replaceImage(_imgobjs[_acquired_index]);

  if(0) logchan_presentout->log("beginFrame: frame=%lu idx=%u acquired", _current_frame, _acquired_index);
  _acquired = true;
}

///////////////////////////////////////////////////////

void VkDisplayClientOutput::endFrame(vkcontext_rawptr_t _ctx) {
  if(0) logchan_presentout->log("endFrame: idx=%u -> texture", _acquired_index);
  auto main_rtg  = _ctx->_fbi->_ensureMainRtg();
  auto main_rtbi = main_rtg->buffer(0)->_impl.getShared<VklRtBufferImpl>();
  main_rtbi->_transitionToTexture(_ctx->primary_cb());
}

///////////////////////////////////////////////////////

void VkDisplayClientOutput::submit(vkcontext_rawptr_t _ctx) {

  auto client_timeline = _display_client->_local.client_timeline;
  auto allSemas  = _ctx->_oneShotSignalSemaphores;
  auto allValues = _ctx->_oneShotSignalValues;
  allSemas.push_back(client_timeline);
  allValues.push_back(_current_frame);

  if(0) logchan_presentout->log("submit: idx=%u frame=%lu signaling client_tv->%lu", _acquired_index, _current_frame, _current_frame + 1);

  OrkVkAssert(vkQueueSubmit(_ctx->_vkqueue_graphics, 1,
    pConst(VkSubmitInfo{
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      pNext(VkTimelineSemaphoreSubmitInfo{
        VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .signalSemaphoreValueCount = (u32)allValues.size(),
        .pSignalSemaphoreValues    = allValues.data(),
      }),
      .commandBufferCount   = 1,
      .pCommandBuffers      = &_ctx->_cmdbufcurpri_gfx->_vkcmdbuf,
      .signalSemaphoreCount = (u32)allSemas.size(),
      .pSignalSemaphores    = allSemas.data(),
    }),
    VK_NULL_HANDLE));

  OrkVkAssert(vkWaitSemaphores(_ctx->_vkdevice,
    pConst(VkSemaphoreWaitInfo{
      VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
      .semaphoreCount = 1,
      .pSemaphores    = &client_timeline,
      .pValues        = &_current_frame,
    }),
    UINT64_MAX));

  if(0) logchan_presentout->log("submit: idx=%u done", _acquired_index);

  _display_client->releaseImage(_ctx->_vkdevice, _acquired_index);

  _incrementFrame();

  _acquired = false;
}

///////////////////////////////////////////////////////

u8 VkDisplayClient::acquireImage(VkDevice device) {
  if(0) logchan_presentout->log("acquireImage: waiting server_wait>=%lu", _server_wait_timeline_value);

  // Wait for server
  OrkVkAssert(vkWaitSemaphores(device,
    pConst(VkSemaphoreWaitInfo{
      VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
      .semaphoreCount = 1,
      .pSemaphores    = &_local.server_timeline,
      .pValues        = &_server_wait_timeline_value,
    }),
    UINT64_MAX));

  auto idxs = _shared->indices.load(std::memory_order_acquire);
  if(0) logchan_presentout->log("acquireImage: client_id=%d server_id=%d", idxs.client, idxs.server);
  return idxs.client;
}

void VkDisplayClient::releaseImage(VkDevice device, u8 idx) {
  int frame_wait_count = _shared->frame_wait_count.load(std::memory_order_relaxed);

  // If frame did not render in expected wait time then update to current server timeline value
  u64 server_tv;
  OrkVkAssert(vkGetSemaphoreCounterValue(device, _local.server_timeline, &server_tv));
  _server_wait_timeline_value = std::max(server_tv, _server_wait_timeline_value + frame_wait_count);

  if(0) logchan_presentout->log("releaseImage: idx=%u next_server_wait=%lu", idx, _server_wait_timeline_value);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
