////////////////////////////////////////////////////////////////
// VkSwapchainMetal
//   Renders to offscreen VkImage, blits to CAMetalDrawable.
//   CVDisplayLink feeds TimePredictor each vsync.
//   Sleep just long enough that rendering finishes at the next scanout.
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#import <CoreVideo/CoreVideo.h>
#include <vulkan/vulkan_metal.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

static auto logchan_moltensc = logger()->configureChannel("VKSCMETAL", fvec3(0.5, 0.8, 1.0), true);

///////////////////////////////////////////////////////

static CVReturn _cvdl_callback(
    CVDisplayLinkRef /*link*/,
    const CVTimeStamp* /*now*/,
    const CVTimeStamp* outputTime,
    CVOptionFlags /*flagsIn*/,
    CVOptionFlags* /*flagsOut*/,
    void* ctx) {
  auto* sw = (VkSwapchainMetal*)ctx;
  sw->_onVsync((const void*)outputTime);
  return kCVReturnSuccess;
}

///////////////////////////////////////////////////////

VkSwapchainMetal::VkSwapchainMetal(vkcontext_rawptr_t ctxVK)
    : _contextVK(ctxVK) {
  _buildup();
}

VkSwapchainMetal::~VkSwapchainMetal() {
  _teardown();
}

///////////////////////////////////////////////////////

