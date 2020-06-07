#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/util/triple_buffer.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
struct RateLevelSurf final : public ui::Surface {
  RateLevelSurf();
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void DoInit(lev2::Context* pt) override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;
  void setTimeWidth(float w) {
    _timewidthsamples = int(samplesPerControlPeriod() * w);
    if (_timewidthsamples != _samples.size()) {
      _samples.resize(_timewidthsamples);
      memset(_samples.data(), 0, _timewidthsamples * sizeof(float));
    }
  }
  ork::lev2::CTXBASE* _ctxbase = nullptr;
  std::vector<float> _samples;
  std::atomic<int> _curwritesample = 0;
  std::atomic<int> _curreadsample  = 0;
  int _cursegindex                 = 0;
  int _updatecount                 = 0;
  const RateLevelEnvData* _envdata = nullptr;
  const RateLevelEnvInst* _envinst = nullptr;
  signalscope_ptr_t _instrument;
  int _timewidthsamples = 0;
};
///////////////////////////////////////////////////////////////////////////////
signalscope_ptr_t create_envelope_analyzer(hudvp_ptr_t vp) {
  auto hudpanel        = std::make_shared<HudPanel>();
  auto ratelevsurf     = std::make_shared<RateLevelSurf>();
  hudpanel->_uipanel   = std::make_shared<ui::Panel>("scope", 0, 0, 32, 32);
  hudpanel->_uisurface = ratelevsurf;
  hudpanel->_uipanel->setChild(hudpanel->_uisurface);
  hudpanel->_uipanel->snap();
  auto instrument = std::make_shared<SignalScope>();
  float timew     = 15.0f;
  instrument->setProperty<float>("timewidth", timew);
  ratelevsurf->setTimeWidth(timew);
  ratelevsurf->_instrument    = instrument;
  instrument->_hudpanel       = hudpanel;
  instrument->_sink           = std::make_shared<ScopeSink>();
  instrument->_sink->_onkeyon = [ratelevsurf](const ScopeSource* src, KeyOnInfo& koi) { //
    auto ratelev = dynamic_cast<const RateLevelEnvInst*>(src->_controller);
    if (ratelev) { // attach
      ratelevsurf->_updatecount    = 0;
      ratelevsurf->_curwritesample = 0;
      ratelevsurf->_curreadsample  = 0;
      ratelevsurf->_envdata        = ratelev->_data;
      ratelevsurf->_envinst        = ratelev;
    }
  };
  instrument->_sink->_onupdate = [ratelevsurf](const ScopeSource* src) { //
    auto ratelev = dynamic_cast<const RateLevelEnvInst*>(src->_controller);
    if (ratelev) {
      ///////////////////////////////
      // single out attached envelope
      ///////////////////////////////
      if (ratelevsurf->_envinst == ratelev) {
        int maxsamps = ratelevsurf->_timewidthsamples;
        switch (ratelev->_state) {
          case 0:
          case 1:
          case 2:
          case 3: {
            ratelevsurf->_cursegindex                 = ratelev->_segmentIndex;
            ratelevsurf->_envdata                     = ratelev->_data;
            int isample                               = ratelevsurf->_curwritesample++;
            ratelevsurf->_samples[isample % maxsamps] = ratelev->_curval;
            ratelevsurf->_curreadsample               = isample;
            break;
          }
          case 4:
          default: // detach
            int isample                               = ratelevsurf->_curwritesample++;
            ratelevsurf->_samples[isample % maxsamps] = ratelev->_curval;
            ratelevsurf->_curreadsample               = isample;
            ratelevsurf->_envdata                     = nullptr;
            ratelevsurf->_envinst                     = nullptr;
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
}
///////////////////////////////////////////////////////////////////////////////
void RateLevelSurf::DoRePaintSurface(ui::drawevent_constptr_t drwev) {
  auto context = drwev->GetTarget();
  auto fbi     = context->FBI();
  auto syn     = synth::instance();
  auto vp      = syn->_hudvp;
  double time  = syn->_timeaccum;

  float timewidth = _instrument->property<float>("timewidth");
  setTimeWidth(timewidth);

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

  int segtextbasex = 128;

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

  drawtext(
      this, //
      context,
      "name       level    time      shape",
      segtextbasex,
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
        segtextbasex,
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
        segtextbasex + 88,
        y,
        fontscale,
        1,                 // r
        iscurseg ? 0 : 1,  // g
        iscurseg ? 0 : 1); // b
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
  for (int i = 0; i < _timewidthsamples; i++) {
    float fi  = float(_timewidthsamples - i) / float(_timewidthsamples);
    float fni = fi - 1.0f / float(_timewidthsamples);
    if ((i % space) == 0) {
      float x = (fi + fni) * 0.5f;
      lines.push_back(HudLine{
          fvec2(x, 0.0) * scale + bias, //
          fvec2(x, 1.0) * scale + bias,
          fvec3(0.4, 0.3, 0.4)});
    }
  }

  // printf("envmin<%g> envmax<%g> envrange<%g>\n", envmin, envmax, envrange);
  ///////////////////////
  // from hud samples
  ///////////////////////

  int basereadoffset = _curreadsample % _timewidthsamples;
  auto readindex     = [this, basereadoffset](int i) -> int { //
    return (_timewidthsamples + basereadoffset - i) % _timewidthsamples;
  };
  auto normalize = [this](float inp) -> float { //
    return (inp - _envinst->_clampmin) / _envinst->_clamprange;
  };
  float prevsample = normalize(_samples[readindex(0)]);

  for (int i = 0; i < _timewidthsamples; i++) {
    //////////////////////////////
    // scanning right to left!
    // i==0 at right
    // i==_timewidthsamples at left
    //////////////////////////////
    float fright = float(_timewidthsamples - i) / float(_timewidthsamples);
    float fleft  = fright - 1.0f / float(_timewidthsamples);
    ////////////////////////////////////
    // dont read past provided samples
    ////////////////////////////////////
    float rawsample = (i < _curreadsample) ? _samples[readindex(i)] : 0.0f;
    ////////////////////////////////////
    float mappedsample = normalize(rawsample);
    auto p_right       = fvec2(fright, prevsample) * scale + bias;
    auto p_left        = fvec2(fleft, mappedsample) * scale + bias;
    lines.push_back(HudLine{
        p_right, //
        p_left,
        fvec3(1, 1, 1)});
    prevsample = mappedsample;
  }

  /////////////////////////////////////////////////
  // draw misc envelope state
  /////////////////////////////////////////////////

  int miscx   = segtextbasex + 384;
  int miscy   = 0;
  int miscspc = 16;

  drawtext(
      this, //
      context,
      FormatString("sustainseg: %d", _envdata->_sustainSegment),
      miscx,
      (miscy++) * miscspc,
      fontscale,
      1,  // r
      1,  // g
      1); // b

  drawtext(
      this, //
      context,
      FormatString("releaseseg: %d", _envdata->_releaseSegment),
      miscx,
      (miscy++) * miscspc,
      fontscale,
      1,  // r
      1,  // g
      1); // b

  drawtext(
      this, //
      context,
      FormatString("isAmpEnv: %d", int(_envdata->_ampenv)),
      miscx,
      (miscy++) * miscspc,
      fontscale,
      1,  // r
      1,  // g
      1); // b

  drawtext(
      this, //
      context,
      FormatString("isBipolar: %d", int(_envdata->_bipolar)),
      miscx,
      (miscy++) * miscspc,
      fontscale,
      1,  // r
      1,  // g
      1); // b

  drawtext(
      this, //
      context,
      FormatString("state: %d", _envinst->_state),
      miscx,
      (miscy++) * miscspc,
      fontscale,
      1,  // r
      1,  // g
      1); // b

  drawtext(
      this, //
      context,
      FormatString("curshape: %g", _envinst->_curshape),
      miscx,
      (miscy++) * miscspc,
      fontscale,
      1,  // r
      1,  // g
      1); // b

  drawtext(
      this, //
      context,
      FormatString("curval: %g", _envinst->_curval),
      miscx,
      (miscy++) * miscspc,
      fontscale,
      1,  // r
      1,  // g
      1); // b

  drawtext(
      this, //
      context,
      FormatString("lerpindex: %g", _envinst->_lerpindex),
      miscx,
      (miscy++) * miscspc,
      fontscale,
      1,  // r
      1,  // g
      1); // b

  drawtext(
      this, //
      context,
      FormatString("rawtime: %g", _envinst->_rawtime),
      miscx,
      (miscy++) * miscspc,
      fontscale,
      1,  // r
      1,  // g
      1); // b
  drawtext(
      this, //
      context,
      FormatString("rawlevel: %g", _envinst->_rawdestval),
      miscx,
      (miscy++) * miscspc,
      fontscale,
      1,  // r
      1,  // g
      1); // b

  drawtext(
      this, //
      context,
      FormatString("adjtime: %g", _envinst->_rawtime),
      miscx,
      (miscy++) * miscspc,
      fontscale,
      1,  // r
      1,  // g
      1); // b
  drawtext(
      this, //
      context,
      FormatString("adjlevel: %g", _envinst->_rawdestval),
      miscx,
      (miscy++) * miscspc,
      fontscale,
      1,  // r
      1,  // g
      1); // b

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
