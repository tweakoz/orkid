////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// EntityRefModuleData — outputs the SRT decomposition of an
// ECS-published entity transform, looked up at compute time via
// GraphInst::_resolveEntityXf. One instance per unique entity name in
// the graph; the HyperSyn DSL lowerer creates them lazily from
// Expr.entity("name").pos / .quat / .scale references.

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>

namespace dflow = ::ork::dataflow;
namespace ork::lev2::particle {

///////////////////////////////////////////////////////////////////////////////

struct EntityRefModuleInst : dflow::DgModuleInst {

  EntityRefModuleInst(const EntityRefModuleData* data, dataflow::GraphInst* ginst)
      : dflow::DgModuleInst(data, ginst)
      , _erd(data) {
  }

  void onLink(dflow::GraphInst* /*inst*/) final {
    _outputPos          = typedOutputNamed<dflow::Vec3fPlugTraits>("Pos");
    _outputQuat         = typedOutputNamed<dflow::QuatfPlugTraits>("Quat");
    _outputScale        = typedOutputNamed<dflow::Vec3fPlugTraits>("Scale");
    _outputScaleUniform = typedOutputNamed<dflow::FloatPlugTraits>("ScaleUniform");
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t /*updata*/) final {
    fvec3 pos(0, 0, 0);
    fquat quat;
    float scale_u = 1.0f;
    // No name set, or no host resolver wired → identity defaults.
    // Standalone (non-ECS) graphs see this every frame.
    if (not _erd->_entity_name.empty() && inst->_resolveEntityXf) {
      if (auto xf = inst->_resolveEntityXf(_erd->_entity_name)) {
        xf->composed().decompose(pos, quat, scale_u);
      }
    }
    _outputPos->setValue(pos);
    _outputQuat->setValue(quat);
    _outputScale->setValue(fvec3(scale_u, scale_u, scale_u));
    _outputScaleUniform->setValue(scale_u);
  }

  void onActivate(dflow::GraphInst* /*inst*/) final {}
  void onReset(dflow::GraphInst* /*inst*/) final {}

  const EntityRefModuleData* _erd;
  dflow::fvec3_out_pluginst_ptr_t _outputPos;
  dflow::fquat_out_pluginst_ptr_t _outputQuat;
  dflow::fvec3_out_pluginst_ptr_t _outputScale;
  dflow::float_out_pluginst_ptr_t _outputScaleUniform;
};

///////////////////////////////////////////////////////////////////////////////

EntityRefModuleData::EntityRefModuleData() {
}

static void _reshapeEntityRefIOs(dataflow::moduledata_ptr_t data) {
  ModuleData::createOutputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "Pos");
  ModuleData::createOutputPlug<dflow::QuatfPlugTraits>(data, dflow::EPR_UNIFORM, "Quat");
  ModuleData::createOutputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "Scale");
  // Scalar (uniform) scale — the value DecompTransform::decompose returns.
  // For scenes that don't need a vec3 of scale axes, this saves a vec3
  // construction at consume time and binds directly to scalar plugs.
  ModuleData::createOutputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "ScaleUniform");
}

std::shared_ptr<EntityRefModuleData> EntityRefModuleData::createShared() {
  auto data = std::make_shared<EntityRefModuleData>();
  _initPoolIOs(data);
  _reshapeEntityRefIOs(data);
  return data;
}

std::shared_ptr<EntityRefModuleData> EntityRefModuleData::createWithName(const std::string& name) {
  auto data           = createShared();
  data->_entity_name  = name;
  return data;
}

dflow::dgmoduleinst_ptr_t EntityRefModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<EntityRefModuleInst>(this, ginst);
}

void EntityRefModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t {
    return EntityRefModuleData::createShared();
  });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs",
      [](dataflow::moduledata_ptr_t mdata) { _reshapeEntityRefIOs(mdata); });
  clazz->directProperty("entity_name", &EntityRefModuleData::_entity_name);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::particle
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::EntityRefModuleData, "psys::EntityRefModuleData");
