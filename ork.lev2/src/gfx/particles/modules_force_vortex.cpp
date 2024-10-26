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

struct VortexModuleInst : public ParticleModuleInst {

  VortexModuleInst(const VortexModuleData* gmd, dataflow::GraphInst* ginst)
      : ParticleModuleInst(gmd, ginst) {
  }

  ////////////////////////////////////////////////////

  void onLink(GraphInst* inst) final {

    _onLink(inst);

    /////////////////
    // inputs
    /////////////////

    _input_vortex_strength  = typedInputNamed<FloatXfPlugTraits>("VortexStrength");
    _input_outward_strength = typedInputNamed<FloatXfPlugTraits>("OutwardStrength");
    _input_falloff          = typedInputNamed<FloatXfPlugTraits>("Falloff");

  }

  ////////////////////////////////////////////////////

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    float vortexstrength  = _input_vortex_strength->value();
    float outwardstrength = _input_outward_strength->value();
    float falloff         = _input_falloff->value();
    float dt              = updata->_dt;

    //printf( "vortexstrength<%g>\n", vortexstrength );
    for (int i = 0; i < _pool->GetNumAlive(); i++) {
      BasicParticle* particle = _pool->GetActiveParticle(i);
      fvec3 Pos2D             = particle->mPosition;
      Pos2D.y                 = (0.0f);
      fvec3 N                 = particle->mPosition;
      N.y = 0.0f;
      N = N.normalized();
      fvec3 Dir               = N.crossWith(fvec3::unitY());
      float fstr              = 1.0f / (1.0f + falloff / Pos2D.magnitude());
      fvec3 Force             = Dir * (vortexstrength * fstr);
      Force += N * (outwardstrength * fstr);

      particle->mVelocity += Force * dt;
    }
  }

  ////////////////////////////////////////////////////

  floatxf_inp_pluginst_ptr_t _input_vortex_strength;
  floatxf_inp_pluginst_ptr_t _input_outward_strength;
  floatxf_inp_pluginst_ptr_t _input_falloff;

};

//////////////////////////////////////////////////////////////////////////

VortexModuleData::VortexModuleData() {
}

///////////////////////////////////////////////////////////////////////////////

static void _reshapeVortexIOs( dataflow::moduledata_ptr_t mdata ){
  auto typed = std::dynamic_pointer_cast<VortexModuleData>(mdata);
  ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "VortexStrength")->_range = {-100,100};
  ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "OutwardStrength")->_range = {-100,100};
  ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "Falloff")->_range = {0,10};
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<VortexModuleData> VortexModuleData::createShared() {
  auto data = std::make_shared<VortexModuleData>();
  _initPoolIOs(data);
  _reshapeVortexIOs(data);
  return data;
}

//////////////////////////////////////////////////////////////////////////

void VortexModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory( [] -> rtti::castable_ptr_t {
    return VortexModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs",[](dataflow::moduledata_ptr_t mdata){
    _reshapeVortexIOs(mdata);
  });
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t VortexModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<VortexModuleInst>(this,ginst);
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::VortexModuleData, "psys::VortexModuleData");
