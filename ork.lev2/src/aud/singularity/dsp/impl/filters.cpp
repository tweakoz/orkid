#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

static constexpr float OSR  = getSampleRate();
static constexpr float OISR = getInverseSampleRate();

///////////////////////////////////////////////////////////////////////////////

float BW2Q(float fc, float BWoct) {
  float w0    = fc * PI2XISR;
  float sii   = logf(2.0f) / 2.0f * BWoct * w0 / sin(w0);
  float denom = 2.0f * sinf(sii);
  float Q     = 1.0f / denom;
  return Q;
}

///////////////////////////////////////////////////////////////////////////////
// Trapezoidal SVF (http://www.cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf)
///////////////////////////////////////////////////////////////////////////////

TrapSVF::TrapSVF() {
  Clear();
}

void TrapSVF::Clear() {
  v0    = 0;
  v1    = 0;
  v2    = 0;
  v3    = 0;
  ic1eq = 0;
  ic2eq = 0;
}

// double oversample for higher center

void TrapSVF::SetWithQ(eFilterMode mode, float center, float Q) {
  const float FCMAX = OSR / 6.0f; // with oversampling
  center            = clip_float(center, 30, FCMAX);

  // Q = fc/dF
  // fc = Q*dF
  // dF=fc/Q
  float dF = center / Q;
  // printf( "svf<%p> center<%f> Q<%f> dF<%f>\n", this, center, Q, dF );
  // Q = clip_float(Q,0.0025,100);
  Q = clip_float(Q, 0.0125, 18);

  // printf( "center<%f> Q<%f> OSR<%f>\n", center, Q, OSR );

  float g = tan(pi * center / OSR);
  float k = 1.0f / Q;
  a1      = 1.0f / (1.0f + g * (g + k));
  a2      = g * a1;
  a3      = g * a2;

  switch (mode) {
    case EM_LPF:
      m0 = 0;
      m1 = 0;
      m2 = 1;
      break;
    case EM_HPF:
      m0 = 1;
      m1 = -k;
      m2 = -1;
      break;
    case EM_BPF:
      m0 = 0;
      m1 = 1;
      m2 = 0;
      break;
    case EM_NOTCH:
      m0 = 1;
      m1 = -k;
      m2 = 0;
      break;
    default:
      m0 = 0;
      m1 = 0;
      m2 = 0;
      break;
  }
}

void TrapSVF::SetWithRes(eFilterMode mode, float center, float res) {
  // auto Q = BW2Q(center,bwOct/res);
  auto Q = 0.5f / (2.0f - (2.0f * res));

  float sq2 = sqrtf(2.0f);
  Q         = powf(sq2, res / 3.0f);
  // 1.5 = 3 // (2) .5
  // 2 = 6   // (3) 1
  // 4 = 12  // (3) 2
  // 16 = 24 // (4) 4

  // Q = 32.f; //
  // printf( "res<%f> Q<%f>\n", res, Q );

  SetWithQ(mode, center, Q);
  ; // res );
}
void TrapSVF::SetWithBWoct(eFilterMode mode, float center, float bwOct) {
  auto Q = BW2Q(center, bwOct);
  // printf( "center<%f> bwOct<%f> Q<%f>\n", center, bwOct, Q );
  SetWithQ(mode, center, Q);
}

void TrapSVF::compute(float input) {
  v0     = input;
  v3     = v0 - ic2eq;
  v1     = a1 * ic1eq + a2 * v3;
  v2     = ic2eq + a2 * ic1eq + a3 * v3;
  ic1eq  = 2 * v1 - ic1eq;
  ic2eq  = 2 * v2 - ic2eq;
  output = m0 * v0 + m1 * v1 + m2 * v2;
}

void TrapSVF::Tick(float input) {
  compute(input);
  compute(input);
}

///////////////////////////////////////////////////////////////////////////////
// Trapezoidal allpass
///////////////////////////////////////////////////////////////////////////////

TrapAllpass::TrapAllpass() {
  Clear();
}

