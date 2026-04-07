#if defined(__APPLE__)
#include "headers/vulkan_ctx.h"
#include <ork/kernel/timer.h>
#include <ork/util/logger.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#include <mach/mach_time.h>
#include <memory>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
////////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_displaylink = logger()->configureChannel("VKDISPLAYLINK", fvec3(0.7, 0.9, 0.7), false);

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

    logchan_displaylink->log("starting, epochOffset=%.3f estimator=%p",
           _displayLinkEpochOffsetMS, (void*)w->estimator.get());

    CVDisplayLinkCreateWithActiveCGDisplays(&w->cvLink);
    CVDisplayLinkSetOutputCallback(w->cvLink, displayLinkCallback, w);
    CVDisplayLinkStart(w->cvLink);

    _displayLink = (void*)w;
    logchan_displaylink->log("started cvLink=%p", (void*)w->cvLink);
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
