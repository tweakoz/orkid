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

struct SphAttractorModuleInst : public ParticleModuleInst {

  SphAttractorModuleInst(const SphAttractorModuleData* gmd, dataflow::GraphInst* ginst)
      : ParticleModuleInst(gmd, ginst) {
  }

  ////////////////////////////////////////////////////

  void onLink(GraphInst* inst) final {

  _onLink(inst);

    /////////////////
    // inputs
    /////////////////

    _input_radius           = typedInputNamed<FloatXfPlugTraits>("Radius");
    _input_inertia        = typedInputNamed<FloatXfPlugTraits>("Inertia");
    _input_dampening = typedInputNamed<FloatXfPlugTraits>("Dampening");
    _input_center = typedInputNamed<Vec3XfPlugTraits>("Center");

  }

  ////////////////////////////////////////////////////

void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
  float fradius = _input_radius->value();
  float finertia = _input_inertia->value();
  fvec3 center = _input_center->value();
  float dampenFactor = _input_dampening->value(); 
  float dt = updata->_dt;

  for (int i = 0; i < _pool->GetNumAlive(); i++) {
    BasicParticle* particle = _pool->GetActiveParticle(i);
    fvec3 position = particle->mPosition;
    fvec3 directionToCenter = center - position;
    float distanceToCenter = directionToCenter.magnitude();

    // Calculate the direction from the particle to the closest point on the sphere's surface
    fvec3 directionToSurface;
    if (distanceToCenter > fradius) {
      // For particles outside the sphere, the closest surface point is along the direction to the center
      directionToSurface = directionToCenter.normalized() * (distanceToCenter - fradius);
    } else {
      // For particles inside the sphere, direct them towards the nearest surface point in the opposite direction
      directionToSurface = -directionToCenter.normalized() * (fradius - distanceToCenter);
    }

    // Apply force magnitude inversely proportional to the distance; closer = stronger, to simulate a gravitational pull
    float forceMagnitude = 1.0 / (1.0 + directionToSurface.magnitude());

    // Calculate the acceleration vector
    fvec3 accel = directionToSurface.normalized() * forceMagnitude / finertia;

    // Update the velocity
    particle->mVelocity += accel * dt;

    // Apply dampening to reduce velocity as particles get close to the surface to simulate a gentle landing
    particle->mVelocity *= dampenFactor;
  }
}



  ////////////////////////////////////////////////////

  floatxf_inp_pluginst_ptr_t _input_dampening;
  floatxf_inp_pluginst_ptr_t _input_radius;
  floatxf_inp_pluginst_ptr_t _input_inertia;
  fvec3xf_inp_pluginst_ptr_t _input_center;

};

//////////////////////////////////////////////////////////////////////////

SphAttractorModuleData::SphAttractorModuleData() {
}

//////////////////////////////////////////////////////////////////////////

static void _reshapeSphAttractorIOs( dataflow::moduledata_ptr_t data ){
  auto typed = std::dynamic_pointer_cast<SphAttractorModuleData>(data);
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Radius")->_range = {-10.0f, 10.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Inertia")->_range = {-10.0f, 10.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Dampening")->_range = {0,1};
  ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Center")->_range = {-1000.0f, 1000.0f};
}
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<SphAttractorModuleData> SphAttractorModuleData::createShared() {
  auto data = std::make_shared<SphAttractorModuleData>();
  _initPoolIOs(data);
  _reshapeSphAttractorIOs(data);
  return data;
}

rtti::castable_ptr_t SphAttractorModuleData::sharedFactory(){
  return createShared();
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t SphAttractorModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<SphAttractorModuleInst>(this, ginst);
}

//////////////////////////////////////////////////////////////////////////

void SphAttractorModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory( [] -> rtti::castable_ptr_t {
    return SphAttractorModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs",[](dataflow::moduledata_ptr_t mdata){
    _reshapeSphAttractorIOs(mdata);
  });
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::SphAttractorModuleData, "psys::SphAttractorModuleData");
