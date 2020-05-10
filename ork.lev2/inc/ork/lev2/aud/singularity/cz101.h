#pragma once
#include <ork/kernel/svariant.h>
#include "krztypes.h"

namespace ork::audio::singularity {

struct Wavetable;
struct DspKeyOnInfo;

struct CzEnvelope {
  int _endStep   = 0;
  int _sustPoint = -1;
  bool _decreasing[8];
  float _time[8];
  int _level[8];
};
struct CzOscData {
  int _dcoWaveA     = 0;
  int _dcoWaveB     = 0;
  bool _enaWaveB    = false;
  int _dcoWindow    = 0;
  int _dcaKeyFollow = 0;
  int _dcwKeyFollow = 0;
  int _dcaVelFollow = 0;
  int _dcwVelFollow = 0;
  int _dcaDepth     = 0;
  int _dcwDepth     = 0;
  CzEnvelope _dcoEnv;
  CzEnvelope _dcaEnv;
  CzEnvelope _dcwEnv;
};
struct CzOsc {
  CzOsc();
  ~CzOsc();

  void keyOn(const DspKeyOnInfo& koi, const CzOscData& opd);
  void keyOff();
  float compute(float frq, float df);

  void setWave(int iwA, int iwB);

  static constexpr float kinv64k = 1.0f / 65536.0f;
  static constexpr float kinv32k = 1.0f / 32768.0f;

  float _baseFrequency;
  float _modIndex;

  int64_t _pbIndex;
  int64_t _pbIndexNext;
  int64_t _pbIncrBase;

  int64_t _mIndex;
  int64_t _mIndexNext;
  int64_t _mIncrBase;

  float _prevOutput;

  const Wavetable* _waveformA;
  const Wavetable* _waveformB;
  CzOscData _data;
};
struct CzProgData {
  void dump() const;

  int _octave       = 0;
  int _lineSel      = 0;
  int _lineMod      = 0;
  int _detuneCents  = 0;
  int _vibratoWave  = 0;
  int _vibratoDelay = 0;
  int _vibratoRate  = 0;
  int _vibratoDepth = 0;
  CzOscData _oscData[2];
  std::string _name;
};

struct DspKeyOnInfo;
struct DspBuffer;
struct czsyn {
  czsyn();
  void compute(DspBuffer& dspbuf);
  void keyOn(const DspKeyOnInfo& koi);
  void keyOff();

  CzProgData _data;
  ork::svarp_t _pimpl;
};

} // namespace ork::audio::singularity
