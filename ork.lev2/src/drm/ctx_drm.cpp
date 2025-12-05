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
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/context.h>
#include <ork/util/logger.h>
#include <ork/kernel/string/string.h>
#include <ork/application/application.h>
#include <GLFW/glfw3.h>  // For GLFW keycodes (unified keycode system)

extern "C" {
#include <libudev.h>
#include <libinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <linux/input-event-codes.h>
#include <signal.h>
#include <sys/select.h>
#include <stdlib.h>

// Mouse button codes from linux/input-event-codes.h
#ifndef BTN_LEFT
#define BTN_LEFT 0x110
#endif
#ifndef BTN_RIGHT
#define BTN_RIGHT 0x111
#endif
#ifndef BTN_MIDDLE
#define BTN_MIDDLE 0x112
#endif
}

// Save Linux KEY_DOWN/KEY_UP values before undefining (they conflict with ui::EventCode)
static constexpr uint32_t LINUX_KEY_DOWN = KEY_DOWN;
static constexpr uint32_t LINUX_KEY_UP = KEY_UP;
#undef KEY_DOWN
#undef KEY_UP

// Global state for async-signal-safe terminal cleanup
static struct {
    std::atomic<bool> termios_saved{false};
    struct termios original_termios;
    int stdin_fd = STDIN_FILENO;
} g_terminal_state;

// Global pointer for signal handler
static ork::lev2::CtxDRM* g_ctxdrm_for_signal = nullptr;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_ctxdrm = logger()->configureChannel("CTXDRM", fvec3(0.8, 0.4, 0.2), true);

// Forward declaration (defined later in file)
static int linux_to_glfw_keycode(uint32_t linux_key);

///////////////////////////////////////////////////////////////////////////////

// Async-signal-safe terminal cleanup (ONLY uses async-signal-safe functions)
// Can be called from signal handlers, atexit, or normal code
static void restore_terminal_async_safe() {
    if (g_terminal_state.termios_saved.load(std::memory_order_relaxed)) {
        // Restore terminal settings (async-signal-safe)
        tcsetattr(g_terminal_state.stdin_fd, TCSANOW, &g_terminal_state.original_termios);

        // Flush stdin (async-signal-safe)
        tcflush(g_terminal_state.stdin_fd, TCIFLUSH);

        // Mark as restored (prevent double-restore)
        g_terminal_state.termios_saved.store(false, std::memory_order_relaxed);
    }
}

// atexit handler for normal program exit
static void atexit_restore_terminal() {
    restore_terminal_async_safe();
}

// Signal handler for Ctrl-C (ASYNC-SIGNAL-SAFE)
static void drm_signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        // Restore terminal FIRST (async-signal-safe)
        restore_terminal_async_safe();

        // Write message using async-signal-safe write()
        const char msg[] = "\nSignal received, exiting...\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);

        // Signal exit to main loop
        if (g_ctxdrm_for_signal) {
            g_ctxdrm_for_signal->signalExit();
        }
    }
}

