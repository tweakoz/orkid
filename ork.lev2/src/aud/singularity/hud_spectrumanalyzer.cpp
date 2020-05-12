#include <ork/lev2/aud/singularity/hud.h>

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

float* initfftbuffer() {
  float* buffer = new float[koscopelength / 2];
  for (int i = 0; i < koscopelength / 2; i++) {
    buffer[i] = 0.0f;
  }
  return buffer;
}

float* fftbuffer() {
  static float* buffer = initfftbuffer();
  return buffer;
}

void DrawSpectra(
    lev2::Context* context, //
    const hudaframe& HAF,
    const float* samples,
    fvec2 xy,
    fvec2 wh) { //
  hudlines_t lines;

  int inumframes = koscopelength;

  const float ANA_X1 = xy.x;
  const float ANA_Y1 = xy.y;
  const float ANA_W  = wh.x;
  const float ANA_H  = wh.y;
  const float ANA_X2 = (xy + wh).x;
  const float ANA_Y2 = (xy + wh).y;
  const float ANA_HH = ANA_H * 0.5;
  const float ANA_CY = ANA_Y1 + ANA_HH;

  //////////////////////////////
  // fill in FFT buffer using window func
  //////////////////////////////

  const size_t fftSize = inumframes; // Needs to be power of 2!

  std::vector<float> input(fftSize, 0.0f);
  std::vector<float> re(audiofft::AudioFFT::ComplexSize(fftSize));
  std::vector<float> im(audiofft::AudioFFT::ComplexSize(fftSize));
  std::vector<float> output(fftSize);

  for (int i = 0; i < inumframes; i++) {
    float s       = samples[i];
    float win_num = pi2 * float(i);
    float win_den = inumframes - 1;
    float win     = 0.5f * (1 - cosf(win_num / win_den));
    float s2      = samples[i];
    input[i]      = s2 * win;
  }

  //////////////////////////////
  // do the FFT
  //////////////////////////////

  audiofft::AudioFFT fft;
  fft.init(fftSize);
  fft.fft(input.data(), re.data(), im.data());

  //////////////////////////////
  // map bin -> Y
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

  //////////////////////////////
  // draw grid
  //////////////////////////////

  auto gridcolor = fvec3(.1, .3, .4);

  for (int dB = 36; dB >= -96; dB -= 12) {
    lines.push_back(HudLine{
        fvec2(ANA_X1 + 32, mapFFTY(dB)), //
        fvec2(ANA_X2 + 32, mapFFTY(dB)),
        gridcolor}); // dB grid
  }

  constexpr int KMAXNOTE     = 12 * 11;
  constexpr float KFIMAXNOTE = 1.0f / float(KMAXNOTE);
  for (int note = 0; note <= KMAXNOTE; note += 12) {
    float x = 32 + ANA_X1 + ANA_W * float(note) * KFIMAXNOTE;
    lines.push_back(HudLine{
        fvec2(x, ANA_Y1), //
        fvec2(x, ANA_Y2),
        gridcolor}); // vertical grid
  }

  ////////////////////////////////////////
  // draw amplitude labels
  ////////////////////////////////////////

  for (int i = 36; i >= -96; i -= 12) {
    float db0 = i;
    float y   = mapFFTY(db0);
    drawtext(
        context, //
        FormatString("%g dB", db0),
        ANA_X1 - 22,
        y - 4,
        fontscale,
        gridcolor.x,
        gridcolor.y,
        gridcolor.z);
  }

  ////////////////////////////////////////
  // draw frequency labels
  ////////////////////////////////////////

  for (int note = 0; note < KMAXNOTE; note += 12) {
    float x = ANA_X1 + ANA_W * float(note) * KFIMAXNOTE;
    float f = midi_note_to_frequency(note);
    drawtext(
        context, //
        FormatString("  midi\n   %d\n(%d hz)", note, int(f)),
        x,
        ANA_Y2 + 30,
        fontscale,
        gridcolor.x,
        gridcolor.y,
        gridcolor.z);
  }

  //////////////////////////////
  // spectral plot
  //////////////////////////////

  float dB = mapDecibels(re[0], im[0]);
  float x1 = ANA_X1; // + 500 + ANA_W * float(0) / float(inumframes);
  float y1 = mapFFTY(dB);

  auto fftbuf = fftbuffer();
  for (int i = 0; i < inumframes / 2; i++) {

    float dB = mapDecibels(re[i], im[i]);
    fftbuf[i] += dB * 0.03 + 0.0001f;
    fftbuf[i] *= 0.97f;
    dB = fftbuf[i];

    // printf( "dB<%f>\n", dB);
    float fi = float(i) / float(inumframes);

    float frq      = fi * getSampleRate() * 2;
    float midinote = frequency_to_midi_note(frq);
    // printf("i<%d> frq<%g> midinote<%g>\n", i, frq, midinote);
    float x2 = 32 + ANA_X1 + ANA_W * midinote * KFIMAXNOTE;
    float y2 = mapFFTY(dB - 12);
    lines.push_back(HudLine{
        fvec2(x1, y1), //
        fvec2(x2, y2),
        fvec3(.3, .7, 1)}); // spectral plot

    x1 = x2;
    y1 = y2;
  }
  // freqbins[index] = complex_t(0,0);

  ////////////////////////////////////////

  drawHudLines(context, lines);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
