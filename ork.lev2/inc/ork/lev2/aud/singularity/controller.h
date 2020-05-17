#pragma once

#include "alg.h"
#include "konoff.h"

namespace ork::audio::singularity {

struct LfoData;
struct FunData;

///////////////////////////////////////////////////////////////////////////////

struct ControlBlockData {
  template <typename T> std::shared_ptr<T> addController() {
    OrkAssert((_numcontrollers + 1) <= kmaxctrlperblock);
    auto c                    = std::make_shared<T>();
    _cdata[_numcontrollers++] = c;
    return c;
  }
  controllerdata_constptr_t _cdata[kmaxctrlperblock];
  size_t _numcontrollers = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct ControllerData {
  virtual ControllerInst* instantiate(Layer* layer) const = 0;
  virtual ~ControllerData() {
  }

  std::string _name;
};

///////////////////////////////////////////////////////////////////////////////

struct ControllerInst {
  ControllerInst(Layer* layer);
  virtual ~ControllerInst() {
  }

  virtual void keyOn(const KeyOnInfo& KOI) = 0;
  virtual void keyOff()                    = 0;
  virtual void compute(int inumfr)         = 0;
  float getval() const {
    return _curval;
  }

  float _curval;
  Layer* _layer = nullptr;
};

struct ControlBlockInst {
  void compute(int inumfr);
  void keyOn(const KeyOnInfo& KOI, controlblockdata_constptr_t CBD);
  void keyOff();

  ControllerInst* _cinst[kmaxctrlperblock] = {0, 0, 0, 0};
};

///////////////////////////////////////////////////////////////////////////////

struct LfoData : public ControllerData {
  LfoData();
  ControllerInst* instantiate(Layer* layer) const final;

  float _initialPhase;
  float _minRate;
  float _maxRate;
  std::string _controller;
  std::string _shape;
};

///////////////////////////////////////////////////////////////////////////////

struct LfoInst : public ControllerInst {
  LfoInst(const LfoData* data, Layer* layer);

  void reset();
  void keyOn(const KeyOnInfo& KOI) final;
  void keyOff() final;
  void compute(int inumfr) final;
  ////////////////////////////
  const LfoData* _data;
  float _phaseInc;
  float _phase;
  float _currate;
  bool _enabled;
  float _rateLerp;
  float _bias;
  mapper_t _mapper;
};

///////////////////////////////////////////////////////////////////////////////

struct FunData : public ControllerData {
  ControllerInst* instantiate(Layer* layer) const final;

  std::string _a, _b, _op;
};

///////////////////////////////////////////////////////////////////////////////

struct FunInst : public ControllerInst {
  FunInst(const FunData* data, Layer* layer);
  void compute(int inumfr) final;
  void keyOn(const KeyOnInfo& KOI) final;
  void keyOff() final;
  ////////////////////////////
  const FunData* _data;
  controller_t _a;
  controller_t _b;
  controller_t _op;
};

} // namespace ork::audio::singularity
