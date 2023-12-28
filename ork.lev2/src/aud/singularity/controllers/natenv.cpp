////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//#include <audiofile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/sampler.h>

ImplementReflectionX(ork::audio::singularity::NatEnvWrapperData, "SynNatEnvWrapperData");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// Natural envelopes
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void NatEnvWrapperData::describeX(class_t* clazz) {
}

NatEnvWrapperData::NatEnvWrapperData() {
}
ControllerInst* NatEnvWrapperData::instantiate(layer_ptr_t layer) const {
  return new NatEnvWrapperInst(this, layer);
}

NatEnvWrapperInst::NatEnvWrapperInst(const NatEnvWrapperData* data, layer_ptr_t l)
  : ControllerInst(l) {
  _natenv = std::make_shared<NatEnv>();
}
void NatEnvWrapperInst::compute() {
}
void NatEnvWrapperInst::keyOn(const KeyOnInfo& KOI) {
  // FIND sample which has std::vector<natenvseg> _natenv;
  // use to init NatEnv::keyOn
  /*OrkAssert(_layer);
  auto l = _layer;
  auto ld = l->_layerdata;
  auto algdata = ld->_algdata;
  OrkAssert(algdata);
  auto stagedata0 = algdata->_stages[0];
  OrkAssert(stagedata0);
  auto blkdata0 = stagedata0->_blockdatas[0];
  auto blkdata1 = stagedata0->_blockdatas[1];
  auto sampler = std::dynamic_pointer_cast<SAMPLER_DATA>(blkdata1);
  OrkAssert(sampler);
  RegionSearch RF = sampler->findRegion(ld,KOI);
  auto sample = RF._sample;
  printf("blkdata0<%p:%s>\n", (void*) blkdata0.get(), blkdata0->_name.c_str());
  printf("sampler<%p:%s>\n", (void*) sampler.get(), sampler->_name.c_str());
  printf("sample<%p:%s> natenvcount<%d>\n", (void*) sample, sample->_name.c_str(), int(sample->_natenv.size()));
  */
}
void NatEnvWrapperInst::keyOff() {
}

NatEnv::NatEnv()
    : _layer(nullptr)
    , _curseg(0)
    , _prvseg(0)
    , _numseg(0)
    , _framesrem(0)
    , _slopePerSecond(0.0f)
    , _slopePerSample(0.0f)
    , _SR(getSampleRate())
    , _state(0) {
  _envadjust = [](const EnvPoint& inp, //
                  int iseg,
                  const KeyOnInfo& KOI) -> EnvPoint { //
    return inp;
  };
}

///////////////////////////////////////////////////////////////////////////////

void NatEnv::keyOn(const KeyOnInfo& KOI, const sample* s) {
  auto ld = KOI._layerdata;
  assert(ld != nullptr);
  _layer = KOI._layer;
  _layer->retain();

  _natenvseg.clear();
  for (const auto& item : s->_natenv)
    _natenvseg.push_back(item);
  _numseg        = _natenvseg.size();
  _curseg        = 0;
  _prvseg        = -1;
  _curamp        = 1.0f;
  _state         = 1;
  _ignoreRelease = ld->_ignRels;

  initSeg(0);
}

///////////////////////////////////////////////////////////////////////////////

void NatEnv::keyOff() {
  _state = 2;
  if (_ignoreRelease)
    return;

  if (_numseg - 1 >= 0)
    initSeg(_numseg - 1);
}

///////////////////////////////////////////////////////////////////////////////

const natenvseg& NatEnv::getCurSeg() const {
  assert(_curseg >= 0);
  assert(_curseg < _numseg);
  return _natenvseg[_curseg];
}

///////////////////////////////////////////////////////////////////////////////

void NatEnv::initSeg(int iseg) {
  _curseg         = iseg;
  const auto& seg = getCurSeg();

  _slopePerSecond = seg._slope;
  _slopePerSample = slopeDBPerSample(_slopePerSecond, 192000.0);

  /* TODO : envadjust processing
  auto EC         = ld->_envCtrlData;
  auto DKT        = EC->_decKeyTrack;
  const auto& RKT = EC->_relKeyTrack;

  _decAdjust = EC->_decAdjust;
  _relAdjust = EC->_relAdjust;

  if (ikey > 60) {
    float flerp = float(ikey - 60) / float(127 - 60);
    printf("flerp<%f>\n", flerp);
    _decAdjust = lerp(_decAdjust, DKT, flerp);
    _relAdjust = lerp(_relAdjust, RKT, flerp);
  } else if (ikey < 60) {
    float flerp = float(59 - ikey) / 59.0f;
    _decAdjust  = lerp(_decAdjust, 1.0 / DKT, flerp);
    _relAdjust  = lerp(_relAdjust, 1.0 / RKT, flerp);
  }
   */
  /*
  switch (_state) {
    case 2:
      _slopePerSecond *= _relAdjust;
      break;
    default:
      _slopePerSecond *= _decAdjust;
      break;
  }*/

  _segtime   = seg._time;
  _framesrem = seg._time; /// 16.0f;// * _SR / 48000.0f;

  printf(
      "SEG<%d/%d> CURAMP<%f> SLOPEPERSEC<%f> "
      "_slopePerSample<%f>  SEGT<%f>\n",
      _curseg + 1,
      _numseg,
      _curamp,
      _slopePerSecond,
      _slopePerSample,
      seg._time);
}

///////////////////////////////////////////////////////////////////////////////

float NatEnv::compute() {
  bool doslope = true;

  // printf( "a: _curamp<%f> _slopePerSample<%f>\n", _curamp, _slopePerSample );
  switch (_state) {
    case 1: {
      // doslope = true;
      _framesrem--;
      if (_framesrem <= 0) {
        if (_curseg + 2 < _numseg)
          initSeg(_curseg + 1);
        else //
        {
          // doslope = false;
          // printf( "decay.. dbps<%f>\n",_slopePerSecond);
        }
      }
      break;
    }
    case 2: // released
    {
      float dbatten = linear_amp_ratio_to_decibel(_curamp);
      if (((_curseg + 1) == _numseg) and (dbatten < -72.0f)) {
        _state = 3;
        auto s = synth::instance();
        s->releaseLayer(_layer);
      }
      break;
    }
    case 3: // dead
      doslope = false;
      break;
    default:
      break;
  }
  if (doslope)
    _curamp *= _slopePerSample;

  _curamp = softsat(_curamp, 1.01f);
  //    assert(_curamp<=1.0f);
  _curamp = clip_float(_curamp, 0, 1);
  // assert(_curamp>=-1.0f);
  // if( ((_curseg+1)==_numseg) and (dbatten<-96.0f) )
  // if( _layer && _layer->isHudLayer() )
  //   printf( "seg<%d> _curamp<%f> _slopePerSecond<%f> _slopePerSample<%f>\n", _curseg, _curamp, _slopePerSecond, _slopePerSample
  //   );
  // assert(false);
  return _curamp;
}

} // namespace ork::audio::singularity
