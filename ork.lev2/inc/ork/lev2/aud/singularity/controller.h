#pragma once

#include "alg.h"
#include "konoff.h"

namespace ork::audio::singularity {

struct LfoData;
struct FunData;

///////////////////////////////////////////////////////////////////////////////

struct ControllerInst {
  ControllerInst();
  virtual ~ControllerInst() {
  }

  virtual void keyOn(const KeyOnInfo& KOI) = 0;
  virtual void keyOff()                    = 0;
  virtual void compute(int inumfr)         = 0;
  float getval() const {
    return _curval;
  }

  float _curval;
};

struct ControlBlockInst {
  void compute(int inumfr);
  void keyOn(const KeyOnInfo& KOI, const ControlBlockData* CBD);
  void keyOff();

  ControllerInst* _cinst[kmaxctrlperblock] = {0, 0, 0, 0};
};

///////////////////////////////////////////////////////////////////////////////

struct LfoInst : public ControllerInst {
  LfoInst(const LfoData* data);

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

struct FunInst : public ControllerInst {
  FunInst(const FunData* data);
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
