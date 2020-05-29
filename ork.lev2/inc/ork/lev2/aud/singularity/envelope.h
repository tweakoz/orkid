#pragma once

#include "controller.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

struct EnvPoint {
  float _time  = 0.0f;
  float _level = 0.0f;
  float _power = 1.0f;
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
  RateLevelEnvData();
  ControllerInst* instantiate(Layer* layer) const final;
  bool isBiPolar() const;

  void addSegment(std::string name, float time, float level, float power = 1.0f);
  std::vector<EnvPoint> _segments;
  std::vector<std::string> _segmentNames;
  bool _ampenv;
  bool _bipolar;
  RlEnvType _envType;
  envadjust_method_t _envadjust;
  int _sustainSegment = -1;
};

///////////////////////////////////////////////////////////////////////////////

struct AsrData : public ControllerData {
  AsrData();
  ControllerInst* instantiate(Layer* layer) const final;

  std::string _trigger;
  std::string _mode;
  float _delay;
  float _attack;
  float _release;
  envadjust_method_t _envadjust;
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

///////////////////////////////////////////////////////////////////////////////

struct AsrInst : public ControllerInst {
  AsrInst(const AsrData* data, Layer* l);
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

struct RateLevelEnvInst : public ControllerInst {
  RateLevelEnvInst(const RateLevelEnvData* data, Layer* l);
  void compute() final;
  void keyOn(const KeyOnInfo& KOI) final;
  void keyOff() final;
  ////////////////////////////
  float shapedvalue() const;
  ////////////////////////////
  void initSeg(int iseg);
  bool done() const;
  const RateLevelEnvData* _data;
  Layer* _layer;
  int _segmentIndex;
  float _startval;
  float _destval;
  float _lerpindex;
  float _lerpincr;
  float _curpower = 1.0f;

  int _framesrem;
  bool _released;
  bool _ignoreRelease;
  bool _ampenv;
  int _state;
  RlEnvType _envType;
  KeyOnInfo _konoffinfo;
  int _updatecount = 0;
};

} // namespace ork::audio::singularity
