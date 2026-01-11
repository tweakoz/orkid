////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Grpahics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/varmap.inl>

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/kernel/mutex.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/event/Event.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/ui/event.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
struct AppInitData;
using appinitdata_ptr_t = std::shared_ptr<AppInitData>;
}
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////


/// ////////////////////////////////////////////////////////////////////////////
/// Graphics Context Base
/// this abstraction allows us to switch UI toolkits (Qt/Fltk, etc...)
/// ////////////////////////////////////////////////////////////////////////////

typedef void* CTFLXID;

enum ERefreshPolicy {
  EREFRESH_FASTEST = 0, // refresh as fast as the update loop can go
  EREFRESH_WHENDIRTY,   // refresh whenever dirty
  EREFRESH_FIXEDFPS,    // refresh at a fixed frame rate
  EREFRESH_END,
};

struct RefreshPolicyItem {
  ERefreshPolicy _policy = EREFRESH_END;
  int _fps               = -1;
};

class CTXBASE : public ork::Object {
  RttiDeclareAbstract(CTXBASE, ork::Object);


public:

  // Mouse cursor control (for fullscreen mouse mode)
  virtual void disableMouseCursor() {}
  virtual void hideMouseCursor() {}
  virtual void showMouseCursor() {}

  // Fullscreen mouse mode accessor
  bool fsMouseMode() const { return _fsMouseMode; }

  // UI event accessors
  ui::event_ptr_t uievent() { return _uievent; }
  ui::event_constptr_t uievent() const { return _uievent; }

  bool isGlobal() const;

  void pushRefreshPolicy(RefreshPolicyItem policy);
  void popRefreshPolicy();

  void enqueueWindowResize( int w, int h );

  Context* GetTarget() const;
  Window* GetWindow() const;
  void setContext(Context* ctx);
  void SetWindow(Window* pw);

  virtual void SlotRepaint(void) {
  }
  virtual void _setRefreshPolicy(RefreshPolicyItem policy) {
    _curpolicy = policy;
  }

  virtual void Show() {
  }
  virtual void Hide() {
  }
  virtual fvec2 MapCoordToGlobal(const fvec2& v) const {
    return v;
  }
  virtual void _doEnqueueWindowResize( int w, int h ) {}

  // Query the backend's current framebuffer size
  virtual void queryFramebufferSize(int& w, int& h) const { w = 0; h = 0; }

  // Platform-specific runloop methods
  virtual void initWithData(appinitdata_ptr_t aid) {}
  virtual void signalExit() {}
  virtual void _runloopBegin() {}
  virtual void _runloopEnd() {}
  virtual void _runloopIter(bool pollevents = true) {}

  RefreshPolicyItem currentRefreshPolicy() const;

  std::stack<RefreshPolicyItem> _policyStack;

  Context* _target = nullptr;
  Window* _orkwindow = nullptr;
  ui::event_ptr_t _uievent;
  bool _needsInitialize = true;
  svar16_t _pimpl_progress;
  svarshp_t _implEV;
  varmap::varmap_constptr_t _vars;
  RefreshPolicyItem _curpolicy;
  object::autoslot_ptr_t _slotRepaint;
  float _contentScaleX = 1.0f;
  float _contentScaleY = 1.0f;

  // Platform-specific members
  int _runstate = 0;
  void_lambda_t _onRunLoopIteration;
  using gpuupdfn_t = std::function<void(Context*)>;
  gpuupdfn_t _onGpuInit;
  gpuupdfn_t _onGpuUpdate;
  gpuupdfn_t _onGpuExit;

  // Fullscreen mouse mode (hardware cursor hidden, virtual cursor rendered)
  bool _fsMouseMode = false;

  protected:
    void onSharedCreate(std::shared_ptr<CTXBASE> this_shared);
    CTXBASE(Window* pwin);
    virtual ~CTXBASE();

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