void TrapAllpass::Clear() {
  s1 = 0.0f;
  s2 = 0.0f;
}
// set coefficients
void TrapAllpass::Set(float cutoff) {
  cutoff        = clip_float(cutoff, 30, 16000);
  const float Q = 0.5f;
  damping       = 1.0 / Q;
  float g       = tan(pi * cutoff / SR);
  g1            = g;
  g2            = 1.0 / (1 + g * (damping + g));
  g3            = damping + g1;
}
float TrapAllpass::Tick(float input) {
  // process loop
  y0           = (input - s1 * g3 - s2) * g2;
  y1           = y0 * g1 + s1;
  s1           = y0 * g1 + y1;
  y2           = y1 * g1 + s2;
  s2           = y1 * g1 + y2;
  float output = y0 - y1 * damping + y2;
  return output;
}

// desc:parametric equalizer
// slider1:4000<10,40000,40>band (Hz)
// slider2:0<-120,120,1>gain (dB)
// slider3:0.7<0.01,50,0.05>width

void ParaOne::Clear() {
  _c0 = _c1 = _c2 = 0;
  _del1 = _del2 = 0;
  _li1 = _li2 = 0;
  _spl0       = 0.0f;
}

void ParaOne::Set(float f, float w, float g) {
  // float arc=f*pi/(srate*0.5);
  float arc  = f * pi2 * ISR; ///(srate*0.5);
  float gain = powf(2.0f, g / 6.0f);
  _a         = (sinf(arc) * w) * ((gain < 1.0f) ? 1.0f : 0.25f);
  float tmp  = 1.0f / (1.0f + _a);

  _c0 = tmp * _a * (gain - 1.0f);
  _c1 = tmp * 2.0f * cosf(arc);
  _c2 = tmp * (_a - 1.0f);

  // printf( "c0<%f> c1<%f> c2<%f>\n", _c0, _c1, _c2 );
}

float ParaOne::compute(float inp) {
  float tmp = _c0 * (inp - _del2) + _c1 * _li1 + _c2 * _li2;

  _del2 = _del1;
  _del1 = inp;
  _li2  = _li1;
  _li1  = tmp;
  inp += _li1;

  return inp;
}

/*static void SetAllpass( BiQuad& biq, float kfco )
{
    //kfco = 100.0f;

    float Q = 0.01f;
    //float V = decibel_to_linear_amp_ratio(peakGain);
    float K = std::tan(pi*kfco*ISR);
    float KK = K*K;
    float KdQ = K/Q;
    float norm = 1.0f/(1.0f+KdQ+KK);

    //a0 = 1 - alpha
    //a1 = -2*cos(w0)
    //a2 = 1 + alpha
    //b0 = a2
    //b1 = a1
    //b2 = a0

    //printf( "V<%f> K<%f> Q<%f>\n", V, K, Q );
    biq._mfb0 = KdQ*norm;
    biq._mfb1 = 2.0*biq._mfb0;
    biq._mfb2 = biq._mfb0;

    biq._mfa1 = 2.0f*(KK-1.0f)*norm;
    biq._mfa2 = (1.0f+KK-KdQ)*norm;
}*/

#if 0

w0 = 2*pi*f0/Fs
alpha = sin(w0)/(2*Q)

ALLPASS
a0 = 1 - alpha
a1 = -2*cos(w0)
a2 = 1 + alpha
b0 = a2
b1 = a1
b2 = a0

BPF
a0 =   alpha
a1 =   0
a2 =  -a0
b0 =   1 + alpha
b1 =  -2*cos(w0)
b2 =   1 - alpha

NOTCH
a0 =   1
a1 =  -2*cos(w0)
a2 =   a0
b0 =   1 + alpha
b1 =  a1
b2 =   1 - alpha

