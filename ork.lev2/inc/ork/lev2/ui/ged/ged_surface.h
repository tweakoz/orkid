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

  sigslot2::scoped_connection _connection_repaint;
  objectmodel_ptr_t _model;
  GedContainer _container;
  GedObject* mpActiveNode;
  int miScrollY;
  const GedObject* mpMouseOverNode;
  ork::msgrouter::subscriber_t _simulation_subscriber;
};

} //namespace ork::lev2::ged {
