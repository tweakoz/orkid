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

namespace dflow = ::ork::dataflow;

namespace ork::lev2::particle {

struct GlobalModuleInst : dflow::DgModuleInst {
  //DeclareFloatXfPlug(TimeScale);
  //DeclareFloatOutPlug(RelTime);
  //DeclareFloatOutPlug(Time);
  //DeclareFloatOutPlug(TimeDiv10);
  //DeclareFloatOutPlug(TimeDiv100);

  //DeclareFloatOutPlug(Random);
  //DeclareVect3OutPlug(RandomNormal);
  //DeclareFloatOutPlug(Noise);
  //DeclareFloatOutPlug(SlowNoise);
  //DeclareFloatOutPlug(FastNoise);

  //dataflow::inplugbase* GetInput(int idx) const final {
    //return &mPlugInpTimeScale;
  //}
  //dataflow::outplugbase* GetOutput(int idx) const final;
  //void Compute(float dt) final;
  //void OnStart() final;

  GlobalModuleInst(const GlobalModuleData* data)
    : dflow::DgModuleInst(data){

    }
   
};

using globalmoduleinst_ptr_t = std::shared_ptr<GlobalModuleInst>;

void GlobalModuleData::describeX(class_t* clazz) {

}

GlobalModuleData::GlobalModuleData(){
  _timeBase = std::make_shared<float>(0.0f);

  _noiseRat = std::make_shared<float>(0.0f);
  _noisePrv = std::make_shared<float>(0.0f);
  _noiseNew = std::make_shared<float>(0.0f);
  _noiseBas = std::make_shared<float>(0.0f);
  _noiseTim = std::make_shared<float>(0.0f);

  _slowNoiseRat = std::make_shared<float>(0.0f);
  _slowNoisePrv = std::make_shared<float>(0.0f);
  _slowNoiseBas = std::make_shared<float>(0.0f);
  _slowNoiseTim = std::make_shared<float>(0.0f);

  _fastNoiseRat = std::make_shared<float>(0.0f);
  _fastNoisePrv = std::make_shared<float>(0.0f);
  _fastNoiseBas = std::make_shared<float>(0.0f);
  _fastNoiseTim = std::make_shared<float>(0.0f);

}

dflow::dgmoduleinst_ptr_t GlobalModuleData::createInstance() const {
  return std::make_shared<GlobalModuleInst>(this);
}

}

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::GlobalModuleData, "psys::GlobalModuleData");
