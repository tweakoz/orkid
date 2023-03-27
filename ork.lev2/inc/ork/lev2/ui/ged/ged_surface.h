#pragma once

#include "ged.h"
#include <ork/lev2/ui/surface.h>

namespace ork::lev2::ged {
///////////////////////////////////////////////////////////////////////////////

class GedSurface : public ui::Surface {
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

  objectmodel_ptr_t _model;
  GedContainer _container;
  GedObject* mpActiveNode;
  int miScrollY;
  const GedObject* mpMouseOverNode;
  ork::msgrouter::subscriber_t _simulation_subscriber;
};

} //namespace ork::lev2::ged {
