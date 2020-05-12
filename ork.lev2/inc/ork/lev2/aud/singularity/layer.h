#pragma once

#include <mutex>
#include "alg.h"
#include "envelope.h"
#include "konoff.h"
#include "hud_data.h"

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
constexpr int KPARALLELBLOCKMAX = 4; // max number of parallel blocks per layer
///////////////////////////////////////////////////////////////////////////////

struct layer {

  layer(synth& syn);
  ~layer();

  void compute(outputBuffer& obuf);
  void keyOn(int note, int vel, lyrdata_constptr_t ld);
  void keyOff();
  void reset();
  controller_t getController(const std::string& n) const;

  controller_t getSRC1(const BlockModulationData& mods);
  controller_t getSRC2(const BlockModulationData& mods);

  void updateSampSRRatio();

  void retain();
  void release();
  bool isDone() const {
    return _keepalive <= 0;
  }
  bool isHudLayer() const;

  std::mutex _mutex;

  synth& _syn;
  int _curnote;
  int _curvel;
  int _ldindex;
  float _layerGain;
  float _curPitchOffsetInCents;
  float _centsPerKey;
  int _lyrPhase;
  bool _useNatEnv;
  bool _ignoreRelease;

  int _layerBasePitch; // in cents

  float _pchc1;
  float _pchc2;
  float _sinrepPH = 0.0f;
  bool _doNoise;
  float _masterGain = 0.0f;
  float* _AENV;
  float _USERAMPENV[1024];
  float _layerTime;
  dspblk_ptr_t _pchBlock;

  ControlBlockInst* _ctrlBlock[kmaxctrlblocks] = {0, 0, 0, 0, 0, 0, 0, 0};

  std::map<std::string, ControllerInst*> _controlMap;
  alg_ptr_t _alg;

  outputBuffer _layerObuf;
  dspbuf_ptr_t _dspbuffer;

  hudkframe _HKF;
  hudaframe _HAF;
  int _HAF_nenvseg;

  lyrdata_constptr_t _LayerData;

private:
  int _keepalive;
};

} // namespace ork::audio::singularity