function calcBiquad(type, Fc, Fs, Q, peakGain) {
    var a0,a1,a2,b1,b2,norm;

    var V = Math.pow(10, Math.abs(peakGain) / 20);
    var K = Math.tan(Math.PI * Fc / Fs);
    switch (type) {
        case "lowpass":
            norm = 1 / (1 + K / Q + K * K);
            a0 = K * K * norm;
            a1 = 2 * a0;
            a2 = a0;
            b1 = 2 * (K * K - 1) * norm;
            b2 = (1 - K / Q + K * K) * norm;
            break;

        case "highpass":
            norm = 1 / (1 + K / Q + K * K);
            a0 = 1 * norm;
            a1 = -2 * a0;
            a2 = a0;
            b1 = 2 * (K * K - 1) * norm;
            b2 = (1 - K / Q + K * K) * norm;
            break;

        case "bandpass":
            norm = 1 / (1 + K / Q + K * K);
            a0 = K / Q * norm;
            a1 = 0;
            a2 = -a0;
            b1 = 2 * (K * K - 1) * norm;
            b2 = (1 - K / Q + K * K) * norm;
            break;

        case "notch":
            norm = 1 / (1 + K / Q + K * K);
            a0 = (1 + K * K) * norm;
            a1 = 2 * (K * K - 1) * norm;
            a2 = a0;
            b1 = a1;
            b2 = (1 - K / Q + K * K) * norm;
            break;

        case "peak":
            if (peakGain >= 0) {    // boost
                norm = 1 / (1 + 1/Q * K + K * K);
                a0 = (1 + V/Q * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - V/Q * K + K * K) * norm;
                b1 = a1;
                b2 = (1 - 1/Q * K + K * K) * norm;
            }
            else {    // cut
                norm = 1 / (1 + V/Q * K + K * K);
                a0 = (1 + 1/Q * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - 1/Q * K + K * K) * norm;
                b1 = a1;
                b2 = (1 - V/Q * K + K * K) * norm;
            }
            break;
        case "lowShelf":
            if (peakGain >= 0) {    // boost
                norm = 1 / (1 + Math.SQRT2 * K + K * K);
                a0 = (1 + Math.sqrt(2*V) * K + V * K * K) * norm;
                a1 = 2 * (V * K * K - 1) * norm;
                a2 = (1 - Math.sqrt(2*V) * K + V * K * K) * norm;
                b1 = 2 * (K * K - 1) * norm;
                b2 = (1 - Math.SQRT2 * K + K * K) * norm;
            }
            else {    // cut
                norm = 1 / (1 + Math.sqrt(2*V) * K + V * K * K);
                a0 = (1 + Math.SQRT2 * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - Math.SQRT2 * K + K * K) * norm;
                b1 = 2 * (V * K * K - 1) * norm;
                b2 = (1 - Math.sqrt(2*V) * K + V * K * K) * norm;
            }
            break;
        case "highShelf":
            if (peakGain >= 0) {    // boost
                norm = 1 / (1 + Math.SQRT2 * K + K * K);
                a0 = (V + Math.sqrt(2*V) * K + K * K) * norm;
                a1 = 2 * (K * K - V) * norm;
                a2 = (V - Math.sqrt(2*V) * K + K * K) * norm;
                b1 = 2 * (K * K - 1) * norm;
                b2 = (1 - Math.SQRT2 * K + K * K) * norm;
            }
            else {    // cut
                norm = 1 / (V + Math.sqrt(2*V) * K + K * K);
                a0 = (1 + Math.SQRT2 * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - Math.SQRT2 * K + K * K) * norm;
                b1 = 2 * (K * K - V) * norm;
                b2 = (V - Math.sqrt(2*V) * K + K * K) * norm;
            }
            break;
    }
#endif
// http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt

//=================================================
void OnePoleLoPass::init() {
  lp_b1   = 0.0f;
  lp_a0   = 0.0f;
  lp_outl = 0.0f;
}

void OnePoleLoPass::set(float cutoff) {
  cutoff       = clip_float(cutoff, 10, 20000);
  float lp_cut = pi2 * cutoff;
  float lp_n   = 1.0f / (lp_cut + 3.0f * SR);
  lp_b1        = (3 * SR - lp_cut) * lp_n;
  lp_a0        = lp_cut * lp_n;
}
float OnePoleLoPass::compute(float input) {
  lp_outl = 2.0f * input * lp_a0 + lp_outl * lp_b1;
  return lp_outl;
}
//=================================================
void OnePoleHiPass::init() {
  lp_b1   = 0.0f;
  lp_a0   = 0.0f;
  lp_outl = 0.0f;
}
void OnePoleHiPass::set(float cutoff) {
  const float SR = getSampleRate();
  cutoff         = clip_float(cutoff, 0, 20000);
  float lp_cut   = pi2 * cutoff;
  float lp_n     = 1.0f / (lp_cut + 3.0f * SR);
  lp_b1          = (3.0f * SR - lp_cut) * lp_n;
  lp_a0          = lp_cut * lp_n;
}
float OnePoleHiPass::compute(float input) {
  lp_outl = 2.0f * input * lp_a0 + lp_outl * lp_b1;

  return (input - lp_outl);
}

} // namespace ork::audio::singularity