void VkSwapchainMetal::_buildup() {
  logchan_moltensc->log("_buildup: start");
  auto& vkdev = _contextVK->_vkdevice;

  ////////////////////////////////////////
  // vkExportMetalObjectsEXT — must be loaded dynamically
  ////////////////////////////////////////

  auto vkExportMetalObjects = (PFN_vkExportMetalObjectsEXT)vkGetDeviceProcAddr(vkdev, "vkExportMetalObjectsEXT");
  OrkAssert(vkExportMetalObjects != nullptr);

  ////////////////////////////////////////
  // Fences
  ////////////////////////////////////////

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    _frame_fences[i] = std::make_shared<VulkanFenceObject>(_contextVK);

  ////////////////////////////////////////
  // Window dimensions
  ////////////////////////////////////////

  auto ctx_glfw = _contextVK->_impl.getShared<VkPlatformObject>()->_ctxbase;
  auto glfw_win = ctx_glfw->_glfwWindow;
  int width, height;
  glfwGetFramebufferSize(glfw_win, &width, &height);
  _width  = width;
  _height = height;
  logchan_moltensc->log("_buildup: framebuffer %dx%d", width, height);

  ////////////////////////////////////////
  // CAMetalLayer + MTLDevice from VkDevice
  ////////////////////////////////////////

  @autoreleasepool {
    NSWindow*    nswin = glfwGetCocoaWindow(glfw_win);
    NSView*      view  = nswin.contentView;
    OrkAssertI([view.layer isKindOfClass:[CAMetalLayer class]],
               "VkSwapchainMetal: NSView layer is not a CAMetalLayer — "
               "ensure glfwCreateWindowSurface was called first");
    CAMetalLayer* layer = (CAMetalLayer*)view.layer;

    VkExportMetalDeviceInfoEXT devInfo{ 
      VK_STRUCTURE_TYPE_EXPORT_METAL_DEVICE_INFO_EXT,
    };
    VkExportMetalObjectsInfoEXT exportInfo{ 
      VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT, 
      &devInfo,
    };
    vkExportMetalObjects(vkdev, &exportInfo);
    id<MTLDevice> mtlDevice = devInfo.mtlDevice;
    OrkAssert(mtlDevice != nil);

    layer.device              = mtlDevice;
    layer.pixelFormat         = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly     = NO;
    layer.maximumDrawableCount = 2;
    layer.displaySyncEnabled  = YES;
    _metalLayer = (void*)layer; // non-owning

    id<MTLCommandQueue> q = [mtlDevice newCommandQueue];
    q.label = @"VkSwapchainMetal::present";
    _presentCommandQueue = (void*)q; // owned

    // Timeline event pre-seeded to MAX_FRAMES_IN_FLIGHT so early frames never block.
    id<MTLSharedEvent> ev = [mtlDevice newSharedEvent];
    ev.signaledValue = MAX_FRAMES_IN_FLIGHT;
    _timeline = (void*)ev; // owned
  }

  ////////////////////////////////////////
  // Offscreen VkImages
  //  Create vulkan images with dedicated memory allocaiton to export from metal.
  ////////////////////////////////////////

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkImage vkimg = VK_NULL_HANDLE;
    OrkVkAssert(vkCreateImage(vkdev, 
      pConst(VkImageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = VK_FORMAT_B8G8R8A8_UNORM,
        .extent      = { (u32)width, (u32)height, 1u },
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      }), 
      ORK_VK_ALLOC, &vkimg));
    VK_SET_DEBUG_NAME(vkdev, vkimg, FormatString("metal-swapchain-color-%zu", i).c_str());

    VkMemoryRequirements memreqs;
    vkGetImageMemoryRequirements(vkdev, vkimg, &memreqs);

    VkDeviceMemory vkmem = VK_NULL_HANDLE;
    OrkVkAssert(vkAllocateMemory(vkdev, 
      pConst(VkMemoryAllocateInfo{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        pNext(VkMemoryDedicatedAllocateInfo{
          VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
          .image = vkimg,
        }),
        .allocationSize  = memreqs.size,
        .memoryTypeIndex = _contextVK->_findMemoryType(memreqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
      }), 
      ORK_VK_ALLOC, &vkmem));
    VK_SET_DEBUG_NAME(vkdev, vkmem, FormatString("metal-swapchain-mem-%zu", i).c_str());

    OrkVkAssert(vkBindImageMemory(vkdev, vkimg, vkmem, 0));

    VkImageView vkimgview = VK_NULL_HANDLE;
    OrkVkAssert(vkCreateImageView(vkdev, 
      pConst(VkImageViewCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = vkimg,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_B8G8R8A8_UNORM,
        .subresourceRange = VkColorSubresourceRange{},
      }), 
      ORK_VK_ALLOC, &vkimgview));
    VK_SET_DEBUG_NAME(vkdev, vkimgview, FormatString("metal-swapchain-view-%zu", i).c_str());

    auto imgobj              = std::make_shared<VulkanImageObject>(_contextVK, vkimg, vkimgview, VK_FORMAT_B8G8R8A8_UNORM);
    imgobj->_vkdevicemem     = vkmem;
    imgobj->_delete_devicemem = true;
    _offscreen_imgobjs[i]   = imgobj;

    // Export MTLTexture — non-owning; MoltenVK retains it for the lifetime of the VkImage.
    @autoreleasepool {
      VkExportMetalTextureInfoEXT texInfo{
        VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT,
        .image = imgobj->_vkimage,
        .plane = VK_IMAGE_ASPECT_COLOR_BIT,
      };
      VkExportMetalObjectsInfoEXT texExport{ 
        VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT, 
        &texInfo 
      };
      vkExportMetalObjects(vkdev, &texExport);
      _offscreen_mtltextures[i] = (void*)texInfo.mtlTexture;
      OrkAssert(_offscreen_mtltextures[i] != nullptr);
    }

    logchan_moltensc->log("_buildup: offscreen image[%zu] VkImage=%p MTLTexture=%p",
                         i, (void*)imgobj->_vkimage, _offscreen_mtltextures[i]);
  }

  ////////////////////////////////////////
  // RTG setup: ensure main rtg exists, then attach depth buffer
  ////////////////////////////////////////

  auto main_rtg = _contextVK->_fbi->_ensureMainRtg();

  _contextVK->_fbi->_ensureDepth(main_rtg, width, height, VkRtbCreateOption{
    ._format       = VK_FORMAT_D32_SFLOAT,
    ._usage        = "depth"_crcu,
    ._with_texture = false,
  });

  ////////////////////////////////////////
  // CVDisplayLink — starts on main display, retargeted each beginFrame()
  ////////////////////////////////////////

  CVDisplayLinkRef link;
  CVDisplayLinkCreateWithCGDisplay(kCGDirectMainDisplay, &link);
  CVDisplayLinkSetOutputCallback(link, _cvdl_callback, this);
  _displayLink = (void*)link; // owned

  CVDisplayLinkStart(link);
  logchan_moltensc->log("_buildup: done");
}

///////////////////////////////////////////////////////

