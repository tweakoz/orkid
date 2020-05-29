#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/util/triple_buffer.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
static const int MAXSAMPLES = 4096;
///////////////////////////////////////////////////////////////////////////////
struct RateLevelSurf final : public ui::Surface {
  RateLevelSurf();
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void DoInit(lev2::Context* pt) override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;
  ork::lev2::CTXBASE* _ctxbase = nullptr;
  std::vector<float> _samples;
  std::atomic<int> _curwritesample = 0;
  std::atomic<int> _curreadsample  = 0;
  int _cursegindex                 = 0;
  const RateLevelEnvData* _envdata = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
signalscope_ptr_t create_envelope_analyzer(hudvp_ptr_t vp) {
  auto hudpanel        = std::make_shared<HudPanel>();
  auto ratelevsurf     = std::make_shared<RateLevelSurf>();
  hudpanel->_uipanel   = std::make_shared<ui::Panel>("scope", 0, 0, 32, 32);
  hudpanel->_uisurface = ratelevsurf;
  hudpanel->_uipanel->setChild(hudpanel->_uisurface);
  hudpanel->_uipanel->snap();
  auto instrument              = std::make_shared<SignalScope>();
  instrument->_hudpanel        = hudpanel;
  instrument->_sink            = std::make_shared<ScopeSink>();
  instrument->_sink->_onupdate = [ratelevsurf](const ScopeSource& src) { //
    auto ratelev = dynamic_cast<const RateLevelEnvInst*>(src._controller);
    if (ratelev) {
      ratelevsurf->_cursegindex                   = ratelev->_segmentIndex;
      ratelevsurf->_envdata                       = ratelev->_data;
      int isample                                 = ratelevsurf->_curwritesample++;
      ratelevsurf->_samples[isample % MAXSAMPLES] = ratelev->_curval;
      ratelevsurf->_curreadsample                 = isample;
    }
    ratelevsurf->SetDirty();
  };
  vp->addChild(hudpanel->_uipanel);
  vp->_hudpanels.insert(hudpanel);
  return instrument;
}
///////////////////////////////////////////////////////////////////////////////
RateLevelSurf::RateLevelSurf() //
    : ui::Surface("Scope", 0, 0, 32, 32, fvec3(0.2, 0.1, 0.2), 1.0) {
  _samples.resize(MAXSAMPLES); //
}
///////////////////////////////////////////////////////////////////////////////
void RateLevelSurf::DoRePaintSurface(ui::drawevent_constptr_t drwev) {
  auto context = drwev->GetTarget();
  auto fbi     = context->FBI();
  auto syn     = synth::instance();
  auto vp      = syn->_hudvp;
  double time  = syn->_timeaccum;

  // auto scopebuf = _scopebuffers.begin_pull();
  // if (nullptr == scopebuf)
  // return;
  if (nullptr == _envdata)
    return;

  // const float* _samples = scopebuf->_samples;

  mRtGroup->_clearColor = _clearColor;
  fbi->rtGroupClear(mRtGroup);

  hudlines_t lines;

  std::string sampname = FormatString("env-%s", _envdata->_name.c_str());

  /////////////////////////////////////////////////
  // draw envelope values
  /////////////////////////////////////////////////

  if (_envdata) {

    //////////////////////
    // draw envelope name
    //////////////////////

    drawtext(
        this, //
        context,
        _envdata->_name,
        0,
        0,
        fontscale,
        1,
        0,
        .5);

    //////////////////////

    float minval = 0.0f;
    float maxval = 1.00f;

    auto& AE = _envdata->_segments;
    ///////////////////////////////////////
    for (int i = 0; i < AE.size(); i++) {
      auto seg = AE[i];
      auto lev = seg._level;
      if (lev > maxval)
        maxval = lev;
      if (lev < minval)
        minval = lev;
    }
    float range = maxval - minval;
    ///////////////////////////////////////

    int spcperseg = 120;

    drawtext(
        this, //
        context,
        "name       lev        tim",
        16,
        24,
        fontscale,
        1,  // r
        1,  // g
        0); // b

    ///////////////////////////////////////////////////
    // draw segment names/times/level text
    ///////////////////////////////////////////////////
    for (int i = 0; i < AE.size(); i++) {
      bool iscurseg = (i == _cursegindex);
      int y         = 48 + i * 20;

      drawtext(
          this, //
          context,
          _envdata->_segmentNames[i],
          16,
          y,
          fontscale,
          1,                  // r
          iscurseg ? 0.5 : 1, // g
          0);                 // b

      drawtext(
          this, //
          context,
          FormatString("%0.2f     %0.3f", AE[i]._level, AE[i]._time),
          108,
          y,
          fontscale,
          1,                 // r
          iscurseg ? 0 : 1,  // g
          iscurseg ? 0 : 1); // b
    }
  }

  /////////////////////////////////////////////////
  // draw envelope segments
  /////////////////////////////////////////////////

  ///////////////////////
  // draw border
  ///////////////////////

  const float ktime = 4.0f;

  bool bipolar = _envdata ? _envdata->_bipolar : false;

  ///////////////////////
  // draw grid, origin
  ///////////////////////

  if (bipolar) {
    float fy = miH / 2.0;
    float x1 = 64;
    float x2 = miW;
    lines.push_back(HudLine{fvec2(x1, fy), fvec2(x2, fy), fvec3(.5, .2, .5)});
  }

  ///////////////////////
  // from hud samples
  ///////////////////////

  fvec2 scale(miW, -miH / 2);
  fvec2 bias(0.0, miH);

  int off          = _curreadsample % MAXSAMPLES;
  int j            = (MAXSAMPLES + off - 0) % MAXSAMPLES;
  float prevsample = _samples[j];
  for (int i = 0; i < MAXSAMPLES; i++) {
    float fi     = float(MAXSAMPLES - i) / float(MAXSAMPLES);
    float fni    = fi - 1.0f / float(MAXSAMPLES);
    int off      = _curreadsample % MAXSAMPLES;
    int j        = (MAXSAMPLES + off - i) % MAXSAMPLES;
    float sample = _samples[j];
    auto p1      = fvec2(fi, prevsample) * scale + bias;
    auto p2      = fvec2(fni, sample) * scale + bias;
    lines.push_back(HudLine{p1, p2, fvec3(1, 1, 1)});
    prevsample = sample;
  }

  /////////////////////////////////////////////////
  drawHudLines(this, context, lines);
}

///////////////////////////////////////////////////////////////////////////////
void RateLevelSurf::DoInit(lev2::Context* pt) {
  _pickbuffer = new lev2::PickBuffer(this, pt, miW, miH);
  _ctxbase    = pt->GetCtxBase();
}
///////////////////////////////////////////////////////////////////////////////
ui::HandlerResult RateLevelSurf::DoOnUiEvent(ui::event_constptr_t EV) {
  ui::HandlerResult ret(this);
  return ret;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
