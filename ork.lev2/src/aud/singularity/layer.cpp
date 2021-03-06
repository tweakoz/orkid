////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

ImplementReflectionX(ork::audio::singularity::LayerData, "SynLayer");

namespace ork::audio::singularity {

static synth_ptr_t the_synth = synth::instance();

///////////////////////////////////////////////////////////////////////////////

void LayerData::describeX(class_t* clazz) {
  clazz->directObjectMapProperty("Controllers", &LayerData::_controllermap);
  clazz->directObjectProperty("Algorithm", &LayerData::_algdata);
}

///////////////////////////////////////////////////////////////////////////////

bool LayerData::postDeserialize(reflect::serdes::IDeserializer&) {
  int icid = 0;
  for (auto item : _controllermap) {
    auto controller            = item.second;
    _ctrlBlock->_cdata[icid++] = controller;
  }
  _ctrlBlock->_numcontrollers = _controllermap.size();
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
  _outbus      = "main";
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
controllerdata_ptr_t LayerData::controllerByName(const std::string& named) const {
  auto it = _controllermap.find(named);
  return it != _controllermap.end() ? it->second : nullptr;
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
    : _layerdata(nullptr)
    , _layerLinGain(1.0)
    , _curPitchOffsetInCents(0.0f)
    , _centsPerKey(100.0f)
    , _lyrPhase(-1)
    , _curnote(0)
    , _alg(nullptr)
    , _doNoise(false)
    , _keepalive(0) {
  // printf( "Layer Init<%p>\n", this );
  _dspbuffer = std::make_shared<DspBuffer>();

  for (int i = 0; i < kmaxdspblocksperstage; i++) {
    _oschsynctracks[i]  = std::make_shared<OscillatorSyncTrack>();
    _scopesynctracks[i] = std::make_shared<ScopeSyncTrack>();
  }
}

Layer::~Layer() {
  if (_ctrlBlock)
    delete _ctrlBlock;
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

void Layer::release() {
  if ((--_keepalive) == 0) {
    // printf("LAYER<%p> DONE\n", this);
    the_synth->freeLayer(this);
  }
  assert(_keepalive >= 0);
  // printf( "layer<%p> release cnt<%d>\n", this, _keepalive );
}
///////////////////////////////////////////////////////////////////////////////
void Layer::compute(int base, int count) {
  _dspwritecount = count;
  _dspwritebase  = base;
  ///////////////////////
  if (nullptr == _layerdata) {
    printf("gotnull ld layer<%p>\n", this);
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
void Layer::mixToBus(int base, int count) {
  float* lyroutl  = _dspbuffer->channel(0) + base;
  float* lyroutr  = _dspbuffer->channel(1) + base;
  auto& out_buf   = _outbus->_buffer;
  float* bus_outl = out_buf._leftBuffer + base;
  float* bus_outr = out_buf._rightBuffer + base;
  for (int i = 0; i < count; i++) {
    bus_outl[i] += lyroutl[i] * _layerLinGain;
    bus_outr[i] += lyroutr[i] * _layerLinGain;
  }
  if (0) { // test tone
    for (int i = 0; i < _numFramesForBlock; i++) {
      double phase = 120.0 * pi2 * double(_testtoneph) / getSampleRate();
      float samp   = sinf(phase) * .6;
      bus_outl[i]  = samp * _layerLinGain;
      bus_outr[i]  = samp * _layerLinGain;
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
  for (int i = 0; i < count; i++) {
    bus_outl[i] = lyroutl[i] * _layerLinGain;
    bus_outr[i] = lyroutr[i] * _layerLinGain;
  }
}
///////////////////////////////////////////////////////////////////////////////
void Layer::endCompute() {
  if (_alg)
    _alg->endCompute();
}
//////////////////////////////////////
// SignalScope
//////////////////////////////////////
void Layer::updateScopes(int ibase, int icount) {
  if (this == the_synth->_hudLayer) {
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
  return (this == the_synth->_hudLayer);
}

///////////////////////////////////////////////////////////////////////////////

controller_t Layer::getController(controllerdata_constptr_t cdat) const {
  auto it = _controld2iMap.find(cdat);
  if (it != _controld2iMap.end()) {
    auto cinst = it->second;
    return [cinst]() { return cinst->_curval; };
  }
  return [this]() { return 0.0f; };
}

///////////////////////////////////////////////////////////////////////////////

controller_t Layer::getController(const std::string& srcn) const {
  auto it = _controlMap.find(srcn);
  if (it != _controlMap.end()) {
    auto cinst = it->second;
    // printf("getcon<%s> -> %p\n", srcn.c_str(), cinst);
    return [cinst]() { return cinst->_curval; };
  } else if (srcn == "OFF")
    return [this]() { return 0.0f; };
  else if (srcn == "ON")
    return [this]() { return 1.0f; };
  else if (srcn == "-ON")
    return [this]() { return -1.0f; };
  else if (srcn == "MPress") {
    auto state = new float(0);
    return [this, state]() {
      float v  = the_synth->_doPressure;
      (*state) = (*state) * 0.99 + v * 0.01;
      return (*state);
    };
  } else if (srcn == "MWheel") {
    auto state = new float(0);
    return [this, state]() {
      float v  = the_synth->_doModWheel;
      (*state) = (*state) * 0.99 + v * 0.01;
      return (*state);
    };
  } else if (srcn == "KeyNum")
    return [this]() { return this->_curnote / float(127.0f); };
  else if (srcn == "MIDI(49)")
    return [this]() {
      float lt = this->_layerTime;
      float s  = sinf(lt * pi2 * 8.0f);
      s        = (s >= 0.0f) ? 1.0f : 0.0f;
      return s;
    };
  else if (srcn == "RandV1")
    return [this]() {
      float lt = -1.0f + float(rand() & 0xffff) / 32768.0f;
      return lt;
    };
  else if (srcn == "AttVel")
    return [this]() {
      float atkvel = float(this->_curvel) / 128.0f;
      return atkvel;
    };
  else if (srcn == "VTRIG1")
    return [this]() {
      float atkvel = float(this->_curvel > 64);
      return atkvel;
    };
  else if (srcn == "VTRIG2")
    return [this]() {
      float atkvel = float(this->_curvel > 96);
      return atkvel;
    };
  else {
    printf("CONTROLLER<%s> not found!\n", srcn.c_str());
    float fv = atof(srcn.c_str());
    if (fv != 0.0f) {
      return [=]() { // printf( "fv<%f>\n", fv);
        return fv;
      };
    }
  }

  return []() { return 0.0f; };
}

///////////////////////////////////////////////////////////////////////////////

controller_t Layer::getSRC1(dspparammod_constptr_t mods) {
  auto src1 = this->getController(mods->_src1);

  auto it = [=]() -> float {
    float src1depth = mods->_src1Depth;
    float out       = src1() * src1depth;
    // printf( "src1out<%f>\n", out );
    return out;
  };

  return it;
}

controller_t Layer::getSRC2(dspparammod_constptr_t mods) {
  auto src2     = this->getController(mods->_src2);
  auto depthcon = this->getController(mods->_src2DepthCtrl);

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

///////////////////////////////////////////////////////////////////////////////

void Layer::keyOn(int note, int vel, lyrdata_constptr_t ld) {
  std::lock_guard<std::mutex> lock(_mutex);

  reset();

  assert(ld != nullptr);

  _koi._key       = note;
  _koi._vel       = vel;
  _koi._layer     = this;
  _koi._layerdata = ld;

  _HKF._miscText   = "";
  _HKF._note       = note;
  _HKF._vel        = vel;
  _HKF._layerdata  = ld;
  _HKF._layerIndex = _ldindex;
  _HKF._useFm4     = false;

  _layerBasePitch = clip_float(note * 100, -0, 12700);

  _ignoreRelease = ld->_ignRels;
  _curnote       = note;
  _layerdata     = ld;
  _outbus        = the_synth->outputBus(ld->_outbus);
  _layerLinGain  = ld->_layerLinGain;

  _curvel = vel;

  _layerTime = 0.0f;

  this->retain();

  /////////////////////////////////////////////
  // controllers
  /////////////////////////////////////////////

  if (ld->_ctrlBlock) {
    _ctrlBlock = new ControlBlockInst;
    _ctrlBlock->keyOn(_koi, ld->_ctrlBlock);
  }

  ///////////////////////////////////////

  _alg = _layerdata->_algdata->createAlgInst();
  // assert(_alg);
  if (_alg) {
    _alg->keyOn(_koi);
  }

  _HKF._alg = _alg;

  ///////////////////////////////////////

  _lyrPhase = 0;
  _sinrepPH = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::keyOff() {
  _lyrPhase = 1;
  this->release();

  ///////////////////////////////////////

  if (_ctrlBlock)
    _ctrlBlock->keyOff();

  ///////////////////////////////////////

  if (_ignoreRelease)
    return;

  ///////////////////////////////////////

  if (_alg)
    _alg->keyOff();
}

///////////////////////////////////////////////////////////////////////////////

void Layer::reset() {
  _layerdata = nullptr;
  _curnote   = 0;
  _keepalive = 0;

  // todo pool controllers
  if (_ctrlBlock)
    delete _ctrlBlock;
  _ctrlBlock = nullptr;

  _controlMap.clear();
}

} // namespace ork::audio::singularity
