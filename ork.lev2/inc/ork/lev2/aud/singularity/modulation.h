#pragma once

#include <ork/lev2/aud/singularity/krztypes.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

struct DspParam {
  DspParam();
  void reset();
  void keyOn(int ikey, int ivel);
  float eval(bool dump = false);

  dspparam_constptr_t _data;

  controller_t _C1;
  controller_t _C2;
  float _keyOff  = 0.0f;
  float _unitVel = 0.0f;

  float _s1val = 0.0f;
  float _s2val = 0.0f;
  float _kval  = 0.0f;
  float _vval  = 0.0f;

  evalit_t _evaluator;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
