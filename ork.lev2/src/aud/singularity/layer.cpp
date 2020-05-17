#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/dspblocks.h>

namespace ork::audio::singularity {

static synth_ptr_t the_synth = synth::instance();

///////////////////////////////////////////////////////////////////////////////

LayerData::LayerData() {
  _pchBlock    = nullptr;
  _algdata     = std::make_shared<AlgData>();
  _ctrlBlock   = std::make_shared<ControlBlockData>();
  _envCtrlData = std::make_shared<EnvCtrlData>();  // todo move to samplerdata
  _kmpBlock    = std::make_shared<KmpBlockData>(); // todo move to samplerdata
}

///////////////////////////////////////////////////////////////////////////////

dspstagedata_ptr_t LayerData::appendStage() {
  OrkAssert(_algdata->_numstages < kmaxdspstagesperlayer);
  auto stage                                = std::make_shared<DspStageData>();
  _algdata->_stages[_algdata->_numstages++] = stage;
  return stage;
}

///////////////////////////////////////////////////////////////////////////////

dspstagedata_ptr_t LayerData::stage(int index) {
  OrkAssert(index < _algdata->_numstages);
  OrkAssert(index >= 0);
  auto stage = _algdata->_stages[index];
  return stage;
}

///////////////////////////////////////////////////////////////////////////////

Layer::Layer()
    : _LayerData(nullptr)
    , _layerGain(0.25)
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
    // printf( "LAYER<%p> DONE\n", this );
    the_synth->freeLayer(this);
  }
  assert(_keepalive >= 0);
  // printf( "layer<%p> release cnt<%d>\n", this, _keepalive );
}

///////////////////////////////////////////////////////////////////////////////

void Layer::compute(outputBuffer& obuf) {
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

  float dt = float(inumframes) / the_synth->_sampleRate;

  //////////////////////////////////////
  // update controllers
  //////////////////////////////////////

  if (_ctrlBlock)
    _ctrlBlock->compute(inumframes);

  ///////////////////////////////////////////////
  // HUD AFRAME
  ///////////////////////////////////////////////
  int envcount = 0;
  int asrcount = 0;
  int lfocount = 0;
  int funcount = 0;
  auto cb      = _ctrlBlock;
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
          envf._curseg = env->_curseg;
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
  ///////////////////////////////////////////////

  // printf( "pchc1<%f> pchc2<%f> poic<%f> currat<%f>\n", _pchc1, _pchc2, _curPitchOffsetInCents, currat );
  ////////////////////////////////////////

  // printf( "doBlockStereo<%d>\n", int(doBlockStereo) );
  ////////////////////////////////////////

  if (true) {
    bool bypassDSP     = the_synth->_bypassDSP;
    auto lastblock     = _alg->lastBlock();
    bool doBlockStereo = bypassDSP //
                             ? false
                             : lastblock ? (lastblock->numOutputs() == 2) : false;

    float synsr = the_synth->_sampleRate;

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

    switch (the_synth->_genmode) {
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
      auto ibuf = the_synth->_ibuf._leftBuffer;
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

    if (this == the_synth->_hudLayer) {
      _HAF._oscopebuffer.resize(inumframes);
      _HAF._oscopesync.resize(inumframes);
      ///////////////////////////////////////////////
      // find oscope sync source
      ///////////////////////////////////////////////

      scopesynctrack_ptr_t syncsource = nullptr;
      int istage                      = 0;
      _alg->forEachStage([&](dspstage_ptr_t stage) {
        bool ena = the_synth->_stageEnable[istage];
        if (ena) {
          int iblock = 0;
          stage->forEachBlock([&](dspblk_ptr_t block) {
            if (block->isScopeSyncSource()) {
              syncsource = _scopesynctracks[iblock];
            }
            iblock++;
          }); // forEachBlock
          istage++;
        } // if(ena)
      });
      ///////////////////////////////////////////////
      if (syncsource) {
        for (int i = 0; i < inumframes; i++)
          _HAF._oscopesync[i] = syncsource->_triggers[i];
      } else {
        for (int i = 0; i < inumframes; i++)
          _HAF._oscopesync[i] = false;
      }
      ///////////////////////////////////////////////
      if (doBlockStereo) {
        for (int i = 0; i < inumframes; i++) {
          int inpi              = i;
          float l               = _layerObuf._leftBuffer[inpi];
          float r               = _layerObuf._rightBuffer[inpi];
          _HAF._oscopebuffer[i] = (l + r) * 0.5f;
        }

      } else {
        for (int i = 0; i < inumframes; i++) {
          int inpi              = i;
          float l               = _layerObuf._leftBuffer[inpi];
          _HAF._oscopebuffer[i] = l;
        }
      }
      ///////////////////////////////////////////////
      _HAF._baseserial = _num_sent_to_scope;
      //_HAF._owcount += the_synth->_ostrack;
      _num_sent_to_scope += inumframes;
      ///////////////////////////////////////////////
      the_synth->_hudbuf.push(_HAF);
    }
  }

  _layerTime += dt;
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
  KOI._LayerData = ld;

  _HKF._miscText   = "";
  _HKF._note       = note;
  _HKF._vel        = vel;
  _HKF._layerdata  = ld;
  _HKF._layerIndex = _ldindex;
  _HKF._useFm4     = false;

  _layerBasePitch = clip_float(note * 100, -0, 12700);

  _ignoreRelease = ld->_ignRels;
  _curnote       = note;
  _LayerData     = ld;
  _layerGain     = 1.0f; // ld->_outputGain;
  _masterGain    = the_synth->_masterGain;

  _curvel = vel;

  _layerTime = 0.0f;

  _HAF_nenvseg = 1;

  this->retain();

  /////////////////////////////////////////////
  // controllers
  /////////////////////////////////////////////

  if (ld->_ctrlBlock) {
    _ctrlBlock = new ControlBlockInst;
    _ctrlBlock->keyOn(KOI, ld->_ctrlBlock);
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
} // namespace ork::audio::singularity

///////////////////////////////////////////////////////////////////////////////

void Layer::reset() {
  _LayerData = nullptr;
  _curnote   = 0;
  _keepalive = 0;

  // for( int i=0; i<kmaxdspblocksperlayer; i++ )
  //   _fp[i].reset();

  // todo pool controllers
  if (_ctrlBlock)
    delete _ctrlBlock;
  _ctrlBlock = nullptr;

  _controlMap.clear();

} // namespace ork::audio::singularity

} // namespace ork::audio::singularity
