////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/aud/singularity/synth.h>
#include <ork/kernel/svariant.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include "synthdata.h"
#include "synth.h"
#include "fft.h"
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/surface.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/panel.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ezapp.h>

namespace ork::audio::singularity {

float hud_contentscale();
int hud_lineheight();

///////////////////////////////////////////////////////////////////////////////

using vtx_t        = lev2::SVtxV16T16C16;
using vtxbuf_t     = lev2::DynamicVertexBuffer<vtx_t>;
using vtxbuf_ptr_t = std::shared_ptr<vtxbuf_t>;
vtxbuf_ptr_t get_vertexbuffer(lev2::Context* context);
lev2::freestyle_mtl_ptr_t hud_material(lev2::Context* context);

///////////////////////////////////////////////////////////////////////////////

typedef ork::svar1024_t svar_t;
void drawtext(
    ui::Surface* surface, //
    lev2::Context* ctx,   //
    const std::string& str,
    float x,
    float y,
    float scale,
    float r,
    float g,
    float b);

struct HudLine {

  fvec2 _from;
  fvec2 _to;
  fvec3 _color;
};
using hudlines_t = std::vector<HudLine>;

void drawHudLines(
    ui::Surface* surface,   //
    lev2::Context* context, //
    const hudlines_t& lines);

///////////////////////////////////////////////////////////////////////////////

struct Rect {
  int X1;
  int Y1;
  int W;
  int H;
  int VPW;
  int VPH;

  void PushOrtho(lev2::Context* context) const;
  void PopOrtho(lev2::Context* context) const;
};

///////////////////////////////////////////////////////////////////////////////

struct ItemDrawReq {
  synth* s;
  int ldi;
  int ienv;
  lyrdata_constptr_t ld;
  const Layer* l;
  ork::svar256_t _data;
  Rect rect;

  bool shouldCollectSample() const {
    return ((s->_lnoteframe >> 3) % 3 == 0);
  }
};

///////////////////////////////////////////////////////////////////////////////
struct HudPanel {
  void setRect(int iX, int iY, int iW, int iH, bool snap = false);
  ui::anchor::layout_ptr_t _panelLayout;
  ui::layoutgroup_ptr_t _layoutgroup;
  ui::panel_ptr_t _uipanel;
  ui::surface_ptr_t _uisurface;
};
///////////////////////////////////////////////////////////////////////////////
struct ScopeBuffer {
  ScopeBuffer(int tbufindex = 0);
  float _samples[koscopelength];
  int _tbindex; // triple buffer index
};
///////////////////////////////////////////////////////////////////////////////
struct ScopeSource {
  void updateMono(int numframes, const float* mono, bool notifysinks = true);
  void updateStereo(int numframes, const float* left, const float* right, bool notifysinks = true);
  void updateController(const ControllerInst* controller);

  void notifySinksUpdated();
  void notifySinksKeyOn(KeyOnInfo& koi);
  void notifySinksKeyOff();
  void connect(scopesink_ptr_t sink);
  void disconnect(scopesink_ptr_t sink);
  std::unordered_set<scopesink_ptr_t> _sinks;
  ScopeBuffer _scopebuffer;
  const ControllerInst* _controller = nullptr;
  int _dspchannel                   = 0;
  int _writehead                    = 0;
  void* _cursrcimpl                 = nullptr;
};
struct ScopeSink {
  void sourceUpdated(const ScopeSource* src);
  void sourceKeyOn(const ScopeSource* src, KeyOnInfo& koi);
  void sourceKeyOff(const ScopeSource* src);
  std::function<void(const ScopeSource*)> _onupdate                = nullptr;
  std::function<void(const ScopeSource*, KeyOnInfo& koi)> _onkeyon = nullptr;
  std::function<void(const ScopeSource*)> _onkeyoff                = nullptr;
};
struct SignalScope {
  void setRect(int iX, int iY, int iW, int iH, bool snap = false);
  ///////////////////////////////////////////////////////////////////////////
  template <typename T, typename... A> void setProperty(const varmap::key_t& key, A&&... args) {
    _vars.makeValueForKey<T>(key, std::forward<A>(args)...);
  }
  ///////////////////////////////////////////////////////////////////////////
  template <typename T> const T& property(const varmap::key_t& key) const {
    return _vars.typedValueForKey<T>(key).value();
  }
  ///////////////////////////////////////////////////////////////////////////
  hudpanel_ptr_t _hudpanel;
  scopesink_ptr_t _sink;
  varmap::VarMap _vars;
};
///////////////////////////////////////////////////////////////////////////////
signalscope_ptr_t create_oscilloscope(
    hudvp_ptr_t vp, //
    const ui::anchor::Bounds& bounds,
    std::string named = "");
signalscope_ptr_t create_spectrumanalyzer(
    hudvp_ptr_t vp, //
    const ui::anchor::Bounds& bounds,
    std::string named = "");
signalscope_ptr_t create_envelope_analyzer(
    hudvp_ptr_t vp, //
    const ui::anchor::Bounds& bounds,
    std::string named = "");
hudpanel_ptr_t createProgramView(
    hudvp_ptr_t vp, //
    const ui::anchor::Bounds& bounds,
    std::string named = "");
hudpanel_ptr_t createProfilerView(
    hudvp_ptr_t vp, //
    const ui::anchor::Bounds& bounds,
    std::string named = "");
hudpanel_ptr_t createEnvYmEditView(
    hudvp_ptr_t vp, //
    std::string named,
    fvec4 color,
    controllerdata_ptr_t envdata,
    const ui::anchor::Bounds& bounds);
hudpanel_ptr_t createPmxEditView(
    hudvp_ptr_t vp, //
    std::string named,
    fvec4 color,
    dspblkdata_ptr_t dbdata,
    const ui::anchor::Bounds& bounds);
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

void DrawEnv(lev2::Context* context, const ItemDrawReq& EDR);
void DrawAsr(lev2::Context* context, const ItemDrawReq& EDR);
void DrawLfo(lev2::Context* context, const ItemDrawReq& EDR);
void DrawFun(lev2::Context* context, const ItemDrawReq& EDR);
float FUNH(float vpw, float vph);
float FUNW(float vpw, float vph);
float FUNX(float vpw, float vph);
float ENVW(float vpw, float vph);
float ENVH(float vpw, float vph);
float ENVX(float vpw, float vph);
float DSPW(float vpw, float vph);
float DSPX(float vpw, float vph);
void DrawBorder(lev2::Context* context, int X1, int Y1, int X2, int Y2, int color = 0);

///////////////////////////////////////////////////////////////////////////////

static const float fontscale = 0.40;

} // namespace ork::audio::singularity
