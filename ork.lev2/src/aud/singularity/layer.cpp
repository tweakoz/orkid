#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/dspblocks.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

layer::layer()
    : _LayerData(nullptr)
    , _layerGain(0.25)
    , _curPitchOffsetInCents(0.0f)
    , _centsPerKey(100.0f)
    , _lyrPhase(-1)
    , _curnote(0)
    , _useNatEnv(false)
    , _alg(nullptr)
    , _doNoise(false)
    , _keepalive(0)
    , _AENV(nullptr) {
  // printf( "Layer Init<%p>\n", this );
  _dspbuffer = std::make_shared<DspBuffer>();
}

layer::~layer() {
  for (int i = 0; i < kmaxctrlblocks; i++)
    if (_ctrlBlock[i])
      delete _ctrlBlock[i];
  // if( _alg )
  //    delete _alg;
}

///////////////////////////////////////////////////////////////////////////////

void layer::retain() {
  ++_keepalive;

  // printf( "layer<%p> retain cnt<%d>\n", this, _keepalive );
}

///////////////////////////////////////////////////////////////////////////////

void layer::release() {
  if ((--_keepalive) == 0) {
    // printf( "LAYER<%p> DONE\n", this );
    synth::instance()->freeLayer(this);
  }
  assert(_keepalive >= 0);
  // printf( "layer<%p> release cnt<%d>\n", this, _keepalive );
}

///////////////////////////////////////////////////////////////////////////////

