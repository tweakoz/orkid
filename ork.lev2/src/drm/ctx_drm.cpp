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
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/util/logger.h>
#include <ork/kernel/string/string.h>
#include <ork/application/application.h>

extern "C" {
#include <libudev.h>
#include <libinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <signal.h>
}

// Global pointer for signal handler
static ork::lev2::CtxDRM* g_ctxdrm_for_signal = nullptr;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_ctxdrm = logger()->configureChannel("CTXDRM", fvec3(0.8, 0.4, 0.2), true);

///////////////////////////////////////////////////////////////////////////////

// Signal handler for Ctrl-C
static void drm_signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        logchan_ctxdrm->log("Signal %d received (Ctrl-C), requesting exit", signum);
        if (g_ctxdrm_for_signal) {
            g_ctxdrm_for_signal->signalExit();
        }
    }
}

CtxDRM::CtxDRM(Window* pwin)
    : CTXBASE(pwin) {
    logchan_ctxdrm->log("CtxDRM constructor");

    // Set up signal handlers for Ctrl-C
    g_ctxdrm_for_signal = this;
    signal(SIGINT, drm_signal_handler);
    signal(SIGTERM, drm_signal_handler);
    logchan_ctxdrm->log("Signal handlers installed (Ctrl-C will exit)");
}

///////////////////////////////////////////////////////////////////////////////

CtxDRM::~CtxDRM() {
    logchan_ctxdrm->log("CtxDRM destructor");

    // Clear signal handler
    if (g_ctxdrm_for_signal == this) {
        g_ctxdrm_for_signal = nullptr;
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
    }

    _shutdownInput();
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

    // Initialize input (only works on physical console, not SSH)
    if (getenv("SSH_TTY") == nullptr) {
        logchan_ctxdrm->log("Physical console detected, enabling libinput");
        _initInput();
    } else {
        logchan_ctxdrm->log("SSH session detected, libinput disabled (use Ctrl-C to exit)");
    }
}

///////////////////////////////////////////////////////////////////////////////

