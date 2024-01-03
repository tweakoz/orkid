////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/shelveeq.h>
#include <ork/lev2/aud/singularity/filters.h>

// from http://www.musicdsp.org/files/filters003.txt

/*
slider1:0<0,1,1{Stereo,Mono}>Processing
slider2:50<0,100,0.05>Low Shelf (Scale)
slider3:0<-24,24,0.01>Gain (dB)
slider4:50<0,100,0.05>High Shelf (Scale)
slider5:0<-24,24,0.01>Gain (dB)
slider6:0<-24,24,0.05>Output (dB)
*/

namespace ork::audio::singularity {

///////////////////////////////////////////

void ShelveEq::init() {
  _SPN = 0;
  _yl = _x1l = _x2l = _y1l = _y2l = _yr = _x1r = _x2r = _y1r = _y2r = 0;
}

void LoShelveEq::set(float cf, float peakg) {
  float freq1 = cf;
  cf *= getInverseSampleRate();

  float sa  = tanf(pi * (cf - 0.25));
  float asq = sa * sa;
  float A   = powf(10, (peakg / 20.0));

  float F = ((peakg < 6.0) && (peakg > -6.0)) //
          ? sqrt(A) //
          : (A > 1.0) //
            ? A / sqrt(2.0) //
            : A * sqrt(2.0);

  float F2  = F * F;
  float tmp = A * A - F2;

  float gammad = (fabs(tmp) <= _SPN) ? 1.0 : powf((F2 - 1.0) / tmp, 0.25);

  float gamman  = sqrt(A) * gammad;
  float gamma2  = gamman * gamman;
  float gam2p1  = 1.0 + gamma2;
  float siggam2 = 2.0 * sqrt(2.0) / 2.0 * gamman;
  float ta0     = gam2p1 + siggam2;
  float ta1     = -2.0 * (1.0 - gamma2);
  float ta2     = gam2p1 - siggam2;
  gamma2        = gammad * gammad;
  gam2p1        = 1.0 + gamma2;
  siggam2       = 2.0 * sqrt(2.0) / 2.0 * gammad;
  float tb0     = gam2p1 + siggam2;
  float tb1     = -2.0 * (1.0 - gamma2);
  float tb2     = gam2p1 - siggam2;

  float aa1 = sa * ta1;
  float a0  = ta0 + aa1 + asq * ta2;
  float a1  = 2.0 * sa * (ta0 + ta2) + (1.0 + asq) * ta1;
  float a2  = asq * ta0 + aa1 + ta2;

  float ab1 = sa * tb1;
  float b0  = tb0 + ab1 + asq * tb2;
  float b1  = 2.0 * sa * (tb0 + tb2) + (1.0 + asq) * tb1;
  float b2  = asq * tb0 + ab1 + tb2;

  float recipb0 = 1.0 / b0;
  a0 *= recipb0;
  a1 *= recipb0;
  a2 *= recipb0;
  b1 *= recipb0;
  b2 *= recipb0;

  _a0 = a0;
  _a1 = a1;
  _a2 = a2;
  _b1 = -b1;
  _b2 = -b2;
}

void HiShelveEq::set(float cf, float peakg) {
  cf *= getInverseSampleRate();
  float boost = -peakg;

  float sa  = tan(pi * (cf - 0.25));
  float asq = sa * sa;
  float A   = powf(10, (boost / 20.0));
  float F   = ((boost < 6.0) && (boost > -6.0)) ? sqrt(A) : (A > 1.0) ? A / sqrt(2.0) : A * sqrt(2.0);

  float F2      = F * F;
  float tmp     = A * A - F2;
  float gammad  = (fabs(tmp) <= _SPN) ? 1.0 : powf((F2 - 1.0) / tmp, 0.25);
  float gamman  = sqrt(A) * gammad;
  float gamma2  = gamman * gamman;
  float gam2p1  = 1.0 + gamma2;
  float siggam2 = 2.0 * sqrt(2.0) / 2.0 * gamman;
  float ta0     = gam2p1 + siggam2;
  float ta1     = -2.0 * (1.0 - gamma2);
  float ta2     = gam2p1 - siggam2;
  gamma2        = gammad * gammad;
  gam2p1        = 1.0 + gamma2;
  siggam2       = 2.0 * sqrt(2.0) / 2.0 * gammad;
  float tb0     = gam2p1 + siggam2;
  float tb1     = -2.0 * (1.0 - gamma2);
  float tb2     = gam2p1 - siggam2;

  float aa1 = sa * ta1;
  float a0  = ta0 + aa1 + asq * ta2;
  float a1  = 2.0 * sa * (ta0 + ta2) + (1.0 + asq) * ta1;
  float a2  = asq * ta0 + aa1 + ta2;

  float ab1 = sa * tb1;
  float b0  = tb0 + ab1 + asq * tb2;
  float b1  = 2.0 * sa * (tb0 + tb2) + (1.0 + asq) * tb1;
  float b2  = asq * tb0 + ab1 + tb2;

  float recipb0 = 1.0 / b0;
  a0 *= recipb0;
  a1 *= recipb0;
  a2 *= recipb0;
  b1 *= recipb0;
  b2 *= recipb0;

  float gain = powf(10.0f, (boost / 20.0));
  _a0        = a0 / gain;
  _a1        = a1 / gain;
  _a2        = a2 / gain;
  _b1        = -b1;
  _b2        = -b2;
}

///////////////////////////////////////////

float LoShelveEq::compute(float inp) {
  float xl = inp;

  _yl = _a0 * xl +   //
        _a1 * _x1l + //
        _a2 * _x2l + //
        _b1 * _y1l + //
        _b2 * _y2l;

  _x2l = _x1l;
  _x1l = xl;
  _y2l = _y1l;
  _y1l = _yl;

  return _yl;
}

///////////////////////////////////////////

float HiShelveEq::compute(float inp) {
  float xl = inp;

  _yl = _a0 * xl +   //
        _a1 * _x1l + //
        _a2 * _x2l + //
        _b1 * _y1l + //
        _b2 * _y2l;
  _x2l = _x1l;
  _x1l = xl;
  _y2l = _y1l;
  _y1l = _yl;

  return _yl;
}

} // namespace ork::audio::singularity
