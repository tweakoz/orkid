////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// PolyDrag — polynomial-of-speed drag force. See modular_forces.h header
// comment for the model. Per particle: compute decel = c0 + c1*s + c2*s² +
// c3*s³ where s = |v|, then reduce |v| by decel*dt antiparallel to v.
// Clamps to v=0 instead of overshooting (so a stationary particle can't
// be flipped by an over-eager c0 with a long dt).

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;

namespace ork::lev2::particle {

struct PolyDragModuleInst : public ParticleModuleInst {

  PolyDragModuleInst(const PolyDragModuleData* d, dataflow::GraphInst* ginst)
      : ParticleModuleInst(d, ginst) {}

  void onLink(GraphInst* inst) final {
    _onLink(inst);
    _input_c0 = typedInputNamed<FloatXfPlugTraits>("Constant");
    _input_c1 = typedInputNamed<FloatXfPlugTraits>("Linear");
    _input_c2 = typedInputNamed<FloatXfPlugTraits>("Quadratic");
    _input_c3 = typedInputNamed<FloatXfPlugTraits>("Cubic");
  }

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    float c0 = _input_c0->value();
    float c1 = _input_c1->value();
    float c2 = _input_c2->value();
    float c3 = _input_c3->value();
    float dt = updata->_dt;

    // Fast-out: all coefficients zero (default state) → no work to do.
    if (c0 == 0.0f and c1 == 0.0f and c2 == 0.0f and c3 == 0.0f) return;

    const int n = _pool->GetNumAlive();
    for (int i = 0; i < n; i++) {
      BasicParticle* ptc = _pool->GetActiveParticle(i);
      fvec3 v   = ptc->mVelocity;
      float s   = v.magnitude();
      if (s < 1.0e-6f) {
        // Stationary particle — even Coulomb friction can't kick it from
        // rest. Skip to avoid divide-by-zero and to match physical reality.
        continue;
      }
      // Horner's form for the polynomial: c0 + s*(c1 + s*(c2 + s*c3))
      float decel = c0 + s * (c1 + s * (c2 + s * c3));
      float delta = decel * dt;
      if (delta >= s) {
        // Drag would reverse the particle this frame — clamp to rest
        // instead. Without this, large coefficients + long dt produce
        // jitter (particle oscillates around zero).
        ptc->mVelocity = fvec3(0, 0, 0);
      } else {
        ptc->mVelocity = v * ((s - delta) / s);
      }
    }
  }

  floatxf_inp_pluginst_ptr_t _input_c0;
  floatxf_inp_pluginst_ptr_t _input_c1;
  floatxf_inp_pluginst_ptr_t _input_c2;
  floatxf_inp_pluginst_ptr_t _input_c3;
};

//////////////////////////////////////////////////////////////////////////

PolyDragModuleData::PolyDragModuleData() {}

static void _reshapePolyDragIOs(dataflow::moduledata_ptr_t data) {
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Constant")->_range  = {0.0f, 1000.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Linear")->_range    = {0.0f, 100.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Quadratic")->_range = {0.0f, 100.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Cubic")->_range     = {0.0f, 100.0f};
}

std::shared_ptr<PolyDragModuleData> PolyDragModuleData::createShared() {
  auto data = std::make_shared<PolyDragModuleData>();
  _initPoolIOs(data);
  _reshapePolyDragIOs(data);
  // All terms default to 0 — a default PolyDrag has no effect until the
  // author wires at least one coefficient.
  return data;
}

dgmoduleinst_ptr_t PolyDragModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<PolyDragModuleInst>(this, ginst);
}

void PolyDragModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t {
    return PolyDragModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs", [](dataflow::moduledata_ptr_t mdata) {
    _reshapePolyDragIOs(mdata);
  });
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::PolyDragModuleData, "psys::PolyDragModuleData");