void CtxDRM::Show() {
    logchan_ctxdrm->log("CtxDRM::Show");

    if (_orkwindow) {
        _orkwindow->SetDirty(true);

        if (_needsInitialize) {
            logchan_ctxdrm->log("Initializing graphics context");
            _orkwindow->initContext();
            _orkwindow->OnShow();
            _needsInitialize = false;
        }
    }

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

void CtxDRM::signalExit() {
    logchan_ctxdrm->log("CtxDRM::signalExit");
    _runstate = 2;
}

///////////////////////////////////////////////////////////////////////////////

void CtxDRM::_runloopBegin() {
    logchan_ctxdrm->log("CtxDRM::_runloopBegin");
    OrkAssert(_target);

    lev2::ThreadGfxContext l2ctx_track(_target);

    _target->makeCurrentContext();

    if (_onGpuInit) {
        FontMan::gpuInit(_target);
        _target->gpuPreInit(); // Initialize Context GPU resources
        _onGpuInit(_target);
        _target->gpuPostInit(); // Initialize Context GPU resources
    }

    _runstate = 1;
}

///////////////////////////////////////////////////////////////////////////////

void CtxDRM::_runloopIter(bool pollevents) {
    lev2::ThreadGfxContext l2ctx_track(_target);

    //////////////////////////////
    // poll input events (keyboard, etc.)
    //////////////////////////////

    if (pollevents) {
        _pollInput();
    }

    //////////////////////////////
    // run main thread app logic
    //////////////////////////////

    _onRunLoopIteration();

    //////////////////////////////
    // redraw
    //////////////////////////////

    if (_onGpuUpdate) {
        _onGpuUpdate(_target);
    }

    SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////

void CtxDRM::_runloopEnd() {
    logchan_ctxdrm->log("CtxDRM::_runloopEnd");

    lev2::ThreadGfxContext l2ctx_track(_target);

    //////////////////////////////

    if (_onGpuExit) {
        _onGpuExit(_target);
    }

    //////////////////////////////
    // DRM context will restore CRTC automatically (RAII)
    //////////////////////////////

    _runstate = 3;
}

///////////////////////////////////////////////////////////////////////////////
// Input handling with libinput
///////////////////////////////////////////////////////////////////////////////

static int _libinput_open_restricted(const char* path, int flags, void* user_data) {
    int fd = open(path, flags);
    if (fd < 0) {
        logchan_ctxdrm->log("Failed to open input device: %s", path);
    }
    return fd;
}

static void _libinput_close_restricted(int fd, void* user_data) {
    close(fd);
}

static const struct libinput_interface _libinput_interface = {
    .open_restricted = _libinput_open_restricted,
    .close_restricted = _libinput_close_restricted,
};

void CtxDRM::_initInput() {
    logchan_ctxdrm->log("Initializing libinput");

    // Create udev context
    struct udev* udev = udev_new();
    if (!udev) {
        logchan_ctxdrm->log("ERROR: Failed to create udev context");
        return;
    }
    _udev = udev;

    // Create libinput context
    struct libinput* libinput = libinput_udev_create_context(&_libinput_interface, nullptr, udev);
    if (!libinput) {
        logchan_ctxdrm->log("ERROR: Failed to create libinput context");
        udev_unref(udev);
        _udev = nullptr;
        return;
    }
    _libinput = libinput;

    // Assign seat (default seat0)
    if (libinput_udev_assign_seat(libinput, "seat0") != 0) {
        logchan_ctxdrm->log("ERROR: Failed to assign libinput seat");
        libinput_unref(libinput);
        _libinput = nullptr;
        udev_unref(udev);
        _udev = nullptr;
        return;
    }

    // Get libinput file descriptor for polling
    _libinput_fd = libinput_get_fd(libinput);
    if (_libinput_fd < 0) {
        logchan_ctxdrm->log("ERROR: Failed to get libinput file descriptor");
        libinput_unref(libinput);
        _libinput = nullptr;
        udev_unref(udev);
        _udev = nullptr;
        return;
    }

    // Set non-blocking mode
    int flags = fcntl(_libinput_fd, F_GETFL, 0);
    fcntl(_libinput_fd, F_SETFL, flags | O_NONBLOCK);

    logchan_ctxdrm->log("libinput initialized successfully");
}

void CtxDRM::_shutdownInput() {
    if (_libinput) {
        logchan_ctxdrm->log("Shutting down libinput");
        libinput_unref(static_cast<struct libinput*>(_libinput));
        _libinput = nullptr;
        _libinput_fd = -1;
    }
    if (_udev) {
        udev_unref(static_cast<struct udev*>(_udev));
        _udev = nullptr;
    }
}

void CtxDRM::_pollInput() {
    if (!_libinput) return;

    struct libinput* libinput = static_cast<struct libinput*>(_libinput);

    // Dispatch events
    libinput_dispatch(libinput);

    // Process all available events
    struct libinput_event* event;
    while ((event = libinput_get_event(libinput)) != nullptr) {
        auto event_type = libinput_event_get_type(event);

        switch (event_type) {
            case LIBINPUT_EVENT_KEYBOARD_KEY:
                _processKeyboardEvent(event);
                break;
            default:
                // Ignore other event types for now
                break;
        }

        libinput_event_destroy(event);
    }
}

void CtxDRM::_processKeyboardEvent(void* event_ptr) {
    struct libinput_event* event = static_cast<struct libinput_event*>(event_ptr);
    auto keyboard_event = libinput_event_get_keyboard_event(event);
    uint32_t key = libinput_event_keyboard_get_key(keyboard_event);
    auto key_state = libinput_event_keyboard_get_key_state(keyboard_event);

    // Debug: log all key events
    const char* state_str = (key_state == LIBINPUT_KEY_STATE_PRESSED) ? "PRESSED" : "RELEASED";
    logchan_ctxdrm->log("Keyboard event: key=%u (%s)", key, state_str);

    // Only process key presses
    if (key_state == LIBINPUT_KEY_STATE_PRESSED) {
        // ESC key hardwired to exit (KEY_ESC = 1)
        if (key == KEY_ESC) {
            logchan_ctxdrm->log("ESC key pressed - calling signalExit()");
            signalExit();
            logchan_ctxdrm->log("signalExit() called, _runstate=%d", _runstate);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
