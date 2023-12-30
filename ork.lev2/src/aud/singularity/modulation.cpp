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
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectObject.inl>
#include <ork/util/logger.h>

ImplementReflectionX(ork::audio::singularity::BlockModulationData, "SynBlockModulation");
ImplementReflectionX(ork::audio::singularity::DspParamData, "SynDspParam");

namespace ork::audio::singularity {
static logchannel_ptr_t logchan_modulation = logger()->createChannel("singul.mod", fvec3(1, 0.3, 1), false);

///////////////////////////////////////////////////////////////////////////////

void BlockModulationData::describeX(class_t* clazz) {
  clazz->directObjectProperty("Src1", &BlockModulationData::_src1);
  clazz->directProperty("Src1Depth", &BlockModulationData::_src1Depth);
  clazz->directObjectProperty("Src2", &BlockModulationData::_src2);
  clazz->directProperty("Src2MinDepth", &BlockModulationData::_src2MinDepth);
  clazz->directProperty("Src2MaxDepth", &BlockModulationData::_src2MaxDepth);
  clazz->directObjectProperty("Src2DepthControl", &BlockModulationData::_src2DepthCtrl);
}

///////////////////////////////////////////////////////////////////////////////

BlockModulationData::BlockModulationData() {
  _evaluator = [](DspParam& param_inst) -> //
      float { return param_inst._data->_coarse; };
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::describeX(class_t* clazz) {
  clazz->directObjectProperty("Mods", &DspParamData::_mods);
  clazz->directProperty("Name", &DspParamData::_name);
  clazz->directProperty("EvaluatorID", &DspParamData::_evaluatorid);
  clazz->directProperty("Coarse", &DspParamData::_coarse);
  clazz->directProperty("Fine", &DspParamData::_fine);
  clazz->directProperty("FineHZ", &DspParamData::_fineHZ);
  clazz->directProperty("KeyTrack", &DspParamData::_keyTrack);
  clazz->directProperty("VelTrack", &DspParamData::_velTrack);
  clazz->directProperty("KeyStart", &DspParamData::_keystartNote);
  clazz->directProperty("KeyStartBipolar", &DspParamData::_keystartBipolar);
}

///////////////////////////////////////////////////////////////////////////////

DspParamData::DspParamData(std::string name) {
  _name = name;
  _mods = std::make_shared<BlockModulationData>();
  useDefaultEvaluator();
}

///////////////////////////////////////////////////////////////////////////////

bool DspParamData::postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) { // override

  if (_evaluatorid == "default")
    useDefaultEvaluator();
  else if (_evaluatorid == "amplitude")
    useAmplitudeEvaluator();
  else if (_evaluatorid == "pitch")
    usePitchEvaluator();
  else if (_evaluatorid == "frequency")
    useFrequencyEvaluator();
  else if (_evaluatorid == "krzpos")
    useKrzPosEvaluator();
  else if (_evaluatorid == "krzevnodd")
    useKrzEvnOddEvaluator();
  else {
    OrkAssert(false);
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::useDefaultEvaluator() {
  _evaluatorid            = "default";
  _edit_coarse_min        = -1.0f;
  _edit_coarse_max        = 1.0f;
  _edit_coarse_numsteps   = 102;
  _edit_fine_min          = -0.01f;
  _edit_fine_max          = 0.01f;
  _edit_fine_numsteps     = 102;
  _edit_keytrack_min      = -1.0f;
  _edit_keytrack_max      = 1.0f;
  _edit_keytrack_numsteps = 402;
  _edit_keytrack_shape    = 1.0f;
  _mods->_evaluator       = [this](DspParam& param_inst) -> float {
    float kt = _keyTrack * param_inst._keyOff;
    float vt = -_velTrack * param_inst._unitVel;
    float rv = _coarse     //
               + _fine     //
               + param_inst._C1() //
               + param_inst._C2() //
               + kt + vt;
     //printf("defaulteval rv<%g>\n", rv);
    return rv;
  };
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::useAmplitudeEvaluator() {
  _evaluatorid      = "amplitude";
  _mods->_evaluator = [this](DspParam& param_inst) -> float {
    param_inst._kval  = _keyTrack * param_inst._keyOff;
    param_inst._vval  = lerp(-_velTrack, 0.0f, param_inst._unitVel);
    param_inst._s1val = param_inst._C1();
    param_inst._s2val = param_inst._C2();
    float x    = (_coarse) //
              + param_inst._s1val //
              + param_inst._s2val //
              + param_inst._kval  //
              + param_inst._vval;
     printf("vt<%f> kt<%f> x<%f>\n", _velTrack, _keyTrack, x);
    return x;
  };
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::usePitchEvaluator() {
  _evaluatorid = "pitch";

  _units = "cents";
  _edit_coarse_min        = 0.0f;
  _edit_coarse_numsteps   = 120;
  _edit_coarse_max        = _edit_coarse_numsteps * 100.0f;
  _edit_fine_min          = -50.0f;
  _edit_fine_max          = 50.0f;
  _edit_fine_numsteps     = 102;
  _edit_keytrack_numsteps = 402;
  _edit_keytrack_min      = -200.0f;
  _edit_keytrack_max      = 200.0f;
  _edit_keytrack_numsteps = 400;
  _keyTrack = 100.0;

  _mods->_evaluator = [this](DspParam& param_inst) -> float {

    float kt       = _keyTrack * param_inst._keyOff;
    float vt       = _velTrack * param_inst._unitVel;
    float totcents = param_inst._keyRaw*100 // 60(midi-middle-C) * 100 cents
                     + (_coarse)   //
                     + _fine       //
                     + param_inst._C1()   //
                     + param_inst._C2()   //
                     + kt          //
                     + vt;
    // float ratio = cents_to_linear_freq_ratio(totcents);
    // printf( "rat<%f>\n", ratio);
    
    if(param_inst._update_count==0){
      logchan_modulation->log( "keyRaw<%d>", param_inst._keyRaw);
      logchan_modulation->log( "keyOff<%f>", param_inst._keyOff);
      logchan_modulation->log( "coarse<%f>", _coarse);
      logchan_modulation->log( "fine<%f>", _fine);
      logchan_modulation->log( "c1<%f>", param_inst._C1());
      logchan_modulation->log( "c2<%f>", param_inst._C2());
      logchan_modulation->log( "vt<%f>", vt);
      logchan_modulation->log( "kt<%f>", kt);
      logchan_modulation->log( "totcents<%f>", totcents);
    }

    param_inst._update_count++;
    return totcents;
  };
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::useFrequencyEvaluator() {
  _evaluatorid      = "frequency";
  _mods->_evaluator = [this](DspParam& param_inst) -> float {
    float ktcents  = _keyTrack * param_inst._keyOff;
    param_inst._vval      = _velTrack * param_inst._unitVel;
    float vtcents  = param_inst._vval;
    float totcents = param_inst._C1() + param_inst._C2() + ktcents + vtcents;
    float ratio    = cents_to_linear_freq_ratio(totcents);
     //printf( "vtcents<%f> ratio<%f>\n", vtcents, ratio );
     //printf( "ratio<%f>\n", ratio);
    return _coarse * ratio;
  };
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::useKrzPosEvaluator() {
  _evaluatorid      = "krzpos";
  _mods->_evaluator = [this](DspParam& param_inst) -> float {
    param_inst._kval  = _keyTrack * param_inst._keyOff;
    param_inst._vval  = _velTrack * param_inst._unitVel;
    param_inst._s1val = param_inst._C1();
    param_inst._s2val = param_inst._C2();
    float x    = (_coarse) //
              + param_inst._s1val //
              + param_inst._s2val //
              + param_inst._kval  //
              + param_inst._vval;
    return clip_float(x, -100, 100);
  };
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::useKrzEvnOddEvaluator() {
  _evaluatorid      = "krzevnodd";
  _mods->_evaluator = [this](DspParam& param_inst) -> float {
    float kt = _keyTrack * param_inst._keyOff;
    float vt = lerp(-_velTrack, 0.0f, param_inst._unitVel);
    float x  = (_coarse)  //
              + param_inst._C1() //
              + param_inst._C2() //
              + kt        //
              + vt;
    // printf( "vt<%f> kt<%f> x<%f>\n", vt, kt, x );
    return clip_float(x, -10, 10);
  };
}

///////////////////////////////////////////////////////////////////////////////

DspParam::DspParam() {
  _data = std::make_shared<DspParamData>();
  reset();
}

void DspParam::reset() {
  _evaluator = [](DspParam& param_inst) { return 0.0f; };
  _C1        = []() { return 0.0f; };
  _C2        = []() { return 0.0f; };
}

void DspParam::keyOn(int ikey, int ivel) {


  _update_count = 0;
  _keyRaw  = ikey;
  _keyOff  = float(60 - _data->_keystartNote);
  _unitVel = float(ivel) / 127.0f;

   logchan_modulation->log( "DspParam::keyOn: ikey<%d> ivel<%d> keystart<%d> _keyOff<%g>", ikey, ivel, _data->_keystartNote, _keyOff );

  if (false == _data->_keystartBipolar) {
    if (_keyOff < 0)
      _keyOff = 0;

    if (_data->_keystartNote == 0)
      _keyOff = 0;

  }

  //logchan_modulation->log( "DspParam: _keystartNote<%d>", _data->_keystartNote );
  //logchan_modulation->log( "DspParam: _keyOff<%f>", _keyOff );
  //logchan_modulation->log( "DspParam: _unitVel<%f>", _unitVel );

  if(ikey!=0){
    //OrkAssert(false);

  }

}

///////////////////////////////////////////////////////////////////////////////

float DspParam::eval(bool dump) {
  float tot = _evaluator(*this);
  if (dump)
    printf("coarse<%g> c1<%g> c2<%g> tot<%g>\n", _data->_coarse, _C1(), _C2(), tot);

  return tot;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
