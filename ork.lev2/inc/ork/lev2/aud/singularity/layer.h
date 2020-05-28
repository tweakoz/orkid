////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <mutex>
#include "envelope.h"
#include "konoff.h"
#include "hud_data.h"

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
struct LayerData {
  LayerData();

  dspstagedata_ptr_t appendStage(const std::string& named);
  dspstagedata_ptr_t stageByIndex(int index);
  dspstagedata_ptr_t stageByName(const std::string& named);

  ///////////////////////////////////////////////////
  template <typename T>                                           //
  inline std::shared_ptr<T> appendController(std::string named) { //
    std::shared_ptr<T> controllerdata = _ctrlBlock->addController<T>();
    controllerdata->_name             = named;
    _controllerset.insert(controllerdata);
    return controllerdata;
  }
  ///////////////////////////////////////////////////
  scopesource_ptr_t createScopeSource();
  ///////////////////////////////////////////////////

  int _numdspblocks       = 0;
  int _loKey              = 0;
  int _hiKey              = 127;
  int _loVel              = 0;
  int _hiVel              = 127;
  float _channelGains[4]  = {0, 0, 0, 0};
  int _channelPans[4]     = {0, 0, 0, 0};
  int _channelPanModes[4] = {0, 0, 0, 0};
  bool _ignRels           = false;
  bool _atk1Hold          = false; // ThrAtt
  bool _atk3Hold          = false; // TilDec
  bool _usenatenv         = false; // todo: move to krz

  algdata_ptr_t _algdata;

  kmpblockdata_ptr_t _kmpBlock;
  dspblkdata_ptr_t _pchBlock;
  keymap_constptr_t _keymap;
  std::set<controllerdata_ptr_t> _controllerset;
  controlblockdata_ptr_t _ctrlBlock;
  scopesource_ptr_t _scopesource;
};

///////////////////////////////////////////////////////////////////////////////

struct Layer {

  Layer();
  ~Layer();

  void resize(int numframes);
  void compute(outputBuffer& obuf, int numframes);
  void keyOn(int note, int vel, lyrdata_constptr_t ld);
  void keyOff();
  void reset();
  controller_t getController(const std::string& n) const;
  controller_t getController(controllerdata_constptr_t cdat) const;

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

  int _dspwritebase;
  int _dspwritecount;

  int _curnote;
  int _curvel;
  int _ldindex;
  float _layerGain;
  float _curPitchOffsetInCents;
  float _centsPerKey;
  int _lyrPhase;
  bool _ignoreRelease;
  int64_t _testtoneph = 0;

  int _layerBasePitch; // in cents

  float _pchc1;
  float _pchc2;
  float _sinrepPH = 0.0f;
  bool _doNoise;
  float _layerTime;
  dspblk_ptr_t _pchBlock;

  ControlBlockInst* _ctrlBlock = nullptr;

  std::map<std::string, ControllerInst*> _controlMap;
  std::map<controllerdata_constptr_t, ControllerInst*> _controld2iMap;
  alg_ptr_t _alg;

  outputBuffer _layerObuf;
  dspbuf_ptr_t _dspbuffer;

  HudFrameControl _HKF;
  lyrdata_constptr_t _layerdata;
  oschardsynctrack_ptr_t _oschsynctracks[kmaxdspblocksperstage];
  scopesynctrack_ptr_t _scopesynctracks[kmaxdspblocksperstage];

private:
  int _keepalive;
};

} // namespace ork::audio::singularity
