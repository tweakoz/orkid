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

    // Terminal input (SSH mode)
    void _initTerminalInput();
    void _shutdownTerminalInput();
    void _pollTerminalInput();
    void _processTerminalInput(const char* buf, ssize_t len);

    // No input for now (future: libinput)
    void disableMouseCursor() final {}
    void hideMouseCursor() final {}

private:
    // libinput state (use void* to avoid exposing C structs in header)
    void* _udev = nullptr;
    void* _libinput = nullptr;
    int _libinput_fd = -1;

    // Terminal input state (SSH mode - use void* to avoid exposing C structs)
    bool _using_terminal_input = false;
    int _stdin_fd = -1;
    void* _original_termios = nullptr;  // Actually struct termios*
    bool _termios_saved = false;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
