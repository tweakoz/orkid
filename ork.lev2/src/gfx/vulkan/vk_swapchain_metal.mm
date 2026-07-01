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
#include <ork/lev2/vr/vr.h>
#include <mutex>
#include <cstdint>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

static auto logchan_moltensc = logger()->configureChannel("VKSCMETAL", fvec3(0.5, 0.8, 1.0), true);

///////////////////////////////////////////////////////
// M2PL motion-to-photon instrumentation. Per frame we capture the render-start tick (≈ pose
// latch) and the scanout TARGET the predictor returns; in the drawable's presentedTime
// handler we read the ACTUAL photon time and compare. drawable.presentedTime is in the
// CACurrentMediaTime (mach) domain, so ×1e9 == Timer::getSystemTick() ns — same clock.
//   true_m2p   = actual_photon − render_start   (ground-truth pipeline depth)
//   pred_lead  = scanout_target − render_start  (what the predictor leads by)
//   target_err = scanout_target − actual_photon (honest if ≈0; + over-target, − under-target)
// One swapchain + render thread sets the per-frame statics in begin/present order; the
// async present handler accumulates under a mutex and logs every 120 frames.
static uint64_t s_dbg_latch   = 0;
static uint64_t s_dbg_spred   = 0;
static uint64_t s_dbg_refresh = 11111111;   // one refresh period (ns); 90Hz default

// M2PL Stage 5 — RACE-THE-BEAM. ORKEXP_PRESENT_LEAD_MS (default 0 = unchanged): finish rendering
// this many ms BEFORE the target vsync so the present catches THAT vsync instead of the next one,
// cutting ~1 refresh (~11ms) of present→scanout latency. When >0 the predictor margin is zeroed so
// _predictHmdPose targets the vsync we race into (keeps target_err≈0). Too small vs render-jitter
// ⇒ missed vsync ⇒ that frame falls back to next-vsync (true_m2p spike); dial up until stable.
static uint64_t s_present_lead_ns  = 0;
static bool     s_present_lead_read = false;

// M2PL — low-latency present. vsync-OFF measurement proved the ~25ms (3-frame) enqueued-present
// FLOOR is the macOS deep vsync present QUEUE (vsync-off → ~0.66 frame). presentsWithTransaction
// presents as soon as the work is SCHEDULED (commit → waitUntilScheduled → [drawable present])
// instead of queuing deep, while displaySyncEnabled still aligns scanout to vsync (NO tearing).
// ORKEXP_PRESENT_TRANSACTION=1 to enable. Default 0 = today's deep-queued presentDrawable.
static bool s_present_transaction = false;

// M2PL — present-at-time (anti-bloat). Triple-buffering (maximumDrawableCount=3) cures the
// nextDrawable starvation, but in a GPU-heavy scene a render spike fills the 3-deep queue and at
// steady 120fps (producer==consumer) it NEVER drains → enqueued_present climbs to ~3 frames and
// stays (classic buffer bloat; target_err drifts negative while pred_lead stays pinned). Fix: pin
// each frame to its predicted scanout slot via [cmd presentDrawable:atTime:] so the queue can't
// bloat past one frame's lead. The target is s_dbg_spred (this frame's predicted photon tick, mach
// ns) → seconds (CACurrentMediaTime domain). ORKEXP_PRESENT_AT_TIME=1 to enable. Default 0.
static bool s_present_at_time      = false;
static bool s_present_at_time_read = false;

