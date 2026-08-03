////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <mutex>
#include <atomic>
#include "reflection.h"
#include "envelope.h"
#include "konoff.h"
#include "hud_data.h"

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
// SF2 live encode, resolved onto a voice program by configureSoundFieldSend
//  (spatializer.h - the authoring surface and the settled semantics live
//  there). PLAIN DATA, not reflected: like _pchBlock/_keymap this is the
//  product of a program build, not authored program state.
//  the angle source is the voice's own PANNER2D ANGLE data param - the
//  emitter systems' per-frame azimuth write, which is what makes a moving
//  emitter track. it is held as STRUCTURAL COORDINATES into the alg graph
//  rather than as a pointer so that a cloned LayerData (which clones its own
//  alg graph) carries the send by plain copy instead of aliasing the original
//  layer's blocks. keyOn turns the three indices into the pointer.
///////////////////////////////////////////////////////////////////////////////
struct SoundFieldSendConfig {
  bool _enabled   = false;
  float _levelDB  = 0.0f;
  float _spread   = 0.0f;
  int _angleStage = -1;
  int _angleBlock = -1;
  int _angleParam = -1;
};
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
  int numDspStages() const;

  const ProgramData* _programdata = nullptr;
  int _loKey                      = 0;
  int _hiKey                      = 127;
  int _loVel                      = 0;
  int _hiVel                      = 127;
  std::atomic<float> _channelGains[4] = {0, 0, 0, 0};
  std::atomic<float> _channelPans[4]  = {0, 0, 0, 0};
  int _channelPanModes[4]         = {0, 0, 0, 0};
  bool _ignRels                   = false;
  bool _atk1Hold                  = false; // ThrAtt
  bool _atk3Hold                  = false; // TilDec
  bool _usenatenv                 = false; // todo: move to krz
  float _layerLinGain             = 1.0f;
  int _panmode = -1;
  int _pan = 0;
  int _headroom = 0;
  float _floatPan = 0.0f; // -1.0f to +1.0f

  algdata_ptr_t _algdata;
  std::string _outbus;
  // per-voice send: the voice's dry signal goes to _outbus as always, and a
  //  _sendLevel-weighted (post-fader) copy is summed into the NAMED _sendbus.
  //  empty _sendbus == dry only. this is the shared-send-bus shape: many
  //  voices feeding one reverb/aux instance rather than one reverb per voice.
  std::string _sendbus;
  float _sendLevel = 1.0f; // linear amplitude
  // the soundfield send is a SEPARATE, additional send: a voice may have both,
  //  and it never touches _sendbus/_sendLevel.
  SoundFieldSendConfig _soundfieldSend;
  std::string _name;

  kmpblockdata_ptr_t _kmpBlock;
  dspblkdata_ptr_t _pchBlock;
  keymap_ptr_t _keymap;
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

  // the raw instance behind a controller reference, or null when the layer's
  //  control block has none by that name/identity. the callables above are thin
  //  wrappers on these: a controller_t that captures the INSTANCE fits inline,
  //  one that captures another controller_t never can.
  ControllerInst* getControllerInst(const std::string& n) const;
  ControllerInst* getControllerInst(controllerdata_constptr_t cdat) const;

  // called by ControlBlockInst::keyOn as it instantiates. the layer's slots are
  //  a lookup index ONLY - _ctrlBlock owns the instances (see releaseControllers).
  void bindController(const ControllerData* cdat, ControllerInst* cinst);
  void releaseControllers();

  controller_t getSRC1(dspparammod_constptr_t mods);
  controller_t getSRC2(dspparammod_constptr_t mods);

  void mixToBus(int base, int count);
  void mixToSendBus(int base, int count);
  void encodeToSoundField(int base, int count);
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

  programinst_ptr_t _programinst = nullptr;
  std::mutex _mutex;

  // voice-pool bookkeeping, owned by synth (see RtPoolSet).
  //  _poolIndex is assigned once at pool construction and never changes.
  //  _wantsDeactivate replaces the old deactivation queue: releaseLayer only
  //  raises it, and deactivateVoices sweeps the active set for it.
  int _poolIndex        = -1;
  bool _wantsDeactivate = false;

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
  // send target, resolved from LayerData::_sendbus by synth::_keyOnLayer (the
  //  bus map is not touched from the audio thread). null == no send.
  outbus_ptr_t _sendbus;
  float _sendLevel = 0.0f;
  // soundfield send, resolved from LayerData::_soundfieldSend by
  //  synth::_keyOnLayer. null _sfield == no encode; the pointer is the field
  //  ALREADY published by whoever created it (SoundField::rtInstance) - the
  //  keyOn path must never construct one, that spawns a thread.
  //  _sfGains carries the previous control pass's four encode gains so the
  //  pass can ramp into the new ones (anti-zipper, pass-count driven: no wall
  //  clock, so two renders stay byte-identical).
  //  _sfAngleParam is the voice's OWN panner ANGLE param instance, read with
  //  eval() exactly as PANNER2D reads it: SimpleSoundEmitter drives that param
  //  through the data's _coarse and StochWavSoundEmitter drives it through a
  //  controller, and eval() is the one read that sees both.
  SoundField* _sfield             = nullptr;
  DspParam* _sfAngleParam         = nullptr;
  float _sfLevelLin               = 0.0f;
  float _sfSpread                 = 0.0f;
  float _sfGains[kfoanumchannels] = {0.0f, 0.0f, 0.0f, 0.0f};
  bool _sfPrimed                  = false;
  KeyOnInfo _koi;
  scopesource_ptr_t _scopesource;
  keyonmod_ptr_t _keymods;
  std::string _name;
  // PERSISTENT: constructed once with the layer, cleared (not replaced) at every
  //  keyOn. a per-note make_shared here was one of the note-on's allocations.
  ctrlblockinst_ptr_t _ctrlBlock;

  // the controller lookup index, by data identity and by name. a closed array
  //  (never grows, never allocates) sized by the same cap ControlBlockInst uses
  //  for the instances it owns. these are NON-OWNING aliases: every slot is
  //  dropped in releaseControllers, in the same statement that destroys the
  //  instances, so no lookup can ever hand out a freed instance.
  struct ControllerSlot {
    const ControllerData* _data = nullptr;
    ControllerInst* _inst       = nullptr;
  };
  ControllerSlot _controlSlots[kmaxctrlperblock];
  int _numControlSlots = 0;

  alg_ptr_t _alg;

  bool _is_bus_processor = false;

  dspbuf_ptr_t _dspbuffer;

  HudFrameControl _HKF;
  lyrdata_constptr_t _layerdata;
  oschardsynctrack_ptr_t _oschsynctracks[kmaxdspblocksperstage];
  scopesynctrack_ptr_t _scopesynctracks[kmaxdspblocksperstage];

  float currentPan() const;
  // the voice's current output weighting (program/layer/bus gain and pan) —
  //  shared by the dry mix and the send so a send always tracks its voice.
  void currentMixGains(float& gain, float& panl, float& panr) const;

private:

  friend struct synth;

  int _keepalive;
};

using layer_ptr_t = std::shared_ptr<Layer>;


} // namespace ork::audio::singularity
