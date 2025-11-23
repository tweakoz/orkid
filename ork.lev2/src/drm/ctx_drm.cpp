////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if defined(__linux__)

#include <ork/lev2/drm/ctx_drm.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/util/logger.h>
#include <ork/kernel/string/string.h>
#include <ork/application/application.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_ctxdrm = logger()->configureChannel("CTXDRM", fvec3(0.8, 0.4, 0.2), true);

///////////////////////////////////////////////////////////////////////////////

CtxDRM::CtxDRM(Window* pwin)
    : CTXBASE(pwin) {
    logchan_ctxdrm->log("CtxDRM constructor");
}

///////////////////////////////////////////////////////////////////////////////

CtxDRM::~CtxDRM() {
    logchan_ctxdrm->log("CtxDRM destructor");
    // DRM context will clean up automatically (RAII)
}

///////////////////////////////////////////////////////////////////////////////

void CtxDRM::initWithData(appinitdata_ptr_t aid) {
    logchan_ctxdrm->log("CtxDRM::initWithData");

    // Parse DRM mode string (e.g., "b0" = device 'b', mode 0)
    std::string mode_str = aid->_drm_mode;

    if (mode_str.length() < 2) {
        logchan_ctxdrm->log("ERROR: Invalid DRM mode string: %s (expected format: 'a0', 'b1', etc.)", mode_str.c_str());
        throw std::runtime_error("Invalid DRM mode string");
    }

    char device = mode_str[0];

    // Normalize to lowercase
    if (device >= 'A' && device <= 'Z') {
        device = device - 'A' + 'a';
    }

    // Parse mode index
    int mode_index = 0;
    try {
        mode_index = std::stoi(mode_str.substr(1));
    } catch (...) {
        logchan_ctxdrm->log("ERROR: Invalid mode index in DRM mode string: %s", mode_str.c_str());
        throw std::runtime_error("Invalid DRM mode index");
    }

    // Create DRM context
    try {
        _drmctx = std::make_shared<drm::DRMContext>(device, mode_index);
        logchan_ctxdrm->log("DRM context created for device %c, mode %d", device, mode_index);
    } catch (const std::exception& e) {
        logchan_ctxdrm->log("ERROR: Failed to create DRM context: %s", e.what());
        throw;
    }
}

///////////////////////////////////////////////////////////////////////////////

void CtxDRM::Show() {
    logchan_ctxdrm->log("CtxDRM::Show");
    // DRM mode will be set when first frame is rendered (drmModeSetCrtc)
}

///////////////////////////////////////////////////////////////////////////////

void CtxDRM::Hide() {
    logchan_ctxdrm->log("CtxDRM::Hide");
    // Restore original CRTC (done in DRMContext destructor)
}

///////////////////////////////////////////////////////////////////////////////

void CtxDRM::SlotRepaint() {
    // No-op for DRM (vblank-driven)
}

///////////////////////////////////////////////////////////////////////////////

fvec2 CtxDRM::MapCoordToGlobal(const fvec2& v) const {
    // DRM is fullscreen, no coordinate mapping needed
    return v;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
