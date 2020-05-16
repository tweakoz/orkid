#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/synth.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// 7-seg rate/level envelopes
///////////////////////////////////////////////////////////////////////////////

ControllerInst* AsrData::instantiate(Layer* l) const // final
{
  return new AsrInst(this, l);
}

AsrInst::AsrInst(const AsrData* data, Layer* l)
    : ControllerInst(l)
    , _data(data)
    , _curslope_persamp(0.0f)
    , _curseg(-1)
    , _released(false)
    , _mode(0) {
}

///////////////////////////////////////////////////////////////////////////////

void AsrInst::initSeg(int iseg) {
  if (!_data)
    return;

  _curseg = iseg;
  assert(_curseg >= 0);
  assert(_curseg <= 3);
  const auto& edata = *_data;
  float segtime     = 0.0f;
  float dstlevl     = 0.0f;

  switch (iseg) {
    case 0: // delay
      segtime = edata._delay;
      dstlevl = 0;
      break;
    case 1: // atk
      segtime = edata._attack;
      segtime /= _atkAdjust;
      dstlevl = 1;
      break;
    case 2: // hold
      _curval           = 1;
      _curslope_persamp = 0.0f;
      return;
      break;
    case 3: // release
      segtime = edata._release;
      segtime /= _relAdjust;
      dstlevl = 0;
      break;
  }

  if (segtime == 0.0f) {
    _curval           = dstlevl;
    _curslope_persamp = 0.0f;
  } else {
    float deltlev     = (dstlevl - _curval);
    float slope       = (deltlev / segtime);
    _curslope_persamp = slope / getSampleRate();
  }
  _framesrem = segtime * getSampleRate(); // / 48000.0f;

  // printf( "AsrInst<%p> _data<%p> initSeg<%d> segtime<%f> dstlevl<%f> _curval<%f>\n", this, _data, iseg, segtime, dstlevl, _curval
  // );
}

///////////////////////////////////////////////////////////////////////////////

void AsrInst::compute(int inumfr) // final
{
  auto L = [this]() {
    if (nullptr == _data)
      _curval = 0.5f;

    const auto& edata = *_data;
    _framesrem--;
    if (_framesrem <= 0) {
      switch (_mode) {
        case 0: // normal
          if (_curseg < 3)
            initSeg(_curseg + 1);
          break;
        case 1: // hold
          if (_curseg == 0)
            initSeg(1);
          break;
        case 2: // repeat
          if (_curseg < 3)
            initSeg(_curseg + 1);
          else
            initSeg(0);
          break;
      }
    }

    _curval += _curslope_persamp;
    _curval = clip_float(_curval, 0.0f, 1.0f);
    // printf( "compute ASR<%s> seg<%d> fr<%d> _mode<%d> _curval<%f>\n", _data->_name.c_str(), _curseg, _framesrem, _mode, _curval
    // );
  };
  for (int i = 0; i < inumfr; i++)
    L();
}

///////////////////////////////////////////////////////////////////////////////

void AsrInst::keyOn(const KeyOnInfo& KOI) // final
{
  auto ld = KOI._LayerData;

  const auto& EC = ld->_envCtrlData;
  _atkAdjust     = EC._atkAdjust;
  _relAdjust     = EC._relAdjust;
  _ignoreRelease = ld->_ignRels;
  assert(_data);
  _curval   = 0.0f;
  _released = false;
  if (_data->_mode == "Normal")
    _mode = 0;
  if (_data->_mode == "Hold")
    _mode = 1;
  if (_data->_mode == "Repeat")
    _mode = 2;

  initSeg(0);
}

///////////////////////////////////////////////////////////////////////////////

void AsrInst::keyOff() // final
{
  _released  = true;
  bool dorel = true;

  if (false == _ignoreRelease)
    dorel = false;
  if (_data and _data->_trigger != "ON")
    dorel = false;

  if (dorel)
    initSeg(3);
}

bool AsrInst::isValid() const {
  return _data and _data->_name.length();
}

///////////////////////////////////////////////////////////////////////////////
// 7-seg rate/level envelopes
///////////////////////////////////////////////////////////////////////////////

RateLevelEnvData::RateLevelEnvData()
    : _ampenv(false)
    , _bipolar(false)
    , _envType(RlEnvType::ERLTYPE_DEFAULT) {
}

ControllerInst* RateLevelEnvData::instantiate(Layer* l) const // final
{
  return new RateLevelEnvInst(this, l);
}

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
  float segtime     = curseg._rate;

  switch (iseg) {
    case 0:
    case 1:
    case 2: // atk
      segtime /= _atkAdjust;
      break;
    case 3: // decay
      segtime /= _decAdjust;
      break;
    case 4:
    case 5:
    case 6: // rel
      segtime /= _relAdjust;
      break;
  }

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
  // printf( "env<%s> iseg<%d> segt<%f> curv<%f> dest<%f>\n", _data->_name.c_str(), iseg, segtime, _curval, _dstval );
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
  _layer = KOI._layer;

  auto ld  = KOI._LayerData;
  int ikey = KOI._key;

  const auto& EC  = ld->_envCtrlData;
  const auto& DKT = EC._decKeyTrack;
  const auto& RKT = EC._relKeyTrack;
  // printf("ikey<%d> DKT<%f>\n", ikey, DKT);

  float fkl = -1.0f + float(ikey) / 63.5f;

  _atkAdjust = EC._atkAdjust;
  _decAdjust = EC._decAdjust;
  _relAdjust = EC._relAdjust;

  if (ikey > 60) {
    float flerp = float(ikey - 60) / float(127 - 60);
    _decAdjust  = lerp(_decAdjust, DKT, flerp);
    _relAdjust  = lerp(_relAdjust, RKT, flerp);
  } else if (ikey < 60) {
    float flerp = float(59 - ikey) / 59.0f;
    _decAdjust  = lerp(_decAdjust, 1.0 / DKT, flerp);
    _relAdjust  = lerp(_relAdjust, 1.0 / RKT, flerp);
  }
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
