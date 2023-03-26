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
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::AsrData, "SynAsr");

namespace ork::audio::singularity {

void AsrData::describeX(class_t* clazz) {
}

AsrData::AsrData() {
  _envadjust = [](const EnvPoint& inp, //
                  int iseg,
                  const KeyOnInfo& KOI) -> EnvPoint { //
    return inp;
  };
}

ControllerInst* AsrData::instantiate(layer_ptr_t l) const // final
{
  return new AsrInst(this, l);
}

AsrInst::AsrInst(const AsrData* data, layer_ptr_t l)
    : ControllerInst(l)
    , _data(data)
    , _curseg(-1)
    , _mode(0)
    , _released(false)
    , _curslope_persamp(0.0f) {
}

///////////////////////////////////////////////////////////////////////////////

void AsrInst::initSeg(int iseg) {
  if (!_data)
    return;

  _curseg = iseg;
  assert(_curseg >= 0);
  assert(_curseg <= 3);
  const auto& edata = *_data;
  EnvPoint ep       = {0, 0};

  switch (iseg) {
    case 0: // delay
      ep._time  = edata._delay;
      ep._level = 0;
      break;
    case 1: // atk
      ep._time  = edata._attack;
      ep._level = 1;
      break;
    case 2: // hold
      _curval           = 1;
      _curslope_persamp = 0.0f;
      return;
      break;
    case 3: // release
      ep._time  = edata._release;
      ep._level = 0;
      break;
  }

  ep = edata._envadjust(ep, iseg, _konoffinfo);

  if (ep._time == 0.0f) {
    _curval           = ep._level;
    _curslope_persamp = 0.0f;
  } else {
    float deltlev     = (ep._level - _curval);
    float slope       = (deltlev / ep._time);
    _curslope_persamp = slope / getSampleRate();
  }
  _framesrem = ep._time * getSampleRate(); // / 48000.0f;

  // printf( "AsrInst<%p> _data<%p> initSeg<%d> segtime<%f> dstlevl<%f> _curval<%f>\n", this, _data, iseg, segtime, dstlevl, _curval
  // );
}

///////////////////////////////////////////////////////////////////////////////

void AsrInst::compute() // final
{
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
}

///////////////////////////////////////////////////////////////////////////////

void AsrInst::keyOn(const KeyOnInfo& KOI) // final
{
  _konoffinfo    = KOI;
  auto ld        = KOI._layerdata;
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
} // namespace ork::audio::singularity
