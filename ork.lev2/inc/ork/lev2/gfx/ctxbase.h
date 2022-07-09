////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Grpahics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/kernel/mutex.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/event/Event.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/ui/event.h>

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

class CTXBASE : public ork::AutoConnector {
  RttiDeclareAbstract(CTXBASE, ork::AutoConnector);

  DeclarePublicAutoSlot(Repaint);

public:

  CTXBASE(Window* pwin);
  virtual ~CTXBASE();

  bool isGlobal() const;

  void progressHandler(opq::progressdata_ptr_t data);

  void pushRefreshPolicy(RefreshPolicyItem policy);
  void popRefreshPolicy();

  void enqueueWindowResize( int w, int h );

  Context* GetTarget() const;
  Window* GetWindow() const;
  void setContext(Context* ctx);
  void SetWindow(Window* pw);

  virtual void makeCurrent(){}
  
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

  RefreshPolicyItem currentRefreshPolicy() const;

protected:
  std::stack<RefreshPolicyItem> _policyStack;

  Context* _target;
  Window* _orkwindow;
  ui::event_ptr_t _uievent;
  bool _needsInitialize;
  svar16_t _pimpl_progress;

  RefreshPolicyItem _curpolicy;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
