////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;

namespace ork::lev2::particle {

struct TurbulenceModuleInst : public DgModuleInst {

  TurbulenceModuleInst(const TurbulenceModuleData* gmd)
      : DgModuleInst(gmd) {
  }

  ////////////////////////////////////////////////////

  void onLink(GraphInst* inst) final {

    /////////////////
    // inputs
    /////////////////

    _input_buffer = typedInputNamed<ParticleBufferPlugTraits>("pool");
    _input_amount = typedInputNamed<Vec3XfPlugTraits>("Amount");

    /////////////////
    // outputs
    /////////////////

    _output_buffer = typedOutputNamed<ParticleBufferPlugTraits>("pool");

    if (_input_buffer->_connectedOutput) {
      _pool                              = _input_buffer->value()._pool;
      _output_buffer->value_ptr()->_pool = _pool;
    } else {
      OrkAssert(false);
    }
  }

  ////////////////////////////////////////////////////

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    fvec3 amt = _input_amount->value();
    float dt  = updata->_dt;

    for (int i = 0; i < _pool->GetNumAlive(); i++) {
      BasicParticle* particle = _pool->GetActiveParticle(i);
      float furx              = ((std::rand() % 256) / 256.0f) - 0.5f;
      float fury              = ((std::rand() % 256) / 256.0f) - 0.5f;
      float furz              = ((std::rand() % 256) / 256.0f) - 0.5f;
      /////////////////////////////////////////
      F32 randX = amt.x * furx;
      F32 randY = amt.y * fury;
      F32 randZ = amt.z * furz;
      ork::fvec4 accel(randX, randY, randZ);
      particle->mVelocity += accel * dt;
    }
  }

  ////////////////////////////////////////////////////

  particlebuf_inpluginst_ptr_t _input_buffer;
  particlebuf_outpluginst_ptr_t _output_buffer;

  fvec3xf_inp_pluginst_ptr_t _input_amount;

  pool_ptr_t _pool;
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

  createInputPlug<ParticleBufferPlugTraits>(data, EPR_UNIFORM, "pool");
  createOutputPlug<ParticleBufferPlugTraits>(data, EPR_UNIFORM, "pool");

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
