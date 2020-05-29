#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/util/triple_buffer.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
static const int MAXSAMPLES = 1500 * 15;
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
  int _updatecount                 = 0;
  const RateLevelEnvData* _envdata = nullptr;
  const RateLevelEnvInst* _envinst = nullptr;
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
      if (0 == ratelev->_updatecount) { // attach
        ratelevsurf->_updatecount    = 0;
        ratelevsurf->_curwritesample = 0;
        ratelevsurf->_curreadsample  = 0;
        ratelevsurf->_envdata        = ratelev->_data;
        ratelevsurf->_envinst        = ratelev;
      }
      ///////////////////////////////
      // single out attached envelope
      ///////////////////////////////
      if (ratelevsurf->_envinst == ratelev) {
        switch (ratelev->_state) {
          case 0:
          case 1:
          case 2:
          case 3: {
            ratelevsurf->_cursegindex                   = ratelev->_segmentIndex;
            ratelevsurf->_envdata                       = ratelev->_data;
            int isample                                 = ratelevsurf->_curwritesample++;
            ratelevsurf->_samples[isample % MAXSAMPLES] = ratelev->_curval;
            ratelevsurf->_curreadsample                 = isample;
            break;
          }
          case 4:
          default: // detach
            int isample                                 = ratelevsurf->_curwritesample++;
            ratelevsurf->_samples[isample % MAXSAMPLES] = ratelev->_curval;
            ratelevsurf->_curreadsample                 = isample;
            ratelevsurf->_envdata                       = nullptr;
            ratelevsurf->_envinst                       = nullptr;
        }
      }
      ratelevsurf->_updatecount++;
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

    int segbasex = 128;

    drawtext(
        this, //
        context,
        "name       lev        tim      shape",
        segbasex,
        0,
        fontscale,
        1,  // r
        1,  // g
        0); // b

    ///////////////////////////////////////////////////
    // draw segment names/times/level text
    ///////////////////////////////////////////////////
    for (int i = 0; i < AE.size(); i++) {
      bool iscurseg = (i == _cursegindex);
      int y         = 16 + i * 16;

      drawtext(
          this, //
          context,
          _envdata->_segmentNames[i],
          segbasex,
          y,
          fontscale,
          1,                  // r
          iscurseg ? 0.5 : 1, // g
          0);                 // b

      drawtext(
          this, //
          context,
          FormatString(
              "%0.2f     %0.3f     %0.3f", //
              AE[i]._level,
              AE[i]._time,
              AE[i]._shape),
          segbasex + 88,
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

  fvec2 scale(miW, -miH / 2);
  fvec2 bias(0.0, miH);

  ///////////////////////
  // timegrid lines
  ///////////////////////

  float controlframerate = getSampleRate() / float(frames_per_controlpass);
  int space              = int(controlframerate);
  for (int i = 0; i < MAXSAMPLES; i++) {
    float fi  = float(MAXSAMPLES - i) / float(MAXSAMPLES);
    float fni = fi - 1.0f / float(MAXSAMPLES);
    if ((i % space) == 0) {
      float x = (fi + fni) * 0.5f;
      lines.push_back(HudLine{
          fvec2(x, 0.0) * scale + bias, //
          fvec2(x, 1.0) * scale + bias,
          fvec3(0.4, 0.3, 0.4)});
    }
  }

  ///////////////////////
  // from hud samples
  ///////////////////////

  int off          = _curreadsample % MAXSAMPLES;
  int j            = (MAXSAMPLES + off) % MAXSAMPLES;
  float prevsample = _samples[j];

  for (int i = 0; i < MAXSAMPLES; i++) {
    float fi  = float(MAXSAMPLES - i) / float(MAXSAMPLES);
    float fni = fi - 1.0f / float(MAXSAMPLES);
    int off   = _curreadsample % MAXSAMPLES;
    int j     = (MAXSAMPLES + off - i) % MAXSAMPLES;

    float sample = (i < _updatecount) ? _samples[j] : 0.0f;
    auto p1      = fvec2(fi, prevsample) * scale + bias;
    auto p2      = fvec2(fni, sample) * scale + bias;
    lines.push_back(HudLine{
        p1, //
        p2,
        fvec3(1, 1, 1)});
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
