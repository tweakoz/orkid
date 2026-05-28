////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/dataflow/basic_modules.h>
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/plug_inst.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>
#include <algorithm>

namespace ork::dataflow {

///////////////////////////////////////////////////////////////////////////////
// Min — value = min(A, B)
///////////////////////////////////////////////////////////////////////////////

struct MinModuleInst : DgModuleInst {

  MinModuleInst(const MinModuleData* data, GraphInst* ginst)
      : DgModuleInst(data, ginst) {}

  void compute(GraphInst*, ui::updatedata_ptr_t) final {
    _output->setValue(std::min(_inputA->value(), _inputB->value()));
  }

  void onLink(GraphInst*) final {
    _inputA = typedInputNamed<FloatXfPlugTraits>("A");
    _inputB = typedInputNamed<FloatXfPlugTraits>("B");
    _output = typedOutputNamed<FloatPlugTraits>("value");
  }

  floatxf_inp_pluginst_ptr_t _inputA;
  floatxf_inp_pluginst_ptr_t _inputB;
  float_out_pluginst_ptr_t   _output;
};

MinModuleData::MinModuleData() {}

static void _reshapeMinIOs(moduledata_ptr_t data) {
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "A");
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "B");
  ModuleData::createOutputPlug<FloatPlugTraits>(data, EPR_UNIFORM, "value");
}

std::shared_ptr<MinModuleData> MinModuleData::createShared() {
  auto data = std::make_shared<MinModuleData>();
  _reshapeMinIOs(data);
  return data;
}

dgmoduleinst_ptr_t MinModuleData::createInstance(GraphInst* ginst) const {
  return std::make_shared<MinModuleInst>(this, ginst);
}

void MinModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t {
    return MinModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>(
      "reshapeIOs", [](moduledata_ptr_t mdata) { _reshapeMinIOs(mdata); });
}

///////////////////////////////////////////////////////////////////////////////
// Max — value = max(A, B)
///////////////////////////////////////////////////////////////////////////////

struct MaxModuleInst : DgModuleInst {

  MaxModuleInst(const MaxModuleData* data, GraphInst* ginst)
      : DgModuleInst(data, ginst) {}

  void compute(GraphInst*, ui::updatedata_ptr_t) final {
    _output->setValue(std::max(_inputA->value(), _inputB->value()));
  }

  void onLink(GraphInst*) final {
    _inputA = typedInputNamed<FloatXfPlugTraits>("A");
    _inputB = typedInputNamed<FloatXfPlugTraits>("B");
    _output = typedOutputNamed<FloatPlugTraits>("value");
  }

  floatxf_inp_pluginst_ptr_t _inputA;
  floatxf_inp_pluginst_ptr_t _inputB;
  float_out_pluginst_ptr_t   _output;
};

MaxModuleData::MaxModuleData() {}

static void _reshapeMaxIOs(moduledata_ptr_t data) {
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "A");
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "B");
  ModuleData::createOutputPlug<FloatPlugTraits>(data, EPR_UNIFORM, "value");
}

std::shared_ptr<MaxModuleData> MaxModuleData::createShared() {
  auto data = std::make_shared<MaxModuleData>();
  _reshapeMaxIOs(data);
  return data;
}

dgmoduleinst_ptr_t MaxModuleData::createInstance(GraphInst* ginst) const {
  return std::make_shared<MaxModuleInst>(this, ginst);
}

void MaxModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t {
    return MaxModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>(
      "reshapeIOs", [](moduledata_ptr_t mdata) { _reshapeMaxIOs(mdata); });
}

///////////////////////////////////////////////////////////////////////////////
// Lerp — value = A * (1 - T) + B * T  (no clamp on T, matches GLSL mix)
///////////////////////////////////////////////////////////////////////////////

struct LerpModuleInst : DgModuleInst {

  LerpModuleInst(const LerpModuleData* data, GraphInst* ginst)
      : DgModuleInst(data, ginst) {}

  void compute(GraphInst*, ui::updatedata_ptr_t) final {
    float a = _inputA->value();
    float b = _inputB->value();
    float t = _inputT->value();
    _output->setValue(a * (1.0f - t) + b * t);
  }

  void onLink(GraphInst*) final {
    _inputA = typedInputNamed<FloatXfPlugTraits>("A");
    _inputB = typedInputNamed<FloatXfPlugTraits>("B");
    _inputT = typedInputNamed<FloatXfPlugTraits>("T");
    _output = typedOutputNamed<FloatPlugTraits>("value");
  }

  floatxf_inp_pluginst_ptr_t _inputA;
  floatxf_inp_pluginst_ptr_t _inputB;
  floatxf_inp_pluginst_ptr_t _inputT;
  float_out_pluginst_ptr_t   _output;
};

LerpModuleData::LerpModuleData() {}

static void _reshapeLerpIOs(moduledata_ptr_t data) {
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "A");
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "B");
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "T");
  ModuleData::createOutputPlug<FloatPlugTraits>(data, EPR_UNIFORM, "value");
}