static std::mutex s_dbg_mtx;
static uint64_t s_dbg_n = 0;
static int64_t  s_dbg_m2p_sum = 0, s_dbg_m2p_min = 0, s_dbg_m2p_max = 0, s_dbg_lead_sum = 0, s_dbg_err_sum = 0;
// M2PL ATW-ceiling: split true_m2p into the RENDER portion (frame_start→fence, where onGpuPresent
// would latch = what ATW RECOVERS) vs the ENQUEUED-PRESENT portion (fence→photon: drawable queue +
// vsync = the irreducible ATW FLOOR). render = _last_frame_delta_ns; present = m2p − render.
static uint64_t s_dbg_render = 0;          // per-frame snapshot (frame_start→fence)
static int64_t  s_dbg_render_sum = 0, s_dbg_present_sum = 0;

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
    // Triple-buffer. With only 2 drawables, nextDrawable() in _blitThenPresent BLOCKS (up to
    // ~1s) whenever both drawables are still in-flight — which happens the instant the compositor
    // holds them longer: direct-scanout (ORKEXP_TRUE_FULLSCREEN) or vsync-aligned pacing. That
    // starvation IS the 2-FPS stall. 3 is the CAMetalLayer maximum and the standard low-latency-
    // game choice: always a free drawable to render into while two are in flight, and (because we
    // still present the freshest frame each loop) it adds NO latency here. ORKEXP_MAX_DRAWABLES
    // overrides (legal values 2 or 3) for A/B.
    NSUInteger maxdraw = 3;
    if (const char* v = getenv("ORKEXP_MAX_DRAWABLES")) { int n = atoi(v); if (n == 2 || n == 3) maxdraw = (NSUInteger)n; }
    layer.maximumDrawableCount = maxdraw;
    logchan_moltensc->log("maximumDrawableCount = %lu (ORKEXP_MAX_DRAWABLES)", (unsigned long)maxdraw);
    layer.displaySyncEnabled  = YES;
    // M2PL low-latency present (see s_present_transaction). Set the layer flag here; the present
    // path in _blitThenPresent branches on it. displaySync stays on (vsync-aligned scanout, no tear).
    if (const char* v = getenv("ORKEXP_PRESENT_TRANSACTION")) s_present_transaction = (atoi(v) != 0);
    layer.presentsWithTransaction = s_present_transaction ? YES : NO;
    logchan_moltensc->log("M2PL presentsWithTransaction = %d (ORKEXP_PRESENT_TRANSACTION)",
                          (int)s_present_transaction);
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

  auto vrdev = ork::lev2::orkidvr::device();
  if (vrdev and (vrdev->_scan_out_predictor==nullptr)) {
    vrdev->_scan_out_predictor = _scan_out_predictor;
  }

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
    if (!s_present_lead_read) {
      s_present_lead_read = true;
      const char* v = getenv("ORKEXP_PRESENT_LEAD_MS");
      s_present_lead_ns = v ? (u64)(atof(v) * 1.0e6) : 0;
      logchan_moltensc->log("M2PL race-the-beam: present_lead = %.2f ms (ORKEXP_PRESENT_LEAD_MS)",
                            double(s_present_lead_ns) * 1e-6);
    }
    u64 target = _scan_out_predictor->predictNextTargetMarginSystemTick();
    // Race-the-beam: wake present_lead_ns EARLIER so render finishes before the target vsync and
    // the present catches it (not the next). Guard underflow → sleepUntilTick(past) renders ASAP.
    u64 wake = (target > _last_frame_delta_ns) ? (target - _last_frame_delta_ns) : 0;
    wake = (wake > s_present_lead_ns) ? (wake - s_present_lead_ns) : 0;
    _begin_wait.sleepUntilTick(wake);
    _frame_start_tick = Timer::getSystemTick();
    // M2PL: snapshot the latch tick + the scanout TARGET for this frame (the device's
    // _predictHmdPose queries the same predictor a touch later during render-assemble).
    s_dbg_latch   = _frame_start_tick;
    s_dbg_spred   = _scan_out_predictor->predictNextTargetSystemTick();
    s_dbg_refresh = _scan_out_predictor->_margin_ns;
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

    // M2PL instrumentation — true motion-to-photon for THIS frame via its actual scanout time.
    {
      uint64_t latch = s_dbg_latch, spred = s_dbg_spred, refresh = s_dbg_refresh;
      uint64_t render_ns = _last_frame_delta_ns;   // frame_start→fence (the ATW latch point)
      [drawable addPresentedHandler:^(id<MTLDrawable> d){
        double pt = d.presentedTime;                  // seconds, CACurrentMediaTime (mach) domain
        if (pt <= 0.0 || latch == 0) return;          // 0 ⇒ dropped/never shown
        uint64_t actual = (uint64_t)(pt * 1.0e9);     // ×1e9 == Timer::getSystemTick() ns
        if (actual <= latch) return;
        int64_t m2p  = (int64_t)actual - (int64_t)latch;   // render-start → photon (ground truth)
        int64_t lead = (int64_t)spred  - (int64_t)latch;   // scanout target − render-start
        int64_t err  = (int64_t)spred  - (int64_t)actual;  // target − actual photon (≈0 = honest)
        int64_t render  = (int64_t)render_ns;          // frame_start→fence (ATW recovers this)
        int64_t present = m2p - render;                 // fence→photon (enqueued present = ATW floor)
        std::lock_guard<std::mutex> lk(s_dbg_mtx);
        if (s_dbg_n == 0) { s_dbg_m2p_min = m2p; s_dbg_m2p_max = m2p; }
        else { if (m2p < s_dbg_m2p_min) s_dbg_m2p_min = m2p; if (m2p > s_dbg_m2p_max) s_dbg_m2p_max = m2p; }
        s_dbg_m2p_sum += m2p; s_dbg_lead_sum += lead; s_dbg_err_sum += err;
        s_dbg_render_sum += render; s_dbg_present_sum += present; s_dbg_n++;
        if (s_dbg_n >= 120) {
          double inv = 1.0 / double(s_dbg_n);
          double rf  = (refresh > 0) ? double(refresh) : 11111111.0;
          logchan_moltensc->log(
            "M2PL photon: true_m2p avg=%.1f min=%.1f max=%.1f ms (~%.2f frames) | "
            "pred_lead avg=%.1f ms (~%.2f frames) | target_err avg=%+.1f ms",
            s_dbg_m2p_sum*inv*1e-6, double(s_dbg_m2p_min)*1e-6, double(s_dbg_m2p_max)*1e-6,
            (s_dbg_m2p_sum*inv)/rf,
            s_dbg_lead_sum*inv*1e-6, (s_dbg_lead_sum*inv)/rf,
            s_dbg_err_sum*inv*1e-6);
          // ATW ceiling: render (recoverable by latching at onGpuPresent) vs enqueued-present (floor)
          logchan_moltensc->log(
            "M2PL split: render avg=%.1f ms (~%.2f f, ATW recovers) | enqueued_present avg=%.1f ms (~%.2f f, ATW FLOOR)",
            s_dbg_render_sum*inv*1e-6, (s_dbg_render_sum*inv)/rf,
            s_dbg_present_sum*inv*1e-6, (s_dbg_present_sum*inv)/rf);
          s_dbg_n=0; s_dbg_m2p_sum=0; s_dbg_lead_sum=0; s_dbg_err_sum=0;
          s_dbg_render_sum=0; s_dbg_present_sum=0;
        }
      }];
    }

    if (!s_present_at_time_read) {
      s_present_at_time_read = true;
      if (const char* v = getenv("ORKEXP_PRESENT_AT_TIME")) s_present_at_time = (atoi(v) != 0);
      logchan_moltensc->log("M2PL present-at-time = %d (ORKEXP_PRESENT_AT_TIME)", (int)s_present_at_time);
    }

    // displaySyncEnabled=YES on the layer handles vsync alignment (no tearing) every branch.
    if (s_present_at_time && s_dbg_spred != 0) {
      // Anti-bloat: pin this frame to its predicted scanout slot. Metal won't show it before `when`,
      // so frames can't queue ahead of their lead and the 3-deep pool can't bloat to +2 frames.
      // s_dbg_spred is the predicted photon tick (mach ns) captured for THIS frame in beginFrame.
      double when = double(s_dbg_spred) * 1.0e-9; // → seconds, CACurrentMediaTime (mach) domain
      [cmd presentDrawable:drawable atTime:when];
      [cmd commit];
    } else if (s_present_transaction) {
      // M2PL low-latency: present as soon as the work is SCHEDULED, not deep-queued.
      [cmd commit];
      [cmd waitUntilScheduled];
      [drawable present];
    } else {
      [cmd presentDrawable:drawable];
      [cmd commit];
    }
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
  // This is the predictor margin: the photon for a frame lands one refresh AFTER the vsync its
  // render finishes into (present catches the NEXT vsync). _predictHmdPose targets scanout+margin.
  // M2PL Stage 5: when racing the beam (present_lead>0) the present catches the SAME vsync the
  // render finishes into, so the photon == that vsync ⇒ margin 0 (predict to the nearer vsync).
  u64 refresh_ns = (u64(outputTime->videoRefreshPeriod) * NS_PER_SEC) / u64(outputTime->videoTimeScale);
  _scan_out_predictor->_margin_ns = (s_present_lead_ns > 0) ? 0 : refresh_ns;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////

#endif // __APPLE__
