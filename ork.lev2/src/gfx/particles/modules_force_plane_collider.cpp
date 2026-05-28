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
#include <ork/lev2/gfx/particle/collider_common.inl>

using namespace ork::dataflow;

namespace ork::lev2::particle {

///////////////////////////////////////////////////////////////////////////////

struct PlaneColliderModuleInst : public ParticleModuleInst {

  PlaneColliderModuleInst(const PlaneColliderModuleData* d, dataflow::GraphInst* ginst)
      : ParticleModuleInst(d, ginst) {
  }

  void onLink(GraphInst* inst) final {
    _onLink(inst);
    _input_center      = typedInputNamed<Vec3XfPlugTraits>("Center");
    _input_normal      = typedInputNamed<Vec3XfPlugTraits>("Normal");
    _input_restitution = typedInputNamed<FloatXfPlugTraits>("Restitution");
    _input_friction    = typedInputNamed<FloatXfPlugTraits>("Friction");
  }

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    fvec3 center      = _input_center->value();
    fvec3 normal_raw  = _input_normal->value();
    float restitution = _input_restitution->value();
    float friction    = _input_friction->value();

    // Renormalize so authors can write unnormalized normals like vec3(0,1,0)
    // without surprises. Guard the zero-vector case (skip frame).
    float n_len2 = normal_raw.dotWith(normal_raw);
    if (n_len2 < 1e-8f) return;
    fvec3 normal = normal_raw * (1.0f / std::sqrt(n_len2));

    const int n = _pool->GetNumAlive();
    for (int i = 0; i < n; i++) {
      BasicParticle* ptc = _pool->GetActiveParticle(i);
      float d = (ptc->mPosition - center).dotWith(normal);
      if (d < 0.0f) {
        resolve_collision(*ptc, normal, -d, restitution, friction, COLLIDER_BIT_PLANE);
      }
    }
  }

  fvec3xf_inp_pluginst_ptr_t _input_center;
  fvec3xf_inp_pluginst_ptr_t _input_normal;
  floatxf_inp_pluginst_ptr_t _input_restitution;
  floatxf_inp_pluginst_ptr_t _input_friction;
};

///////////////////////////////////////////////////////////////////////////////

PlaneColliderModuleData::PlaneColliderModuleData() {
}

static void _reshapePlaneColliderIOs(dataflow::moduledata_ptr_t mdata) {
  auto typed = std::dynamic_pointer_cast<PlaneColliderModuleData>(mdata);
  ModuleData::createInputPlug<Vec3XfPlugTraits>(mdata, EPR_UNIFORM, "Center")->_range      = {-1000.0f, 1000.0f};
  ModuleData::createInputPlug<Vec3XfPlugTraits>(mdata, EPR_UNIFORM, "Normal")->_range      = {-1.0f, 1.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "Restitution")->_range = {0.0f, 2.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "Friction")->_range    = {0.0f, 1.0f};
}

std::shared_ptr<PlaneColliderModuleData> PlaneColliderModuleData::createShared() {
  auto data = std::make_shared<PlaneColliderModuleData>();
  _initPoolIOs(data);
  _reshapePlaneColliderIOs(data);
  // Defaults: floor at y=0, +Y normal, half bounce, no friction.
  auto in_center = std::dynamic_pointer_cast<inplugdata<Vec3XfPlugTraits>>(data->inputNamed("Center"));
  auto in_normal = std::dynamic_pointer_cast<inplugdata<Vec3XfPlugTraits>>(data->inputNamed("Normal"));
  auto in_rest   = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Restitution"));
  auto in_fric   = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Friction"));
  if (in_center) in_center->setValue(fvec3(0, 0, 0));
  if (in_normal) in_normal->setValue(fvec3(0, 1, 0));
  if (in_rest)   in_rest->setValue(0.5f);
  if (in_fric)   in_fric->setValue(0.0f);
  return data;
}

void PlaneColliderModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t {
    return PlaneColliderModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs", [](dataflow::moduledata_ptr_t mdata) {
    _reshapePlaneColliderIOs(mdata);
  });
}

dgmoduleinst_ptr_t PlaneColliderModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<PlaneColliderModuleInst>(this, ginst);
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::PlaneColliderModuleData, "psys::PlaneColliderModuleData");
