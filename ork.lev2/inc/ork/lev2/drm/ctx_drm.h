////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#if defined(__linux__)

#include <ork/lev2/gfx/ctxbase.h>
#include <ork/lev2/drm/drm_types.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct CtxDRM : public CTXBASE {
    drm::drm_context_ptr_t _drmctx;

    CtxDRM(Window* pwin);
    ~CtxDRM() override;

    void initWithData(appinitdata_ptr_t aid) override;

    // CTXBASE overrides
    void Show() override;
    void Hide() override;
    void SlotRepaint() override;
    fvec2 MapCoordToGlobal(const fvec2& v) const override;

    // Runloop methods
    void signalExit() override;
    void _runloopBegin() override;
    void _runloopEnd() override;
    void _runloopIter(bool pollevents = true) override;

    // Input handling
    void _initInput();
    void _shutdownInput();
    void _pollInput();
    void _processKeyboardEvent(void* event);
    void _processPointerMotionEvent(void* event);
    void _processPointerMotionAbsoluteEvent(void* event);
    void _processPointerButtonEvent(void* event);
    void _processPointerAxisEvent(void* event);

    // Terminal input (SSH mode)
    void _initTerminalInput();
    void _shutdownTerminalInput();
    void _pollTerminalInput();
    void _processTerminalInput(const char* buf, ssize_t len);

    // Mouse cursor control (DRM has no system cursor, these are no-ops)
    void disableMouseCursor() final;
    void hideMouseCursor() final;
    void showMouseCursor() final;

    // Query framebuffer size
    void queryFramebufferSize(int& w, int& h) const final;

    // UI event firing
    void _fire_ui_event();

private:
    // libinput state (use void* to avoid exposing C structs in header)
    void* _udev = nullptr;
    void* _libinput = nullptr;
    int _libinput_fd = -1;

    // Terminal input state (SSH mode)
    // NOTE: Original termios stored in global state for async-signal-safe cleanup
    bool _using_terminal_input = false;
    int _stdin_fd = -1;
    bool _termios_saved = false;

    // Mouse state tracking
    int _mouseX = 0;
    int _mouseY = 0;
    int _buttonState = 0;  // Bitmask: bit 0=left, bit 1=middle, bit 2=right

    // Keyboard modifier state tracking
    bool _shiftDown = false;
    bool _ctrlDown = false;
    bool _altDown = false;
    bool _superDown = false;
    // emulated caps-LOCK state (GLFW semantics: the key reads as held while the
    // lock is engaged) — raw libinput only reports the physical press/release
    bool _capsLockState = false;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
