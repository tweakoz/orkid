////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/rtti/RTTI.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace ork::dataflow;

namespace ork::lev2::particle {


struct PointAttractorModuleInst : public ParticleModuleInst {

  PointAttractorModuleInst(const PointAttractorModuleData* gmd, dataflow::GraphInst* ginst)
      : ParticleModuleInst(gmd, ginst) {
  }

  ////////////////////////////////////////////////////

  void onLink(GraphInst* inst) final {

    _onLink(inst);
    _input_pos  = typedInputNamed<Vec3XfPlugTraits>("position");

  }

  ////////////////////////////////////////////////////

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    auto wdragvec  = _input_pos->value();
    float dt              = updata->_dt;
    //printf( "vortexstrength<%g>\n", vortexstrength );
    for (int i = 0; i < _pool->GetNumAlive(); i++) {
      BasicParticle* particle = _pool->GetActiveParticle(i);
      fvec3 pos = particle->mPosition;
      fvec3 delta = (wdragvec - pos);
      particle->mVelocity += delta * dt;
    }
  }

  ////////////////////////////////////////////////////
  fvec3xf_inp_pluginst_ptr_t _input_pos;

};

//////////////////////////////////////////////////////////////////////////

PointAttractorModuleData::PointAttractorModuleData() {
}

///////////////////////////////////////////////////////////////////////////////

static void _reshapePointAttractorIOs( dataflow::moduledata_ptr_t mdata ){
  auto typed = std::dynamic_pointer_cast<PointAttractorModuleData>(mdata);
  ModuleData::createInputPlug<Vec3XfPlugTraits>(mdata, EPR_UNIFORM, "position");
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<PointAttractorModuleData> PointAttractorModuleData::createShared() {
  auto data = std::make_shared<PointAttractorModuleData>();
  _initPoolIOs(data);
  _reshapePointAttractorIOs(data);
  return data;
}

//////////////////////////////////////////////////////////////////////////

void PointAttractorModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory( [] -> rtti::castable_ptr_t {
    return PointAttractorModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs",[](dataflow::moduledata_ptr_t mdata){
    _reshapePointAttractorIOs(mdata);
  });
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t PointAttractorModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<PointAttractorModuleInst>(this,ginst);
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::PointAttractorModuleData, "psys::PointAttractorModuleData");
