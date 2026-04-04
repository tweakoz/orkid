#include "headers/vulkan_ctx.h"
#include "vulkan_captureasync.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkOffscreen::VkOffscreen(vkcontext_rawptr_t ctxVK) {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    _frame_fences[i] = std::make_shared<VulkanFenceObject>(ctxVK);
}

///////////////////////////////////////////////////////////////////////////////

void VkOffscreen::endFrame(vkcontext_rawptr_t ctxVK) {
  // Transition to SHADER_READ_ONLY_OPTIMAL so the rendered image
  // can be read back as a texture or captured.
  auto main_rtg  = ctxVK->_fbi->_ensureMainRtg();
  auto main_rtbi = main_rtg->buffer(0)->_impl.getShared<VklRtBufferImpl>();
  main_rtbi->_transitionToTexture(ctxVK->primary_cb());
}

void VkOffscreen::submit(vkcontext_rawptr_t ctxVK) {
  // Offscreen rendering - handle completion semaphores
  VkTimelineSemaphoreSubmitInfo timelineInfo{
    .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
    .signalSemaphoreValueCount = (u32)ctxVK->_oneShotSignalValues.size(),
    .pSignalSemaphoreValues    = ctxVK->_oneShotSignalValues.data(),
  };
  VkSubmitInfo SI{
    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext                = &timelineInfo,
    .commandBufferCount   = 1,
    .pCommandBuffers      = &ctxVK->_cmdbufcurpri_gfx->_vkcmdbuf,
    .signalSemaphoreCount = (u32)ctxVK->_oneShotSignalSemaphores.size(),
    .pSignalSemaphores    = ctxVK->_oneShotSignalSemaphores.data(),
  };

  auto& fence = _frame_fences[_sub_index];
  fence->reset();
  ctxVK->_gfxqueue->queueSubmit(&SI, fence->_vkfence);
  fence->wait();
  _incrementFrame();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
