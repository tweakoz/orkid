////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// DirectionalForceModule — constant per-particle acceleration in a
// uniform direction (Δv = Direction.normalized() * Magnitude * dt).
// Sibling of Gravity but position-independent; useful for wind / thrust /
// constant pull where the existing point-attractor Gravity isn't a fit.

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;

namespace ork::lev2::particle {

struct DirectionalForceModuleInst : public ParticleModuleInst {

  DirectionalForceModuleInst(const DirectionalForceModuleData* d, dataflow::GraphInst* g)
      : ParticleModuleInst(d, g) {
  }

  void onLink(GraphInst* inst) final {
    _onLink(inst);
    _input_direction = typedInputNamed<Vec3XfPlugTraits>("Direction");
    _input_magnitude = typedInputNamed<FloatXfPlugTraits>("Magnitude");
  }

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    fvec3 dir = _input_direction->value();
    // Normalize defensively — authors may bind a non-unit Direction
    // (e.g. Expr.entity(x).transformDir(vec3(0,-1,0)) under non-uniform
    // scale stretches the vector). Zero-length input → no-op, not NaN.
    float mag2 = dir.magnitudeSquared();
    if (mag2 < 1e-8f) return;
    dir = dir * (1.0f / std::sqrt(mag2));

    fvec3 accel = dir * _input_magnitude->value();
    float dt    = updata->_dt;
    fvec3 dv    = accel * dt;

    const int n = _pool->GetNumAlive();
    for (int i = 0; i < n; i++) {
      BasicParticle* p = _pool->GetActiveParticle(i);
      p->mVelocity += dv;
    }
  }

  floatxf_inp_pluginst_ptr_t _input_magnitude;
  fvec3xf_inp_pluginst_ptr_t _input_direction;
};

//////////////////////////////////////////////////////////////////////////

DirectionalForceModuleData::DirectionalForceModuleData() {}

//////////////////////////////////////////////////////////////////////////

static void _reshapeDirectionalForceIOs(dataflow::moduledata_ptr_t data) {
  // Direction default (0,-1,0) makes the no-input case behave like
  // canonical down-gravity; authors override as needed.
  auto dir = ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Direction");
  dir->_range = {-1.0f, 1.0f};
  dir->setValue(fvec3(0, -1, 0));
  auto mag = ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Magnitude");
  mag->_range = {-1000.0f, 1000.0f};
  mag->setValue(9.81f);
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DirectionalForceModuleData> DirectionalForceModuleData::createShared() {
  auto data = std::make_shared<DirectionalForceModuleData>();
  _initPoolIOs(data);
  _reshapeDirectionalForceIOs(data);
  return data;
}

rtti::castable_ptr_t DirectionalForceModuleData::sharedFactory() {
  return DirectionalForceModuleData::createShared();
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t DirectionalForceModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<DirectionalForceModuleInst>(this, ginst);
}

//////////////////////////////////////////////////////////////////////////

void DirectionalForceModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t {
    return DirectionalForceModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs", [](dataflow::moduledata_ptr_t mdata) {
    _reshapeDirectionalForceIOs(mdata);
  });
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::DirectionalForceModuleData, "psys::DirectionalForceModuleData");
