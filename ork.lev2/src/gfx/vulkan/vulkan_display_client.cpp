#include "headers/vulkan_ctx.h"
#include <ork/util/logger.h>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
////////////////////////////////////////////////////////////////////////////////

static auto logchan_presentout = logger()->configureChannel("DisplayClientOut", fvec3(0.2, 0.8, 0.5), false);

////////////////////////////////////////////////////////////////////////////////

VkDisplayClientOutput::VkDisplayClientOutput(vkcontext_rawptr_t vk_ctx, int width, int height, vkdisplayclient_ptr_t client)
    : _gfx_ctx(vk_ctx)
    , _display_client(client) {

  _width  = width;
  _height = height;

  OrkAssertI(_display_client->_shared->state != OrkDisplayClientState::Uninitialized,
    "VkDisplayClientOutput: Trying to create VkDisplayClientOutput with uninitialized DisplayClient!");

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    auto imgobj = std::make_shared<VulkanImageObject>(vk_ctx, _display_client->_local.images[i], _display_client->_local.views[i], VK_FORMAT_R8G8B8A8_UNORM);
    imgobj->_delete_image     = false;
    imgobj->_delete_imageview = false;
    _imgobjs[i] = imgobj;
  }

  // Set up main RTG vulkan impl so beginFrame() can access VklRtBufferImpl.
  // Same pattern as swapchain/DRM _buildup — the color buffer's placeholder
  // image will be replaced each frame via _replaceImage().
  auto main_rtg = vk_ctx->_fbi->_ensureMainRtg();
  vk_ctx->_fbi->_createRtGroupImpl(main_rtg.get());

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

  logchan_presentout->log("beginFrame: frame=%lu acquiring", _current_frame);
  _acquired_index = _display_client->acquireImage(vk_ctx->_vkdevice);

  auto main_rtg  = vk_ctx->_fbi->_ensureMainRtg();
  auto main_rtb  = main_rtg->buffer(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();
  main_rtbi->_is_surface = false;
  main_rtbi->_replaceImage(_imgobjs[_acquired_index]);

  logchan_presentout->log("beginFrame: frame=%lu slot=%u acquired", _current_frame, _acquired_index);
  _acquired = true;
}

///////////////////////////////////////////////////////

void VkDisplayClientOutput::endFrame(vkcontext_rawptr_t _ctx) {
  logchan_presentout->log("endFrame: slot=%u -> texture", _acquired_index);
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
  allValues.push_back(_current_frame + 1);

  logchan_presentout->log("submit: slot=%u frame=%lu signaling client_tv->%lu",
    _acquired_index, _current_frame, _current_frame + 1);

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
      .pValues        = pConst<u64>(_current_frame + 1),
    }),
    UINT64_MAX));

  logchan_presentout->log("submit: slot=%u done", _acquired_index);

  _display_client->releaseImage(_acquired_index);

  _incrementFrame();

  _acquired = false;
}

///////////////////////////////////////////////////////

u32 VkDisplayClient::acquireImage(VkDevice device) {
  logchan_presentout->log("acquireImage: waiting server_wait>=%lu", _server_wait_timeline_value);

  // Wait for server
  OrkVkAssert(vkWaitSemaphores(device,
    pConst(VkSemaphoreWaitInfo{
      VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
      .semaphoreCount = 1,
      .pSemaphores    = &_local.server_timeline,
      .pValues        = &_server_wait_timeline_value,
    }),
    UINT64_MAX));

  // Client will render into (client_timeline_value + 1) % MAX_FRAMES_IN_FLIGHT
  // While server displays client_timeline_value % MAX_FRAMES_IN_FLIGHT
  // Alternating frames based on timeline value.
  u64 client_timeline_value = _shared->client_timeline_value.load(std::memory_order_acquire);
  u32 id = (u32)(client_timeline_value + 1) % MAX_FRAMES_IN_FLIGHT;

  logchan_presentout->log("acquireImage: slot=%u client_tv=%lu", id, client_timeline_value);
  return id;
}

void VkDisplayClient::releaseImage(u32 idx) {
  // Signal server to flip to the new client frame.
  u64 prev = _shared->client_timeline_value.fetch_add(1, std::memory_order_release);

  // Next server cycle means it has released the current frame.
  // server_timeline_value + 1 to wait on the next server cycle.
  u64 server_timeline_value = _shared->server_timeline_value.load(std::memory_order_acquire);
  _server_wait_timeline_value = server_timeline_value + 1;

  logchan_presentout->log("releaseImage: slot=%u client_tv=%lu->%lu next_server_wait=%lu",
    idx, prev, prev + 1, _server_wait_timeline_value);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
