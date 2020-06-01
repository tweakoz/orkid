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

namespace ork::audio::singularity {

static synth_ptr_t the_synth = synth::instance();

///////////////////////////////////////////////////////////////////////////////

LayerData::LayerData() {
  _pchBlock    = nullptr;
  _algdata     = std::make_shared<AlgData>();
  _ctrlBlock   = std::make_shared<ControlBlockData>();
  _kmpBlock    = std::make_shared<KmpBlockData>(); // todo move to samplerdata
  _scopesource = nullptr;
  _outbus      = the_synth->outputBus("main");
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
    printf("LAYER<%p> DONE\n", this);
    the_synth->freeLayer(this);
  }
  assert(_keepalive >= 0);
  // printf( "layer<%p> release cnt<%d>\n", this, _keepalive );
}

///////////////////////////////////////////////////////////////////////////////

void Layer::compute(int numframes) {

  _dspbuffer->resize(numframes);
  ///////////////////////

  if (nullptr == _layerdata) {
    printf("gotnull ld layer<%p>\n", this);
    return;
  }

  if (nullptr == _alg)
    return;

  auto& out_buf   = _outbus->_buffer;
  float* bus_outl = out_buf._leftBuffer;
  float* bus_outr = out_buf._rightBuffer;

  ////////////////////////////////////////

  if (true) {

    float* lyroutl = _dspbuffer->channel(0);
    float* lyroutr = _dspbuffer->channel(1);

    if (_is_bus_processor) {
    } else {
      for (int i = 0; i < numframes; i++) {
        lyroutl[i] = 0.0f;
        lyroutr[i] = 0.0f;
      }
    }

    ///////////////////////////////////
    // DspAlgorithm
    ///////////////////////////////////

    int ifrpending = numframes;
    _dspwritecount = frames_per_controlpass;
    _dspwritebase  = 0;
    while (ifrpending > 0) {
      // printf("_dspwritecount<%d> _dspwritebase<%d>\n", _dspwritecount, _dspwritebase);
      ////////////////////////////////
      // update controllers
      ////////////////////////////////
      if (_ctrlBlock)
        _ctrlBlock->compute();
      ////////////////////////////////
      // update dsp modules
      ////////////////////////////////
      _alg->compute();
      ////////////////////////////////
      // update indices
      ////////////////////////////////
      _dspwritebase += frames_per_controlpass;
      ifrpending -= frames_per_controlpass;
      ////////////////////////////////
      _layerTime += float(frames_per_controlpass) * getInverseSampleRate();
    }

    ///////////////////////////////////
    // amp / out
    ///////////////////////////////////
    if (_is_bus_processor) {
      for (int i = 0; i < numframes; i++) {
        bus_outl[i] = lyroutl[i] * _layerLinGain;
        bus_outr[i] = lyroutr[i] * _layerLinGain;
      }
    } else {
      for (int i = 0; i < numframes; i++) {
        bus_outl[i] += lyroutl[i] * _layerLinGain;
        bus_outr[i] += lyroutr[i] * _layerLinGain;
      }
    }
    ///////////////////////////////////////////////
    // test tone ?
    ///////////////////////////////////////////////
    if (0) {
      for (int i = 0; i < numframes; i++) {
        double phase = 60.0 * pi2 * double(_testtoneph) / getSampleRate();
        float samp   = sinf(phase) * .6;
        bus_outl[i]  = samp * _layerLinGain;
        bus_outr[i]  = samp * _layerLinGain;
        _testtoneph++;
      }
    }
    //////////////////////////////////////
    // SignalScope
    //////////////////////////////////////
    if (this == the_synth->_hudLayer) {
      if (_layerdata->_scopesource) {
        _layerdata->_scopesource->updateStereo(numframes, lyroutl, lyroutr);
      }
    }
    ///////////////////////////////////////////////
  } // if(true)

} // namespace ork::audio::singularity

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
    printf("getcon<%s> -> %p\n", srcn.c_str(), cinst);
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

controller_t Layer::getSRC1(const BlockModulationData& mods) {
  auto src1       = this->getController(mods._src1);
  float src1depth = mods._src1Depth;

  auto it = [=]() -> float {
    float out = src1() * src1depth;
    // printf( "src1out<%f>\n", out );
    return out;
  };

  return it;
}

controller_t Layer::getSRC2(const BlockModulationData& mods) {
  auto src2      = this->getController(mods._src2);
  auto depthcon  = this->getController(mods._src2DepthCtrl);
  float mindepth = mods._src2MinDepth;
  float maxdepth = mods._src2MaxDepth;

  auto it = [=]() -> float {
    float dc    = clip_float(depthcon(), 0, 1);
    float depth = lerp(mindepth, maxdepth, dc);
    float out   = src2() * depth;
    return out;
  };

  return it;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::keyOn(int note, int vel, lyrdata_constptr_t ld) {
  std::lock_guard<std::mutex> lock(_mutex);

  reset();

  assert(ld != nullptr);

  KeyOnInfo KOI;
  KOI._key       = note;
  KOI._vel       = vel;
  KOI._layer     = this;
  KOI._layerdata = ld;

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
  _outbus        = ld->_outbus;
  _layerLinGain  = ld->_layerLinGain;

  _curvel = vel;

  _layerTime = 0.0f;

  this->retain();

  /////////////////////////////////////////////
  // controllers
  /////////////////////////////////////////////

  if (ld->_ctrlBlock) {
    _ctrlBlock = new ControlBlockInst;
    _ctrlBlock->keyOn(KOI, ld->_ctrlBlock);
  }

  ///////////////////////////////////////

  _alg = _layerdata->_algdata->createAlgInst();
  // assert(_alg);
  if (_alg) {
    DspKeyOnInfo koi;
    koi._key       = note;
    koi._vel       = vel;
    koi._layer     = this;
    koi._layerdata = ld;
    _alg->keyOn(koi);
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
