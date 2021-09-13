#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/util/triple_buffer.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
struct NaturalEnvSurf final : public ui::Surface {
  NaturalEnvSurf();
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void DoInit(lev2::Context* pt) override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;
  ork::lev2::CTXBASE* _ctxbase = nullptr;
  concurrent_triple_buffer<ScopeBuffer> _scopebuffers;
};
///////////////////////////////////////////////////////////////////////////////
signalscope_ptr_t create_natural_envelope_analyzer(hudvp_ptr_t vp) {
  auto hudpanel        = std::make_shared<HudPanel>();
  auto ratelevsurf     = std::make_shared<NaturalEnvSurf>();
  hudpanel->_uipanel   = std::make_shared<ui::Panel>("scope", 0, 0, 32, 32);
  hudpanel->_uisurface = ratelevsurf;
  hudpanel->_uipanel->setChild(hudpanel->_uisurface);
  hudpanel->_uipanel->snap();
  auto instrument              = std::make_shared<SignalScope>();
  instrument->_hudpanel        = hudpanel;
  instrument->_sink            = std::make_shared<ScopeSink>();
  instrument->_sink->_onupdate = [ratelevsurf](const ScopeSource* src) { //
    ratelevsurf->SetDirty();
  };
  vp->addChild(hudpanel->_uipanel);
  vp->_hudpanels.insert(hudpanel);
  return instrument;
}
///////////////////////////////////////////////////////////////////////////////
NaturalEnvSurf::NaturalEnvSurf() //
    : ui::Surface("Scope", 0, 0, 32, 32, fvec3(0.2, 0.1, 0.2), 1.0) {
}
///////////////////////////////////////////////////////////////////////////////
void NaturalEnvSurf::DoRePaintSurface(ui::drawevent_constptr_t drwev) {
  auto context = drwev->GetTarget();
  auto fbi     = context->FBI();
  auto syn     = synth::instance();
  auto vp      = syn->_hudvp;
  double time  = syn->_timeaccum;

  auto scopebuf = _scopebuffers.begin_pull();
  if (nullptr == scopebuf)
    return;
  const float* _samples = scopebuf->_samples;

  mRtGroup->_clearColor = _clearColor;
  fbi->rtGroupClear(mRtGroup);
  _scopebuffers.end_pull(scopebuf);
}

///////////////////////////////////////////////////////////////////////////////
void NaturalEnvSurf::DoInit(lev2::Context* pt) {
  _pickbuffer = new lev2::PickBuffer(this, pt, width(), height());
  _ctxbase    = pt->GetCtxBase();
}
///////////////////////////////////////////////////////////////////////////////
ui::HandlerResult NaturalEnvSurf::DoOnUiEvent(ui::event_constptr_t EV) {
  ui::HandlerResult ret(this);
  return ret;
}
///////////////////////////////////////////////////////////////////////////////

void DrawNaturalEnv(lev2::Context* context, const ItemDrawReq& EDR) {
  /*  const auto& R = EDR.rect;
    hudlines_t lines;

    float X2 = R.X1 + R.W;
    float Y2 = R.Y1 + R.H;

    const auto& KFIN              = EDR.s->_curhud_kframe;
    const auto& ENVFRAME          = EDR._data.get<envframe>();
    const NaturalEnvEnvData* ENVD = ENVFRAME._data;

    bool collsamp = EDR.shouldCollectSample();

    std::string sampname = sampname = "natural";

    assert(EDR.ienv < kmaxenvperlayer);

    auto lyr = EDR.l;

    /////////////////////////////////////////////////
    // collect hud samples
    /////////////////////////////////////////////////

    auto& HUDSAMPS = EDR.s->_hudsample_map[sampname];

    if (collsamp) {
      hudsample hs;
      hs._time  = EDR.s->_lnotetime;
      hs._value = ENVFRAME._value;
      HUDSAMPS.push_back(hs);
    }

    /////////////////////////////////////////////////
    // draw envelope values
    /////////////////////////////////////////////////

    float env_by  = R.Y1;
    float env_bx  = R.X1 + 70.0;
    int spcperseg = 120;
    float minval  = 0.0f;
    float maxval  = 1.00f;
    float range   = 1.0f;

      auto sample     = KFIN._kmregion->_sample;
      const auto& NES = sample->_natenv;
      int nseg        = NES.size();
      int icurseg     = ENVFRAME._curseg;

      // drawtext(context, "NATENV", env_bx, env_by, fontscale, 1, 0, 0);

      for (int i = 0; i < nseg; i++) {
        bool iscurseg = (i == icurseg);
        float r       = 1;
        float g       = iscurseg ? 0.5 : 1;

        auto hudstr = FormatString("seg%d", i);
        // drawtext(context, hudstr, env_bx + spcperseg * i, env_by + 20, fontscale, r, g, 0);
      }
      // drawtext(context, "dB/s\ntim", R.X1 + 15, env_by + 40, .45, 1, 1, 0);
      for (int i = 0; i < nseg; i++) {
        const auto& seg = NES[i];

        auto hudstr   = FormatString("%0.1f\n%d", seg._slope, int(seg._time));
        bool iscurseg = (i == icurseg);
        float r       = 1;
        float g       = iscurseg ? 0 : 1;
        float b       = iscurseg ? 0 : 1;
        // drawtext(context, hudstr, env_bx + spcperseg * i, env_by + 40, fontscale, r, g, 1);
      }


    ///////////////////////
    // natural env dB slopehull
    ///////////////////////

    if (useNENV) {
      fpx = fxb;
      fpy = fyb;
      for (int i = 0; i < HUDSAMPS.size(); i++) {
        const auto& hs = HUDSAMPS[i];
        auto p1        = fvec2(fpx, fpy);

        float val = hs._value;
        val       = 96.0 + linear_amp_ratio_to_decibel(val);
        if (val < 0.0f)
          val = 0.0f;
        val = val / 96.0f;

        fpx     = fxb + hs._time * (fw / ktime);
        fpy     = fyb - fh * val;
        auto p2 = fvec2(fpx, fpy);
        lines.push_back(HudLine{p1, p2, fvec3(1, 0, 0)});
      }
    }
    R.PopOrtho(context);*/
  /////////////////////////////////////////////////
  // drawHudLines(context, lines);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
