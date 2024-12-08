#pragma once


#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/surface.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/panel.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ezapp.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

using vtx_t        = lev2::SVtxV16T16C16;
using vtxbuf_t     = lev2::DynamicVertexBuffer<vtx_t>;
using vtxbuf_ptr_t = std::shared_ptr<vtxbuf_t>;
vtxbuf_ptr_t get_vertexbuffer(lev2::Context* context);
lev2::freestyle_mtl_ptr_t hud_material(lev2::Context* context);

///////////////////////////////////////////////////////////////////////////////
struct HudLayoutGroup final : public ui::LayoutGroup {
  HudLayoutGroup();
  void onUpdateThreadTick(ui::updatedata_ptr_t updata);
  std::unordered_set<hudpanel_ptr_t> _hudpanels;
  std::map<char, int> _notemap;
  std::map<char, int> _handledkeymap;
  std::map<int, programInst*> _activenotes;
  lev2::orkezapp_ptr_t _ezapp;
  int _updcount    = 0;
  int _velocity    = 127;
  int _octaveshift = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct ProgramView final : public ui::Surface {
  ProgramView();
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void _doGpuInit(lev2::Context* pt) override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;
  ork::lev2::CTXBASE* _ctxbase = nullptr;
  int _updatecount             = 0;
  prgdata_constptr_t _curprogram;
  int _octaveshift = 0;
  int _velocity    = 127;
};

} // namespace ork::audio::singularity
