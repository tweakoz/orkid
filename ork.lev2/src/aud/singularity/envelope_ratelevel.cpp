#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// 7-seg rate/level envelopes
///////////////////////////////////////////////////////////////////////////////

bool RateLevelEnvData::isBiPolar() const {
  bool rval = false;
  for (auto& item : _segments)
    if (item._level < 0.0f)
      rval = true;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

RateLevelEnvData::RateLevelEnvData()
    : _ampenv(false)
    , _bipolar(false)
    , _envType(RlEnvType::ERLTYPE_DEFAULT) {

  _envadjust = [](const EnvPoint& inp, //
                  int iseg,
                  const KeyOnInfo& KOI) -> EnvPoint { //
    return inp;
  };
}

///////////////////////////////////////////////////////////////////////////////

ControllerInst* RateLevelEnvData::instantiate(Layer* l) const // final
{
  return new RateLevelEnvInst(this, l);
}

///////////////////////////////////////////////////////////////////////////////

RateLevelEnvInst::RateLevelEnvInst(const RateLevelEnvData* data, Layer* l)
    : ControllerInst(l)
    , _data(data)
    , _curslope_persamp(0.0f)
    , _curseg(-1)
    , _released(false)
    , _ampenv(data->_ampenv)
    , _bipolar(data->_bipolar)
    , _envType(data->_envType)
    , _layer(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

void RateLevelEnvInst::initSeg(int iseg) {
  if (!_data)
    return;

  _curseg = iseg;
  assert(_curseg >= 0);
  assert(_curseg < 7);
  const auto& edata = *_data;
  auto curseg       = edata._segments[_curseg];
  curseg            = edata._envadjust(curseg, iseg, _konoffinfo);
  float segtime     = curseg._time;

  if (segtime == 0.0f) {
    switch (_envType) {
      case RlEnvType::ERLTYPE_KRZAMPENV:
        if (iseg == 0 or iseg == 6) {
          _dstval           = curseg._level;
          _curval           = _dstval;
          _curslope_persamp = 0.0f;
        }
        break;
      case RlEnvType::ERLTYPE_DEFAULT:
        if (iseg == 0 or iseg == 6) {
          _dstval           = curseg._level;
          _curval           = _dstval;
          _curslope_persamp = 0.0f;
        }
        break;
      default:
        // if( iseg>0 ) { // iseg==1 or iseg==2 or iseg==4 or iseg==5 ){
        // attack segss 2 and 3 only have effect
        // if their times are not 0
        //    printf( "segt0 iseg<%d>\n", iseg );
        //}
        break;
    }

  } else {
    _dstval           = curseg._level;
    float deltlev     = (_dstval - _curval);
    float slope       = (deltlev / segtime);
    _curslope_persamp = slope / getSampleRate();
  }
  _framesrem = segtime * getSampleRate(); // / 48000.0f;
                                          // assert(false);
  printf("env<%s> iseg<%d> segt<%f> curv<%f> dest<%f>\n", _data->_name.c_str(), iseg, segtime, _curval, _dstval);
}

///////////////////////////////////////////////////////////////////////////////

void RateLevelEnvInst::compute(int inumfr) // final
{
  // printf( "RateLevelEnvInst<%p> inumfr<%d>\n", this, inumfr );
  auto L = [this]() -> float {
    if (nullptr == _data)
      return 0.0f;

    const auto& edata = *_data;
    const auto& segs  = edata._segments;
    bool done         = false;
    _framesrem--;
    if (_framesrem <= 0) {
      if (_released) {
        if (_curseg <= 5) {
          initSeg(_curseg + 1);
        } else {
          done    = true;
          _curval = _dstval;
          //_data = nullptr;
        }
      } else // go up to decay
      {
        if (_curseg == 3) {
          float declev = segs[3]._level;
          if (_curval < declev) {
            _curslope_persamp = 0.0f;
            _curval           = declev;
          }
        } else if (_curseg <= 2)
          initSeg(_curseg + 1);
      }
    }
    if (!done) {
      _curval += _curslope_persamp;
      if (_curslope_persamp > 0.0f && _curval > _dstval)
        _curval = _dstval;
      else if (_curslope_persamp < 0.0f && _curval < _dstval)
        _curval = _dstval;
    }

    _filtval = _filtval * 0.995f + _curval * 0.005f;

    float sign = _filtval < 0.0f ? -1.0f : 1.0f;

    float rval = sign * clip_float(powf(_filtval, 2.0f), -1.0f, 1.0f);

    float dbatten = linear_amp_ratio_to_decibel(rval);

    done = (_curseg == 6) && (dbatten < -96.0f);
    if (done && _ampenv) {
      _layer->release();
      _data = nullptr;
      // printf("ENV RELEASING LAYER<%p>\n", _layer);
    }
    return rval;
  };
  /////////////////////////////////////
  for (int i = 0; i < inumfr; i++)
    _USERAMPENV[i] = L();
  /////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void RateLevelEnvInst::keyOn(const KeyOnInfo& KOI) {
  _layer      = KOI._layer;
  _konoffinfo = KOI;

  auto ld  = KOI._LayerData;
  int ikey = KOI._key;

  // printf( "flerp<%f> _decAdjust<%f>\n", flerp,_decAdjust);

  // printf( "_relAdjust<%f>\n", _relAdjust );
  _ignoreRelease = ld->_ignRels;
  _curval        = 0.0f;
  _dstval        = 0.0f;
  _released      = false;

  if (_data) {
    if (_ampenv)
      _layer->retain();

    initSeg(0);
  }
}

///////////////////////////////////////////////////////////////////////////////

void RateLevelEnvInst::keyOff() {
  _released = true;
  if (_data && false == _ignoreRelease)
    initSeg(4);
}

} // namespace ork::audio::singularity
