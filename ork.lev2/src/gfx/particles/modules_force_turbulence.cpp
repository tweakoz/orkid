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

  TurbulenceModuleInst(const TurbulenceModuleData* gmd, dataflow::GraphInst* ginst)
      : ParticleModuleInst(gmd, ginst) {
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
      //printf( "accel<%g %g %g>\n", accel.x, accel.y, accel.z);
      particle->mVelocity += accel * dt;
    }
  }

  ////////////////////////////////////////////////////

  fvec3xf_inp_pluginst_ptr_t _input_amount;

  RandGen _randgen;
};

//////////////////////////////////////////////////////////////////////////

TurbulenceModuleData::TurbulenceModuleData() {
}

///////////////////////////////////////////////////////////////////////////////

static void _reshapeTurbulenceIOs( dataflow::moduledata_ptr_t data ){
  auto typed = std::dynamic_pointer_cast<TurbulenceModuleData>(data);
  ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Amount")->_range = {-1000.0f,1000.0f};
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<TurbulenceModuleData> TurbulenceModuleData::createShared() {
  auto data = std::make_shared<TurbulenceModuleData>();

  _initPoolIOs(data);
  _reshapeTurbulenceIOs(data);

  return data;
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t TurbulenceModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<TurbulenceModuleInst>(this,ginst);
}

//////////////////////////////////////////////////////////////////////////

void TurbulenceModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory( [] -> rtti::castable_ptr_t {
    return TurbulenceModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs",[](dataflow::moduledata_ptr_t mdata){
    _reshapeTurbulenceIOs(mdata);
  });
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::TurbulenceModuleData, "psys::TurbulenceModuleData");
