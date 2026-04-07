#if defined(__APPLE__)
#ifndef GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include "headers/vulkan_ctx.h"
#include <ork/kernel/timer.h>
#include <ork/util/logger.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#import <CoreVideo/CoreVideo.h>
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#include <mach/mach_time.h>
#include <memory>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
////////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_displaylink = logger()->configureChannel("VKDISPLAYLINK", fvec3(0.7, 0.9, 0.7), true);

// CVDisplayLink fires once per vblank on its own thread. outputTime->hostTime
// is the upcoming scanout in Mach absolute time — converted to epoch ms and
// fed into TimePredictor, giving vblank-accurate scanout prediction.

struct CVDisplayLinkWrapper {
    CVDisplayLinkRef          cvLink        = nullptr;
    time_predictor_ptr_t      estimator;
    double                    epochOffsetMS = 0.0;
    double                    machToNsScale = 1.0;
};

static CVReturn displayLinkCallback(CVDisplayLinkRef,
                                    const CVTimeStamp*,
                                    const CVTimeStamp* outputTime,
                                    CVOptionFlags,
                                    CVOptionFlags*,
                                    void* ctx) {
    auto* w = (CVDisplayLinkWrapper*)ctx;
    double hostTime_ms      = double(outputTime->hostTime) * w->machToNsScale * 1e-6;
    double scanout_epoch_ms = hostTime_ms + w->epochOffsetMS;
    w->estimator->markPredictionTarget(scanout_epoch_ms);

    static int tick_count = 0;
    if (tick_count++ < 5) {
        logchan_displaylink->log("tick %d scanout_epoch_ms=%.3f avgInterval=%.3fms",
               tick_count, scanout_epoch_ms, w->estimator->avgIntervalMS());
    }
    return kCVReturnSuccess;
}

void VkContext::_startDisplayLink() {
    mach_timebase_info_data_t tbinfo;
    mach_timebase_info(&tbinfo);
    double machToNs = double(tbinfo.numer) / double(tbinfo.denom);

    double mach_now_ms = double(mach_absolute_time()) * machToNs * 1e-6;
    _displayLinkEpochOffsetMS = Timer::getEpochMS() - mach_now_ms;

    auto* w = new CVDisplayLinkWrapper{
        nullptr,
        _render_timing_estimator,
        _displayLinkEpochOffsetMS,
        machToNs
    };

    // Get the CGDirectDisplayID for whichever display the window is on,
    // so CVDisplayLink fires at that display's actual refresh rate (e.g. 90Hz)
    // rather than the lowest-common-denominator of all active displays.
    CGDirectDisplayID displayID = CGMainDisplayID();
    auto glfw_container = (CtxGLFW*)mCtxBase;
    if (glfw_container && glfw_container->_glfwWindow) {
        NSWindow* nswin = glfwGetCocoaWindow(glfw_container->_glfwWindow);
        if (nswin) {
            NSScreen* screen = nswin.screen;
            if (screen) {
                NSDictionary* desc = screen.deviceDescription;
                NSNumber* screenID = desc[@"NSScreenNumber"];
                if (screenID) {
                    displayID = (CGDirectDisplayID)screenID.unsignedIntValue;
                }
            }
        }
    }

    logchan_displaylink->log("starting on display=0x%x epochOffset=%.3f estimator=%p",
           displayID, _displayLinkEpochOffsetMS, (void*)w->estimator.get());

    CVDisplayLinkCreateWithCGDisplay(displayID, &w->cvLink);
    CVDisplayLinkSetOutputCallback(w->cvLink, displayLinkCallback, w);
    CVDisplayLinkStart(w->cvLink);

    CVTime period = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(w->cvLink);
    double hz = (period.flags & kCVTimeIsIndefinite) ? 0.0
              : double(period.timeScale) / double(period.timeValue);
    logchan_displaylink->log("started cvLink=%p display=0x%x refresh=%.2fHz", (void*)w->cvLink, displayID, hz);
    _displayLink = (void*)w;
}

void VkContext::_stopDisplayLink() {
    if (_displayLink) {
        auto* w = (CVDisplayLinkWrapper*)_displayLink;
        _displayLink = nullptr;
        CVDisplayLinkStop(w->cvLink);
        CVDisplayLinkRelease(w->cvLink);
        delete w;
    }
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
////////////////////////////////////////////////////////////////////////////////
#endif
