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
  _startval = _curval;
  _destval  = adjusted_segment._level;
  //////////////////////////////////////////////
  float segtime = adjusted_segment._time;
  //////////////////////////////////////////////
  if (segtime > 0.0f) {
    _lerpincr  = getInverseControlRate() / segtime;
    _lerpindex = 0.0f;
  } else {
    _lerpincr  = 0.0f;
    _lerpindex = 1.0f;
  }
  if (1)
    printf(
        "env<%p> initseg<%d> _startval<%g> _destval<%g> segtime<%g> _lerpincr<%g>\n", //
        this,
        iseg,
        _startval,
        _destval,
        segtime,
        _lerpincr);
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
      _curval          = lerp(_startval, _destval, std::clamp(_lerpindex, 0.0f, 1.0f));
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
      _curval          = lerp(_startval, _destval, std::clamp(_lerpindex, 0.0f, 1.0f));
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
      printf("env<%p> done\n", this);
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
  // printf("env<%p> _state<%d> _curval<%g>\n", this, _state, _curval);
}

///////////////////////////////////////////////////////////////////////////////

void RateLevelEnvInst::keyOn(const KeyOnInfo& KOI) {
  _layer         = KOI._layer;
  _konoffinfo    = KOI;
  _state         = 0;
  auto ld        = KOI._LayerData;
  int ikey       = KOI._key;
  _ignoreRelease = ld->_ignRels;
  _released      = false;
  //////////////////////////////////
  // printf("env<%p> keyon\n", this);
  //////////////////////////////////
  if (_data) {
    _curval = 0.0f;
    initSeg(0);
    if (_ampenv)
      _layer->retain();
  }
  //////////////////////////////////
  else {
    _startval = 0.0f;
    _destval  = 0.0f;
    _curval   = 0.0f;
  }
  //////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void RateLevelEnvInst::keyOff() {
  // printf("env<%p> keyoff\n", this);
  _released = true;
  _state    = 2;
  if (_data && false == _ignoreRelease) {
    if (_data->_sustainSegment >= 0)
      initSeg(_data->_sustainSegment + 1);
  }
}

} // namespace ork::audio::singularity
