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
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/aud/singularity/alg_pan.inl>

ImplementReflectionX(ork::audio::singularity::LayerData, "SynLayer");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void LayerData::describeX(class_t* clazz) {
  // clazz->directObjectMapProperty("Controllers", &LayerData::_controllermap);
  clazz->directObjectProperty("Algorithm", &LayerData::_algdata);
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

  for (int i = 0; i < kmaxdspblocksperstage; i++) {
    _oschsynctracks[i]  = std::make_shared<OscillatorSyncTrack>();
    _scopesynctracks[i] = std::make_shared<ScopeSyncTrack>();
  }
}

Layer::~Layer() {
  std::lock_guard<std::mutex> lock(_mutex);
  _pchBlock  = nullptr;
  _outbus    = nullptr;
  _ctrlBlock = nullptr;
  _alg       = nullptr;
  _dspbuffer = nullptr;
  _layerdata = nullptr;
  _controlMap.clear();
  _controld2iMap.clear();
}

///////////////////////////////////////////////////////////////////////////////

void Layer::reset() {
  _layerdata = nullptr;
  _curnote   = 0;
  _keepalive = 0;

  // todo pool controllers
  _ctrlBlock = nullptr;

  _controlMap.clear();
}

///////////////////////////////////////////////////////////////////////////////

void Layer::reTriggerMono(int note, int velocity){
  this->_layerBasePitch = clip_float(note * 100, -0, 12700);
  this->_curnote = note;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::keyOn(int note, int velocity, lyrdata_ptr_t ld, outbus_ptr_t obus) {
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
  this->_layerLinGain  = ld->_layerLinGain;
  this->_gainModifier = decibel_to_linear_amp_ratio(obus->_prog_gain);

  this->_curvel = velocity;

  this->_layerTime = 0.0f;

  this->retain();

  /////////////////////////////////////////////
  // controllers
  /////////////////////////////////////////////

  if (ld->_ctrlBlock) {
    this->_ctrlBlock = std::make_shared<ControlBlockInst>();
    this->_ctrlBlock->keyOn(this->_koi, ld->_ctrlBlock);
  }

  ///////////////////////////////////////
  auto algname = ld->_algdata->_name;
  // printf( "LAYER KEYON<%d> alg<%s>\n", note, algname.c_str() );

  this->_alg = this->_layerdata->_algdata->createAlgInst();
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
    _sampleindex += frames_per_controlpass;
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
  switch(panmode){
    case 0: // Fixed
      break;
    case 1: // +MIDI
      fpan += 0.5f;
      break;
    case 2:{ // Auto
      int ko = _curnote-60;
      fpan = float(ko)/60.0;
      break;
    }
    case 3:{ // Reverse(Auto)
      int ko = -(_curnote-60);
      fpan = float(ko)/60.0;
      break;
    }
  }
  return fpan;
}
///////////////////////////////////////////////////////////////////////////////
void Layer::mixToBus(int base, int count) {
  float prggain = decibel_to_linear_amp_ratio(_layerdata->_programdata->_gainDB);
  prggain *= decibel_to_linear_amp_ratio(_programinst->_gain);
  float* lyroutl  = _dspbuffer->channel(0) + base;
  float* lyroutr  = _dspbuffer->channel(1) + base;
  auto& out_buf   = _outbus->_buffer;
  float* bus_outl = out_buf._leftBuffer + base;
  float* bus_outr = out_buf._rightBuffer + base;
  //////////////////////////////////
  float fpan = currentPan();
  float panL = panBlend(fpan).lmix;
  float panR = panBlend(fpan).rmix;
  float headroom = decibel_to_linear_amp_ratio(_layerdata->_headroom);
  float LG = prggain * _layerLinGain * _gainModifier * headroom;
  //////////////////////////////////
  for (int i = 0; i < count; i++) {
    bus_outl[i] += (lyroutl[i]*LG*panL);
    bus_outr[i] += (lyroutr[i]*LG*panR);
  }
  if (0) { // test tone
    for (int i = 0; i < _numFramesForBlock; i++) {
      double phase = 120.0 * pi2 * double(_testtoneph) / getSampleRate();
      float samp   = sinf(phase) * .6;
      bus_outl[i]  = samp * _layerLinGain * _gainModifier;
      bus_outr[i]  = samp * _layerLinGain * _gainModifier;
      _testtoneph++;
    }
  }
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

controller_t Layer::getController(controllerdata_constptr_t cdat) const {
  auto it = _controld2iMap.find(cdat);
  if (it != _controld2iMap.end()) {
    auto cinst = it->second;
    return [cinst]() { return cinst->_value.x; };
  }
  return [this]() { return 0.0f; };
}

///////////////////////////////////////////////////////////////////////////////

controller_t Layer::getController(const std::string& srcn) const {
  auto it = _controlMap.find(srcn);
  if (it != _controlMap.end()) {
    auto cinst = it->second;
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

controller_t Layer::getSRC1(dspparammod_constptr_t mods) {
  auto src1 = this->getController(mods->_src1);
  // printf("src1<%p>\n", (void*) mods->_src1.get());
  // if(mods->_src1){
  // printf("src1<%p:%s>\n", (void*) mods->_src1.get(), mods->_src1->_name.c_str());
  //}

  auto it = [=]() -> float {
    float src1scale = mods->_src1Scale;
    float out       = src1() * src1scale + mods->_src1Bias ;
    // printf( "src1out<%f>\n", out );
    return out;
  };

  return it;
}

controller_t Layer::getSRC2(dspparammod_constptr_t mods) {
  auto src2     = this->getController(mods->_src2);
  auto depthcon = this->getController(mods->_src2DepthCtrl);
  // printf("src2<%p>\n", (void*) mods->_src2.get());
  // if(mods->_src2){
  // printf("src2<%p:%s>\n", (void*) mods->_src2.get(), mods->_src2->_name.c_str());
  //}

  auto it = [=]() -> float {
    float mindepth = mods->_src2MinDepth;
    float maxdepth = mods->_src2MaxDepth;
    float dc       = clip_float(depthcon(), 0, 1);
    float depth    = lerp(mindepth, maxdepth, dc);
    float out      = src2() * depth;
    return out;
  };

  return it;
}

} // namespace ork::audio::singularity
