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

    // No input for now (future: libinput)
    void disableMouseCursor() final {}
    void hideMouseCursor() final {}
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
