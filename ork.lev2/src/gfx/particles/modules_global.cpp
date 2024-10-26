////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

///////////////////////////////////////////////////////////////////////////////

namespace dflow = ::ork::dataflow;
namespace ork::lev2::particle {

///////////////////////////////////////////////////////////////////////////////

struct GlobalModuleInst : dflow::DgModuleInst {

  GlobalModuleInst(const GlobalModuleData* data, dataflow::GraphInst* ginst)
      : dflow::DgModuleInst(data,ginst)
      , _gmd(data) {
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final {

    if (_first_compute) {
      _timebasebase  = updata->_abstime;
      _first_compute = false;
    }
    float time = updata->_abstime - _timebasebase;

    float reltime = time                           //
                        * _inputTimeScale->value() //
                    + _inputTimeBase->value();

    float reltimediv10  = reltime * 0.1f;
    float reltimediv100 = reltime * 0.01f;

    _outputRelTime->setValue(reltime);
    _outputRelTimeDiv10->setValue(reltimediv10);
    _outputRelTimeDiv100->setValue(reltimediv100);

   // printf("computing particle globals<%p> reltime<%g>\n", this, reltime);

    /**(_outputTimeBase->_value) = *(_gmd->_timeBase);
     *(_outputNoiseRat->_value) = *(_gmd->_noiseRat);
     *(_outputNoisePrv->_value) = *(_gmd->_noisePrv);
     *(_outputNoiseNew->_value) = *(_gmd->_noiseNew);
     *(_outputNoiseBas->_value) = *(_gmd->_noiseBas);
     *(_outputNoiseTim->_value) = *(_gmd->_noiseTim);*/
  }

  void onLink(dflow::GraphInst* inst) final {

    auto ptcl_context = inst->_impl.getShared<Context>();

    _inputTimeBase       = typedInputNamed<dflow::FloatPlugTraits>("TimeBase");
    _inputTimeScale      = typedInputNamed<dflow::FloatPlugTraits>("TimeScale");
    _outputRelTime       = typedOutputNamed<dflow::FloatPlugTraits>("RelTime");
    _outputRelTimeDiv10  = typedOutputNamed<dflow::FloatPlugTraits>("RelTimeDiv10");
    _outputRelTimeDiv100 = typedOutputNamed<dflow::FloatPlugTraits>("RelTimeDiv100");

    _inputTimeBase->_value  = _gmd->typedInputNamed<dflow::FloatPlugTraits>("TimeBase")->_value;
    _inputTimeScale->_value = _gmd->typedInputNamed<dflow::FloatPlugTraits>("TimeScale")->_value;
  }

  void onActivate(dflow::GraphInst* inst) final {
  }

  const GlobalModuleData* _gmd;

  bool _first_compute = true;
  float _timebase     = 0.0f;
  float _timebasebase = 0.0f;

  dflow::float_inp_pluginst_ptr_t _inputTimeBase;
  dflow::float_inp_pluginst_ptr_t _inputTimeScale;
  dflow::float_out_pluginst_ptr_t _outputRelTime;
  dflow::float_out_pluginst_ptr_t _outputRelTimeDiv10;
  dflow::float_out_pluginst_ptr_t _outputRelTimeDiv100;

  float _noiseRat = 0.0f;
  float _noisePrv = 0.0f;
  float _noiseNew = 0.0f;
  float _noiseBas = 0.0f;
  float _noiseTim = 0.0f;

  float _slowNoiseRat = 0.0f;
  float _slowNoisePrv = 0.0f;
  float _slowNoiseBas = 0.0f;
  float _slowNoiseTim = 0.0f;

  float _fastNoiseRat = 0.0f;
  float _fastNoisePrv = 0.0f;
  float _fastNoiseBas = 0.0f;
  float _fastNoiseTim = 0.0f;
};

using globalmoduleinst_ptr_t = std::shared_ptr<GlobalModuleInst>;

///////////////////////////////////////////////////////////////////////////////

GlobalModuleData::GlobalModuleData() {
}

///////////////////////////////////////////////////////////////////////////////

static void _reshapeGlobalIOs( dataflow::moduledata_ptr_t data ){
  auto typed = std::dynamic_pointer_cast<GlobalModuleData>(data);
  auto timebase      = ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "TimeBase");
  auto timescale     = ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "TimeScale");
  auto reltime       = ModuleData::createOutputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "RelTime");
  auto reltimediv10  = ModuleData::createOutputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "RelTimeDiv10");
  auto reltimediv100 = ModuleData::createOutputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "RelTimeDiv100");

  timebase->setValue(0.0f);
  timescale->setValue(1.0f);
}
///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<GlobalModuleData> GlobalModuleData::createShared() {
  auto data = std::make_shared<GlobalModuleData>();
  _initPoolIOs(data);
  _reshapeGlobalIOs(data);
  // DeclareFloatOutPlug(Random);
  // DeclareVect3OutPlug(RandomNormal);
  // DeclareFloatOutPlug(Noise);
  // DeclareFloatOutPlug(SlowNoise);
  // DeclareFloatOutPlug(FastNoise);
  return data;
}

///////////////////////////////////////////////////////////////////////////////

dflow::dgmoduleinst_ptr_t GlobalModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<GlobalModuleInst>(this,ginst);
}

void GlobalModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory( [] -> rtti::castable_ptr_t {
    return GlobalModuleData::createShared();
  });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",[](dataflow::moduledata_ptr_t mdata){
    _reshapeGlobalIOs(mdata);
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::particle
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::GlobalModuleData, "psys::GlobalModuleData");
