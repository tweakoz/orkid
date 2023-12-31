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
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::ControllerData, "SynControllerData");
ImplementReflectionX(ork::audio::singularity::CustomControllerData, "SynCustomControllerData");
ImplementReflectionX(ork::audio::singularity::ConstantControllerData, "SynConstControllerData");

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
void ControllerData::describeX(class_t* clazz) {
  clazz->directProperty("Name", &ControllerData::_name);
}
///////////////////////////////////////////////////////////////////////////////
scopesource_ptr_t ControllerData::createScopeSource() {
  _scopesource = std::make_shared<ScopeSource>();
  return _scopesource;
}
///////////////////////////////////////////////////////////////////////////////

controllerdata_ptr_t ControlBlockData::controllerByName(std::string named){
  controllerdata_ptr_t rval;
  auto it = _controllers_by_name.find(named);
  if(it!=_controllers_by_name.end()){
    rval = it->second;
  }
  else{
    if (named == "OFF") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        cci->_value.x = 0.0f;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "ON") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        cci->_value.x = 1.0f;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "-ON") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        cci->_value.x = -1.0f;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "PWheel") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        float pwheel = 0.0f;//synth::instance()->_doModWheel;
        cci->_value.x = (cci->_value.x* 0.99) + (pwheel * 0.01);
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "MWheel") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        float mwheel = synth::instance()->_doModWheel;
        cci->_value.x = (cci->_value.x* 0.99) + (mwheel * 0.01);
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "MPress") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        float mpress = synth::instance()->_doPressure;
        cci->_value.x = (cci->_value.x* 0.99) + (mpress * 0.01);
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "MIDI(49)") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        float lt = L->_layerTime;
        float s  = sinf(lt * pi2 * 8.0f);
        s        = (s >= 0.0f) ? 1.0f : 0.0f;
        cci->_value.x = (cci->_value.x* 0.99) + (s * 0.01);
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "KeyNum") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        cci->_value.x = L->_curnote / float(127.0f);
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "BKeyNum") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        cci->_value.x = -1.0f+(L->_curnote / float(127.0f))*2.0f;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "RandV1") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        cci->_value.x = -1.0f + float(rand() & 0xffff) / 32768.0f;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "AttVel") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        float atkvel = float(L->_curvel) / 127.0f;
        cci->_value.x = atkvel;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "InvAttVel") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        float atkvel = float(127-L->_curvel) / 127.0f;
        cci->_value.x = atkvel;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "Bi-AVel") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        float atkvel = (float(L->_curvel) / 63.5f)-1.0f;
        cci->_value.x = atkvel;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "VTRIG1") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        float atkvel = float(L->_curvel > 64);
        cci->_value.x = atkvel;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "VTRIG2") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        float atkvel = float(L->_curvel > 96);
        cci->_value.x = atkvel;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "Data") { // data knob
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        cci->_value.x = 0.0f;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "AbsPwl") { // absolute value of pitch wheel
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        cci->_value.x = 0.0f;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "ATKSTATE") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        float atkstate = 0.0f;
        cci->_value.x = atkstate;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "RELSTATE") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        auto L = cci->_layer;
        float relstate = 0.0f;
        cci->_value.x = relstate;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "Chan St") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        float chanst = 0.0f;
        cci->_value.x = chanst;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "Foot") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        float foot = 0.0f;
        cci->_value.x = foot;
        OrkAssert(false);
      };
      rval = CD;
    }
    else if (named == "SAMPLEPBRATE") {
      auto CD = std::make_shared<CustomControllerData>();
      CD->_name = named;
      CD->_oncompute = [](CustomControllerInst* cci) {
        float spbr = 1.0f; // ratio of sample playback rate to sample root ?
        cci->_value.x = spbr;
        OrkAssert(false);
      };
      rval = CD;
    }
    
    else{
      // is a float constant ?
      static std::regex float_regex("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?");
      std::smatch match;
      if (std::regex_search(named, match, float_regex)) {      //
        float fval = std::stof(match[0].str());
        auto CD = std::make_shared<ConstantControllerData>();
        CD->_name = named;
        CD->_constvalue = fval;
        rval = CD;
        //OrkAssert(false);
      }
      else{
        printf("controller not found named<%s>\n", named.c_str());
        //OrkAssert(false);
      }
    }
    _controllers_by_name[named] = rval;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void ControlBlockInst::keyOn(const KeyOnInfo& KOI, controlblockdata_constptr_t CBD) {
  assert(CBD);
  auto l = KOI._layer;

  for (int i = 0; i < kmaxctrlperblock; i++) {
    auto data = CBD->_controller_datas[i];
    if (data) {
      _cinst[i]                   = data->instantiate(l);
      _cinst[i]->_name            = data->_name;

      printf( "INSTANTIATE CONTROLLER<%s>\n", data->_name.c_str() );


      _cinst[i]->_controller_data = data;
      l->_controld2iMap[data]     = _cinst[i];
      l->_controlMap[data->_name] = _cinst[i];
      _cinst[i]->keyOn(KOI);
      if(l->_keymods){
        auto it = l->_keymods->_mods.find(data->_name);
        if(it!=l->_keymods->_mods.end()){
          auto kmdata = it->second;
          OrkAssert(kmdata!=nullptr);
          _cinst[i]->_keymoddata      = kmdata;

          auto as_cci = dynamic_cast<CustomControllerInst*>(_cinst[i]);
          if(as_cci){
            as_cci->_oncompute = [kmdata](CustomControllerInst* cci) {
              cci->_value = (cci->_value*0.99)+(kmdata->_currentValue*0.01);
              //printf("cci->_value.x<%g>\n", cci->_value.x);
            };
          }
        }
      }

    }
  }
}
void ControlBlockInst::keyOff() {
  for (int i = 0; i < kmaxctrlperblock; i++) {
    if (_cinst[i])
      _cinst[i]->keyOff();
  }
}
void ControlBlockInst::compute() {
  for (int i = 0; i < kmaxctrlperblock; i++) {
    if (_cinst[i])
      _cinst[i]->compute();
  }
}