void VkSwapchainMetal::_teardown() {
  logchan_moltensc->log("_teardown: start");

  // Stop callbacks before touching any state.
  if (_displayLink) {
    CVDisplayLinkRef link = (CVDisplayLinkRef)_displayLink;
    CVDisplayLinkStop(link);
    CVDisplayLinkRelease(link);
    _displayLink = nullptr;
  }

  // Drain any in-flight Vulkan work.
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    if (_frame_fences[i])
      _frame_fences[i]->wait();

  @autoreleasepool {
    if (_presentCommandQueue) {
      [(id<MTLCommandQueue>)_presentCommandQueue release];
      _presentCommandQueue = nullptr;
    }
    if (_timeline) {
      [(id<MTLSharedEvent>)_timeline release];
      _timeline = nullptr;
    }
    // _offscreen_mtltextures and _metalLayer are non-owning.
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
      _offscreen_mtltextures[i] = nullptr;
    _metalLayer = nullptr;
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    _offscreen_imgobjs[i].reset();

  logchan_moltensc->log("_teardown: done");
}

///////////////////////////////////////////////////////
// Display retargeting
///////////////////////////////////////////////////////

void VkSwapchainMetal::_checkDisplay() {
  auto ctx_glfw = _contextVK->_impl.getShared<VkPlatformObject>()->_ctxbase;
  auto glfw_win = ctx_glfw->_glfwWindow;

  @autoreleasepool {
    NSWindow* nswin  = glfwGetCocoaWindow(glfw_win);
    NSScreen* screen = nswin ? nswin.screen : nil;
    if (!screen) return;

    NSNumber* num = screen.deviceDescription[@"NSScreenNumber"];
    if (!num) return;

    u32 displayID = (u32)num.unsignedIntValue;
    if (displayID == _current_display_id) return;

    CVDisplayLinkRef link = (CVDisplayLinkRef)_displayLink;
    CVDisplayLinkSetCurrentCGDisplay(link, (CGDirectDisplayID)displayID);
    _current_display_id = displayID;

    CVTime nom = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(link);
    double hz  = (nom.timeValue > 0) ? (double)nom.timeScale / (double)nom.timeValue : 0.0;
    logchan_moltensc->log("_checkDisplay: retargeted to display=0x%x %.2f Hz", displayID, hz);
  }
}

///////////////////////////////////////////////////////
// Per-frame interface
///////////////////////////////////////////////////////

void VkSwapchainMetal::beginFrame(vkcontext_rawptr_t ctxVK) {
  // Retarget CVDisplayLink to the window's current screen if it changed.
  _checkDisplay();

  // Wait until last submission completes. We do this in beginFrame rather than endFrame
  // so rendering can occur in parallel on GPU with what the CPU needs to still do after submission.
  [(id<MTLSharedEvent>)_timeline waitUntilSignaledValue:_current_frame timeoutMS:1000];

  // Rendering of this frame will start before the prior frame is fully scanned out.
  // As actual scanout takes an addtional refresh_ns from submit.
  // So onVsync for the prior submitted frame will NOT have been called at this point. 
  // The scanout of the prior frame is in progress while this frame starts to render.
  // The actual wait for the last vsync, and freeing of a drawable for this frame, occurs at [layer nextDrawable] in blitThenPresent.

  {
    // Sleep until _last_frame_ns before the target scanout so rendering finishes just in time.
    u64 target = _scan_out_predictor->predictNextTargetMarginSystemTick();
    _begin_wait.sleepUntilTick(target - _last_frame_delta_ns);
    _frame_start_tick = Timer::getSystemTick();
  }

  auto main_rtg  = ctxVK->_fbi->_ensureMainRtg();
  auto main_rtb  = main_rtg->buffer(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();
  main_rtbi->_is_surface = true;
  main_rtbi->_replaceImage(_offscreen_imgobjs[_sub_index]);
  _acquired = true;
}

///////////////////////////////////////////////////////

void VkSwapchainMetal::endFrame(vkcontext_rawptr_t ctxVK) {
  // Transition offscreen image COLOR_ATTACHMENT_OPTIMAL → TRANSFER_SRC_OPTIMAL for blit.
  auto imgobj = _offscreen_imgobjs[_sub_index];
  auto cb     = ctxVK->primary_cb();

  vkCmdImageBarrier(cb->_vkcmdbuf,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {
      { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = imgobj->_vkimage,
        .subresourceRange    = VkColorSubresourceRange{} },
    });

  imgobj->_currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  // Keep rtbi layout in sync.
  auto main_rtg  = ctxVK->_fbi->_ensureMainRtg();
  auto main_rtb  = main_rtg->buffer(0);
  auto main_rtbi = main_rtb->_impl.getShared<VklRtBufferImpl>();
  main_rtbi->setLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

///////////////////////////////////////////////////////

void VkSwapchainMetal::submit(vkcontext_rawptr_t ctxVK) {
  u32 sub = _sub_index;

  auto& fence = _frame_fences[sub];
  fence->reset();

  ctxVK->_gfxqueue->queueSubmit(
      pConst(VkSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO,
        pNext(VkTimelineSemaphoreSubmitInfo{
          VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
          .signalSemaphoreValueCount = (u32)ctxVK->_oneShotSignalValues.size(),
          .pSignalSemaphoreValues    = ctxVK->_oneShotSignalValues.data(),
        }),
        .commandBufferCount   = 1,
        .pCommandBuffers      = &ctxVK->_cmdbufcurpri_gfx->_vkcmdbuf,
        .signalSemaphoreCount = (u32)ctxVK->_oneShotSignalSemaphores.size(),
        .pSignalSemaphores    = ctxVK->_oneShotSignalSemaphores.data(),
      }),
      fence->_vkfence);

  // Block until GPU render is done, then blit immediately to minimize latency.
  fence->wait();

  _last_frame_delta_ns = Timer::getSystemTick() - _frame_start_tick;

  _blitThenPresent(sub);

  _incrementFrame();
  _acquired = false;
}

///////////////////////////////////////////////////////
// Blit + present (render thread, immediately after GPU fence)
///////////////////////////////////////////////////////

void VkSwapchainMetal::_blitThenPresent(u32 sub) {
  id<MTLTexture>      src   = (id<MTLTexture>)_offscreen_mtltextures[sub];
  id<MTLCommandQueue> queue = (id<MTLCommandQueue>)_presentCommandQueue;
  CAMetalLayer*       layer = (CAMetalLayer*)_metalLayer;
  if (!src || !queue || !layer) return;

  @autoreleasepool {
    id<CAMetalDrawable> drawable = [layer nextDrawable];

    if (!drawable) {
      logchan_moltensc->log("_blitThenPresent[%u]: nextDrawable nil — skipping", _current_frame);
      ((id<MTLSharedEvent>)_timeline).signaledValue = _current_frame + 1;
      return;
    }

    id<MTLCommandBuffer>      cmd = [queue commandBuffer];
    id<MTLBlitCommandEncoder> enc = [cmd blitCommandEncoder];

    [enc copyFromTexture: src
            sourceSlice: 0
            sourceLevel: 0
           sourceOrigin: MTLOriginMake(0, 0, 0)
             sourceSize: MTLSizeMake(_width, _height, 1)
              toTexture: drawable.texture
       destinationSlice: 0
       destinationLevel: 0
      destinationOrigin: MTLOriginMake(0, 0, 0)];
    [enc endEncoding];

    [cmd encodeSignalEvent:(id<MTLSharedEvent>)_timeline value:_current_frame + 1];

    // displaySyncEnabled=YES on the layer handles vsync alignment.
    [cmd presentDrawable:drawable];
    [cmd commit];
  }
}

///////////////////////////////////////////////////////
// CVDisplayLink callback
///////////////////////////////////////////////////////

void VkSwapchainMetal::_onVsync(const void* rawOutputTime) {
  const CVTimeStamp* outputTime = (const CVTimeStamp*)rawOutputTime;
  
  // outputTime->hostTime is the scanout time for this vsync (mach_absolute_time ticks).
  u64 scanout_ns = Timer::machAbsoluteToSystemTick(outputTime->hostTime);
  _scan_out_predictor->markPredictionTargetTick(scanout_ns);

  // videoRefreshPeriod/videoTimeScale is the exact rational refresh period (e.g. 1/90 s).
  // This used as a set margin offset in the predictor. The final target is scanout_ns + margin_ns as that is when
  // the frame will actually be physcially seen. However internally all rendering aims to be finished by scanout_ns.
  _scan_out_predictor->_margin_ns= (u64(outputTime->videoRefreshPeriod) * NS_PER_SEC) / u64(outputTime->videoTimeScale);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////

#endif // __APPLE__
