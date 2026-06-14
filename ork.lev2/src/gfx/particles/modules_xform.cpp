////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// Vec3 transform / arithmetic modules used by the HyperSyn DSL lowerer:
//   TransformPointModuleData — local_point  -> world_point via host SRT
//   TransformDirModuleData   — local_dir    -> world_dir   via host 3x3
//   Vec3AddModuleData        — A + B = Sum  (componentwise)

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/math/cmatrix4.h>

namespace dflow = ::ork::dataflow;
namespace ork::lev2::particle {

///////////////////////////////////////////////////////////////////////////////
// TransformPointModule
///////////////////////////////////////////////////////////////////////////////

struct TransformPointModuleInst : dflow::DgModuleInst {

  TransformPointModuleInst(const TransformPointModuleData* d, dflow::GraphInst* g)
      : dflow::DgModuleInst(d, g)
      , _d(d) {
  }

  void onLink(dflow::GraphInst* /*inst*/) final {
    _inLocal = typedInputNamed<dflow::Vec3fPlugTraits>("LocalPoint");
    _outWorld = typedOutputNamed<dflow::Vec3fPlugTraits>("WorldPoint");
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t /*updata*/) final {
    fvec3 local = _inLocal->value();
    fvec3 world = local;
    if (not _d->_entity_name.empty() && inst->_resolveEntityXf) {
      if (auto xf = inst->_resolveEntityXf(_d->_entity_name)) {
        fmtx4 m = xf->composed();
        // transform(matrix) is the affine point transform (w=1) in
        // orkid's matrix API. Returns fvec4; drop the .w (it's 1).
        auto p4 = local.transform(m);
        world = fvec3(p4.x, p4.y, p4.z);
      }
    }
    _outWorld->setValue(world);
  }

  void onActivate(dflow::GraphInst* /*inst*/) final {}

  const TransformPointModuleData* _d;
  dflow::fvec3_inp_pluginst_ptr_t _inLocal;
  dflow::fvec3_out_pluginst_ptr_t _outWorld;
};

TransformPointModuleData::TransformPointModuleData() {}

static void _reshapeTransformPointIOs(dataflow::moduledata_ptr_t data) {
  ModuleData::createInputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "LocalPoint");
  ModuleData::createOutputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "WorldPoint");
}

std::shared_ptr<TransformPointModuleData> TransformPointModuleData::createShared() {
  auto data = std::make_shared<TransformPointModuleData>();
  _reshapeTransformPointIOs(data);
  return data;
}
std::shared_ptr<TransformPointModuleData> TransformPointModuleData::createWithName(const std::string& name) {
  auto data          = createShared();
  data->_entity_name = name;
  return data;
}
dflow::dgmoduleinst_ptr_t TransformPointModuleData::createInstance(dataflow::GraphInst* g) const {
  return std::make_shared<TransformPointModuleInst>(this, g);
}
void TransformPointModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t {
    return TransformPointModuleData::createShared();
  });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeTransformPointIOs(m); });
  clazz->directProperty("entity_name", &TransformPointModuleData::_entity_name);
}

///////////////////////////////////////////////////////////////////////////////
// TransformDirModule
///////////////////////////////////////////////////////////////////////////////

struct TransformDirModuleInst : dflow::DgModuleInst {

  TransformDirModuleInst(const TransformDirModuleData* d, dflow::GraphInst* g)
      : dflow::DgModuleInst(d, g)
      , _d(d) {
  }

  void onLink(dflow::GraphInst* /*inst*/) final {
    _inLocal  = typedInputNamed<dflow::Vec3fPlugTraits>("LocalDir");
    _outWorld = typedOutputNamed<dflow::Vec3fPlugTraits>("WorldDir");
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t /*updata*/) final {
    fvec3 local = _inLocal->value();
    fvec3 world = local;
    if (not _d->_entity_name.empty() && inst->_resolveEntityXf) {
      if (auto xf = inst->_resolveEntityXf(_d->_entity_name)) {
        fmtx4 m = xf->composed();
        // 3x3 part: rotation + scale; no translation. transform3x3 is
        // orkid's direct-vector transform on fvec3.
        world = local.transform3x3(m);
      }
    }
    _outWorld->setValue(world);
  }

