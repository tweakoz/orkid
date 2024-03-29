////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "controller.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

inline float env_slope2ror(float slope, float bias) {
  float clamped     = std::clamp(slope + bias, 0.0f, 90.0f - bias);
  float riseoverrun = tanf(clamped * pi / 180.0);
  return riseoverrun;
}

///////////////////////////////////////////////////////////////////////////////

struct EnvPoint {
  float _time  = 0.0f;
  float _level = 0.0f;
  float _shape = 1.0f;
  int _raw_rate = 0;
  int _raw_level = 0;
};

///////////////////////////////////////////////////////////////////////////////

enum struct RlEnvType {
  ERLTYPE_DEFAULT = 0,
  ERLTYPE_KRZAMPENV,
  ERLTYPE_KRZMODENV,
};

using envadjust_method_t = std::function<EnvPoint(const EnvPoint& inp, int iseg, const KeyOnInfo& KOI)>;
///////////////////////////////////////////////////////////////////////////////

struct RateLevelEnvData : public ControllerData {
  DeclareConcreteX(RateLevelEnvData, ControllerData);

  RateLevelEnvData();
  ControllerInst* instantiate(layer_ptr_t layer) const final;
  controllerdata_ptr_t clone() const final;
  bool isBiPolar() const;

  void addSegment(std::string name, float time, float level, float power = 1.0f);
  std::vector<EnvPoint> _segments;
  std::vector<std::string> _segmentNames;
  bool _ampenv;
  bool _bipolar;
  RlEnvType _envType;
  envadjust_method_t _envadjust;
  int _releaseSegment = -1;
  int _sustainSegment = -1;
};

using ratelevelenvdata_ptr_t = std::shared_ptr<RateLevelEnvData>;

///////////////////////////////////////////////////////////////////////////////

struct RateLevelEnvInst : public ControllerInst {
  RateLevelEnvInst(const RateLevelEnvData* data, layer_ptr_t l);
  void compute() final;
  void keyOn(const KeyOnInfo& KOI) final;
  void keyOff() final;
  ////////////////////////////
  float shapedvalue() const;
  ////////////////////////////
  void setState(int istate);
  void initSeg(int iseg);
  bool done() const;
  const RateLevelEnvData* _data;
  layer_ptr_t _layer;
  int _segmentIndex;
  float _startval;
  float _rawdestval;
  float _rawtime;
  float _adjtime;
  float _destval;
  float _lerpindex;
  float _lerpincr;
  float _curshape   = 1.0f;
  float _clampmin   = 0.0f;
  float _clampmax   = 1.0f;
  float _clamprange = 1.0f;

  int _framesrem;
  bool _released;
  bool _ignoreRelease;
  bool _ampenv;
  int _state;
  int _prevstate;
  RlEnvType _envType;
  KeyOnInfo _konoffinfo;
  int _updatecount = 0;
  std::string _debugName;
};


///////////////////////////////////////////////////////////////////////////////

struct NatEnvWrapperData : public ControllerData {
  DeclareConcreteX(NatEnvWrapperData, ControllerData);
  NatEnvWrapperData();
  ControllerInst* instantiate(layer_ptr_t layer) const final;
  controllerdata_ptr_t clone() const final;
  std::vector<natenvseg> _segments;
};

struct NatEnvWrapperInst : public ControllerInst {
  NatEnvWrapperInst(const NatEnvWrapperData* data, layer_ptr_t l);
  void compute() final;
  void keyOn(const KeyOnInfo& KOI) final;
  void keyOff() final;
  KeyOnInfo _konoffinfo;
  natenv_ptr_t _natenv;
};

///////////////////////////////////////////////////////////////////////////////

struct AsrData : public ControllerData {

  DeclareConcreteX(AsrData, ControllerData);

  AsrData();
  ControllerInst* instantiate(layer_ptr_t layer) const final;
  controllerdata_ptr_t clone() const final;

  std::string _trigger;
  std::string _mode;
  float _delay;
  float _attack;
  float _release;
  envadjust_method_t _envadjust;
};

///////////////////////////////////////////////////////////////////////////////