CtxDRM::CtxDRM(Window* pwin)
    : CTXBASE(pwin) {
    logchan_ctxdrm->log("CtxDRM constructor");

    // DRM always uses fullscreen mouse mode (no system cursor)
    _fsMouseMode = true;

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
    _shutdownTerminalInput();
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

    // CRITICAL: Override appinitdata dimensions with actual DRM mode size
    // The user may have requested 900x900, but the actual display mode is 640x480
    // Everything needs to use the actual mode dimensions
    aid->_width = _drmctx->imageExtent.width;
    aid->_height = _drmctx->imageExtent.height;
    logchan_ctxdrm->log("DRM: Overriding appinitdata dimensions to actual mode: %dx%d",
                        aid->_width, aid->_height);

    // Initialize both input methods simultaneously
    // This allows SSH keyboard while using physical mouse
    _initInput();  // libinput (physical mouse/keyboard)
    if (getenv("SSH_TTY") != nullptr) {
        logchan_ctxdrm->log("SSH session detected, enabling terminal keyboard input");
        _initTerminalInput();  // Terminal keyboard (SSH)
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
    if (not GfxEnv::initialized()) {
        return;
    }

    if (this->_target) {
        _target->makeCurrentContext();
        auto gfxwin = _uievent->mpGfxWin;
        _uievent->mpGfxWin = (Window*)_target->FBI()->GetThisBuffer();
        auto drwev = std::make_shared<ui::DrawEvent>(this->_target);

        auto widget = gfxwin ? gfxwin->GetRootWidget() : nullptr;

        if (widget) {
            widget->draw(drwev);
        }
        else {
            _target->beginFrame(false);  // false = non-visual frame
            _target->endFrame();
        }
    }
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
        _pollInput();           // Try libinput (physical console)
        _pollTerminalInput();   // Try terminal input (SSH)
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
    static int motion_event_count = 0;
    while ((event = libinput_get_event(libinput)) != nullptr) {
        auto event_type = libinput_event_get_type(event);

        switch (event_type) {
            case LIBINPUT_EVENT_KEYBOARD_KEY:
                _processKeyboardEvent(event);
                break;
            case LIBINPUT_EVENT_POINTER_MOTION:
                motion_event_count++;
                if (motion_event_count < 5) {
                    logchan_ctxdrm->log("Received POINTER_MOTION (relative) event #%d", motion_event_count);
                }
                _processPointerMotionEvent(event);
                break;
            case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
                motion_event_count++;
                if (motion_event_count < 5) {
                    logchan_ctxdrm->log("Received POINTER_MOTION_ABSOLUTE event #%d", motion_event_count);
                }
                _processPointerMotionAbsoluteEvent(event);
                break;
            case LIBINPUT_EVENT_POINTER_BUTTON:
                _processPointerButtonEvent(event);
                break;
            case LIBINPUT_EVENT_POINTER_AXIS:
                _processPointerAxisEvent(event);
                break;
            default:
                // Ignore other event types
                break;
        }

        libinput_event_destroy(event);
    }
}

void CtxDRM::_processKeyboardEvent(void* event_ptr) {
    struct libinput_event* event = static_cast<struct libinput_event*>(event_ptr);
    auto keyboard_event = libinput_event_get_keyboard_event(event);
    uint32_t linux_key = libinput_event_keyboard_get_key(keyboard_event);
    auto key_state = libinput_event_keyboard_get_key_state(keyboard_event);

    bool pressed = (key_state == LIBINPUT_KEY_STATE_PRESSED);
    const char* state_str = pressed ? "PRESSED" : "RELEASED";

    // Convert Linux keycode to GLFW keycode for unified handling
    int glfw_key = linux_to_glfw_keycode(linux_key);

    logchan_ctxdrm->log("Keyboard event: linux_key=%u glfw_key=%d (%s)", linux_key, glfw_key, state_str);

    // ESC key hardwired to exit (works on both press states for safety)
    if (pressed && linux_key == KEY_ESC) {
        logchan_ctxdrm->log("ESC key pressed - calling signalExit()");
        signalExit();
        logchan_ctxdrm->log("signalExit() called, _runstate=%d", _runstate);
        return;
    }

    auto uiev = _uievent;

    // Update modifier state (track in both member var and uievent)
    switch (linux_key) {
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            _shiftDown = pressed;
            uiev->mbSHIFT = _shiftDown;
            break;
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            _ctrlDown = pressed;
            uiev->mbCTRL = _ctrlDown;
            break;
        case KEY_LEFTALT:
        case KEY_RIGHTALT:
            _altDown = pressed;
            uiev->mbALT = _altDown;
            break;
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA:
            _superDown = pressed;
            uiev->mbSUPER = _superDown;
            break;
    }

    // Fire UI keyboard event
    uiev->_eventcode = pressed ? ui::EventCode::KEY_DOWN : ui::EventCode::KEY_UP;
    uiev->miKeyCode = glfw_key;

    _fire_ui_event();
}

void CtxDRM::_processPointerMotionEvent(void* event_ptr) {
    struct libinput_event* event = static_cast<struct libinput_event*>(event_ptr);
    auto pointer_event = libinput_event_get_pointer_event(event);

    // Get relative motion (dx, dy)
    double dx = libinput_event_pointer_get_dx(pointer_event);
    double dy = libinput_event_pointer_get_dy(pointer_event);

    // Update absolute mouse position (clamped to screen bounds)
    _mouseX += int(dx);
    _mouseY += int(dy);

    if (_mouseX < 0) _mouseX = 0;
    if (_mouseY < 0) _mouseY = 0;
    if (_drmctx) {
        if (_mouseX >= int(_drmctx->imageExtent.width)) _mouseX = _drmctx->imageExtent.width - 1;
        if (_mouseY >= int(_drmctx->imageExtent.height)) _mouseY = _drmctx->imageExtent.height - 1;
    }

    // Fill UI event
    auto uiev = _uievent;
    uiev->miLastX = uiev->miX;
    uiev->miLastY = uiev->miY;
    uiev->miX = _mouseX;
    uiev->miY = _mouseY;

    if (_drmctx) {
        float w = float(_drmctx->imageExtent.width);
        float h = float(_drmctx->imageExtent.height);
        uiev->mfLastUnitX = uiev->mfUnitX;
        uiev->mfLastUnitY = uiev->mfUnitY;
        uiev->mfUnitX = float(_mouseX) / w;
        uiev->mfUnitY = float(_mouseY) / h;
        uiev->miScreenWidth = int(w);
        uiev->miScreenHeight = int(h);
    }

    // Set event code based on button state
    if (_buttonState == 0) {
        uiev->_eventcode = ui::EventCode::MOVE;
    } else {
        uiev->_eventcode = ui::EventCode::DRAG;
    }

    logchan_ctxdrm->log("Mouse motion: dx=%.2f dy=%.2f pos=(%d,%d) unitXY=(%.3f,%.3f) %s",
                        dx, dy, _mouseX, _mouseY, uiev->mfUnitX, uiev->mfUnitY,
                        (_buttonState == 0) ? "MOVE" : "DRAG");

    _fire_ui_event();
}

void CtxDRM::_processPointerMotionAbsoluteEvent(void* event_ptr) {
    struct libinput_event* event = static_cast<struct libinput_event*>(event_ptr);
    auto pointer_event = libinput_event_get_pointer_event(event);

    // Get screen dimensions for coordinate transformation
    uint32_t screen_width = 1920;
    uint32_t screen_height = 1080;
    if (_drmctx) {
        screen_width = _drmctx->imageExtent.width;
        screen_height = _drmctx->imageExtent.height;
    }

    // Get absolute coordinates transformed to screen dimensions
    double abs_x = libinput_event_pointer_get_absolute_x_transformed(pointer_event, screen_width);
    double abs_y = libinput_event_pointer_get_absolute_y_transformed(pointer_event, screen_height);

    // Update mouse position directly (already in screen coordinates)
    _mouseX = int(abs_x);
    _mouseY = int(abs_y);

    // Clamp to screen bounds
    if (_mouseX < 0) _mouseX = 0;
    if (_mouseY < 0) _mouseY = 0;
    if (_mouseX >= int(screen_width)) _mouseX = screen_width - 1;
    if (_mouseY >= int(screen_height)) _mouseY = screen_height - 1;

    // Fill UI event
    auto uiev = _uievent;
    uiev->miLastX = uiev->miX;
    uiev->miLastY = uiev->miY;
    uiev->miX = _mouseX;
    uiev->miY = _mouseY;

    float w = float(screen_width);
    float h = float(screen_height);
    uiev->mfLastUnitX = uiev->mfUnitX;
    uiev->mfLastUnitY = uiev->mfUnitY;
    uiev->mfUnitX = float(_mouseX) / w;
    uiev->mfUnitY = float(_mouseY) / h;
    uiev->miScreenWidth = int(w);
    uiev->miScreenHeight = int(h);

    // Set event code based on button state
    if (_buttonState == 0) {
        uiev->_eventcode = ui::EventCode::MOVE;
    } else {
        uiev->_eventcode = ui::EventCode::DRAG;
    }

    logchan_ctxdrm->log("Mouse abs motion: pos=(%d,%d) unitXY=(%.3f,%.3f) %s",
                        _mouseX, _mouseY, uiev->mfUnitX, uiev->mfUnitY,
                        (_buttonState == 0) ? "MOVE" : "DRAG");

    _fire_ui_event();
}

void CtxDRM::_processPointerButtonEvent(void* event_ptr) {
    struct libinput_event* event = static_cast<struct libinput_event*>(event_ptr);
    auto pointer_event = libinput_event_get_pointer_event(event);

    uint32_t button = libinput_event_pointer_get_button(pointer_event);
    auto button_state = libinput_event_pointer_get_button_state(pointer_event);

    bool DOWN = (button_state == LIBINPUT_BUTTON_STATE_PRESSED);
    const char* state_str = DOWN ? "PRESSED" : "RELEASED";

    auto uiev = _uievent;

    // Map Linux button codes to Orkid button flags
    // BTN_LEFT = 0x110, BTN_RIGHT = 0x111, BTN_MIDDLE = 0x112
    const char* button_name = "UNKNOWN";
    switch (button) {
        case BTN_LEFT:  // 0x110
            button_name = "LEFT";
            uiev->mbLeftButton = DOWN;
            _buttonState = (_buttonState & 6) | int(DOWN);
            break;
        case BTN_MIDDLE:  // 0x112
            button_name = "MIDDLE";
            uiev->mbMiddleButton = DOWN;
            _buttonState = (_buttonState & 5) | (int(DOWN) << 1);
            break;
        case BTN_RIGHT:  // 0x111
            button_name = "RIGHT";
            uiev->mbRightButton = DOWN;
            _buttonState = (_buttonState & 3) | (int(DOWN) << 2);
            break;
    }

    uiev->_eventcode = DOWN ? ui::EventCode::PUSH : ui::EventCode::RELEASE;

    logchan_ctxdrm->log("Mouse button: %s (%s) buttonState=0x%x", button_name, state_str, _buttonState);

    _fire_ui_event();
}

void CtxDRM::_processPointerAxisEvent(void* event_ptr) {
    struct libinput_event* event = static_cast<struct libinput_event*>(event_ptr);
    auto pointer_event = libinput_event_get_pointer_event(event);

    auto uiev = _uievent;
    uiev->_eventcode = ui::EventCode::MOUSEWHEEL;
    uiev->miMWX = 0;
    uiev->miMWY = 0;

    bool has_scroll = false;

    // libinput gives ~15 units per wheel detent
    // GLFW gives ~1.0 per detent, then multiplies by 10 -> ~10 per detent
    // Empirically tuned to match macOS responsiveness (was 10x too fast)
    // Negate Y to match macOS/GLFW scroll direction convention
    constexpr double LIBINPUT_TO_GLFW_SCALE = 1.0 / 15.0;

    // Check if we have vertical scroll
    if (libinput_event_pointer_has_axis(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
        double value = libinput_event_pointer_get_axis_value(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
        uiev->miMWY = int(-value * LIBINPUT_TO_GLFW_SCALE);  // Negate for natural scrolling
        has_scroll = true;
        logchan_ctxdrm->log("Mouse scroll: VERTICAL raw=%.2f scaled=%d", value, uiev->miMWY);
    }

    // Check if we have horizontal scroll
    if (libinput_event_pointer_has_axis(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
        double value = libinput_event_pointer_get_axis_value(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
        uiev->miMWX = int(value * LIBINPUT_TO_GLFW_SCALE);
        has_scroll = true;
        logchan_ctxdrm->log("Mouse scroll: HORIZONTAL raw=%.2f scaled=%d", value, uiev->miMWX);
    }

    if (has_scroll) {
        _fire_ui_event();
    }
}

void CtxDRM::_fire_ui_event() {
    auto uiev = _uievent;
    auto gfxwin = uiev->mpGfxWin;
    auto root = gfxwin ? gfxwin->GetRootWidget() : nullptr;
    uiev->_uicontext = root ? root->_uicontext : nullptr;
    if (root) {
        uiev->setvpDim(root);
        ui::Event::sendToContext(uiev);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Cursor methods (no-ops for DRM - no system cursor)
///////////////////////////////////////////////////////////////////////////////

void CtxDRM::disableMouseCursor() {
    // DRM has no system cursor to disable
}

void CtxDRM::hideMouseCursor() {
    // DRM has no system cursor to hide
}

void CtxDRM::showMouseCursor() {
    // DRM has no system cursor to show
}

///////////////////////////////////////////////////////////////////////////////
// Linux evdev keycode to GLFW keycode mapping
///////////////////////////////////////////////////////////////////////////////

static int linux_to_glfw_keycode(uint32_t linux_key) {
    // Map Linux evdev keycodes (from linux/input-event-codes.h) to GLFW keycodes
    // This allows applications to use a unified keycode system
    switch (linux_key) {
        // Function keys
        case KEY_ESC:       return GLFW_KEY_ESCAPE;
        case KEY_F1:        return GLFW_KEY_F1;
        case KEY_F2:        return GLFW_KEY_F2;
        case KEY_F3:        return GLFW_KEY_F3;
        case KEY_F4:        return GLFW_KEY_F4;
        case KEY_F5:        return GLFW_KEY_F5;
        case KEY_F6:        return GLFW_KEY_F6;
        case KEY_F7:        return GLFW_KEY_F7;
        case KEY_F8:        return GLFW_KEY_F8;
        case KEY_F9:        return GLFW_KEY_F9;
        case KEY_F10:       return GLFW_KEY_F10;
        case KEY_F11:       return GLFW_KEY_F11;
        case KEY_F12:       return GLFW_KEY_F12;

        // Number row
        case KEY_GRAVE:     return GLFW_KEY_GRAVE_ACCENT;
        case KEY_1:         return GLFW_KEY_1;
        case KEY_2:         return GLFW_KEY_2;
        case KEY_3:         return GLFW_KEY_3;
        case KEY_4:         return GLFW_KEY_4;
        case KEY_5:         return GLFW_KEY_5;
        case KEY_6:         return GLFW_KEY_6;
        case KEY_7:         return GLFW_KEY_7;
        case KEY_8:         return GLFW_KEY_8;
        case KEY_9:         return GLFW_KEY_9;
        case KEY_0:         return GLFW_KEY_0;
        case KEY_MINUS:     return GLFW_KEY_MINUS;
        case KEY_EQUAL:     return GLFW_KEY_EQUAL;
        case KEY_BACKSPACE: return GLFW_KEY_BACKSPACE;

        // Tab and letters
        case KEY_TAB:       return GLFW_KEY_TAB;
        case KEY_Q:         return GLFW_KEY_Q;
        case KEY_W:         return GLFW_KEY_W;
        case KEY_E:         return GLFW_KEY_E;
        case KEY_R:         return GLFW_KEY_R;
        case KEY_T:         return GLFW_KEY_T;
        case KEY_Y:         return GLFW_KEY_Y;
        case KEY_U:         return GLFW_KEY_U;
        case KEY_I:         return GLFW_KEY_I;
        case KEY_O:         return GLFW_KEY_O;
        case KEY_P:         return GLFW_KEY_P;
        case KEY_LEFTBRACE: return GLFW_KEY_LEFT_BRACKET;
        case KEY_RIGHTBRACE:return GLFW_KEY_RIGHT_BRACKET;
        case KEY_BACKSLASH: return GLFW_KEY_BACKSLASH;

        // Caps lock and more letters
        case KEY_CAPSLOCK:  return GLFW_KEY_CAPS_LOCK;
        case KEY_A:         return GLFW_KEY_A;
        case KEY_S:         return GLFW_KEY_S;
        case KEY_D:         return GLFW_KEY_D;
        case KEY_F:         return GLFW_KEY_F;
        case KEY_G:         return GLFW_KEY_G;
        case KEY_H:         return GLFW_KEY_H;
        case KEY_J:         return GLFW_KEY_J;
        case KEY_K:         return GLFW_KEY_K;
        case KEY_L:         return GLFW_KEY_L;
        case KEY_SEMICOLON: return GLFW_KEY_SEMICOLON;
        case KEY_APOSTROPHE:return GLFW_KEY_APOSTROPHE;
        case KEY_ENTER:     return GLFW_KEY_ENTER;

        // Shift row
        case KEY_LEFTSHIFT: return GLFW_KEY_LEFT_SHIFT;
        case KEY_Z:         return GLFW_KEY_Z;
        case KEY_X:         return GLFW_KEY_X;
        case KEY_C:         return GLFW_KEY_C;
        case KEY_V:         return GLFW_KEY_V;
        case KEY_B:         return GLFW_KEY_B;
        case KEY_N:         return GLFW_KEY_N;
        case KEY_M:         return GLFW_KEY_M;
        case KEY_COMMA:     return GLFW_KEY_COMMA;
        case KEY_DOT:       return GLFW_KEY_PERIOD;
        case KEY_SLASH:     return GLFW_KEY_SLASH;
        case KEY_RIGHTSHIFT:return GLFW_KEY_RIGHT_SHIFT;

        // Bottom row
        case KEY_LEFTCTRL:  return GLFW_KEY_LEFT_CONTROL;
        case KEY_LEFTMETA:  return GLFW_KEY_LEFT_SUPER;
        case KEY_LEFTALT:   return GLFW_KEY_LEFT_ALT;
        case KEY_SPACE:     return GLFW_KEY_SPACE;
        case KEY_RIGHTALT:  return GLFW_KEY_RIGHT_ALT;
        case KEY_RIGHTMETA: return GLFW_KEY_RIGHT_SUPER;
        case KEY_RIGHTCTRL: return GLFW_KEY_RIGHT_CONTROL;

        // Arrow keys (using saved constants since KEY_DOWN/KEY_UP are undef'd)
        case LINUX_KEY_UP:   return GLFW_KEY_UP;
        case LINUX_KEY_DOWN: return GLFW_KEY_DOWN;
        case KEY_LEFT:       return GLFW_KEY_LEFT;
        case KEY_RIGHT:      return GLFW_KEY_RIGHT;

        // Navigation cluster
        case KEY_INSERT:    return GLFW_KEY_INSERT;
        case KEY_DELETE:    return GLFW_KEY_DELETE;
        case KEY_HOME:      return GLFW_KEY_HOME;
        case KEY_END:       return GLFW_KEY_END;
        case KEY_PAGEUP:    return GLFW_KEY_PAGE_UP;
        case KEY_PAGEDOWN:  return GLFW_KEY_PAGE_DOWN;

        // Numpad
        case KEY_NUMLOCK:   return GLFW_KEY_NUM_LOCK;
        case KEY_KPSLASH:   return GLFW_KEY_KP_DIVIDE;
        case KEY_KPASTERISK:return GLFW_KEY_KP_MULTIPLY;
        case KEY_KPMINUS:   return GLFW_KEY_KP_SUBTRACT;
        case KEY_KP7:       return GLFW_KEY_KP_7;
        case KEY_KP8:       return GLFW_KEY_KP_8;
        case KEY_KP9:       return GLFW_KEY_KP_9;
        case KEY_KPPLUS:    return GLFW_KEY_KP_ADD;
        case KEY_KP4:       return GLFW_KEY_KP_4;
        case KEY_KP5:       return GLFW_KEY_KP_5;
        case KEY_KP6:       return GLFW_KEY_KP_6;
        case KEY_KP1:       return GLFW_KEY_KP_1;
        case KEY_KP2:       return GLFW_KEY_KP_2;
        case KEY_KP3:       return GLFW_KEY_KP_3;
        case KEY_KPENTER:   return GLFW_KEY_KP_ENTER;
        case KEY_KP0:       return GLFW_KEY_KP_0;
        case KEY_KPDOT:     return GLFW_KEY_KP_DECIMAL;

        // Other keys
        case KEY_PRINT:     return GLFW_KEY_PRINT_SCREEN;
        case KEY_SCROLLLOCK:return GLFW_KEY_SCROLL_LOCK;
        case KEY_PAUSE:     return GLFW_KEY_PAUSE;

        default:            return GLFW_KEY_UNKNOWN;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Terminal input (SSH mode)
///////////////////////////////////////////////////////////////////////////////

void CtxDRM::_initTerminalInput() {
    logchan_ctxdrm->log("Initializing terminal input (SSH mode)");

    _stdin_fd = STDIN_FILENO;

    // Save original terminal settings to global state (async-signal-safe access)
    if (tcgetattr(_stdin_fd, &g_terminal_state.original_termios) == 0) {
        _termios_saved = true;

        // Set terminal to raw mode
        struct termios raw = g_terminal_state.original_termios;

        // Disable canonical mode, echo, signals
        raw.c_lflag &= ~(ICANON | ECHO | ISIG);

        // Disable special processing of CR/NL
        raw.c_iflag &= ~(ICRNL | INLCR);

        // Set non-blocking read with minimal char return
        raw.c_cc[VMIN] = 0;   // Return immediately
        raw.c_cc[VTIME] = 0;  // No timeout

        if (tcsetattr(_stdin_fd, TCSANOW, &raw) == 0) {
            // Set stdin to non-blocking mode
            int flags = fcntl(_stdin_fd, F_GETFL, 0);
            fcntl(_stdin_fd, F_SETFL, flags | O_NONBLOCK);

            // Mark global state as saved (for async-signal-safe cleanup)
            g_terminal_state.termios_saved.store(true, std::memory_order_relaxed);

            // Register atexit handler (runs on normal exit)
            static bool atexit_registered = false;
            if (!atexit_registered) {
                atexit(atexit_restore_terminal);
                atexit_registered = true;
                logchan_ctxdrm->log("Registered atexit handler for terminal restoration");
            }

            _using_terminal_input = true;
            logchan_ctxdrm->log("Terminal input initialized successfully (ESC, Ctrl-C, Ctrl-D, or 'q' to exit)");
        } else {
            logchan_ctxdrm->log("ERROR: Failed to set terminal raw mode");
            _termios_saved = false;
        }
    } else {
        logchan_ctxdrm->log("ERROR: Failed to get terminal attributes");
    }
}

void CtxDRM::_shutdownTerminalInput() {
    if (_termios_saved) {
        logchan_ctxdrm->log("Restoring terminal settings");

        // Use async-signal-safe cleanup function
        restore_terminal_async_safe();

        _termios_saved = false;
    }
    _using_terminal_input = false;
}

void CtxDRM::_pollTerminalInput() {
    if (!_using_terminal_input) return;

    // Check if stdin has data available
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(_stdin_fd, &readfds);

    struct timeval timeout = {0, 0};  // Non-blocking check
    int ret = ::select(_stdin_fd + 1, &readfds, nullptr, nullptr, &timeout);

    if (ret > 0 && FD_ISSET(_stdin_fd, &readfds)) {
        char buf[64];
        ssize_t n = read(_stdin_fd, buf, sizeof(buf));

        if (n > 0) {
            _processTerminalInput(buf, n);
        }
    }
}

void CtxDRM::_processTerminalInput(const char* buf, ssize_t len) {
    for (ssize_t i = 0; i < len; i++) {
        unsigned char ch = buf[i];

        // ESC key (ASCII 27)
        if (ch == 27) {
            // Check if this is a lone ESC or start of escape sequence
            if (i + 1 < len && buf[i + 1] == '[') {
                // This is an escape sequence (arrow keys, etc.)
                // Skip for now, could parse if needed
                continue;
            } else {
                logchan_ctxdrm->log("ESC key pressed (terminal input) - calling signalExit()");
                signalExit();
                return;
            }
        }

        // Ctrl-C (ASCII 3)
        else if (ch == 3) {
            logchan_ctxdrm->log("Ctrl-C pressed (terminal input) - calling signalExit()");
            signalExit();
            return;
        }

        // Ctrl-D (ASCII 4) - common Unix EOF signal
        else if (ch == 4) {
            logchan_ctxdrm->log("Ctrl-D pressed (terminal input) - calling signalExit()");
            signalExit();
            return;
        }

        // 'q' or 'Q' to quit (nice fallback)
        else if (ch == 'q' || ch == 'Q') {
            logchan_ctxdrm->log("'%c' key pressed (terminal input) - calling signalExit()", ch);
            signalExit();
            return;
        }

        // Log other keys for debugging (optional, can be removed)
        if (0) {  // Disabled by default to reduce log noise
            if (ch >= 32 && ch <= 126) {
                logchan_ctxdrm->log("Terminal key pressed: '%c' (ASCII %d)", ch, ch);
            } else {
                logchan_ctxdrm->log("Terminal key pressed: ASCII %d", ch);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
