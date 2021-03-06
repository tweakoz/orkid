#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::RateLevelEnvData, "SynRateLevelEnv");

namespace ork::audio::singularity {

void RateLevelEnvData::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////
// 7-seg rate/level envelopes
///////////////////////////////////////////////////////////////////////////////

void RateLevelEnvData::addSegment(std::string name, float time, float level, float power) {
  _segments.push_back(EnvPoint{time, level, power});
  _segmentNames.push_back(name);
}

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
    , _segmentIndex(-1)
    , _released(false)
    , _ampenv(data->_ampenv)
    , _envType(data->_envType)
    , _layer(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

void RateLevelEnvInst::initSeg(int iseg) {
  if (!_data)
    return;
  const auto& edata = *_data;
  const auto& segs  = edata._segments;
  size_t numsegs    = segs.size();
  size_t maxseg     = numsegs - 1;
  OrkAssert(iseg >= 0);
  OrkAssert(iseg < numsegs);
  _segmentIndex = iseg;
  //////////////////////////////////////////////
  const auto& base_segment = segs[_segmentIndex];
  auto adjusted_segment    = edata._envadjust(base_segment, iseg, _konoffinfo);
  //////////////////////////////////////////////
  _startval   = _curval;
  _rawdestval = base_segment._level;
  _rawtime    = base_segment._time;
  _destval    = adjusted_segment._level;
  _curshape   = base_segment._shape;
  //////////////////////////////////////////////
  _adjtime = adjusted_segment._time;
  //////////////////////////////////////////////
  if (_adjtime > 0.0f) {
    _lerpincr  = controlPeriod() / _adjtime;
    _lerpindex = 0.0f;
  } else {
    _lerpincr  = 0.0f;
    _lerpindex = 1.0f;
  }
}
///////////////////////////////////////////////////////////////////////////////
float RateLevelEnvInst::shapedvalue() const {
  float clamped_index = std::clamp(_lerpindex, 0.0f, 1.0f);
  float index         = _curshape >= 0.0f //
                    ? powf(clamped_index, _curshape)
                    : smoothstep(-_curshape * 0.05, 1 + _curshape * 0.05, clamped_index);
  float rawval = lerp(_startval, _destval, index);
  return std::clamp(rawval, _clampmin, _clampmax);
}
///////////////////////////////////////////////////////////////////////////////
void RateLevelEnvInst::compute() // final
{
  if (nullptr == _data)
    return;
  /////////////////////////////
  const auto& edata = *_data;
  const auto& segs  = edata._segments;
  size_t numsegs    = segs.size();
  size_t maxseg     = numsegs - 1;
  /////////////////////////////
  switch (_state) {
    ////////////////////////////////////////////////////////////////////////////
    case 0: { // atkdec
      ///////////////////////////////////////////
      _lerpindex += _lerpincr;
      _curval          = shapedvalue();
      bool try_advance = (_lerpindex >= 1.0);
      ///////////////////////////////////////////
      if (try_advance) {
        if (_segmentIndex == edata._sustainSegment) { // hold at sustain
          _state = 1;
        } else if (edata._sustainSegment == -1) { // this env has no sustain...
          // so go to release
          _state = 2;
        } else if (_segmentIndex < edata._sustainSegment) { // we are not at sustain yet..
          if (_segmentIndex >= maxseg)
            _state = 3; // straight to done..
          else
            initSeg(_segmentIndex + 1);
        } else {
          OrkAssert(false);
        }
      }

      break;
    }
    ////////////////////////////////////////////////////////////////////////////
    case 1: // sustain
      break;
    ////////////////////////////////////////////////////////////////////////////
    case 2: { // release
      ///////////////////////////////////////////
      _lerpindex += _lerpincr;
      _curval          = shapedvalue();
      bool try_advance = (_lerpindex >= 1.0);
      ///////////////////////////////////////////
      if (try_advance) {
        if (_segmentIndex >= maxseg)
          _state = 3;
        else
          initSeg(_segmentIndex + 1);
      } else {
        ///////////////////////////////////////////
        // if its an amp env, the envelope is done
        //  when the value is inaudible
        ///////////////////////////////////////////
        if (_ampenv) {
          float dbatten = linear_amp_ratio_to_decibel(_curval);
          bool done     = (_segmentIndex >= numsegs) //
                      and (dbatten < -96.0f);
          if (done) //
            _state = 3;
        }
        ///////////////////////////////////////////
      }
      break;
    }
      ////////////////////////////////////////////////////////////////////////////
    case 3: // done
      // printf("env<%p> done\n", this);
      if (_ampenv) {
        // printf("ampenv<%p> RELEASING LAYER<%p>\n", this, _layer);
        _layer->release();
        _curval = 0.0;
      } else {
        _curval = segs.rbegin()->_level;
      }
      _data  = nullptr;
      _state = 4;
      break;
      ////////////////////////////////////////////////////////////////////////////
    case 4: // dead (NOP)
      break;
  }
  //////////////////////////////////////
  // SignalScope
  //////////////////////////////////////
  if (_data and _data->_scopesource) {
    _data->_scopesource->updateController(this);
  }
  //////////////////////////////////////
  _updatecount++;
  // printf("env<%p> _state<%d> _curval<%g>\n", this, _state, _curval);
}

///////////////////////////////////////////////////////////////////////////////

void RateLevelEnvInst::keyOn(const KeyOnInfo& KOI) {
  _layer         = KOI._layer;
  _konoffinfo    = KOI;
  _state         = 0;
  auto ld        = KOI._layerdata;
  int ikey       = KOI._key;
  _ignoreRelease = ld->_ignRels;
  _released      = false;
  //////////////////////////////////
  // printf("env<%p> keyon\n", this);
  //////////////////////////////////
  if (_data) {
    ///////////////////////
    // get env vertical extent
    ///////////////////////
    _clampmin = 1e6;
    _clampmax = -1e6;
    for (auto seg : _data->_segments) {
      float s = seg._level;
      if (s > _clampmax)
        _clampmax = s;
      if (s < _clampmin)
        _clampmin = s;
    }
    _clamprange = (_clampmax - _clampmin);
    ///////////////////////
    _curval = 0.0f;
    initSeg(0);
    if (_ampenv)
      _layer->retain();
  }
  //////////////////////////////////
  else {
    _startval   = 0.0f;
    _destval    = 0.0f;
    _curval     = 0.0f;
    _clampmin   = 0.0f;
    _clampmax   = 1.0f;
    _clamprange = 1.0f;
  }
  //////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void RateLevelEnvInst::keyOff() {
  // printf("env<%p> keyoff\n", this);
  _released = true;
  _state    = 2;
  if (_data && false == _ignoreRelease) {
    if (_data->_releaseSegment >= 0)
      initSeg(_data->_releaseSegment);
  }
}

} // namespace ork::audio::singularity
