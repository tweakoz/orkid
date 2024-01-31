////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <mutex>
#include "reflection.h"
#include "envelope.h"
#include "konoff.h"
#include "hud_data.h"

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
struct LayerData : public ork::Object {

  DeclareConcreteX(LayerData, ork::Object);
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) override;

  LayerData(const ProgramData* pdat = nullptr);
  lyrdata_ptr_t clone() const;
  dspstagedata_ptr_t appendStage(const std::string& named);
  dspstagedata_ptr_t stageByIndex(int index);
  dspstagedata_ptr_t stageByName(const std::string& named);

  ///////////////////////////////////////////////////
  template <typename T>                                                  //
  inline std::shared_ptr<T> appendController(const std::string& named) { //
    std::shared_ptr<T> controllerdata = _ctrlBlock->addController<T>(named);
    //_controllermap[named]             = controllerdata;
    return controllerdata;
  }
  ///////////////////////////////////////////////////
  //controllerdata_ptr_t controllerByName(const std::string& named);
  ///////////////////////////////////////////////////
  scopesource_ptr_t createScopeSource();
  ///////////////////////////////////////////////////
  int numDspBlocks() const;

  const ProgramData* _programdata = nullptr;
  int _loKey                      = 0;
  int _hiKey                      = 127;
  int _loVel                      = 0;
  int _hiVel                      = 127;
  float _channelGains[4]          = {0, 0, 0, 0};
  float _channelPans[4]           = {0, 0, 0, 0};
  int _channelPanModes[4]         = {0, 0, 0, 0};
  bool _ignRels                   = false;
  bool _atk1Hold                  = false; // ThrAtt
  bool _atk3Hold                  = false; // TilDec
  bool _usenatenv                 = false; // todo: move to krz
  float _layerLinGain             = 1.0f;
  int _panmode = -1;
  int _pan = 0;
  int _headroom = 0;

  algdata_ptr_t _algdata;
  std::string _outbus;
  std::string _name;

  kmpblockdata_ptr_t _kmpBlock;
  dspblkdata_ptr_t _pchBlock;
  keymap_constptr_t _keymap;
  //std::map<std::string, controllerdata_ptr_t> _controllermap;
  controlblockdata_ptr_t _ctrlBlock = nullptr;
  varmap::varmap_ptr_t _varmap;

  scopesource_ptr_t _scopesource = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct Layer {

  Layer();
  ~Layer();

  void resize(int numframes);
  void beginCompute(int numframes);
  void updateControllers();
  void compute(int base, int count);
  void endCompute();

  void reset();
  controller_t getController(const std::string& n) const;
  controller_t getController(controllerdata_constptr_t cdat) const;

  controller_t getSRC1(dspparammod_constptr_t mods);
  controller_t getSRC2(dspparammod_constptr_t mods);

  void mixToBus(int base, int count);
  void replaceBus(int base, int count);
  void updateScopes(int ibase, int icount);

  void updateSampSRRatio();

  void retain();
  bool isDone() const {
    return _keepalive <= 0;
  }
  bool isHudLayer() const;

  void keyOn(int note, int velocity, lyrdata_ptr_t ld, outbus_ptr_t obus);
  void reTriggerMono(int note, int velocity);
  void keyOff();

  std::mutex _mutex;

  int _dspwritebase;
  int _dspwritecount;
  int _numFramesForBlock = 0;

  int _curnote;
  int _curvel;
  int _ldindex;
  float _layerLinGain = 1.0f;
  float _gainModifier = 1.0f;
  float _curPitchOffsetInCents;
  float _curPitchInCents;
  float _centsPerKey;
  int _lyrPhase;
  bool _ignoreRelease;
  int64_t _testtoneph  = 0;
  int64_t _sampleindex = 0;

  int _layerBasePitch; // in cents
  float _ampenvgain = 1.0f;
  float _pchc1;
  float _pchc2;
  float _sinrepPH = 0.0f;
  bool _doNoise;
  float _layerTime;
  dspblk_ptr_t _pchBlock;
  outbus_ptr_t _outbus;
  KeyOnInfo _koi;
  scopesource_ptr_t _scopesource;
  keyonmod_ptr_t _keymods;
  std::string _name;
  ctrlblockinst_ptr_t _ctrlBlock = nullptr;

  std::map<std::string, ControllerInst*> _controlMap;
  std::map<controllerdata_constptr_t, ControllerInst*> _controld2iMap;
  alg_ptr_t _alg;

  bool _is_bus_processor = false;

  dspbuf_ptr_t _dspbuffer;

  HudFrameControl _HKF;
  lyrdata_constptr_t _layerdata;
  oschardsynctrack_ptr_t _oschsynctracks[kmaxdspblocksperstage];
  scopesynctrack_ptr_t _scopesynctracks[kmaxdspblocksperstage];

  float currentPan() const;

private:

  friend struct synth;

  int _keepalive;
};

using layer_ptr_t = std::shared_ptr<Layer>;


} // namespace ork::audio::singularity
