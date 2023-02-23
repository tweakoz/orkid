////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/reflect/enum_serializer.inl>
#include <ork/math/collision_test.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/lev2_asset.h>
#include <signal.h>

///////////////////////////////////////////////////////////////////////////////

namespace dflow = ::ork::dataflow;
namespace ork::lev2::particle {

///////////////////////////////////////////////////////////////////////////////

struct GlobalModuleInst : dflow::DgModuleInst {


  GlobalModuleInst(const GlobalModuleData* data)
    : dflow::DgModuleInst(data)
    , _gmd(data) {

    }

  void compute(dflow::GraphInst* inst) final {
    printf( "computing particle globals<%p>\n",this);
    /**(_outputTimeBase->_value) = *(_gmd->_timeBase);
    *(_outputNoiseRat->_value) = *(_gmd->_noiseRat);
    *(_outputNoisePrv->_value) = *(_gmd->_noisePrv);
    *(_outputNoiseNew->_value) = *(_gmd->_noiseNew);
    *(_outputNoiseBas->_value) = *(_gmd->_noiseBas);
    *(_outputNoiseTim->_value) = *(_gmd->_noiseTim);*/
  }

  void onLink(dflow::GraphInst* inst) final {
    /*_inputTimeBase = typedInputNamed<dflow::FloatPlugTraits>("TimeBase");
    _InputNoiseRat = typedInputNamed<dflow::FloatPlugTraits>("TimeScale");
    _outputNoisePrv = typedOutputNamed<dflow::FloatPlugTraits>("RelTime");
    _outputNoiseNew = typedOutputNamed<dflow::FloatPlugTraits>("RelTimeDiv10");
    _outputNoiseBas = typedOutputNamed<dflow::FloatPlugTraits>("RelTimeDiv100");*/

  }

  void onActivate(dflow::GraphInst* inst) final {
    _outtimer.Start();
  }

  const GlobalModuleData* _gmd;

  dflow::float_inp_pluginst_ptr_t _inputTimeBase;
  dflow::float_inp_pluginst_ptr_t _inputTimeScale;
  dflow::float_out_pluginst_ptr_t _outputRelTime;
  dflow::float_out_pluginst_ptr_t _outputRelTimeDiv10;
  dflow::float_out_pluginst_ptr_t _outputRelTimeDiv100;
  Timer _outtimer;

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

void GlobalModuleData::describeX(class_t* clazz) {

}

///////////////////////////////////////////////////////////////////////////////

GlobalModuleData::GlobalModuleData(){
  _timeBase = std::make_shared<float>(0.0f);


}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<GlobalModuleData> GlobalModuleData::createShared() {
    auto data = std::make_shared<GlobalModuleData>();
    //ParticlePoolData::sharedConstructor(gmd);
    createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "TimeBase");
    createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "TimeScale");
    createOutputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, data->_relTime, "RelTime");
    createOutputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, data->_relTimeD10, "RelTimeDiv10");
    createOutputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, data->_relTimeD100, "RelTimeDiv100");
    //DeclareFloatOutPlug(Random);
    //DeclareVect3OutPlug(RandomNormal);
    //DeclareFloatOutPlug(Noise);
    //DeclareFloatOutPlug(SlowNoise);
    //DeclareFloatOutPlug(FastNoise);
    return data;
}

///////////////////////////////////////////////////////////////////////////////

dflow::dgmoduleinst_ptr_t GlobalModuleData::createInstance() const {
  return std::make_shared<GlobalModuleInst>(this);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::GlobalModuleData, "psys::GlobalModuleData");
