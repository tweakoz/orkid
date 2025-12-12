////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>
#include <complex>
#include <cfloat>
#include <assert.h>
#include <memory>
#include <stdlib.h>
#include "krztypes.h"
#include "delays.h"

namespace ork::audio::singularity {

static constexpr float SR  = getSampleRate();
static constexpr float ISR = getInverseSampleRate();

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
  float Tick(float input);

private:
  void _compute(float input);
};

struct SimpleAllpass {

  SimpleAllpass();
  ~SimpleAllpass();
  float compute(float input);

  float _feed = 0.99f;
  float _y0 = 0.0f;
  delaycontext_ptr_t _delay;

};

struct TrapAllpass {
  float s1, s2;
  float g1, g2, g3;
  float y0, y1, y2;
  float damping;

  TrapAllpass();

  void Clear();

  // set coefficients
  void set(float cutoff); // tecbnically not cutoff, frq at which phase shift is max
  float Tick(float input);
};

struct ParaOne {
  void Clear();
  void set(float f, float w, float g);
  void set2(float frqHZ, float bandwidthHZ, float gainDB);
  float compute(float inp);

  bool _enable = false;

  float _c0, _c1, _c2;
  float _arc, _gain, _a;
  float _del1, _del2;
  float _li1, _li2;
  float _spl0;
};

struct BiQuad;
using biquad_ptr_t = std::shared_ptr<BiQuad>;

struct BiQuad {
  BiQuad();
  void Clear();
  void lerp(const BiQuad& oth, float index);
  float compute(float input);
  float compute2(float input);
  float compute3(float input);
  void SetLpfReson(float kfco, float krez);
  void SetLpf(float kfco);
  void SetHpf(float kfco);
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
struct MultiStageLoPass {
  void init(int num_stages);
  void set(float cutoff);
  float compute(float inp);
  std::vector<OnePoleLoPass> _stages;
};

struct OnePoleHiPass {
  void init();
  void set(float cutoff);
  float compute(float inp);
  float lp_b1, lp_a0;
  float lp_outl;
};

struct OnePoleHighPass {
    float _xm1 = 0.0f;
    float _ym1 = 0.0f;
    float _alpha = 0.0f;

    inline OnePoleHighPass() {
      set(100,44100);
    }
    inline void set(float fc, float sampleRate) {
        float RC = 1.0f / (2.0f * M_PI * fc);
        float dt = 1.0f / sampleRate;
        _alpha = RC / (RC + dt);
    }

    inline void clear() {
        _xm1 = 0.0f;
        _ym1 = 0.0f;
    }

    inline float compute(float input) {
        float output = _alpha * (_ym1 + input - _xm1);
        _xm1 = input;
        _ym1 = output;
        return output;
    }
};

///////////////////////////////////////////////////////////////////////////////
// Dynamics Compressor
//  - Musician-friendly interface with threshold, ratio, attack, release, makeup
//  - Feed-forward design with RMS envelope detection
///////////////////////////////////////////////////////////////////////////////

struct Compressor;
using compressor_ptr_t = std::shared_ptr<Compressor>;

struct Compressor {
  Compressor();

  // Configuration (musician-friendly)
  void setThreshold(float dB);      // dB below 0 (e.g., -20)
  void setRatio(float ratio);       // compression ratio (e.g., 4.0 = 4:1)
  void setAttack(float ms);         // attack time in milliseconds
  void setRelease(float ms);        // release time in milliseconds
  void setMakeupGain(float dB);     // makeup gain in dB
  void setKnee(float dB);           // soft knee width in dB (0 = hard knee)

  // Preset configurations
  void setupForVoice(float threshold_dB = -12.0f, float ratio = 3.0f);
  void setupGentle();               // light compression for general use
  void setupMedium();               // moderate compression
  void setupHeavy();                // heavy limiting

  // Processing
  float compute(float input);
  void computeBlock(float* samples, size_t count);
  void clear();

  // Read current gain reduction (for metering)
  float getGainReduction() const { return _gain_reduction_dB; }

  // Getters for property bindings
  float getThreshold() const { return _threshold_dB; }
  float getRatio() const { return _ratio; }
  float getMakeupGain() const;  // returns dB
  float getKnee() const { return _knee_dB; }

private:
  float computeGain(float input_dB);

  // Parameters
  float _threshold_dB = -20.0f;
  float _ratio = 4.0f;
  float _attack_coeff = 0.0f;
  float _release_coeff = 0.0f;
  float _makeup_linear = 1.0f;
  float _knee_dB = 6.0f;

  // State
  float _envelope = 0.0f;
  float _gain_reduction_dB = 0.0f;
};

///////////////////////////////////////////////////////////////////////////////
// Noise Gate
//  - Standalone noise gate for sample-by-sample or block processing
//  - Energy-based gating with configurable attack/release
///////////////////////////////////////////////////////////////////////////////

struct NoiseGate;
using noisegate_ptr_t = std::shared_ptr<NoiseGate>;

struct NoiseGate {
  NoiseGate();

  // Configuration
  void setThreshold(float threshold_dB);  // threshold in dB (e.g., -40)
  void setAttack(float ms);               // attack time in milliseconds
  void setRelease(float ms);              // release time in milliseconds
  void setMaxAttenuation(float dB);       // max attenuation when gate closed (e.g., -40dB, 0 = full mute)
  void setInputGain(float gain);          // input gain (linear, default 1.0)
  void setOutputGain(float gain);         // output gain (linear, default 1.0)

  // Preset configurations
  void setupForVoice();                   // tuned for voice input

  // Processing
  float compute(float input);
  void computeBlock(float* samples, size_t count);
  void clear();

  // Read current state (for metering)
  float getEnvelope() const { return _envelope; }
  float getEnergy() const { return _energy; }

  // Getters for property bindings
  float getThreshold() const { return _threshold_dB; }
  float getAttack() const { return _attack_ms; }
  float getRelease() const { return _release_ms; }
  float getMaxAttenuation() const { return _max_atten_dB; }
  float getInputGain() const { return _input_gain; }
  float getOutputGain() const { return _output_gain; }

private:
  float _threshold_dB = -40.0f;
  float _threshold = 0.0001f;             // linear energy threshold
  float _attack_ms = 5.0f;
  float _release_ms = 200.0f;
  float _attack_coeff = 0.0f;
  float _release_coeff = 0.0f;
  float _max_atten_dB = -80.0f;           // max attenuation in dB when closed
  float _min_gain = 0.0001f;              // linear min gain (from max_atten)
  float _input_gain = 1.0f;
  float _output_gain = 1.0f;

  // State
  float _envelope = 0.0f;
  float _energy = 0.0f;
};

} // namespace ork::audio::singularity
