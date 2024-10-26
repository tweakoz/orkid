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
  clazz->directProperty("Src1Depth", &BlockModulationData::_src1Scale);
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

dspparammod_ptr_t BlockModulationData::clone() const{
  auto rval = std::make_shared<BlockModulationData>();
  rval->_src1 = _src1;
  rval->_src2 = _src2;
  rval->_src2DepthCtrl = _src2DepthCtrl;
  rval->_src1Scale = _src1Scale;
  rval->_src2MinDepth = _src2MinDepth;
  rval->_src2MaxDepth = _src2MaxDepth;
  rval->_evaluator = _evaluator;
  return rval;
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

dspparam_ptr_t DspParamData::clone() const{
  auto rval = std::make_shared<DspParamData>();
  rval->_name = _name;
  rval->_mods = _mods->clone();
  rval->_evaluatorid = _evaluatorid;
  rval->_coarse = _coarse;
  rval->_fine = _fine;
  rval->_fineHZ = _fineHZ;
  rval->_keyTrack = _keyTrack;
  rval->_velTrack = _velTrack;
  rval->_keystartNote = _keystartNote;
  rval->_keystartBipolar = _keystartBipolar;
  rval->_edit_coarse_min = _edit_coarse_min;
  rval->_edit_coarse_max = _edit_coarse_max;
  rval->_edit_coarse_numsteps = _edit_coarse_numsteps;
  rval->_edit_fine_min = _edit_fine_min;
  rval->_edit_fine_max = _edit_fine_max;
  rval->_edit_fine_numsteps = _edit_fine_numsteps;
  rval->_edit_keytrack_min = _edit_keytrack_min;
  rval->_edit_keytrack_max = _edit_keytrack_max;
  rval->_edit_keytrack_numsteps = _edit_keytrack_numsteps;
  rval->_edit_keytrack_shape = _edit_keytrack_shape;
  rval->_units = _units;
  rval->_debug = _debug;
  return rval;
}

void DspParamData::dump() const {
  printf( "DspParam<%p:%s>\n", this, _name.c_str() );
  printf( " _coarse<%f>\n", _coarse );
  printf( " _fine<%f>\n", _fine );
  printf( " _fineHZ<%f>\n", _fineHZ );
  printf( " _keyTrack<%f>\n", _keyTrack );
  printf( " _velTrack<%f>\n", _velTrack );
  printf( " _keystartNote<%d>\n", _keystartNote );
  printf( " _keystartBipolar<%d>\n", _keystartBipolar );
  printf( " _units<%s>\n", _units.c_str() );
  printf( " _evaluatorid<%s>\n", _evaluatorid.c_str() ); 
}
///////////////////////////////////////////////////////////////////////////////

DspParamData::DspParamData(std::string name) {
  _name = name;
  _mods = std::make_shared<BlockModulationData>();
  useDefaultEvaluator();
  //_keyTrack = 100.0f;

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
     if(_debug){
      printf("defeval<%s> rv<%g>\n", _name.c_str(), rv);
     }
    return rv;
  };
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::useAmplitudeEvaluator() {
  _evaluatorid      = "amplitude";
  _mods->_evaluator = [this](DspParam& param_inst) -> float {
    float KT = _keyTrack * param_inst._keyOff;
    param_inst._kval  = KT;
    float VT = lerp(-_velTrack, 0.0f, param_inst._unitVel);
    param_inst._vval  = VT;
    float c1 = param_inst._C1();
    float c2 = param_inst._C2();
    param_inst._s1val = c1;
    param_inst._s2val = c2;
    float x    = (_coarse) //
              + c1 //
              + c2 //
              + KT  //
              + VT;
     if(_debug){
      printf("ampeval<%s> unitvel<%g> _coarse<%g> vt<%f> VT<%g> kt<%f> KT<%g> c1<%g> c2<%g> x<%f>\n", _name.c_str(), param_inst._unitVel, _coarse, _velTrack, VT, _keyTrack, KT, c1, c2, x);
     }
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

  _mods->_evaluator = [this](DspParam& param_inst) -> float {

    float KEY = param_inst._keyOff;

    float kr       = (KEY*_keyTrack);
    float vt       = _velTrack * param_inst._unitVel;
    float c1      = param_inst._C1();
    float c2      = param_inst._C2();
    float totcents = (_coarse * 100.0)
                   +_fine
                   + kr // 
                   + c1   //
                   + c2   //
                   + vt;
     if( _debug and param_inst._update_count==0){
       float ratio = cents_to_linear_freq_ratio(totcents);
       printf( "pitcheval<%p:%s> _keyTrack<%g> kr<%g> course<%f> fine<%g> c1<%g> c2<%g> ko<%g> vt<%g> totcents<%f> rat<%f>\n", //
               (void*) this, _name.c_str(), // 
               _keyTrack,
              kr, //  
               _coarse, //
               _fine, //
                c1, //
                c2, //
               param_inst._keyOff, // 
               vt, // 
               totcents, // 
               ratio);
     }
    
    if(param_inst._update_count==0){
      logchan_modulation->log( "keyRaw<%d>", param_inst._keyRaw);
      logchan_modulation->log( "keyOff<%f>", param_inst._keyOff);
      logchan_modulation->log( "coarse<%f>", _coarse);
      logchan_modulation->log( "fine<%f>", _fine);
      logchan_modulation->log( "c1<%f>", param_inst._C1());
      logchan_modulation->log( "c2<%f>", param_inst._C2());
      logchan_modulation->log( "vt<%f>", vt);
      //logchan_modulation->log( "kt<%f>", kt);
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

    float kt       = (param_inst._keyOff *_keyTrack);
    float vt       = (param_inst._unitVel * _velTrack);
    float c1       = param_inst._C1();
    float c2       = param_inst._C2();



    param_inst._vval      = vt;
    float totcents = c1 + c2 + kt + vt;
    float ratio    = cents_to_linear_freq_ratio(totcents);
    float output = _coarse * ratio;
     if(_debug){
       printf( "frqeval<%s> coarse<%g> kt<%f> ko<%g> vt<%f> c1<%g> c2<%g> ratio<%f> output<%g>\n", _name.c_str(), _coarse, kt, param_inst._keyOff, vt, c1, c2, ratio, output );
     }
    return output;
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
    float rval = clip_float(x, -100, 100);

    if(_debug){
      printf( "krzposeval<%s> rval<%f>\n", _name.c_str(), rval );
    }
    return rval;
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
    if(_debug){
      printf( "krzevnoddeval<%s> vt<%f> kt<%f> x<%f>\n", _name.c_str(), vt, kt, x );
    }
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
  _C1        = [] { return 0.0f; };
  _C2        = [] { return 0.0f; };
}

void DspParam::keyOn(int ikey, int ivel) {


  _update_count = 0;
  _keyRaw  = ikey;
  _keyOff  = float(ikey - _data->_keystartNote);
  _unitVel = float(ivel) / 127.0f;
  float kt = _data->_keyTrack;
  
  if(_data->_debug){
   printf( "DspParam<%s> keyOn: ikey<%d> ivel<%d> _unitVel<%g> keytrack<%g>  keystart<%d> _keyOff<%g>\n", _data->_name.c_str(), ikey, ivel, _unitVel, kt, _data->_keystartNote, _keyOff );
  }

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
  if (dump){
    printf("coarse<%g> c1<%g> c2<%g> tot<%g>\n", _data->_coarse, _C1(), _C2(), tot);
  }

  return tot;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
