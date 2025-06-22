////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkSwapChain::VkSwapChain() {
  _semasOkToRender.resize(1);
  _waitOnPipelineStages.resize(1);
  _semasOkToPresent.resize(1);
}

///////////////////////////////////////////////////////////////////////////////

rtgroup_ptr_t VkSwapChain::currentRTG() {
  return _rtgs[_curSwapWriteImage];
}

///////////////////////////////////////////////////////////////////////////////

size_t VkSwapChain::subIndex() const {
  // Return the current frame index modulo MAX_FRAMES_IN_FLIGHT
  // This gives us the index of the current frame in the circular buffer
  return _currentFrame % MAX_FRAMES_IN_FLIGHT;
}

///////////////////////////////////////////////////////////////////////////////

VkResult VkSwapChain::acquireImage(vkcontext_rawptr_t ctxVK) {

  // Ensure we have a valid swapchain

  size_t sub_index = subIndex();

  // After fence wait, we need to acquire the next swapchain image
  // This must happen AFTER fence wait to ensure semaphores are ready

  ///////////////////////////////////////////////////
  // Get SwapChain Image
  ///////////////////////////////////////////////////

  bool ok_to_transition = false;

  while (not ok_to_transition) {

    // Ensure we're using the correct frame's semaphore
    // and that any previous signal has been consumed
    if (_curSwapWriteImage != 0xffffffff) {
      // Previous acquire might have failed mid-operation
      // Wait for device idle to ensure clean state
      vkDeviceWaitIdle(ctxVK->_vkdevice);
    }

    _curSwapWriteImage = 0xffffffff;
    VkResult status    = vkAcquireNextImageKHR(
        ctxVK->_vkdevice,
        _vkSwapChain,
        std::numeric_limits<uint64_t>::max(),
        _imageAcquiredSemaphores[sub_index]->_vksema, // Use current frame's semaphore
        VK_NULL_HANDLE,
        &_curSwapWriteImage);

    switch (status) {
      case VK_SUCCESS:
        ok_to_transition = true;
        break;
      case VK_SUBOPTIMAL_KHR:
      case VK_ERROR_OUT_OF_DATE_KHR: {
        vkDeviceWaitIdle(ctxVK->_vkdevice);
        return status;
        // printf("VK_ERROR_OUT_OF_DATE_KHR\n");
        //  OrkAssert(false);
        //   need to recreate swap chain
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  }

  OrkAssert(_curSwapWriteImage >= 0);
  OrkAssert(_curSwapWriteImage < _rtgs.size());
  // printf( "_curSwapWriteImage<%u>\n", _curSwapWriteImage );
  return VK_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::enqueueFrame(vkcontext_rawptr_t ctxVK) {

  size_t sub_index = subIndex();

  _semasOkToRender[0]      = _imageAcquiredSemaphores[sub_index]->_vksema;
  _waitOnPipelineStages[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo SI = {};
  initializeVkStruct(SI, VK_STRUCTURE_TYPE_SUBMIT_INFO);
  SI.waitSemaphoreCount   = _semasOkToRender.size();
  SI.pWaitSemaphores      = _semasOkToRender.data();
  SI.pWaitDstStageMask    = _waitOnPipelineStages.data();
  SI.commandBufferCount   = 1;
  SI.pCommandBuffers      = &ctxVK->primary_cb()->_vkcmdbuf;
  SI.signalSemaphoreCount = 1;
  SI.pSignalSemaphores    = &(_renderCompleteSemaphores[sub_index]->_vksema);

  // Submit with this frame's fence
 if (sub_index < _frameFences.size()) {
   auto& fence = _frameFences[sub_index];
   fence->reset();
   vkQueueSubmit(ctxVK->_vkqueue_graphics, 1, &SI, fence->_vkfence);
 } else {
   vkQueueSubmit(ctxVK->_vkqueue_graphics, 1, &SI, VK_NULL_HANDLE);
 }
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::enqueuePresentFrame(vkcontext_rawptr_t ctxVK) {

  size_t sub_index = subIndex();

  _semasOkToPresent[0] = (_renderCompleteSemaphores[sub_index]->_vksema);

  VkPresentInfoKHR PRESI{};
  initializeVkStruct(PRESI, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
  PRESI.waitSemaphoreCount = _semasOkToPresent.size();
  PRESI.pWaitSemaphores    = _semasOkToPresent.data();
  PRESI.swapchainCount     = 1;
  PRESI.pSwapchains        = &_vkSwapChain;
  PRESI.pImageIndices      = &_curSwapWriteImage;

  VkResult status = vkQueuePresentKHR(ctxVK->_vkqueue_graphics, &PRESI);
  // printf("vkQueuePresentKHR returned status: %d (0x%x)\n", status, status);

  switch (status) {
    case VK_SUCCESS:
      break;
    case VK_SUBOPTIMAL_KHR: {
      printf("VK_SUBOPTIMAL_KHR: Swap chain is suboptimal\n");
      // Swap chain is still usable, but may not be optimal
      // Consider recreating it on next frame
      break;
    }
    case VK_ERROR_OUT_OF_DATE_KHR: {
      printf("VK_ERROR_OUT_OF_DATE_KHR: Swap chain needs recreation\n");
      // Need to recreate swap chain immediately
      vkDeviceWaitIdle(ctxVK->_vkdevice);
      ctxVK->_fbi->_initSwapChain();
      break;
    }
    case VK_ERROR_DEVICE_LOST: {
      printf("VK_ERROR_DEVICE_LOST: Device has been lost!\n");
      // This is a fatal error - the device is no longer usable
      // All Vulkan objects are now invalid
      // The application needs to recreate everything from scratch
      OrkAssert(false);
      break;
    }
    case VK_ERROR_SURFACE_LOST_KHR: {
      printf("VK_ERROR_SURFACE_LOST_KHR: Surface was lost\n");
      // Surface is no longer available, need to recreate
      // This is fatal for now
      OrkAssert(false);
      break;
    }
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: {
      printf("VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT\n");
      // Can try to reacquire exclusive full-screen access
      //  need to recreate swap chain
      break;
    }
    default:
      OrkAssert(false);
      break;
  } // switch (status)
}

///////////////////////////////////////////////////////////////////////////////

void VkSwapChain::waitPresentFrame(vkcontext_rawptr_t ctxVK) {
  size_t sub_index = subIndex();
  // Wait for the current frame's fence to ensure rendering is complete
  auto& fence = _frameFences[sub_index];

  float pre_time = ctxVK->_present_timer.SecsSinceStart();
  float time_since_last_present = pre_time - ctxVK->_prev_time;
  ctxVK->_prev_time = pre_time;

  if (fence) {
    //printf("  VkSwapChain<%p> Waiting for fence from frame %zu...\n", (void*) this, _currentFrame);
    fence->wait();
    fence->reset();
  }


  ctxVK->_total_frame_time += time_since_last_present;


  float pos_time = ctxVK->_present_timer.SecsSinceStart();
  float delta_time = pos_time - pre_time;
  ctxVK->_present_wait_time += delta_time;
  ctxVK->_total_wait_time = ctxVK->_present_timer.SecsSinceStart();

  if((_currentFrame&0x1ff)==0) {
    float average_frame_time = ctxVK->_total_frame_time / (_currentFrame + 1);
    float average_wait_time = ctxVK->_present_wait_time / (_currentFrame + 1);
    printf("waittime<%g> total_time<%g>. average_wait_time<%g s> average_frame_time<%g>\n",
           ctxVK->_present_wait_time, ctxVK->_total_wait_time, average_wait_time, average_frame_time);

    ctxVK->_total_frame_time = 0.0f;
    ctxVK->_total_wait_time = 0.0f;
  }
  _currentFrame++;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////
