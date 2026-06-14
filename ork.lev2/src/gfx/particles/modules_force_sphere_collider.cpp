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
#include <ork/lev2/gfx/particle/collider_common.inl>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;

namespace ork::lev2::particle {

///////////////////////////////////////////////////////////////////////////////

struct SphereColliderModuleInst : public ParticleModuleInst {

  SphereColliderModuleInst(const SphereColliderModuleData* d, dataflow::GraphInst* ginst)
      : ParticleModuleInst(d, ginst) {
  }

  void onLink(GraphInst* inst) final {
    _onLink(inst);
    _input_center      = typedInputNamed<Vec3XfPlugTraits>("Center");
    _input_radius      = typedInputNamed<FloatXfPlugTraits>("Radius");
    _input_restitution = typedInputNamed<FloatXfPlugTraits>("Restitution");
    _input_friction    = typedInputNamed<FloatXfPlugTraits>("Friction");
  }

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    fvec3 center      = _input_center->value();
    float radius      = _input_radius->value();
    float restitution = _input_restitution->value();
    float friction    = _input_friction->value();

    if (radius <= 0.0f) return;
    float radius2 = radius * radius;

    const int n = _pool->GetNumAlive();
    for (int i = 0; i < n; i++) {
      BasicParticle* ptc = _pool->GetActiveParticle(i);
      fvec3 delta = ptc->mPosition - center;
      float dist2 = delta.dotWith(delta);
      if (dist2 < radius2) {
        // Guard the degenerate case: particle exactly at sphere center
        // → no defined outward direction. Pick world-up so resolution
        // still happens; this is rare unless an emitter spawns inside.
        float dist = std::sqrt(dist2);
        fvec3 normal = (dist > 1e-6f)
                         ? (delta * (1.0f / dist))
                         : fvec3(0, 1, 0);
        float penetration = radius - dist;
        resolve_collision(*ptc, normal, penetration, restitution, friction,
                          COLLIDER_BIT_SPHERE);
      }
    }
  }

  fvec3xf_inp_pluginst_ptr_t _input_center;
  floatxf_inp_pluginst_ptr_t _input_radius;
  floatxf_inp_pluginst_ptr_t _input_restitution;
  floatxf_inp_pluginst_ptr_t _input_friction;
};

///////////////////////////////////////////////////////////////////////////////

SphereColliderModuleData::SphereColliderModuleData() {
}

static void _reshapeSphereColliderIOs(dataflow::moduledata_ptr_t mdata) {
  auto typed = std::dynamic_pointer_cast<SphereColliderModuleData>(mdata);
  ModuleData::createInputPlug<Vec3XfPlugTraits>(mdata, EPR_UNIFORM, "Center")->_range      = {-1000.0f, 1000.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "Radius")->_range     = {0.0f, 100.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "Restitution")->_range = {0.0f, 2.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "Friction")->_range    = {0.0f, 1.0f};
}

std::shared_ptr<SphereColliderModuleData> SphereColliderModuleData::createShared() {
  auto data = std::make_shared<SphereColliderModuleData>();
  _initPoolIOs(data);
  _reshapeSphereColliderIOs(data);
  // Defaults: 1m sphere at origin, half bounce, no friction.
  auto in_center = std::dynamic_pointer_cast<inplugdata<Vec3XfPlugTraits>>(data->inputNamed("Center"));
  auto in_radius = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Radius"));
  auto in_rest   = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Restitution"));
  auto in_fric   = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Friction"));
  if (in_center) in_center->setValue(fvec3(0, 0, 0));
  if (in_radius) in_radius->setValue(1.0f);
  if (in_rest)   in_rest->setValue(0.5f);
  if (in_fric)   in_fric->setValue(0.0f);
  return data;
}

void SphereColliderModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t {
    return SphereColliderModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs", [](dataflow::moduledata_ptr_t mdata) {
    _reshapeSphereColliderIOs(mdata);
  });
}

dgmoduleinst_ptr_t SphereColliderModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<SphereColliderModuleInst>(this, ginst);
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::SphereColliderModuleData, "psys::SphereColliderModuleData");
