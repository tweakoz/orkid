#pragma once

#include <cmath>
#include <complex>
#include <cfloat>
#include <assert.h>
#include <memory>
#include <stdlib.h>
#include "krztypes.h"

namespace ork::audio::singularity {

static const float SR     = getSampleRate();
static const float ISR    = 1.0f / SR;
static const float PI2ISR = pi2 * ISR;

float BW2Q(float fc, float BWoct);

enum eFilterMode {
  EM_LPF = 0,
  EM_BPF,
  EM_HPF,
  EM_NOTCH,
};

struct TrapSVF {
  float v0, v1, v2, v3;
  float a1, a2, a3;
  float m0, m1, m2;
  float ic1eq, ic2eq;
  float output;

  TrapSVF();

  void Clear();
  void SetWithQ(eFilterMode m, float center, float Q);
  void SetWithRes(eFilterMode m, float center, float res);
  void SetWithBWoct(eFilterMode m, float center, float bwOct);
  void Tick(float input);

private:
  void compute(float input);
};

struct TrapAllpass {
  float s1, s2;
  float g1, g2, g3;
  float y0, y1, y2;
  float damping;

  TrapAllpass();

  void Clear();

  // set coefficients
  void Set(float cutoff);
  float Tick(float input);
};

struct ParaOne {
  void Clear();
  void Set(float f, float w, float g);
  float compute(float inp);
  float _c0, _c1, _c2;
  float _arc, _gain, _a;
  float _del1, _del2;
  float _li1, _li2;
  float _spl0;
};

struct BiQuad {
  BiQuad();
  void Clear();
  float compute(float input);
  float compute2(float input);
  void SetLpfReson(float kfco, float krez);
  void SetLpfWithPeakGain(float kfco, float peakg);
  void SetHpfWithPeakGain(float kfco, float peakg);
  void SetBpfWithQ(float kfco, float Q, float peakGain);
  void SetBpfWithBWoct(float kfco, float BWoct, float peakGain);
  void SetBpfMeth2(float kfco);
  void SetNotchWithQ(float kfco, float Q, float peakGain);
  void SetNotchWithBWoct(float kfco, float BWoct, float peakGain);
  void SetLpfNoQ(float kfco);
  void SetLowShelf(float kfco, float peakGain);
  void SetHighShelf(float kfco, float peakGain);
  void SetParametric(float kfco, float wid, float peakGain);
  void SetParametric2(float kfco, float wid, float peakGain);

  float _xm1, _xm2;
  float _ym1, _ym2;
  float _mfa0, _mfa1, _mfa2;
  float _mfb0, _mfb1, _mfb2;
};

struct OnePoleLoPass {
  void init();
  void set(float cutoff);
  float compute(float inp);
  float lp_b1, lp_a0;
  float lp_outl;
};

struct OnePoleHiPass {
  void init();
  void set(float cutoff);
  float compute(float inp);
  float lp_b1, lp_a0;
  float lp_outl;
};

} // namespace ork::audio::singularity
