////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;

namespace ork::lev2::particle {

struct TurbulenceModuleInst : public ParticleModuleInst {

  TurbulenceModuleInst(const TurbulenceModuleData* gmd)
      : ParticleModuleInst(gmd) {
  }

  ////////////////////////////////////////////////////

  void onLink(GraphInst* inst) final {

    _onLink(inst);
    /////////////////
    // inputs
    /////////////////

    _input_amount = typedInputNamed<Vec3XfPlugTraits>("Amount");

  }

  ////////////////////////////////////////////////////

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    fvec3 amt = _input_amount->value();
    float dt  = updata->_dt;

    for (int i = 0; i < _pool->GetNumAlive(); i++) {
      BasicParticle* particle = _pool->GetActiveParticle(i);
      float furx              = _randgen.ranged_rand(-.5,.5);
      float fury              = _randgen.ranged_rand(-.5,.5);
      float furz              = _randgen.ranged_rand(-.5,.5);
      /////////////////////////////////////////
      F32 randX = amt.x * furx;
      F32 randY = amt.y * fury;
      F32 randZ = amt.z * furz;
      ork::fvec4 accel(randX, randY, randZ);
      particle->mVelocity += accel * dt;
    }
  }

  ////////////////////////////////////////////////////

  fvec3xf_inp_pluginst_ptr_t _input_amount;

  RandGen _randgen;
};

//////////////////////////////////////////////////////////////////////////

void TurbulenceModuleData::describeX(class_t* clazz) {
}

//////////////////////////////////////////////////////////////////////////

TurbulenceModuleData::TurbulenceModuleData() {
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<TurbulenceModuleData> TurbulenceModuleData::createShared() {
  auto data = std::make_shared<TurbulenceModuleData>();

  _initShared(data);

  //RegisterFloatXfPlug(TurbulenceModule, AmountX, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  //RegisterFloatXfPlug(TurbulenceModule, AmountY, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  //RegisterFloatXfPlug(TurbulenceModule, AmountZ, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Amount");

  return data;
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t TurbulenceModuleData::createInstance() const {
  return std::make_shared<TurbulenceModuleInst>(this);
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::TurbulenceModuleData, "psys::TurbulenceModuleData");