void layer::compute(outputBuffer& obuf) {
  _HAF._items.clear();

  ///////////////////////

  if (nullptr == _LayerData) {
    printf("gotnull ld layer<%p>\n", this);
    return;
  }

  if (nullptr == _alg)
    return;

  // printf( "layer<%p> compute\n", this );
  int inumframes = obuf._numframes;
  float* outl    = obuf._leftBuffer;
  float* outr    = obuf._rightBuffer;

  float dt = float(inumframes) / synth::instance()->_sampleRate;

  //////////////////////////////////////
  // update controllers
  //////////////////////////////////////

  for (int i = 0; i < kmaxctrlblocks; i++) {
    if (_ctrlBlock[i])
      _ctrlBlock[i]->compute(inumframes);
  }

  ///////////////////////////////////
  // extract AMPENV
  ///////////////////////////////////
  // TODO - get rid og hard coded location in CB0

  auto CB0 = this->_ctrlBlock[0];
  if (CB0) {
    if (auto as_env = dynamic_cast<RateLevelEnvInst*>(CB0->_cinst[0])) // ampenv ?
    {
      for (int i = 0; i < inumframes; i++) {
        float e0       = as_env->_USERAMPENV[i];
        _USERAMPENV[i] = e0;
      }
    }
  }

  ///////////////////////////////////////////////
  // HUD AFRAME
  ///////////////////////////////////////////////
  int envcount = 0;
  int asrcount = 0;
  int lfocount = 0;
  int funcount = 0;
  for (int icb = 0; icb < kmaxctrlblocks; icb++) {
    auto cb = _ctrlBlock[icb];
    if (cb) {
      for (int ic = 0; ic < kmaxctrlperblock; ic++) {
        auto cinst = cb->_cinst[ic];
        if (auto env = dynamic_cast<RateLevelEnvInst*>(cinst)) {
          envframe envf;
          envf._index  = envcount++;
          envf._value  = env->_curval;
          envf._data   = env->_data;
          envf._curseg = env->_curseg;
          if (env->_data && env->_data->_ampenv)
            envf._curseg = _useNatEnv ? _HAF_nenvseg : env->_curseg;
          _HAF._items.push_back(envf);
        } else if (auto asr = dynamic_cast<AsrInst*>(cinst)) {
          asrframe asrf;
          asrf._index  = asrcount++;
          asrf._value  = asr->_curval;
          asrf._curseg = asr->_curseg;
          asrf._data   = asr->_data;
          _HAF._items.push_back(asrf);
          // printf( "add asr item\n");
        } else if (auto lfo = dynamic_cast<LfoInst*>(cinst)) {
          lfoframe lfof;
          lfof._index   = lfocount++;
          lfof._value   = lfo->_curval;
          lfof._currate = lfo->_currate;
          lfof._data    = lfo->_data;
          _HAF._items.push_back(lfof);
        } else if (auto fun = dynamic_cast<FunInst*>(cinst)) {
          funframe funfr;
          funfr._index = funcount++;
          funfr._data  = fun->_data;
          funfr._value = fun->_curval;
          _HAF._items.push_back(funfr);
        }
      }
    }
  }

  ///////////////////////////////////////////////

  // printf( "pchc1<%f> pchc2<%f> poic<%f> currat<%f>\n", _pchc1, _pchc2, _curPitchOffsetInCents, currat );
  ////////////////////////////////////////

  // printf( "doBlockStereo<%d>\n", int(doBlockStereo) );
  ////////////////////////////////////////

  if (true) {
    bool bypassDSP     = synth::instance()->_bypassDSP;
    auto lastblock     = _alg->lastBlock();
    bool doBlockStereo = bypassDSP //
                             ? false
                             : lastblock ? (lastblock->numOutputs() == 2) : false;

    float synsr = synth::instance()->_sampleRate;

    outputBuffer laybuf;

    _layerObuf.resize(inumframes);
    float* lyroutl = _layerObuf._leftBuffer;
    float* lyroutr = _layerObuf._rightBuffer;

    ///////////////////////////////////
    // sample osc
    ///////////////////////////////////

    bool do_noise = _doNoise;
    bool do_sine  = false;
    bool do_input = false;

    switch (synth::instance()->_genmode) {
      case 1: // force sine
        do_sine  = true;
        do_noise = false;
        break;
      case 2: // force noise
        do_sine  = false;
        do_noise = true;
        break;
      case 3: // input
        do_sine  = false;
        do_noise = false;
        do_input = true;
        break;
    }

    if (do_noise) {
      for (int i = 0; i < inumframes; i++) {
        float o    = ((rand() & 0xffff) / 32768.0f) - 1.0f;
        lyroutl[i] = o;
        lyroutr[i] = 0.0f;
      }
    } else if (do_input) {
      auto ibuf = synth::instance()->_ibuf._leftBuffer;
      for (int i = 0; i < inumframes; i++) {
        float o    = ibuf[i] * 8.0f;
        lyroutl[i] = o;
        lyroutr[i] = 0.0f;
      }

    } else if (do_sine) {
      float F        = midi_note_to_frequency(float(_layerBasePitch) * 0.01);
      float phaseinc = pi2 * F / synsr;

      for (int i = 0; i < inumframes; i++) {
        float o = sinf(_sinrepPH) * 0.5;
        _sinrepPH += phaseinc;
        lyroutl[i] = o;
        lyroutr[i] = 0.0f;
      }

    } else // clear
      for (int i = 0; i < inumframes; i++) {
        lyroutl[i] = 0.0f;
        lyroutr[i] = 0.0f;
      }

    ///////////////////////////////////
    // DSP F1-F3
    ///////////////////////////////////

    if (false == bypassDSP)
      _alg->compute(_layerObuf);

    ///////////////////////////////////
    // amp / out
    ///////////////////////////////////

    if (doBlockStereo) {
      for (int i = 0; i < inumframes; i++) {
        float tgain = _layerGain * _masterGain;
        outl[i] += lyroutl[i] * tgain;
        outr[i] += lyroutr[i] * tgain;
      }
    } else if (bypassDSP) {
      for (int i = 0; i < inumframes; i++) {
        float tgain = _layerGain * _masterGain;
        float inp   = lyroutl[i];
        outl[i] += inp * tgain * 0.5f;
        outr[i] += inp * tgain * 0.5f;
      }
    } else {
      for (int i = 0; i < inumframes; i++) {
        float tgain = _layerGain * _masterGain;
        outl[i] += lyroutl[i] * tgain;
        outr[i] += lyroutl[i] * tgain;
      }
    }

    /////////////////
    // oscope
    /////////////////

    if (this == synth::instance()->_hudLayer) {
      _HAF._oscopebuffer.resize(inumframes);
      _HAF._trackoffset += synth::instance()->_ostrack;

      if (doBlockStereo) {
        for (int i = 0; i < inumframes; i++) {
          float l = _layerObuf._leftBuffer[i];
          float r = _layerObuf._rightBuffer[i];
          // tailb[i] = l;//doBlockStereo ? l+r : l;
          _HAF._oscopebuffer[i] = (l + r) * 0.5f;
        }

      } else {
        for (int i = 0; i < inumframes; i++) {
          float l               = _layerObuf._leftBuffer[i];
          _HAF._oscopebuffer[i] = l;
        }
      }
      synth::instance()->_hudbuf.push(_HAF);
    }
  }

  _layerTime += dt;
}

