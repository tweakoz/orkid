#include <ork/lev2/aud/singularity/hud.h>

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
struct SpectraSurf final : public ui::Surface {
  SpectraSurf();
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void DoInit(lev2::Context* pt) override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;
  ork::lev2::CTXBASE* _ctxbase = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
HudPanel create_spectrumanalyzer() {
  HudPanel rval;
  rval._panel   = std::make_shared<ui::Panel>("spectra", 0, 256, 256, 256);
  rval._surface = std::make_shared<SpectraSurf>();
  rval._panel->setChild(rval._surface);
  rval._panel->snap();
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
static const int inumframes           = koscopelength;
static constexpr size_t fftoversample = 16;
static constexpr size_t DOWNSHIFT     = bitsToHold<fftoversample>() - 1;
static const size_t fftSize           = inumframes * fftoversample; // Needs to be power of 2!
static constexpr int KMAXNOTE         = 141;
static auto lab1color                 = fvec3(0.5, 0.5, 0.7);
static auto lab2color                 = fvec3(0.5, 0.5, 0.9);
static auto gridcolor                 = fvec3(.1, .2, .4);
static auto plotcolor                 = fvec3(.2, .4, 5);
///////////////////////////////////////////////////////////////////////////////
struct FFT_Context {
  FFT_Context()
      : input(fftSize, 0)
      , re(audiofft::AudioFFT::ComplexSize(fftSize))
      , im(audiofft::AudioFFT::ComplexSize(fftSize)) {
  }
  void compute() {
    fft.init(fftSize);
    fft.fft(input.data(), re.data(), im.data());
  }
  ///////////////////////////////////////////////////////////////////////////////
  audiofft::AudioFFT fft;
  std::vector<float> input;
  std::vector<float> re;
  std::vector<float> im;
};
///////////////////////////////////////////////////////////////////////////////
static FFT_Context& _fftcontext() {
  static FFT_Context _ctx;
  return _ctx;
}
///////////////////////////////////////////////////////////////////////////////
static float* initfftsmoothingbuffer() {
  float* buffer = new float[fftSize];
  for (int i = 0; i < fftSize; i++) {
    buffer[i] = 0.0f;
  }
  return buffer;
}
///////////////////////////////////////////////////////////////////////////////
static float* fftsmoothingbuffer() {
  static float* buffer = initfftsmoothingbuffer();
  return buffer;
}
///////////////////////////////////////////////////////////////////////////////
SpectraSurf::SpectraSurf() //
    : ui::Surface("Spectra", 0, 0, 128, 128, fvec3(), 1.0) {
}
///////////////////////////////////////////////////////////////////////////////
void SpectraSurf::DoRePaintSurface(ui::drawevent_constptr_t drwev) {
  auto context         = drwev->GetTarget();
  auto syn             = synth::instance();
  auto vp              = syn->_hudvp;
  const float* samples = syn->_oscopebuffer;

  hudlines_t lines;

  const float ANA_X1 = 32;
  const float ANA_Y1 = 32;
  const float ANA_W  = miW - 64;
  const float ANA_H  = miH - 64;
  const float ANA_X2 = miW;
  const float ANA_Y2 = ANA_H;
  const float ANA_HH = ANA_H * 0.5;
  const float ANA_CY = ANA_Y1 + ANA_HH;

  //////////////////////////////
  // fill in FFT buffer using window func
  //////////////////////////////

  auto& fftcontext = _fftcontext();

  for (int i = 0; i < fftSize; i++) {
    float s             = samples[i >> DOWNSHIFT];
    float win_num       = pi2 * float(i);
    float win_den       = fftSize - 1;
    float win           = 0.5f * (1 - cosf(win_num / win_den));
    float s2            = samples[i >> DOWNSHIFT];
    fftcontext.input[i] = s2 * win;
  }

  fftcontext.compute(); // do the FFT

  //////////////////////////////
  // map fft-bin -> Y
  //////////////////////////////

  auto mapDecibels = [&](float re, float im) -> float {
    float mag = re * re + im * im;
    float dB  = 10.0f * log_base(10.0f, mag) - 6.0f;
    return dB;
  };
  auto mapFFTY = [&](float dbin) -> float {
    float dbY = (dbin + 96.0f) / 132.0f;
    float y   = ANA_Y2 - dbY * ANA_H;
    if (y > ANA_Y2)
      y = ANA_Y2;
    return y;
  };
  auto mapFFTX = [&](float frq) -> float {
    float midinote    = frequency_to_midi_note(frq);
    const float kfinv = 1.0f / 24000.0f;
    float normf       = (frq)*kfinv;
    float x           = powf(normf, 0.3);
    return ANA_X1 - 16 + ANA_W * x;
  };

  //////////////////////////////
  // draw grid
  //////////////////////////////

  for (int dB = 36; dB >= -96; dB -= 12) {
    float f1 = midi_note_to_frequency(0);
    float f2 = midi_note_to_frequency(KMAXNOTE);
    lines.push_back(HudLine{
        fvec2(mapFFTX(f1), mapFFTY(dB)), //
        fvec2(mapFFTX(f2), mapFFTY(dB)),
        gridcolor}); // dB grid
  }

  for (int note = 0; note <= KMAXNOTE + 11; note += 12) {
    if (note > KMAXNOTE)
      note = KMAXNOTE;

    float f = midi_note_to_frequency(note);
    float x = mapFFTX(f);
    lines.push_back(HudLine{
        fvec2(x, mapFFTY(36)), //
        fvec2(x, mapFFTY(-96)),
        lab2color * 0.5}); // vertical grid
  }

  ////////////////////////////////////////
  // draw amplitude labels
  ////////////////////////////////////////

  for (int i = 36; i >= -96; i -= 12) {
    float db0 = i;
    float y   = mapFFTY(db0);
    drawtext(
        this,
        context, //
        FormatString("%g dB", db0),
        ANA_X1 - 22,
        y - hud_lineheight() / 2,
        fontscale,
        gridcolor.x,
        gridcolor.y,
        gridcolor.z);
  }

  ////////////////////////////////////////
  // draw frequency labels
  ////////////////////////////////////////

  int ycursor = ANA_Y2 + hud_lineheight();

  drawtext(
      this,
      context, //
      "midinote",
      ANA_X1 - 32,
      ycursor,
      fontscale,
      lab1color.x,
      lab1color.y,
      lab1color.z);

  ycursor += hud_lineheight();

  drawtext(
      this,
      context, //
      "frequency",
      ANA_X1 - 32,
      ycursor,
      fontscale,
      lab2color.x,
      lab2color.y,
      lab2color.z);

  for (int note = 0; note <= KMAXNOTE + 11; note += 12) {
    if (note > KMAXNOTE)
      note = KMAXNOTE;
    float f = midi_note_to_frequency(note);
    float x = mapFFTX(f);
    ycursor = ANA_Y2 + hud_lineheight();

    drawtext(
        this,
        context, //
        FormatString("%d", note),
        x - 8,
        ycursor,
        fontscale,
        lab1color.x,
        lab1color.y,
        lab1color.z);

    ycursor += hud_lineheight();

    drawtext(
        this,
        context, //
        FormatString("%d", int(f)),
        x - 8,
        ycursor,
        fontscale,
        lab2color.x,
        lab2color.y,
        lab2color.z);
  }

  //////////////////////////////
  // spectral plot
  //////////////////////////////

  float dB = mapDecibels(fftcontext.re[0], fftcontext.im[0]);
  float x1 = mapFFTX(8);
  float y1 = mapFFTY(dB);

  auto fftsmoothbuf = fftsmoothingbuffer();
  bool done         = false;
  int i             = 0;
  while (not done) {

    ///////////////////////////////////////////////
    // average fft samples to smooth out spectral plot
    ///////////////////////////////////////////////
    float dB = mapDecibels(fftcontext.re[i], fftcontext.im[i]); // instantaneous sample
    fftsmoothbuf[i] += dB * 0.03 + 0.0001f;                     // accumulate
    fftsmoothbuf[i] *= 0.99f;                                   // dampen
    dB = fftsmoothbuf[i] * 0.23;                                // scale to unity (todo: find coef)
    ///////////////////////////////////////////////

    // printf( "dB<%f>\n", dB);
    float fi = float(i) / float(fftSize);

    float frq = fi * getSampleRate() * float(fftoversample);
    float x2  = mapFFTX(frq);
    float y2  = mapFFTY(dB - 12);

    if (frq >= 8.0f)
      lines.push_back(HudLine{
          fvec2(x1, y1), //
          fvec2(x2, y2),
          plotcolor}); // spectral plot

    x1 = x2;
    y1 = y2;
    i++;

    done = frq >= 24000.0f; // were done at 1/2 nyquist
  }
  // freqbins[index] = complex_t(0,0);

  ////////////////////////////////////////

  drawHudLines(this, context, lines);
}
///////////////////////////////////////////////////////////////////////////////
void SpectraSurf::DoInit(lev2::Context* pt) {
}
///////////////////////////////////////////////////////////////////////////////
ui::HandlerResult SpectraSurf::DoOnUiEvent(ui::event_constptr_t EV) {
  ui::HandlerResult ret(this);
  return ret;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
