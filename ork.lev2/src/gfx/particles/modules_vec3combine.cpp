////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// Vec3CombineModuleData — combines three scalar (float) inputs into an fvec3
// output. The HyperSyn DSL lowerer emits one of these per Expr.vec3(...)
// binding: each axis is lowered to its own scalar chain (Globals or
// ParametersModule connection + floatxf stages), each chain feeds the
// corresponding X/Y/Z input here, and the "value" output then connects to
// the target vec3 plug on the consuming particle module.

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

namespace dflow = ::ork::dataflow;
namespace ork::lev2::particle {

///////////////////////////////////////////////////////////////////////////////

struct Vec3CombineModuleInst : dflow::DgModuleInst {

  Vec3CombineModuleInst(const Vec3CombineModuleData* data, dflow::GraphInst* ginst)
      : dflow::DgModuleInst(data, ginst) {
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final {
    fvec3 v(_inputX->value(), _inputY->value(), _inputZ->value());
    _output->setValue(v);
  }

  void onLink(dflow::GraphInst* inst) final {
    // FloatXf (not Float) inputs — the lowerer attaches per-axis xf chains
    // (scale/bias/sine/...) to each input plug's transformer. A plain float
    // plug has no transformer.
    _inputX = typedInputNamed<dflow::FloatXfPlugTraits>("X");
    _inputY = typedInputNamed<dflow::FloatXfPlugTraits>("Y");
    _inputZ = typedInputNamed<dflow::FloatXfPlugTraits>("Z");
    _output = typedOutputNamed<dflow::Vec3fPlugTraits>("value");
  }

  void onActivate(dflow::GraphInst* inst) final {
  }

  dflow::floatxf_inp_pluginst_ptr_t _inputX;
  dflow::floatxf_inp_pluginst_ptr_t _inputY;
  dflow::floatxf_inp_pluginst_ptr_t _inputZ;
  dflow::fvec3_out_pluginst_ptr_t   _output;
};

///////////////////////////////////////////////////////////////////////////////

Vec3CombineModuleData::Vec3CombineModuleData() {
}

///////////////////////////////////////////////////////////////////////////////

static void _reshapeVec3CombineIOs(dataflow::moduledata_ptr_t data) {
  ModuleData::createInputPlug<dflow::FloatXfPlugTraits>(data, dflow::EPR_UNIFORM, "X");
  ModuleData::createInputPlug<dflow::FloatXfPlugTraits>(data, dflow::EPR_UNIFORM, "Y");
  ModuleData::createInputPlug<dflow::FloatXfPlugTraits>(data, dflow::EPR_UNIFORM, "Z");
  ModuleData::createOutputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "value");
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<Vec3CombineModuleData> Vec3CombineModuleData::createShared() {
  // No _initPoolIOs — Vec3Combine is auxiliary; it never participates in the
  // pool chain. The base ParticleModuleData class is inherited only for
  // consistency with sibling auxiliary modules (Globals follows the same
  // shape but does call _initPoolIOs because it predated this distinction).
  auto data = std::make_shared<Vec3CombineModuleData>();
  _reshapeVec3CombineIOs(data);
  return data;
}

///////////////////////////////////////////////////////////////////////////////

dflow::dgmoduleinst_ptr_t Vec3CombineModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<Vec3CombineModuleInst>(this, ginst);
}

///////////////////////////////////////////////////////////////////////////////

void Vec3CombineModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t {
    return Vec3CombineModuleData::createShared();
  });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t mdata) {
        _reshapeVec3CombineIOs(mdata);
      });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::particle
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::Vec3CombineModuleData, "psys::Vec3CombineModuleData");
