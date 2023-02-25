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

struct VortexModuleInst : public DgModuleInst {

  VortexModuleInst(const VortexModuleData* gmd)
      : DgModuleInst(gmd) {
  }

  ////////////////////////////////////////////////////

  void onLink(GraphInst* inst) final {

    /////////////////
    // inputs
    /////////////////

    _input_buffer           = typedInputNamed<ParticleBufferPlugTraits>("ParticleBuffer");
    _input_vortex_strength  = typedInputNamed<FloatXfPlugTraits>("VortexStrength");
    _input_outward_strength = typedInputNamed<FloatXfPlugTraits>("OutwardStrength");
    _input_falloff          = typedInputNamed<FloatXfPlugTraits>("Falloff");

    /////////////////
    // outputs
    /////////////////

    _output_buffer = typedOutputNamed<ParticleBufferPlugTraits>("ParticleBuffer");

    if (_input_buffer->_connectedOutput) {
      _pool                              = _input_buffer->value()._pool;
      _output_buffer->value_ptr()->_pool = _pool;
    } else {
      OrkAssert(false);
    }
  }

  ////////////////////////////////////////////////////

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    float vortexstrength  = _input_vortex_strength->value();
    float outwardstrength = _input_outward_strength->value();
    float falloff         = _input_falloff->value();
    float dt              = updata->_dt;

    for (int i = 0; i < _pool->GetNumAlive(); i++) {
      BasicParticle* particle = _pool->GetActiveParticle(i);
      fvec3 Pos2D             = particle->mPosition;
      Pos2D.y                 = (0.0f);
      fvec3 N                 = particle->mPosition.normalized();
      fvec3 Dir               = N.crossWith(fvec3::unitY());
      float fstr              = 1.0f / (1.0f + falloff / Pos2D.magnitude());
      fvec3 Force             = Dir * (vortexstrength * fstr);
      Force += N * (outwardstrength * fstr);

      particle->mVelocity += Force * dt;
    }
  }

  ////////////////////////////////////////////////////

  particlebuf_inpluginst_ptr_t _input_buffer;
  particlebuf_outpluginst_ptr_t _output_buffer;

  floatxf_inp_pluginst_ptr_t _input_vortex_strength;
  floatxf_inp_pluginst_ptr_t _input_outward_strength;
  floatxf_inp_pluginst_ptr_t _input_falloff;

  pool_ptr_t _pool;
};

//////////////////////////////////////////////////////////////////////////

void VortexModuleData::describeX(class_t* clazz) {
}

//////////////////////////////////////////////////////////////////////////

VortexModuleData::VortexModuleData() {
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<VortexModuleData> VortexModuleData::createShared() {
  auto data = std::make_shared<VortexModuleData>();

  createInputPlug<ParticleBufferPlugTraits>(data, EPR_UNIFORM, "ParticleBuffer");
  createOutputPlug<ParticleBufferPlugTraits>(data, EPR_UNIFORM, "ParticleBuffer");

  //RegisterFloatXfPlug(VortexModule, Falloff, 0.0f, 10.0f, ged::OutPlugChoiceDelegate);
  //RegisterFloatXfPlug(VortexModule, VortexStrength, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  //RegisterFloatXfPlug(VortexModule, OutwardStrength, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);

  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "VortexStrength");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "OutwardStrength");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Falloff");

  return data;
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t VortexModuleData::createInstance() const {
  return std::make_shared<VortexModuleInst>(this);
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::VortexModuleData, "psys::VortexModuleData");