///////////////////////////////////////////////////////////////////////////////

ControllerInst::ControllerInst(layer_ptr_t l)
    : _value(1,1,1,1)
    , _layer(l) {
}
float ControllerInst::getFloatValue() const{
  return _value.x;
}
fvec4 ControllerInst::getVec4Value() const{
  return _value;
}
void ControllerInst::setFloatValue(float v){
  _value.x = v;
}
void ControllerInst::setVec4Value(fvec4 v){
  _value = v;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void CustomControllerData::describeX(class_t* clazz) {
}

CustomControllerData::CustomControllerData() {
  _oncompute = [](CustomControllerInst* cci) {};
  _onkeyon   = [](CustomControllerInst* cci, const KeyOnInfo& KOI) { cci->_value.x = 0.0f; };
  _onkeyoff  = [](CustomControllerInst* cci) {};
}
///////////////////////////////////////////////////////////////////////////////
ControllerInst* CustomControllerData::instantiate(layer_ptr_t layer) const {
  //
  return new CustomControllerInst(this, layer);
}
///////////////////////////////////////////////////////////////////////////////
CustomControllerInst::CustomControllerInst(const CustomControllerData* data, layer_ptr_t layer)
    : ControllerInst(layer)
    , _data(data) {
    _oncompute = data->_oncompute;
    _onkeyon   = data->_onkeyon;
    _onkeyoff  = data->_onkeyoff;
}
///////////////////////////////////////////////////////////////////////////////
void CustomControllerInst::compute() {
  _oncompute(this);
}
///////////////////////////////////////////////////////////////////////////////
void CustomControllerInst::keyOn(const KeyOnInfo& KOI) {
  _onkeyon(this, KOI);
}
///////////////////////////////////////////////////////////////////////////////
void CustomControllerInst::keyOff() {
  _onkeyoff(this);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void ConstantControllerData::describeX(class_t* clazz) {
  clazz->directProperty("constvalue", &ConstantControllerData::_constvalue);
}

ControllerInst* ConstantControllerData::instantiate(layer_ptr_t layer) const {
  return new ConstantInst(this, layer);
}
ConstantInst::ConstantInst(const ConstantControllerData* data, layer_ptr_t layer)
    : ControllerInst(layer) {
  _value.x = data->_constvalue;
}
void ConstantInst::compute() {
  // printf("_value.x<%g>\n", _value.x);
}
void ConstantInst::keyOn(const KeyOnInfo& KOI) {
}
void ConstantInst::keyOff() {
}

} // namespace ork::audio::singularity
