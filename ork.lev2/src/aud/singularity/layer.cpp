////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/keyon_prof.h>
#include <ork/lev2/aud/singularity/soundfield.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/aud/singularity/alg_pan.inl>

ImplementReflectionX(ork::audio::singularity::LayerData, "SynLayer");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void LayerData::describeX(class_t* clazz) {
  // clazz->directObjectMapProperty("Controllers", &LayerData::_controllermap);
  clazz->directObjectProperty("Algorithm", &LayerData::_algdata);
  clazz->directProperty("SendBus", &LayerData::_sendbus);
  clazz->directProperty("SendLevel", &LayerData::_sendLevel);
}

///////////////////////////////////////////////////////////////////////////////

bool LayerData::postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) {
  int icid = 0;
  // for (auto item : _controllermap) {
  // auto controller            = item.second;
  //_ctrlBlock->_controller_datas[icid++] = controller;
  //}
  //_ctrlBlock->_numcontrollers = _controllermap.size();
  return true;
}

///////////////////////////////////////////////////////////////////////////////

LayerData::LayerData(const ProgramData* pdata)
    : _programdata(pdata) {
  _pchBlock    = nullptr;
  _algdata     = std::make_shared<AlgData>();
  _ctrlBlock   = std::make_shared<ControlBlockData>();
  _kmpBlock    = std::make_shared<KmpBlockData>(); // todo move to samplerdata
  _scopesource = nullptr;
  _varmap      = std::make_shared<varmap::VarMap>();
}
lyrdata_ptr_t LayerData::clone() const {
  auto rval           = std::make_shared<LayerData>();
  rval->_programdata  = _programdata;
  rval->_loKey        = _loKey;
  rval->_hiKey        = _hiKey;
  rval->_loVel        = _loVel;
  rval->_hiVel        = _hiVel;
  rval->_ignRels      = _ignRels;
  rval->_atk1Hold     = _atk1Hold;
  rval->_atk3Hold     = _atk3Hold;
  rval->_usenatenv    = _usenatenv;
  rval->_layerLinGain = _layerLinGain;
  rval->_algdata      = _algdata->clone();
  rval->_outbus       = _outbus;
  rval->_sendbus      = _sendbus;
  rval->_sendLevel    = _sendLevel;
  rval->_soundfieldSend = _soundfieldSend; // structural indices: clone-safe
  rval->_name         = _name;
  rval->_kmpBlock     = _kmpBlock->clone();
  rval->_pchBlock     = _pchBlock->clone();
  rval->_keymap       = _keymap;
  rval->_ctrlBlock    = _ctrlBlock->clone();
  rval->_varmap       = _varmap;
  rval->_scopesource  = _scopesource;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
int LayerData::numDspStages() const {
  int dsps = int(_algdata->_numstages);
  return dsps;
}
int LayerData::numDspBlocks() const {
  int dspb = 0;
  for (int istage = 0; istage < _algdata->_numstages; istage++) {
    auto stage = _algdata->_stages[istage];
    dspb += stage->_numblocks;
  }
  return dspb;
}
///////////////////////////////////////////////////////////////////////////////
dspstagedata_ptr_t LayerData::appendStage(const std::string& named) {
  return _algdata->appendStage(named);
}
///////////////////////////////////////////////////////////////////////////////
dspstagedata_ptr_t LayerData::stageByName(const std::string& named) {
  return _algdata->stageByName(named);
}
///////////////////////////////////////////////////////////////////////////////
dspstagedata_ptr_t LayerData::stageByIndex(int index) {
  return _algdata->stageByIndex(index);
}
///////////////////////////////////////////////////////////////////////////////
scopesource_ptr_t LayerData::createScopeSource() {
  _scopesource = std::make_shared<ScopeSource>();
  return _scopesource;
}
///////////////////////////////////////////////////////////////////////////////

Layer::Layer()
    : _curnote(0)
    , _layerLinGain(1.0)
    , _curPitchOffsetInCents(0.0f)
    , _centsPerKey(100.0f)
    , _lyrPhase(-1)
    , _doNoise(false)
    , _alg(nullptr)
    , _layerdata(nullptr)
    , _keepalive(0) {
  // printf( "Layer Init<%p>\n", this );
  _dspbuffer = std::make_shared<DspBuffer>();
  _ctrlBlock = std::make_shared<ControlBlockInst>();

  for (int i = 0; i < kmaxdspblocksperstage; i++) {
    _oschsynctracks[i]  = std::make_shared<OscillatorSyncTrack>();
    _scopesynctracks[i] = std::make_shared<ScopeSyncTrack>();
  }
}

Layer::~Layer() {
  std::lock_guard<std::mutex> lock(_mutex);
  _pchBlock  = nullptr;
  _outbus    = nullptr;
  _alg       = nullptr;
  releaseControllers();
  _ctrlBlock = nullptr;
  _dspbuffer = nullptr;
  _layerdata = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// releaseControllers - the ONE place a note's controller instances die.
//  ORDER IS LOAD BEARING: the dsp grid's DspParam::_C1/_C2 hold raw instance
//  pointers, so the alg goes first (synth::_reclaimVoice already returned it
//  when the voice was freed; this covers the direct-reset path too) and the
//  lookup slots are dropped in the same breath as the instances themselves.
///////////////////////////////////////////////////////////////////////////////

void Layer::releaseControllers() {
  _alg = nullptr;
  for (int i = 0; i < _numControlSlots; i++) {
    _controlSlots[i]._data = nullptr;
    _controlSlots[i]._inst = nullptr;
  }
  _numControlSlots = 0;
  if (_ctrlBlock)
    _ctrlBlock->clear();
}

///////////////////////////////////////////////////////////////////////////////

void Layer::bindController(const ControllerData* cdat, ControllerInst* cinst) {
  for (int i = 0; i < _numControlSlots; i++) {
    if (_controlSlots[i]._data == cdat) {
      _controlSlots[i]._inst = cinst;
      return;
    }
  }
  OrkAssertIFMT(
      _numControlSlots < kmaxctrlperblock, //
      "singularity layer controller slots exhausted (%d)",
      kmaxctrlperblock);
  _controlSlots[_numControlSlots]._data = cdat;
  _controlSlots[_numControlSlots]._inst = cinst;
  _numControlSlots++;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::reset() {
  _layerdata = nullptr;
  _curnote   = 0;
  _keepalive = 0;

  releaseControllers();
}

///////////////////////////////////////////////////////////////////////////////

void Layer::reTriggerMono(int note, int velocity){
  this->_layerBasePitch = clip_float(note * 100, -0, 12700);
  this->_curnote = note;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::keyOn(int note, int velocity, lyrdata_ptr_t ld, outbus_ptr_t obus) {
  KeyOnProfScope prof(KOP_LAYERKEYON);
  this->reset();
  this->_HKF._miscText   = "";
  this->_HKF._note       = note;
  this->_HKF._vel        = velocity;
  this->_HKF._layerdata  = ld;
  this->_HKF._layerIndex = this->_ldindex;
  this->_HKF._useFm4     = false;
  this->_layerBasePitch = clip_float(note * 100, -0, 12700);

  this->_ignoreRelease = ld->_ignRels;
  this->_curnote       = note;
  this->_layerdata     = ld;
  this->_outbus        = obus;
  // cleared here, resolved by synth::_keyOnLayer (which owns the bus map):
  //  a recycled/stolen voice must never inherit the previous voice's send.
  this->_sendbus       = nullptr;
  this->_sendLevel     = 0.0f;
  this->_sfield        = nullptr;
  this->_sfAngleParam  = nullptr;
  this->_sfLevelLin    = 0.0f;
  this->_sfSpread      = 0.0f;
  this->_sfPrimed      = false;
  this->_layerLinGain  = ld->_layerLinGain;
  this->_gainModifier = decibel_to_linear_amp_ratio(obus->_prog_gain);

  this->_curvel = velocity;

  this->_layerTime = 0.0f;

  this->retain();

  /////////////////////////////////////////////
  // controllers
  /////////////////////////////////////////////

  if (ld->_ctrlBlock) {
    KeyOnProfScope profcb(KOP_CTRLBLOCK);
    this->_ctrlBlock->keyOn(this->_koi, ld->_ctrlBlock);
  }

  ///////////////////////////////////////
  auto algname = ld->_algdata->_name;
  // printf( "LAYER KEYON<%d> alg<%s>\n", note, algname.c_str() );

  {
    KeyOnProfScope profac(KOP_ALGCREATE);
    this->_alg = this->_layerdata->_algdata->createAlgInst();
  }
  // assert(_alg);
  if (this->_alg) {
    this->_alg->keyOn(this->_koi);
  }

  this->_HKF._alg = this->_alg;

  ///////////////////////////////////////

  this->_lyrPhase = 0;
  this->_sinrepPH = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::keyOff() {
  if (this->_ctrlBlock)
    this->_ctrlBlock->keyOff();
  if (this->_ignoreRelease)
    return;
  if (this->_alg)
    this->_alg->keyOff();
}

///////////////////////////////////////////////////////////////////////////////

void Layer::resize(int numframes) {
  for (int i = 0; i < kmaxdspblocksperstage; i++) {
    _oschsynctracks[i]->resize(numframes);
    _scopesynctracks[i]->resize(numframes);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Layer::retain() {
  ++_keepalive;

  // printf( "layer<%p> retain cnt<%d>\n", this, _keepalive );
}

///////////////////////////////////////////////////////////////////////////////
void Layer::compute(int base, int count) {
  _dspwritecount = count;
  _dspwritebase  = base;
  ///////////////////////
  if (nullptr == _layerdata) {
    printf("gotnull ld layer<%p>\n", (void*)this);
    return;
  }
  ////////////////////////////////////////
  if (true) {
    ///////////////////////
    if (_alg)
      _alg->doComputePass();
    ///////////////////////
    // the pass width, not the constant: layer time must track the frames this
    //  pass actually produced (a chunk tail pass can be narrower).
    _sampleindex += count;
    _layerTime = float(_sampleindex) * getInverseSampleRate();
  }
  ////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void Layer::updateControllers() {
  if (_ctrlBlock)
    _ctrlBlock->compute();
}
///////////////////////////////////////////////////////////////////////////////
void Layer::beginCompute(int numframes) {

  _numFramesForBlock = numframes;

  _dspbuffer->resize(numframes);

  float* lyroutl = _dspbuffer->channel(0);
  float* lyroutr = _dspbuffer->channel(1);

  if (_is_bus_processor) {
  } else {
    for (int i = 0; i < numframes; i++) {
      lyroutl[i] = 0.0f;
      lyroutr[i] = 0.0f;
    }
  }

  _dspwritecount = frames_per_controlpass;
  _dspwritebase  = 0;

  if (_alg)
    _alg->beginCompute();
}
///////////////////////////////////////////////////////////////////////////////
float Layer::currentPan() const{
  int panmode = _layerdata->_panmode;
  int pan = _layerdata->_pan;
  float fpan = float(pan-7)/7.0;
  switch(panmode) {
    case 0: // Fixed
      break;
    case 1: // +MIDI
      fpan += 0.5f;
      break;
    case 2: { // Auto
      int ko = _curnote-60;
      fpan = float(ko)/60.0;
      break;
    }
    case 3: { // Reverse(Auto)
      int ko = -(_curnote-60);
      fpan = float(ko)/60.0;
      break;
    }
    case 4: { // Fixed (floatpan)
      fpan = _layerdata->_floatPan;
      break;
    }
  }
  return fpan;
}
///////////////////////////////////////////////////////////////////////////////
void Layer::currentMixGains(float& gain, float& panl, float& panr) const {
  float prggain = decibel_to_linear_amp_ratio(_layerdata->_programdata->_gainDB);
  prggain *= decibel_to_linear_amp_ratio(_programinst->_gain);
  prggain *= _programinst->_fadeGainLinear;
  float fpan     = currentPan();
  float headroom = decibel_to_linear_amp_ratio(_layerdata->_headroom);
  panl = panBlend(fpan).lmix;
  panr = panBlend(fpan).rmix;
  gain = prggain * _layerLinGain * _gainModifier * headroom;
}
///////////////////////////////////////////////////////////////////////////////
void Layer::mixToBus(int base, int count) {
  float* lyroutl  = _dspbuffer->channel(0) + base;
  float* lyroutr  = _dspbuffer->channel(1) + base;
  auto& out_buf   = _outbus->_buffer;
  float* bus_outl = out_buf._leftBuffer + base;
  float* bus_outr = out_buf._rightBuffer + base;
  //////////////////////////////////
  float LG, panL, panR;
  currentMixGains(LG, panL, panR);
  //////////////////////////////////
  for (int i = 0; i < count; i++) {
    bus_outl[i] += (lyroutl[i]*LG*panL);
    bus_outr[i] += (lyroutr[i]*LG*panR);
  }
  if (0) { // test tone
    for (int i = 0; i < count; i++) {
      double phase = 120.0 * pi2 * double(_testtoneph) / getSampleRate();
      float samp   = sinf(phase) * .25;
      bus_outl[i]  = samp * _layerLinGain * _gainModifier;
      bus_outr[i]  = samp * _layerLinGain * _gainModifier;
      _testtoneph++;
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
// mixToSendBus - per-voice send. the dry signal already went to _outbus in
//  mixToBus; this sums a _sendLevel-weighted (post-fader: same gain and pan as
//  the dry) copy into the voice's send bus.
//  CALLED SERIALLY on the audio thread AFTER the per-bus mix jobs have joined:
//  a send crosses bus boundaries, so running it inside _jobMixBusLayers would
//  let two bus workers write one send-bus buffer concurrently.
///////////////////////////////////////////////////////////////////////////////
void Layer::mixToSendBus(int base, int count) {
  if (nullptr == _sendbus)
    return;
  float* lyroutl   = _dspbuffer->channel(0) + base;
  float* lyroutr   = _dspbuffer->channel(1) + base;
  auto& send_buf   = _sendbus->_buffer;
  float* send_outl = send_buf._leftBuffer + base;
  float* send_outr = send_buf._rightBuffer + base;
  //////////////////////////////////
  float LG, panL, panR;
  currentMixGains(LG, panL, panR);
  LG *= _sendLevel;
  //////////////////////////////////
  for (int i = 0; i < count; i++) {
    send_outl[i] += (lyroutl[i]*LG*panL);
    send_outr[i] += (lyroutr[i]*LG*panR);
  }
}
///////////////////////////////////////////////////////////////////////////////
// encodeToSoundField - SF2 live encode. an ADDITIONAL send, independent of
//  _sendbus: the dry copy already went to _outbus, and this sums a
//  first-order-ambisonic encode of the same voice into the one B-format mix
//  point, where it is rotated (see SoundField::computeIntoBus), decoded and
//  written to the "soundfield" bus.
//  CALLED SERIALLY on the audio thread from the same per-voice send pass as
//  mixToSendBus, and therefore ahead of the field's own compute in this
//  control pass. arithmetic only - the field pointer, the level, the spread
//  and the angle param were all resolved at keyOn.
//
//  AZIMUTH: the panner's ANGLE is a = -atan2(x,z) in engine listener space
//  (X=right, Y=up, Z=back), which makes the source's horizontal unit direction
//  v = (-sin a, 0, cos a). the fixed engine->ambisonic permutation is
//  ambi(v) = (-v.z, -v.x, v.y) (soundfield.h), so the ambisonic components of
//  that direction are cos(az) = -cos a and sin(az) = sin a, i.e. az = pi - a.
//  (sanity: a source dead ahead is engine -Z, so a = pi and az = 0; a source
//  to the right gives a = -pi/2 and az = -pi/2, which is -Y, and ambisonic +Y
//  is the LEFT.)
///////////////////////////////////////////////////////////////////////////////
void Layer::encodeToSoundField(int base, int count) {
  if (nullptr == _sfield)
    return;
  // the cached pointer is NOT owning - an owning one would let ~SoundField (it
  //  JOINS the feeder thread) run on the audio thread when the last voice
  //  releases. so a voice that outlives a tearDown re-checks publication, the
  //  same one atomic load synth::compute already does per pass, and goes dry.
  if (_sfield != SoundField::rtInstance()) {
    _sfield = nullptr;
    return;
  }
  const float a = _sfAngleParam ? _sfAngleParam->eval() : 0.0f;
  const float cos_az = -cosf(a);
  const float sin_az = sinf(a);
  // elevation has no source in the live path today; the term stays so a 3D
  //  spatializer only has to supply el.
  constexpr float el = 0.0f;
  const float cos_el = cosf(el);
  const float sin_el = sinf(el);
  //////////////////////////////////
  // post-fader like every other send: the send tracks its voice's gain. the
  //  PAN half of currentMixGains is deliberately unused - direction is the
  //  encode's job (see SoundField::accumulateLive).
  float LG, panL, panR;
  currentMixGains(LG, panL, panR);
  const float amp = LG * _sfLevelLin;
  // SPREAD as directivity interpolation: W whole, directional x (1-spread).
  const float dir = amp * (1.0f - _sfSpread);
  //////////////////////////////////
  float gains[kfoanumchannels];
  gains[int(FoaChannel::W)] = amp * float(1.0 / sqrt2);
  gains[int(FoaChannel::Y)] = dir * sin_az * cos_el;
  gains[int(FoaChannel::Z)] = dir * sin_el;
  gains[int(FoaChannel::X)] = dir * cos_az * cos_el;
  //////////////////////////////////
  // the first pass of a note has no previous gains to ramp from - starting at
  //  zero would fade every note in over one control pass.
  if (not _sfPrimed) {
    for (int ch = 0; ch < kfoanumchannels; ch++)
      _sfGains[ch] = gains[ch];
    _sfPrimed = true;
  }
  const float* lyroutl = _dspbuffer->channel(0) + base;
  const float* lyroutr = _dspbuffer->channel(1) + base;
  _sfield->accumulateLive(lyroutl, lyroutr, count, _sfGains, gains);
  for (int ch = 0; ch < kfoanumchannels; ch++)
    _sfGains[ch] = gains[ch];
}
///////////////////////////////////////////////////////////////////////////////
void Layer::replaceBus(int base, int count) {
  OrkAssert(_is_bus_processor);
  const float* lyroutl = _dspbuffer->channel(0);
  const float* lyroutr = _dspbuffer->channel(1);
  auto& out_buf        = _outbus->_buffer;
  float* bus_outl      = out_buf._leftBuffer + base;
  float* bus_outr      = out_buf._rightBuffer + base;
  //////////////////////////////////
  float fpan = currentPan();
  float panL = panBlend(fpan).lmix;
  float panR = panBlend(fpan).rmix;
  float headroom = decibel_to_linear_amp_ratio(_layerdata->_headroom);
  float LG = _layerLinGain * _gainModifier * headroom;
  //////////////////////////////////
  for (int i = 0; i < count; i++) {
    bus_outl[i] = (lyroutl[i]*LG*panL);
    bus_outr[i] = (lyroutr[i]*LG*panR);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Layer::endCompute() {
  if (_alg) {
    _alg->endCompute();
  }
}
//////////////////////////////////////
// SignalScope
//////////////////////////////////////
void Layer::updateScopes(int ibase, int icount) {
  if (this == synth::instance()->_hudLayer.get()) {
    if (_layerdata and _layerdata->_scopesource) {
      const float* lyroutl = _dspbuffer->channel(0) + ibase;
      const float* lyroutr = _dspbuffer->channel(1) + ibase;
      _layerdata->_scopesource->updateStereo(icount, lyroutl, lyroutr);
      // if (_alg)
      //_alg->notifySinks();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

bool Layer::isHudLayer() const {
  return (this == synth::instance()->_hudLayer.get());
}

///////////////////////////////////////////////////////////////////////////////

ControllerInst* Layer::getControllerInst(controllerdata_constptr_t cdat) const {
  const ControllerData* key = cdat.get();
  for (int i = 0; i < _numControlSlots; i++) {
    if (_controlSlots[i]._data == key)
      return _controlSlots[i]._inst;
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

ControllerInst* Layer::getControllerInst(const std::string& srcn) const {
  for (int i = 0; i < _numControlSlots; i++) {
    if (_controlSlots[i]._data->_name == srcn)
      return _controlSlots[i]._inst;
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

controller_t Layer::getController(controllerdata_constptr_t cdat) const {
  auto cinst = getControllerInst(cdat);
  if (cinst) {
    return [cinst]() { return cinst->_value.x; };
  }
  return []() { return 0.0f; };
}

///////////////////////////////////////////////////////////////////////////////

controller_t Layer::getController(const std::string& srcn) const {
  auto cinst = getControllerInst(srcn);
  if (cinst) {
    // printf("getcon<%s> -> %p\n", srcn.c_str(), cinst);
    return [cinst]() { return cinst->_value.x; };
  } else {
    // auto cdata = _layerdata->controllerByName(scrn);
    printf("CONTROLLER<%s> not found!\n", srcn.c_str());
    float fv = atof(srcn.c_str());
    if (fv != 0.0f) {
      return [=]() { // printf( "fv<%f>\n", fv);
        return fv;
      };
    }
  }

  return [] { return 0.0f; };
}

///////////////////////////////////////////////////////////////////////////////
// getSRC1/getSRC2 - the per-note modulation binding. these capture the raw
//  ControllerInst (null == the "no source" 0.0f substitution the map lookup
//  used to make) and the modulation DATA, never a nested controller_t: the
//  depths and scales stay live reads off the data so a parameter edit is heard
//  without a re-key, exactly as when this was a closure over the shared_ptr.
//  the data outlives the binding - DspParam::_data holds the DspParamData that
//  owns these mods, and _C1/_C2 are only ever written beside _data.
///////////////////////////////////////////////////////////////////////////////

controller_t Layer::getSRC1(dspparammod_constptr_t mods) {
  const BlockModulationData* MODS = mods.get();
  ControllerInst* src1            = this->getControllerInst(mods->_src1);

  return [src1, MODS]() -> float {
    float src1val   = src1 ? src1->_value.x : 0.0f;
    float src1scale = MODS->_src1Scale;
    float out       = src1val * src1scale + MODS->_src1Bias;
    // printf( "src1out<%f>\n", out );
    return out;
  };
}

controller_t Layer::getSRC2(dspparammod_constptr_t mods) {
  const BlockModulationData* MODS = mods.get();
  ControllerInst* src2            = this->getControllerInst(mods->_src2);
  ControllerInst* depthcon        = this->getControllerInst(mods->_src2DepthCtrl);

  return [src2, depthcon, MODS]() -> float {
    float mindepth = MODS->_src2MinDepth;
    float maxdepth = MODS->_src2MaxDepth;
    float dc       = clip_float(depthcon ? depthcon->_value.x : 0.0f, 0, 1);
    float depth    = lerp(mindepth, maxdepth, dc);
    float out      = (src2 ? src2->_value.x : 0.0f) * depth;
    return out;
  };
}

} // namespace ork::audio::singularity