std::shared_ptr<LerpModuleData> LerpModuleData::createShared() {
  auto data = std::make_shared<LerpModuleData>();
  _reshapeLerpIOs(data);
  return data;
}

dgmoduleinst_ptr_t LerpModuleData::createInstance(GraphInst* ginst) const {
  return std::make_shared<LerpModuleInst>(this, ginst);
}

void LerpModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t {
    return LerpModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>(
      "reshapeIOs", [](moduledata_ptr_t mdata) { _reshapeLerpIOs(mdata); });
}

///////////////////////////////////////////////////////////////////////////////
// Pow — value = pow(X, K)  (variable-K case; const-K uses the chain stage)
///////////////////////////////////////////////////////////////////////////////

struct PowModuleInst : DgModuleInst {

  PowModuleInst(const PowModuleData* data, GraphInst* ginst)
      : DgModuleInst(data, ginst) {}

  void compute(GraphInst*, ui::updatedata_ptr_t) final {
    _output->setValue(std::pow(_inputX->value(), _inputK->value()));
  }

  void onLink(GraphInst*) final {
    _inputX = typedInputNamed<FloatXfPlugTraits>("X");
    _inputK = typedInputNamed<FloatXfPlugTraits>("K");
    _output = typedOutputNamed<FloatPlugTraits>("value");
  }

  floatxf_inp_pluginst_ptr_t _inputX;
  floatxf_inp_pluginst_ptr_t _inputK;
  float_out_pluginst_ptr_t   _output;
};

PowModuleData::PowModuleData() {}

static void _reshapePowIOs(moduledata_ptr_t data) {
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "X");
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "K");
  ModuleData::createOutputPlug<FloatPlugTraits>(data, EPR_UNIFORM, "value");
}

std::shared_ptr<PowModuleData> PowModuleData::createShared() {
  auto data = std::make_shared<PowModuleData>();
  _reshapePowIOs(data);
  return data;
}

dgmoduleinst_ptr_t PowModuleData::createInstance(GraphInst* ginst) const {
  return std::make_shared<PowModuleInst>(this, ginst);
}

void PowModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t {
    return PowModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>(
      "reshapeIOs", [](moduledata_ptr_t mdata) { _reshapePowIOs(mdata); });
}

///////////////////////////////////////////////////////////////////////////////
// Vec4Combine — value = vec4(X, Y, Z, W)
///////////////////////////////////////////////////////////////////////////////

struct Vec4CombineModuleInst : DgModuleInst {

  Vec4CombineModuleInst(const Vec4CombineModuleData* data, GraphInst* ginst)
      : DgModuleInst(data, ginst) {}

  void compute(GraphInst*, ui::updatedata_ptr_t) final {
    _output->setValue(fvec4(
        _inputX->value(), _inputY->value(),
        _inputZ->value(), _inputW->value()));
  }

  void onLink(GraphInst*) final {
    _inputX = typedInputNamed<FloatXfPlugTraits>("X");
    _inputY = typedInputNamed<FloatXfPlugTraits>("Y");
    _inputZ = typedInputNamed<FloatXfPlugTraits>("Z");
    _inputW = typedInputNamed<FloatXfPlugTraits>("W");
    _output = typedOutputNamed<Vec4fPlugTraits>("value");
  }

  floatxf_inp_pluginst_ptr_t _inputX;
  floatxf_inp_pluginst_ptr_t _inputY;
  floatxf_inp_pluginst_ptr_t _inputZ;
  floatxf_inp_pluginst_ptr_t _inputW;
  fvec4_out_pluginst_ptr_t   _output;
};

Vec4CombineModuleData::Vec4CombineModuleData() {}

static void _reshapeVec4CombineIOs(moduledata_ptr_t data) {
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "X");
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Y");
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Z");
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "W");
  ModuleData::createOutputPlug<Vec4fPlugTraits>(data, EPR_UNIFORM, "value");
}

std::shared_ptr<Vec4CombineModuleData> Vec4CombineModuleData::createShared() {
  auto data = std::make_shared<Vec4CombineModuleData>();
  _reshapeVec4CombineIOs(data);
  return data;
}

dgmoduleinst_ptr_t Vec4CombineModuleData::createInstance(GraphInst* ginst) const {
  return std::make_shared<Vec4CombineModuleInst>(this, ginst);
}

void Vec4CombineModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t {
    return Vec4CombineModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>(
      "reshapeIOs", [](moduledata_ptr_t mdata) { _reshapeVec4CombineIOs(mdata); });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::dataflow::MinModuleData,         "dflow::MinModuleData");
ImplementReflectionX(ork::dataflow::MaxModuleData,         "dflow::MaxModuleData");
ImplementReflectionX(ork::dataflow::LerpModuleData,        "dflow::LerpModuleData");
ImplementReflectionX(ork::dataflow::PowModuleData,         "dflow::PowModuleData");
ImplementReflectionX(ork::dataflow::Vec4CombineModuleData, "dflow::Vec4CombineModuleData");