struct AsrInst : public ControllerInst {
  AsrInst(const AsrData* data, layer_ptr_t l);
  void compute() final;
  void keyOn(const KeyOnInfo& KOI) final;
  void keyOff() final;
  ////////////////////////////
  void initSeg(int iseg);
  bool isValid() const;
  const AsrData* _data;
  int _curseg;
  int _mode;
  float _dstval;
  int _framesrem;
  bool _released;
  bool _ignoreRelease;
  float _curslope_persamp;
  KeyOnInfo _konoffinfo;
};

///////////////////////////////////////////////////////////////////////////////

struct YmEnvData : public ControllerData {

  DeclareConcreteX(YmEnvData, ControllerData);

  YmEnvData();
  ControllerInst* instantiate(layer_ptr_t layer) const final;
  controllerdata_ptr_t clone() const final;

  int _attackRate = 0; 
  int _decay1Rate  = 0; 
  int _decay1Level = 0;
  int _decay2Rate  = 0; 
  int _releaseRate = 0; 
  int _egshift       = 0;
  int _rateScale     = 0;
  int _levScale     = 0;

  float _attackShape = 0; //

  envadjust_method_t _envadjust;
};

///////////////////////////////////////////////////////////////////////////////

struct YmEnvInst : public ControllerInst {
  YmEnvInst(const YmEnvData* data, layer_ptr_t l);
  void compute() final;
  void keyOn(const KeyOnInfo& KOI) final;
  void keyOff() final;
  ////////////////////////////
  bool isValid() const;
  const YmEnvData* _data = nullptr;
  uint32_t _curseg            = 0;
  KeyOnInfo _koi;
  float _rawout         = 0.0f;
  float _prcout         = 0.0f;
  layer_ptr_t _layer         = nullptr;
  float _atkinc         = 0.0f;
  float _dec1ratefactor = 0.0f;
  float _dec2ratefactor = 0.0f;
  float _relratefactor  = 0.0f;
  float _sustainLevel = 0.0f;
  bool _disable_dec1 = false;
  bool _disable_dec2 = false;
};

///////////////////////////////////////////////////////////////////////////////

struct PolynomialEval{
  PolynomialEval();
  float evaluate(float input) const;
  std::vector<float> _polynomial;  
};

struct TX81ZEnvTables{
  TX81ZEnvTables();
  PolynomialEval _attack;
  PolynomialEval _decay1;
  PolynomialEval _decay2;
  PolynomialEval _release;
  PolynomialEval _level;
};

const TX81ZEnvTables& getTX81ZEnvTables();

struct TX81ZEnvData : public ControllerData {
  DeclareConcreteX(TX81ZEnvData, ControllerData);
  TX81ZEnvData();
  controllerdata_ptr_t clone() const final;
  ControllerInst* instantiate(layer_ptr_t layer) const final;
  int _attackRate = 0;
  int _decay1Rate  = 0;
  int _decay1Level = 0;
  int _decay2Rate  = 0;
  int _releaseRate = 0;
  int _egshift       = 0;
  int _rateScale     = 0;
  int _levScale     = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct TX81ZEnvInst : public ControllerInst {
  TX81ZEnvInst(const TX81ZEnvData* data, layer_ptr_t l);
  void compute() final;
  void keyOn(const KeyOnInfo& KOI) final;
  void keyOff() final;

  const TX81ZEnvData* _envdata;
  int state;
  double time_since_state;
  KeyOnInfo _koi;
  layer_ptr_t _layer         = nullptr;
  float _attackRate = 0.0f;
  float _decay1Rate  = 0.0f;
  float _decay1Level = 0.0f;
  float _decay2Rate  = 0.0f;
  float _releaseRate = 0.0f;
};

///////////////////////////////////////////////////////////////////////////////

struct envframe {
  std::string _name;
  int _index                    = 0;
  float _value                  = 0.0f;
  int _curseg                   = 0;
  const RateLevelEnvData* _data = nullptr;
};
struct asrframe {
  int _index           = 0;
  float _value         = 0.0f;
  int _curseg          = 0;
  const AsrData* _data = nullptr;
};

} // namespace ork::audio::singularity
