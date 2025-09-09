#pragma once

#include <ork/lev2/ui/surface.h>
#include "ged.h"
#include "ged_container.h"

namespace ork::lev2::ged {
///////////////////////////////////////////////////////////////////////////////

struct GedSurface : public ui::Surface {
public:
  // friend class lev2::PickBuffer<GedSurface>;

  GedSurface(const std::string& name, objectmodel_ptr_t model);
  ~GedSurface();

  fvec4 AssignPickId(GedObject* pobj);

  void ResetScroll();

  const GedObject* GetMouseOverNode() const;

  static orkset<GedSurface*> gAllViewports;
  void SetDims(int iw, int ih);
  void onInvalidate();

private:
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void DoSurfaceResize() override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;
  void _doGpuInit(lev2::Context* pt) final;

  int _clampedScroll(int scroll) const;
  
  // UI event helper methods
  void _onUiEventDoLoad(ui::event_constptr_t EV);
  void _onUiEventDoSave(ui::event_constptr_t EV);
  void _onUiEventDoKeyboard(ui::event_constptr_t EV);
  void _onUiEventDoMouseWheel(ui::event_constptr_t EV);
  void _onUiEventDoMove(ui::event_constptr_t EV, ui::event_ptr_t locEV);
  void _onUiEventDoDrag(ui::event_constptr_t EV, ui::event_ptr_t locEV);
  void _onUiEventDoMouseButton(ui::event_constptr_t EV, ui::event_ptr_t locEV);
  void _processCapturedPick(int ilocx, int ilocy, float fx, float fy);

  sigslot2::scoped_connection _connection_repaint;
  objectmodel_ptr_t _model;
  GedContainer _container;
  GedObject* _activeNode = nullptr;
  int miScrollY = 0;
  const GedObject* _mouseoverNode = nullptr;
  ork::msgrouter::subscriber_t _simulation_subscriber;
  
  // Async pick state
  int _lastMouseX = 0;
  int _lastMouseY = 0;
  lev2::captureasync_ptr_t _pendingPickFuture;
  ui::event_constptr_t _pendingPickEvent;
  ui::event_ptr_t _pendingPickLocEvent;
};

} //namespace ork::lev2::ged {
