////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <cstdint>
namespace ork::audio::singularity {
constexpr float KMAXFC = 0.36363636363636365f * getSampleRate();

///////////////////////////////////////////////////////////////////////////////
BiQuad::BiQuad()
    : _xm1(0.0f)
    , _xm2(0.0f)
    , _ym1(0.0f)
    , _ym2(0.0f)
    , _mfa1(0.0f)
    , _mfa2(0.0f)
    , _mfb0(0.0f)
    , _mfb1(0.0f)
    , _mfb2(0.0f) {
}
///////////////////////////////////////////////////////////////////////////////
void BiQuad::Clear() {
  _xm1 = 0.0f;
  _xm2 = 0.0f;
  _ym1 = 0.0f;
  _ym2 = 0.0f;
}
///////////////////////////////////////////////////////////////////////////////
inline bool isNanOrInf(float value) {
    // IEEE 754 float: sign bit (1), exponent (8), fraction (23)
    uint32_t bits;
    //std::memcpy(&bits, &value, sizeof(bits)); // Bitwise copy of the float to an int
    bits = *(reinterpret_cast<uint32_t*>(&value));
    // Check for NaN or Inf: all exponent bits are 1
    // NaN: All exponent bits 1, and non-zero fraction
    // Inf: All exponent bits 1, and zero fraction
    bool exponentAllOnes = ((bits & 0x7F800000) == 0x7F800000);
    bool fractionNonZero = (bits & 0x007FFFFF) != 0;

    return exponentAllOnes && !fractionNonZero;
}
float BiQuad::compute(float input) {

  float outputp = (_mfb0 * input)  //
                  + (_mfb1 * _xm1) //
                  + (_mfb2 * _xm2);

  float outputn = -(_mfa1 * _ym1) //
                  - (_mfa2 * _ym2);

  float output = outputp + outputn;

  _xm2 = _xm1;
  _xm1 = input;
  _ym2 = _ym1;
  _ym1 = output;

  //if (isnan(_ym1) or isinf(_ym1)) {
  if(isNanOrInf(_ym1))
    _ym1 = 0.0f;
  if(isNanOrInf(_ym2))
    _ym2 = 0.0f;

  return output;
}
///////////////////////////////////////////////////////////////////////////////
float BiQuad::compute2(float input) {

  float output = (_mfb0 * input)  //
                 + (_mfb1 * _xm1) //
                 + (_mfb2 * _xm2) //

                 - (_mfa1 * _ym1) //
                 - (_mfa2 * _ym2);

  output /= _mfa0;

  _xm2 = _xm1;
  _xm1 = input;
  _ym2 = _ym1;
  _ym1 = output;

  return output;
}
///////////////////////////////////////////////////////////////////////////////
void BiQuad::SetLpfReson(float kfco, float krez) {
  krez = clip_float(krez, 0.01f, 0.95f);
  kfco = clip_float(kfco, 30.1f, KMAXFC);

  float w0  = kfco * PI2XISR;
  float w02 = (2.0f * w0);

  float cosW0     = cosf(w0);
  float cosW02    = cosf(w02);
  float sinW0     = sinf(w0);
  float sinW02    = sinf(w02);
  float krezsq    = (krez * krez);
  float krezCosW0 = krez * cosW0;

  float kalpha = 1.0f - (2.0f * krezCosW0 * cosW0) + (krezsq * cosW02);

  float kbeta = (krezsq * sinW02) - (2.0f * krezCosW0 * sinW0);

  float kgama = 1.0f + cosW0;

  float kag = kalpha * kgama;
  float kbs = kbeta * sinW0;
  float km1 = kag + kbs;
  float km2 = kag - kbs;

  float kden = sqrtf(km1 * km1 + km2 * km2);

  // YSIDE COEF
  _mfa1 = -2.0f * krez * cosW0;
  _mfa2 = krez * krez;

  // XSIDE COEF
  _mfb0 = 1.5f * (kalpha * kalpha + kbeta * kbeta) / kden;
  _mfb1 = _mfb0;
  _mfb2 = 0.0f;
}
///////////////////////////////////////////////////////////////////////////////
void BiQuad::SetBpfMeth2(float kfco) {
  // H(S) = (S/Q)/(S^2 + S/Q + 1)
  float Q = 8.5f;
  float W = tan(pi * kfco * ISR);
  float N = 1.0f / (W * W + W / Q + 1.0f);
  _mfb0   = N * W / Q;
  _mfb1   = 0.0;
  _mfb2   = -_mfb0;
  _mfa1   = 2.0f * N * (W * W - 1.0f);
  _mfa2   = N * (W * W - W / Q + 1.0f);
}

///////////////////////////////////////////////////////////////////////////////
void BiQuad::SetBpfWithQ(float kfco, float Q, float peakGain)

{
  // float V = decibel_to_linear_amp_ratio(peakGain);
  float K    = std::tan(pi * kfco * ISR);
  float KK   = K * K;
  float KdQ  = K / Q;
  float norm = 1.0f / (1.0f + KdQ + KK);
  // printf( "V<%f> K<%f> Q<%f>\n", V, K, Q );
  _mfb0 = KdQ * norm;
  _mfb1 = 0;
  _mfb2 = -_mfb0;

  _mfa1 = 2.0f * (KK - 1.0f) * norm;
  _mfa2 = (1.0f - KdQ + KK) * norm;
}

///////////////////////////////////////////////////////////////////////////////

void BiQuad::SetBpfWithBWoct(float kfco, float BWoct, float peakGain) {
  BWoct       = clip_float(fabs(BWoct), 0.2, 8);
  float w0    = kfco * PI2XISR;
  float sii   = logf(2.0f) / 2.0f * BWoct * w0 / sin(w0);
  float denom = 2.0f * sinf(sii);
  float Q     = 1.0f / denom;
  // printf( "w0<%f> denom<%f> BWoct<%f> sii<%f> Q<%f>\n", w0, denom, BWoct, sii, Q );
  SetBpfWithQ(kfco, Q, peakGain);
  // float LG = decibel_to_linear_amp_ratio(dBgain);
  // float A  = sqrt( LG ); // rms ?
  //     1/Q = 2*sinh(ln(2)/2*BW*w0/sin(w0))     (digital filter w BLT)
  // or   1/Q = 2*sinh(ln(2)/2*BW)             (analog filter prototype)
  //            The relationship between shelf slope and Q is
  //               1/Q = sqrt((A + 1/A)*(1/S - 1) + 2)
}

///////////////////////////////////////////////////////////////////////////////

void BiQuad::SetNotchWithQ(float kfco, float Q, float peakGain) {
  float V    = decibel_to_linear_amp_ratio(peakGain);
  float K    = std::tan(pi * kfco * ISR);
  float KK   = K * K;
  float KdQ  = K / Q;
  float norm = 1.0f / (1.0f + KdQ + KK);
  // printf( "V<%f> K<%f> Q<%f>\n", V, K, Q );
  _mfb0 = (1.0f + KK) * norm;
  _mfb1 = 2 * (1 + KK) * norm;
  _mfb2 = _mfb0;

  _mfa1 = _mfb1;
  _mfa2 = (1.0f + KK - KdQ) * norm;
}

///////////////////////////////////////////////////////////////////////////////

void BiQuad::SetNotchWithBWoct(float kfco, float BWoct, float peakGain) {
  float w0    = kfco * PI2XISR;
  float sii   = logf(2.0f) / 2.0f * BWoct * w0 / sin(w0);
  float denom = 2.0f * sinf(sii);
  float Q     = 1.0f / denom;
  SetNotchWithQ(kfco, Q, peakGain);
}

///////////////////////////////////////////////////////////////////////////////

void BiQuad::SetLpfNoQ(float kfco) {
  // kfco = 100.0f;

  float Q = 0.01f;
  // float V = decibel_to_linear_amp_ratio(peakGain);
  float K    = std::tan(pi * kfco * ISR);
  float KK   = K * K;
  float KdQ  = K / Q;
  float norm = 1.0f / (1.0f + KdQ + KK);

  // norm = 1 / (1 + K / Q + K * K);
  // a0 = K * K * norm;
  // a1 = 2 * a0;
  // a2 = a0;

  // b1 = 2 * (K * K - 1) * norm;
  // b2 = (1 - K / Q + K * K) * norm;

  // printf( "V<%f> K<%f> Q<%f>\n", V, K, Q );
  _mfb0 = KdQ * norm;
  _mfb1 = 2.0 * _mfb0;
  _mfb2 = _mfb0;

  _mfa1 = 2.0f * (KK - 1.0f) * norm;
  _mfa2 = (1.0f + KK - KdQ) * norm;
}
///////////////////////////////////////////////////////////////////////////////
void BiQuad::SetLpf(float kfco) {
  float Q    = 0.5f;
  float K    = std::tan(pi * kfco * getInverseSampleRate());
  float KK   = K * K;
  float KdQ  = K / Q;
  float norm = 1.0f / (1.0f + KdQ + KK);
  _mfb0      = (KK)*norm;
  _mfb1      = 2 * _mfb0;
  _mfb2      = _mfb0;
  _mfa1      = 2 * (KK - 1) * norm;
  _mfa2      = (1 - KdQ + KK) * norm;
}
///////////////////////////////////////////////////////////////////////////////
void BiQuad::SetHpf(float kfco) {
  float Q    = 0.5f;
  float K    = std::tan(pi * kfco * getInverseSampleRate());
  float KK   = K * K;
  float KdQ  = K / Q;
  float norm = 1.0f / (1.0f + KdQ + KK);
  _mfb0      = norm;
  _mfb1      = -2.0f * _mfb0;
  _mfb2      = _mfb0;
  _mfa1      = 2 * (KK - 1) * norm;
  _mfa2      = (1 - KdQ + KK) * norm;
}
///////////////////////////////////////////////////////////////////////////////
void BiQuad::SetParametric(float kfco, float wid, float peakGain) {
  if (kfco > KMAXFC)
    kfco = KMAXFC;
  if (wid < 0.1)
    wid = 0.1;

  float w0    = kfco * PI2XISR;
  float sii   = logf(2.0f) / 2.0f * wid * w0 / sin(w0);
  float denom = 2.0f * sinf(sii);
  float Q     = 1.0f / denom;
  float V     = powf(10.0f, fabs(peakGain) / 20.0f);
  // float V = decibel_to_linear_amp_ratio(peakGain);
  float K = std::tan(pi * kfco * ISR);
  if (peakGain >= 0.0f) { // boost
    float norm = 1.0f / (1.0f + 1.0f / Q * K + K * K);
    _mfb0      = (1.0f + V / Q * K + K * K) * norm;
    _mfb1      = 2.0f * (K * K - 1.0f) * norm;
    _mfb2      = (1.0f - V / Q * K + K * K) * norm;
    _mfa1      = _mfb1;
    _mfa2      = (1.0f - 1.0f / Q * K + K * K) * norm;
  } else { // cut
    float norm = 1.0f / (1.0f + V / Q * K + K * K);
    _mfb0      = (1.0f + 1.0f / Q * K + K * K) * norm;
    _mfb1      = 2.0f * (K * K - 1.0f) * norm;
    _mfb2      = (1.0f - 1.0f / Q * K + K * K) * norm;
    _mfa1      = _mfb1;
    _mfa2      = (1.0f - V / Q * K + K * K) * norm;
  }
}
///////////////////////////////////////////////////////////////////////////////
void BiQuad::SetParametric2(float kfco, float wid, float dBGain) {
  if (kfco > KMAXFC)
    kfco = KMAXFC;
  if (wid < 0.1)
    wid = 0.1;

#if 0
    float Q = BW2Q(kfco,wid);
    float w0 = pi2*kfco*ISR ;
    float alpha = sin(w0)/(2*Q) ;
    _mfb0 =   alpha;
    _mfb1 =   0;
    _mfb2 =  -_mfa0;
    _mfa0 =   1 + alpha;
    _mfa1 =  -2*cos(w0);
    _mfa2 =   1 - alpha;

#else
  double A     = pow(10, dBGain / 40.0);
  float Q      = BW2Q(kfco, wid * 2);
  double omega = (pi2 * kfco) / getSampleRate();
  double sinw  = sin(omega);
  double cosw  = cos(omega);
  double alpha = sinw / (2.0 * Q);

  _mfb0 = 1 + (alpha * A);
  _mfb1 = -2 * cosw;
  _mfb2 = 1 - (alpha * A);
  _mfa0 = 1 + (alpha / A);
  _mfa1 = -2 * cosw;
  _mfa2 = 1 - (alpha / A);
#endif
}
///////////////////////////////////////////////////////////////////////////////
void BiQuad::SetLowShelf(float kfco, float peakGain) {
  if (kfco > KMAXFC)
    kfco = KMAXFC;
  float V = decibel_to_linear_amp_ratio(peakGain);
  float K = std::tan(pi * kfco * getInverseSampleRate());
  if (peakGain >= 0) { // boost
    float norm = 1 / (1 + sqrtf(2) * K + K * K);
    _mfb0      = (1 + sqrtf(2 * V) * K + V * K * K) * norm;
    _mfb1      = 2 * (V * K * K - 1) * norm;
    _mfb2      = (1 - sqrtf(2 * V) * K + V * K * K) * norm;
    _mfa1      = 2 * (K * K - 1) * norm;
    _mfa2      = (1 - sqrtf(2.0f) * K + K * K) * norm;
  } else { // cut
    float norm = 1 / (1 + sqrtf(2 * V) * K + V * K * K);
    _mfb0      = (1 + sqrtf(2.0f) * K + K * K) * norm;
    _mfb1      = 2 * (K * K - 1) * norm;
    _mfb2      = (1 - sqrtf(2.0f) * K + K * K) * norm;
    _mfa1      = 2 * (V * K * K - 1) * norm;
    _mfa2      = (1 - sqrtf(2 * V) * K + V * K * K) * norm;
    // printf( "PG<%f> V<%f>\n", peakGain, V );
  }
}
///////////////////////////////////////////////////////////////////////////////
void BiQuad::SetHighShelf(float kfco, float peakGain) {
  if (kfco > KMAXFC)
    kfco = KMAXFC;
  float V = decibel_to_linear_amp_ratio(peakGain);
  float K = std::tan(pi * kfco * getInverseSampleRate());
  if (peakGain >= 0) { // boost
    float norm = 1 / (1 + sqrtf(2) * K + K * K);
    _mfb0      = (V + sqrtf(2 * V) * K + K * K) * norm;
    _mfb1      = 2 * (K * K - V) * norm;
    _mfb2      = (V - sqrtf(2 * V) * K + K * K) * norm;
    _mfa1      = 2 * (K * K - 1) * norm;
    _mfa2      = (1 - sqrtf(2) * K + K * K) * norm;
  } else { // cut
    float norm = 1 / (V + sqrtf(2 * V) * K + K * K);
    _mfb0      = (1 + sqrtf(2) * K + K * K) * norm;
    _mfb1      = 2 * (K * K - 1) * norm;
    _mfb2      = (1 - sqrtf(2) * K + K * K) * norm;
    _mfa1      = 2 * (K * K - V) * norm;
    _mfa2      = (V - sqrtf(2 * V) * K + K * K) * norm;
  }
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