  void onActivate(dflow::GraphInst* /*inst*/) final {}

  const TransformDirModuleData* _d;
  dflow::fvec3_inp_pluginst_ptr_t _inLocal;
  dflow::fvec3_out_pluginst_ptr_t _outWorld;
};

TransformDirModuleData::TransformDirModuleData() {}

static void _reshapeTransformDirIOs(dataflow::moduledata_ptr_t data) {
  ModuleData::createInputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "LocalDir");
  ModuleData::createOutputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "WorldDir");
}

std::shared_ptr<TransformDirModuleData> TransformDirModuleData::createShared() {
  auto data = std::make_shared<TransformDirModuleData>();
  _reshapeTransformDirIOs(data);
  return data;
}
std::shared_ptr<TransformDirModuleData> TransformDirModuleData::createWithName(const std::string& name) {
  auto data          = createShared();
  data->_entity_name = name;
  return data;
}
dflow::dgmoduleinst_ptr_t TransformDirModuleData::createInstance(dataflow::GraphInst* g) const {
  return std::make_shared<TransformDirModuleInst>(this, g);
}
void TransformDirModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t {
    return TransformDirModuleData::createShared();
  });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeTransformDirIOs(m); });
  clazz->directProperty("entity_name", &TransformDirModuleData::_entity_name);
}

///////////////////////////////////////////////////////////////////////////////
// Vec3Add
///////////////////////////////////////////////////////////////////////////////

struct Vec3AddModuleInst : dflow::DgModuleInst {

  Vec3AddModuleInst(const Vec3AddModuleData* d, dflow::GraphInst* g)
      : dflow::DgModuleInst(d, g) {
  }

  void onLink(dflow::GraphInst* /*inst*/) final {
    _inA = typedInputNamed<dflow::Vec3fPlugTraits>("A");
    _inB = typedInputNamed<dflow::Vec3fPlugTraits>("B");
    _out = typedOutputNamed<dflow::Vec3fPlugTraits>("Sum");
  }

  void compute(dflow::GraphInst* /*inst*/, ui::updatedata_ptr_t /*updata*/) final {
    _out->setValue(_inA->value() + _inB->value());
  }

  void onActivate(dflow::GraphInst* /*inst*/) final {}

  dflow::fvec3_inp_pluginst_ptr_t _inA;
  dflow::fvec3_inp_pluginst_ptr_t _inB;
  dflow::fvec3_out_pluginst_ptr_t _out;
};

Vec3AddModuleData::Vec3AddModuleData() {}

static void _reshapeVec3AddIOs(dataflow::moduledata_ptr_t data) {
  ModuleData::createInputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "A");
  ModuleData::createInputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "B");
  ModuleData::createOutputPlug<dflow::Vec3fPlugTraits>(data, dflow::EPR_UNIFORM, "Sum");
}

std::shared_ptr<Vec3AddModuleData> Vec3AddModuleData::createShared() {
  auto data = std::make_shared<Vec3AddModuleData>();
  _reshapeVec3AddIOs(data);
  return data;
}
dflow::dgmoduleinst_ptr_t Vec3AddModuleData::createInstance(dataflow::GraphInst* g) const {
  return std::make_shared<Vec3AddModuleInst>(this, g);
}
void Vec3AddModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t {
    return Vec3AddModuleData::createShared();
  });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeVec3AddIOs(m); });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::particle
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::TransformPointModuleData, "psys::TransformPointModuleData");
ImplementReflectionX(ptcl::TransformDirModuleData,   "psys::TransformDirModuleData");
ImplementReflectionX(ptcl::Vec3AddModuleData,        "psys::Vec3AddModuleData");