///////////////////////////////////////////////////////////////////////////////

bool layer::isHudLayer() const {
  return (this == synth::instance()->_hudLayer);
}

///////////////////////////////////////////////////////////////////////////////

controller_t layer::getController(const std::string& srcn) const {
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
      float v  = synth::instance()->_doPressure;
      (*state) = (*state) * 0.99 + v * 0.01;
      return (*state);
    };
  } else if (srcn == "MWheel") {
    auto state = new float(0);
    return [this, state]() {
      float v  = synth::instance()->_doModWheel;
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

controller_t layer::getSRC1(const BlockModulationData& mods) {
  auto src1       = this->getController(mods._src1);
  float src1depth = mods._src1Depth;

  auto it = [=]() -> float {
    float out = src1() * src1depth;
    // printf( "src1out<%f>\n", out );
    return out;
  };

  return it;
}

controller_t layer::getSRC2(const BlockModulationData& mods) {
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

void layer::keyOn(int note, int vel, lyrdata_constptr_t ld) {
  std::lock_guard<std::mutex> lock(_mutex);

  reset();

  assert(ld != nullptr);
  const auto& KMP  = ld->_kmpBlock;
  const auto& ENVC = ld->_envCtrlData;

  KeyOnInfo KOI;
  KOI._key       = note;
  KOI._vel       = vel;
  KOI._layer     = this;
  KOI._LayerData = ld;

  _HKF._miscText   = "";
  _HKF._note       = note;
  _HKF._vel        = vel;
  _HKF._layerdata  = ld;
  _HKF._layerIndex = _ldindex;
  _HKF._useFm4     = false;

  _layerBasePitch = clip_float(note * 100, -0, 12700);

  _useNatEnv     = ENVC._useNatEnv;
  _ignoreRelease = ld->_ignRels;
  _curnote       = note;
  _LayerData     = ld;
  _layerGain     = 1.0f; // ld->_outputGain;
  _masterGain    = synth::instance()->_masterGain;

  _curvel = vel;

  _layerTime = 0.0f;

  _HAF_nenvseg = 1;

  this->retain();

  /////////////////////////////////////////////

  this->_AENV = this->_USERAMPENV;

  /////////////////////////////////////////////
  // controllers
  /////////////////////////////////////////////

  for (int i = 0; i < kmaxctrlblocks; i++) {
    if (ld->_ctrlBlocks[i]) {
      _ctrlBlock[i] = new ControlBlockInst;
      _ctrlBlock[i]->keyOn(KOI, ld->_ctrlBlocks[i]);
    }
  }
  ///////////////////////////////////////

  _alg = _LayerData->_algdata->createAlgInst();
  // assert(_alg);
  if (_alg) {
    DspKeyOnInfo koi;
    koi._key       = note;
    koi._vel       = vel;
    koi._layer     = this;
    koi._LayerData = ld;
    _alg->keyOn(koi);
  }

  _HKF._alg = _alg;

  ///////////////////////////////////////

  _lyrPhase = 0;
  _sinrepPH = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void layer::keyOff() {
  _lyrPhase = 1;
  this->release();

  ///////////////////////////////////////

  for (int i = 0; i < kmaxctrlblocks; i++) {
    if (_ctrlBlock[i])
      _ctrlBlock[i]->keyOff();
  }

  ///////////////////////////////////////

  if (_ignoreRelease)
    return;

  ///////////////////////////////////////

  if (_alg)
    _alg->keyOff();
}

///////////////////////////////////////////////////////////////////////////////

void layer::reset() {
  _LayerData = nullptr;
  _useNatEnv = false;
  _curnote   = 0;
  _keepalive = 0;

  // for( int i=0; i<kmaxdspblocksperlayer; i++ )
  //   _fp[i].reset();

  // todo pool controllers
  for (int i = 0; i < kmaxctrlblocks; i++) {
    if (_ctrlBlock[i])
      delete _ctrlBlock[i];
    _ctrlBlock[i] = 0;
  }
  _controlMap.clear();
  // delete _alg;
  //_alg = nullptr;
}

} // namespace ork::audio::singularity
