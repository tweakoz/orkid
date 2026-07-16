///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/dataflow/all.h>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/context_variable.h>
#include <ork/rtti/RTTI.h>
#include <ork/rtti/Class.h>
#include <ork/object/ObjectClass.h>
#include <ork/reflect/Description.h>
#include <ork/reflect/properties/ITyped.h>
#include <ork/reflect/properties/ITypedArray.h>
#include <ork/reflect/properties/IObjectArray.h>
#include <ork/reflect/properties/IObjectMap.h>
#include <cxxabi.h>
#include <stack>
///////////////////////////////////////////////////////////////////////////////
namespace ork {
using namespace dataflow;
///////////////////////////////////////////////////////////////////////////////
// runtime demangle of a plug's flowing-data type (GetDataTypeId) — the same type
// identity GraphData::canConnect uses for edge validation, rendered human-readable
// so an editor can color/validate typed edges.
static std::string dflow_demangle(const std::type_info& ti) {
  int status            = 0;
  const char* demangled = abi::__cxa_demangle(ti.name(), nullptr, nullptr, &status);
  std::string rval      = (status == 0 and demangled) ? std::string(demangled) : std::string(ti.name());
  if (demangled)
    free((void*)demangled);
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void pyinit_dataflow(py::module& module_core) {
  auto dfgmodule  = module_core.def_submodule("dataflow", "core dataflow operations");
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////
  struct input_proxy {
    dgmoduledata_ptr_t _module;
  };
  using input_proxy_ptr_t = std::shared_ptr<input_proxy>;
  auto input_proxy_type   =                                                //
      py::class_<input_proxy, input_proxy_ptr_t>(dfgmodule, "input_proxy") //
          .def(
              "__repr__",
              [](input_proxy_ptr_t proxy) -> std::string {
                std::string out_str;
                out_str += FormatString("ModuleInputProxy: \n");
                for (auto i : proxy->_module->_inputs) {
                  auto clazzname = i->objectClass()->Name();
                  out_str += FormatString(" input %s:  %s\n", i->_name.c_str(), clazzname.c_str());
                }
                return out_str;
              })
          .def(
              "__getattr__",                                                         //
              [type_codec](input_proxy_ptr_t proxy, std::string key) -> py::object { //
                auto m     = proxy->_module;
                auto input = m->inputNamed(key);
                if (input) {
                  return type_codec->encode(input);
                }
                return py::none();
              })
          .def(
              "__setattr__",                                                             //
              [type_codec](input_proxy_ptr_t proxy, std::string key, py::object value) { //
                auto m     = proxy->_module;
                auto input = m->inputNamed(key);
                if (not input) { // loud PYTHON error, not a process abort — name the plug
                  throw py::attribute_error(FormatString(
                      "dataflow module<%s> has no input plug '%s'", m->_name.c_str(), key.c_str()));
                }
                auto decoded_value = type_codec->decode(value);

                ////////////////////////////////////////////////
                // TODO a cleaner way to do this would be nice..
                //   lots of permutations
                ////////////////////////////////////////////////

                // if the target is an INT plug, set it (cast); returns whether it handled it.
                auto set_int = [input](int value) -> bool {
                  auto iinplug = std::dynamic_pointer_cast<inplugdata<IntPlugTraits>>(input);
                  if (iinplug) {
                    iinplug->setValue(value);
                    return true;
                  }
                  return false;
                };
                auto set_float = [input, set_int](float value) {
                  auto finplug = std::dynamic_pointer_cast<inplugdata<FloatPlugTraits>>(input);
                  if (finplug) {
                    finplug->setValue(value);
                  } else {
                    auto xfinplug = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(input);
                    if (xfinplug) {
                      xfinplug->setValue(value);
                    } else if (set_int(int(value))) {
                      // a numeric value fed to an int plug -> cast to int
                    } else {
                      OrkAssert(false);
                    }
                  }
                };
                auto set_fvec3 = [input](fvec3 value) {
                  auto finplug = std::dynamic_pointer_cast<inplugdata<Vec3fPlugTraits>>(input);
                  if (finplug) {
                    finplug->setValue(value);
                  } else {
                    auto xfinplug = std::dynamic_pointer_cast<inplugdata<Vec3XfPlugTraits>>(input);
                    if (xfinplug) {
                      xfinplug->setValue(value);
                    } else {
                      OrkAssert(false);
                    }
                  }
                };
                auto set_fquat = [input](fquat value) {
                  auto finplug = std::dynamic_pointer_cast<inplugdata<QuatfPlugTraits>>(input);
                  if (finplug) {
                    finplug->setValue(value);
                  } else {
                    auto xfinplug = std::dynamic_pointer_cast<inplugdata<QuatXfPlugTraits>>(input);
                    if (xfinplug) {
                      xfinplug->setValue(value);
                    } else {
                      OrkAssert(false);
                    }
                  }
                };
                auto set_fvec2 = [input](fvec2 value) {
                  auto finplug = std::dynamic_pointer_cast<inplugdata<Vec2fPlugTraits>>(input);
                  if (finplug) {
                    finplug->setValue(value);
                  } else {
                    OrkAssert(false); // vec2 has no Xf variant
                  }
                };
                auto set_fvec4 = [input](fvec4 value) {
                  auto finplug = std::dynamic_pointer_cast<inplugdata<Vec4fPlugTraits>>(input);
                  if (finplug) {
                    finplug->setValue(value);
                  } else {
                    auto xfinplug = std::dynamic_pointer_cast<inplugdata<Vec4XfPlugTraits>>(input);
                    if (xfinplug) {
                      xfinplug->setValue(value);
                    } else {
                      OrkAssert(false);
                    }
                  }
                };

                if (auto as_float = decoded_value.tryAs<float>()) {
                  set_float(as_float.value());
                } else if (auto as_int = decoded_value.tryAs<int>()) {
                  if (not set_int(as_int.value())) // INT plug -> exact int; else coerce to a float plug
                    set_float(as_int.value());
                } else if (auto as_fvec2 = decoded_value.tryAs<fvec2>()) {
                  set_fvec2(as_fvec2.value());
                } else if (auto as_fvec2_ptr = decoded_value.tryAs<fvec2_ptr_t>()) {
                  set_fvec2(*as_fvec2_ptr.value());
                } else if (auto as_fvec3 = decoded_value.tryAs<fvec3>()) {
                  set_fvec3(as_fvec3.value());
                } else if (auto as_fvec3_ptr = decoded_value.tryAs<fvec3_ptr_t>()) {
                  set_fvec3(*as_fvec3_ptr.value());
                } else if (auto as_fvec4 = decoded_value.tryAs<fvec4>()) {
                  set_fvec4(as_fvec4.value());
                } else if (auto as_fvec4_ptr = decoded_value.tryAs<fvec4_ptr_t>()) {
                  set_fvec4(*as_fvec4_ptr.value());
                } else if (auto as_fquat = decoded_value.tryAs<fquat>()) {
                  set_fquat(as_fquat.value());
                } else if (auto as_fquat_ptr = decoded_value.tryAs<fquat_ptr_t>()) {
                  set_fquat(*as_fquat_ptr.value());
                } else {
                  OrkAssert(false);
                }

                ////////////////////////////////////////////////
              });
  /////////////////////////////////////////////////////////////////////////////
  struct output_proxy {
    dgmoduledata_ptr_t _module;
  };
  using output_proxy_ptr_t = std::shared_ptr<output_proxy>;
  auto output_proxy_type   =                                                  //
      py::class_<output_proxy, output_proxy_ptr_t>(dfgmodule, "output_proxy") //
          .def(
              "__repr__",
              [](output_proxy_ptr_t proxy) -> std::string {
                std::string out_str;
                out_str += FormatString("ModuleOutputProxy: \n");
                for (auto i : proxy->_module->_outputs) {
                  auto clazzname = i->objectClass()->Name();
                  out_str += FormatString(" output %s:  %s\n", i->_name.c_str(), clazzname.c_str());
                }
                return out_str;
              })
          .def(
              "__getattr__",                                                          //
              [type_codec](output_proxy_ptr_t proxy, std::string key) -> py::object { //
                auto m     = proxy->_module;
                auto input = m->outputNamed(key);
                if (input) {
                  return type_codec->encode(input);
                }
                return py::none();
              });
  /////////////////////////////////////////////////////////////////////////////
  using moduledata_ptr_t = std::shared_ptr<ModuleData>;
  auto moduledata_type   = //
      py::class_<ModuleData, ork::Object, moduledata_ptr_t>(dfgmodule, "ModuleData")
          .def_property_readonly(
              "numInputs",
              [](moduledata_ptr_t m) -> int { //
                return m->numInputs();
              })
          .def_property_readonly(
              "numOutputs",
              [](moduledata_ptr_t m) -> int { //
                return m->numOutputs();
              })
          .def_property_readonly(
              "name",
              [](moduledata_ptr_t m) -> std::string { return m->_name; });
  type_codec->registerStdCodec<moduledata_ptr_t>(moduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto dgmoduledata_type = //
      py::class_<DgModuleData, ModuleData, dgmoduledata_ptr_t>(dfgmodule, "DgModuleData")
          .def_static("createShared", []() -> dgmoduledata_ptr_t { return DgModuleData::createShared(); })
          .def(
              "createUniformFloatXfInputPlug",
              [](dgmoduledata_ptr_t m, std::string named) -> inplugdata_ptr_t {
                return m->createInputPlug<FloatXfPlugTraits>(m, EPR_UNIFORM, named.c_str());
              })
          .def(
              "createUniformVec3XfInputPlug",
              [](dgmoduledata_ptr_t m, std::string named) -> inplugdata_ptr_t {
                return m->createInputPlug<Vec3XfPlugTraits>(m, EPR_UNIFORM, named.c_str());
              })
          .def(
              "createUniformFloatOutputPlug",
              [](dgmoduledata_ptr_t m, std::string named) -> outplugdata_ptr_t {
                return m->createOutputPlug<FloatPlugTraits>(m, EPR_UNIFORM, named.c_str());
              })
          .def(
              "createUniformVec3OutputPlug",
              [](dgmoduledata_ptr_t m, std::string named) -> outplugdata_ptr_t {
                return m->createOutputPlug<Vec3fPlugTraits>(m, EPR_UNIFORM, named.c_str());
              })
          .def_property_readonly(
              "inputs",
              [](dgmoduledata_ptr_t m) -> input_proxy_ptr_t {
                auto proxy     = std::make_shared<input_proxy>();
                proxy->_module = m;
                return proxy;
              })
          .def_property_readonly(
              "outputs",
              [](dgmoduledata_ptr_t m) -> output_proxy_ptr_t {
                auto proxy     = std::make_shared<output_proxy>();
                proxy->_module = m;
                return proxy;
              })
          .def_property_readonly("mindepth", [](dgmoduledata_ptr_t m) -> size_t { return m->computeMinDepth(); })
          .def_property_readonly("maxdepth", [](dgmoduledata_ptr_t m) -> size_t { return m->computeMaxDepth(); })
          .def_property(
              "bypassed",
              [](dgmoduledata_ptr_t m) -> bool { return m->_bypassed; },
              [](dgmoduledata_ptr_t m, bool v) { m->_bypassed = v; })
          .def("__repr__", [](dgmoduledata_ptr_t m) -> std::string {
            auto clazz     = m->objectClass();
            auto clazzname = clazz->Name();
            return FormatString("DgModuleData(%p:%s)", (void*)m.get(), clazzname.c_str());
          });

  type_codec->registerStdCodec<dgmoduledata_ptr_t>(dgmoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  // todo use trampoline method from https://pybind11.readthedocs.io/en/stable/advanced/classes.html
  //  to allow python subclass of c++ class
  //  so onCompute and onLink can be "virtual" python methods
  /////////////////////////////////////////////////////////////////////////////
  auto lambdamoduledata_type = //
      py::class_<LambdaModuleData, DgModuleData, lambdamoduledata_ptr_t>(dfgmodule, "LambdaModule")
          .def_static("createShared", []() -> lambdamoduledata_ptr_t { return LambdaModuleData::createShared(); })
          .def(
              "onCompute",
              [](lambdamoduledata_ptr_t m, py::object pylambda) { //
                m->_computeLambda = [m, pylambda](
                                        graphinst_ptr_t gi,        //
                                        ui::updatedata_ptr_t ud) { //
                  py::gil_scoped_acquire acquire;
                  pylambda(m, gi, ud);
                };
              })
          .def(
              "onLink",
              [](lambdamoduledata_ptr_t m, py::object pylambda) {    //
                m->_linkLambda = [m, pylambda](graphinst_ptr_t gi) { //
                  py::gil_scoped_acquire acquire;
                  pylambda(m, gi);
                };
              })
          .def("__repr__", [](lambdamoduledata_ptr_t m) -> std::string {
            return FormatString("LambdaModuleData(%p)", (void*)m.get());
          });
  /////////////////////////////////////////////////////////////////////////////
  // Min / Max / Lerp — generic float-in float-out scalar modules. The
  // HyperSyn DSL lowerer emits these from Expr.min/max/lerp(...) but they
  // are usable from any dflow graph.
  /////////////////////////////////////////////////////////////////////////////
  py::class_<MinModuleData, DgModuleData, minmodule_ptr_t>(dfgmodule, "MinModule")
      .def_static("createShared", []() -> minmodule_ptr_t { return MinModuleData::createShared(); })
      .def("__repr__", [](minmodule_ptr_t m) -> std::string {
        return FormatString("MinModuleData(%p)", (void*)m.get());
      });
  py::class_<MaxModuleData, DgModuleData, maxmodule_ptr_t>(dfgmodule, "MaxModule")
      .def_static("createShared", []() -> maxmodule_ptr_t { return MaxModuleData::createShared(); })
      .def("__repr__", [](maxmodule_ptr_t m) -> std::string {
        return FormatString("MaxModuleData(%p)", (void*)m.get());
      });
  py::class_<LerpModuleData, DgModuleData, lerpmodule_ptr_t>(dfgmodule, "LerpModule")
      .def_static("createShared", []() -> lerpmodule_ptr_t { return LerpModuleData::createShared(); })
      .def("__repr__", [](lerpmodule_ptr_t m) -> std::string {
        return FormatString("LerpModuleData(%p)", (void*)m.get());
      });
  py::class_<PowModuleData, DgModuleData, powmodule_ptr_t>(dfgmodule, "PowModule")
      .def_static("createShared", []() -> powmodule_ptr_t { return PowModuleData::createShared(); })
      .def("__repr__", [](powmodule_ptr_t m) -> std::string {
        return FormatString("PowModuleData(%p)", (void*)m.get());
      });
  py::class_<Vec4CombineModuleData, DgModuleData, vec4combinemodule_ptr_t>(dfgmodule, "Vec4CombineModule")
      .def_static("createShared", []() -> vec4combinemodule_ptr_t { return Vec4CombineModuleData::createShared(); })
      .def("__repr__", [](vec4combinemodule_ptr_t m) -> std::string {
        return FormatString("Vec4CombineModuleData(%p)", (void*)m.get());
      });
  /////////////////////////////////////////////////////////////////////////////
  // todo use trampoline method from https://pybind11.readthedocs.io/en/stable/advanced/classes.html
  //  to allow python subclass of c++ class
  //  so onCompute and onLink can be "virtual" python methods
  /////////////////////////////////////////////////////////////////////////////
  struct PyLambdaModuleData : public LambdaModuleData {

    //////////////////////////////////////////
    // python subclass support via trampoline
    //////////////////////////////////////////

    PyLambdaModuleData() {
    }

    void assignClass(py::object module_clazz) {
      bool has_link    = py::hasattr(module_clazz, "onLink");
      bool has_compute = py::hasattr(module_clazz, "onCompute");

      if (has_link) {
        auto on_link = module_clazz.attr("onLink");
        _linkLambda  = [this, on_link](graphinst_ptr_t gi) { //
          on_link(this, gi);
        };
      }
      if (has_compute) {
        auto on_compute = module_clazz.attr("onCompute");
        _computeLambda  = [this, on_compute](
                             graphinst_ptr_t gi,           //
                             ui::updatedata_ptr_t udata) { //
          on_compute(this, gi, udata);
        };
      }
      _pyclazz = module_clazz;
    }

    py::object _pyclazz;
    py::object _self;
  };
  using pylambdamoduledata_ptr_t = std::shared_ptr<PyLambdaModuleData>;
  auto pylambdamoduledata_type   = //
      py::class_<PyLambdaModuleData, LambdaModuleData, pylambdamoduledata_ptr_t>(dfgmodule, "PyLambdaModule")
          .def_static("__dflow_trampoline", []() -> bool { return true; })
          .def_static("createShared", []() -> pylambdamoduledata_ptr_t { return std::make_shared<PyLambdaModuleData>(); })
          .def(
              "hackself",
              [type_codec](pylambdamoduledata_ptr_t m) {
                m->_self    = type_codec->encode(m);
                m->_pyclazz = m->_self.attr("__class__");
                OrkAssert(false);
              })
          .def("__repr__", [](pylambdamoduledata_ptr_t m) -> std::string {
            return FormatString("PyLambdaModuleData(%p)", (void*)m.get());
          });
  type_codec->registerStdCodec<pylambdamoduledata_ptr_t>(pylambdamoduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto fxfmodule  = dfgmodule.def_submodule("floatxf", "float input plug transform operators");
  auto floatxfitembasedata_type = //
      py::class_<floatxfitembasedata, ::ork::Object, floatxfitembasedata_ptr_t>(fxfmodule, "floatxfitembasedata")
          .def("__repr__", [](floatxfitembasedata_ptr_t p) -> std::string {
            return FormatString("floatxfitembasedata(%p)", (void*)p.get());
          });
  type_codec->registerStdCodec<floatxfitembasedata_ptr_t>(floatxfitembasedata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto modscabiasdata_type = //
      py::class_<modscabiasdata, ::ork::Object, modscabiasdata_ptr_t>(fxfmodule, "modscabiasdata")
          .def("__repr__", [](modscabiasdata_ptr_t p) -> std::string { return FormatString("modscabiasdata(%p)", (void*)p.get()); })
          .def_property(
              "scale",
              [](modscabiasdata_ptr_t p) -> float { return p->_scale; }, //
              [](modscabiasdata_ptr_t p, float val) { p->_scale = val; })
          .def_property(
              "bias",
              [](modscabiasdata_ptr_t p) -> float { return p->_bias; },
              [](modscabiasdata_ptr_t p, float val) { p->_bias = val; })
          .def_property(
              "mod",
              [](modscabiasdata_ptr_t p) -> float { return p->_mod; },
              [](modscabiasdata_ptr_t p, float val) { p->_mod = val; });
  type_codec->registerStdCodec<modscabiasdata_ptr_t>(modscabiasdata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto floatxfmoddata_type = //
      py::class_<floatxfmoddata, floatxfitembasedata, floatxfmoddata_ptr_t>(fxfmodule, "mod")
          .def(py::init<>())
          .def(py::init([](float mod) {
            auto rval = std::make_shared<floatxfmoddata>();
            rval->_moddata->_mod = mod;
            return rval;
          }))
          .def("__repr__", [](floatxfmoddata_ptr_t p) -> std::string { return FormatString("floatxfmoddata(%p)", (void*)p.get()); })
          .def_property(
              "mod",
              [](floatxfmoddata_ptr_t p) -> float { return p->_moddata->_mod; }, //
              [](floatxfmoddata_ptr_t p, float mod) { p->_moddata->_mod = mod; })
          .def_property(
              "do_mod",
              [](floatxfmoddata_ptr_t p) -> bool { return p->_domod; }, //
              [](floatxfmoddata_ptr_t p, bool val) { p->_domod = val; });
  type_codec->registerStdCodec<floatxfmoddata_ptr_t>(floatxfmoddata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto floatxfscaledata_type = //
      py::class_<floatxfscaledata, floatxfitembasedata, floatxfscaledata_ptr_t>(fxfmodule, "scale")
          .def(py::init<>())
          .def(py::init([](float scale) {
            auto rval = std::make_shared<floatxfscaledata>();
            rval->_scaledata->_scale = scale;
            return rval;
          }))
          .def(
              "__repr__",
              [](floatxfscaledata_ptr_t p) -> std::string { return FormatString("floatxfscaledata(%p)", (void*)p.get()); })
          .def_property(
              "scale",
              [](floatxfscaledata_ptr_t p) -> float { return p->_scaledata->_scale; }, //
              [](floatxfscaledata_ptr_t p, float scale) { p->_scaledata->_scale = scale; })
          .def_property(
              "do_scale",
              [](floatxfscaledata_ptr_t p) -> bool { return p->_doscale; }, //
              [](floatxfscaledata_ptr_t p, bool val) { p->_doscale = val; });
  type_codec->registerStdCodec<floatxfscaledata_ptr_t>(floatxfscaledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto floatxfbiasdata_type = //
      py::class_<floatxfbiasdata, floatxfitembasedata, floatxfbiasdata_ptr_t>(fxfmodule, "bias")
          .def(py::init<>())
          .def(py::init([](float bias) {
            auto rval = std::make_shared<floatxfbiasdata>();
            rval->_biasdata->_bias = bias;
            return rval;
          }))
          .def(
              "__repr__",
              [](floatxfbiasdata_ptr_t p) -> std::string { return FormatString("floatxfbiasdata(%p)", (void*)p.get()); })
          .def_property(
              "bias",
              [](floatxfbiasdata_ptr_t p) -> float { return p->_biasdata->_bias; }, //
              [](floatxfbiasdata_ptr_t p, float bias) { p->_biasdata->_bias = bias; })
          .def_property(
              "do_bias",
              [](floatxfbiasdata_ptr_t p) -> bool { return p->_dobias; }, //
              [](floatxfbiasdata_ptr_t p, bool val) { p->_dobias = val; });
  type_codec->registerStdCodec<floatxfbiasdata_ptr_t>(floatxfbiasdata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto floatxfsinedata_type = //
      py::class_<floatxfsinedata, floatxfitembasedata, floatxfsinedata_ptr_t>(fxfmodule, "sine")
          .def(py::init<>())
          .def(
              "__repr__",
              [](floatxfsinedata_ptr_t p) -> std::string { return FormatString("floatxfsinedata(%p)", (void*)p.get()); })
          .def_property(
              "do_sine",
              [](floatxfsinedata_ptr_t p) -> bool { return p->_dosine; }, //
              [](floatxfsinedata_ptr_t p, bool val) { p->_dosine = val; });
  type_codec->registerStdCodec<floatxfsinedata_ptr_t>(floatxfsinedata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto floatxfabsdata_type = //
      py::class_<floatxfabsdata, floatxfitembasedata, floatxfabsdata_ptr_t>(fxfmodule, "abs")
          .def(py::init<>())
          .def("__repr__", [](floatxfabsdata_ptr_t p) -> std::string { return FormatString("floatxfabsdata(%p)", (void*)p.get()); })
          .def_property(
              "do_abs",
              [](floatxfabsdata_ptr_t p) -> bool { return p->_doabs; }, //
              [](floatxfabsdata_ptr_t p, bool val) { p->_doabs = val; });
  type_codec->registerStdCodec<floatxfabsdata_ptr_t>(floatxfabsdata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto floatxfsmoothstepdata_type = //
      py::class_<floatxfsmoothstepdata, floatxfitembasedata, floatxfsmoothstepdata_ptr_t>(fxfmodule, "smoothstep")
          .def(py::init<>())
          .def(py::init([](float edge0, float edge1) {
            auto rval = std::make_shared<floatxfsmoothstepdata>();
            rval->_edge0 = edge0;
            rval->_edge1 = edge1;
            return rval;
          }))
          .def(
              "__repr__",
              [](floatxfsmoothstepdata_ptr_t p) -> std::string {
                return FormatString("floatxfsmoothstepdata(%p)", (void*)p.get());
              })
          .def_property(
              "do_smoothstep",
              [](floatxfsmoothstepdata_ptr_t p) -> bool { return p->_dosmoothstep; }, //
              [](floatxfsmoothstepdata_ptr_t p, bool val) { p->_dosmoothstep = val; })
          .def_property(
              "edge0",
              [](floatxfsmoothstepdata_ptr_t p) -> float { return p->_edge0; }, //
              [](floatxfsmoothstepdata_ptr_t p, float val) { p->_edge0 = val; })
          .def_property(
              "edge1",
              [](floatxfsmoothstepdata_ptr_t p) -> float { return p->_edge1; }, //
              [](floatxfsmoothstepdata_ptr_t p, float val) { p->_edge1 = val; });
  type_codec->registerStdCodec<floatxfsmoothstepdata_ptr_t>(floatxfsmoothstepdata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto floatxfquantizedata_type = //
      py::class_<floatxfquantizedata, floatxfitembasedata, floatxfquantizedata_ptr_t>(fxfmodule, "quantize")
          .def(py::init<>())
          .def(py::init([](float quantization) {
            auto rval = std::make_shared<floatxfquantizedata>();
            rval->_quantization = quantization;
            return rval;
          }))
          .def(
              "__repr__",
              [](floatxfquantizedata_ptr_t p) -> std::string {
                return FormatString("floatxfquantizedata(%p)", (void*)p.get());
              })
          .def_property(
              "do_quantize",
              [](floatxfquantizedata_ptr_t p) -> bool { return p->_doquantize; }, //
              [](floatxfquantizedata_ptr_t p, bool val) { p->_doquantize = val; })
          .def_property(
              "quantization",
              [](floatxfquantizedata_ptr_t p) -> float { return p->_quantization; }, //
              [](floatxfquantizedata_ptr_t p, float val) { p->_quantization = val; });
  type_codec->registerStdCodec<floatxfquantizedata_ptr_t>(floatxfquantizedata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto floatxfpowdata_type = //
      py::class_<floatxfpowdata, floatxfitembasedata, floatxfpowdata_ptr_t>(fxfmodule, "power")
          .def(py::init<>())
          .def(py::init([](float power) {
            auto rval = std::make_shared<floatxfpowdata>();
            rval->_power = power;
            return rval;
          }))
          .def("__repr__", [](floatxfpowdata_ptr_t p) -> std::string { return FormatString("floatxfpowdata(%p)", (void*)p.get()); })
          .def_property(
              "do_pow",
              [](floatxfpowdata_ptr_t p) -> bool { return p->_dopow; }, //
              [](floatxfpowdata_ptr_t p, bool val) { p->_dopow = val; })
          .def_property(
              "power",
              [](floatxfpowdata_ptr_t p) -> float { return p->_power; }, //
              [](floatxfpowdata_ptr_t p, float val) { p->_power = val; });
  type_codec->registerStdCodec<floatxfpowdata_ptr_t>(floatxfpowdata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto floatxfcurvedata_type = //
      py::class_<floatxfcurvedata, floatxfitembasedata, floatxfcurvedata_ptr_t>(fxfmodule, "multicurve")
          .def(py::init<>())
          .def(
              "__repr__",
              [](floatxfcurvedata_ptr_t p) -> std::string { return FormatString("floatxfcurvedata(%p)", (void*)p.get()); })
          .def_property(
              "multicurve",
              [](floatxfcurvedata_ptr_t p) -> multicurve1d_ptr_t { return p->_multicurve; }, //
              [](floatxfcurvedata_ptr_t p, multicurve1d_ptr_t curve) { p->_multicurve = curve; })
          .def_property(
              "do_curve",
              [](floatxfcurvedata_ptr_t p) -> bool { return p->_docurve; }, //
              [](floatxfcurvedata_ptr_t p, bool val) { p->_docurve = val; });
  type_codec->registerStdCodec<floatxfcurvedata_ptr_t>(floatxfcurvedata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto floatxfdata_type = //
      py::class_<floatxfdata, ::ork::Object, floatxfdata_ptr_t>(fxfmodule, "floatxfdata")
          .def(py::init<>())
          .def("__repr__", [](floatxfdata_ptr_t p) -> std::string { return FormatString("floatxfdata(%p)", (void*)p.get()); })
          .def("set", [](floatxfdata_ptr_t p, std::string name, floatxfitembasedata_ptr_t item) { //
            p->_transforms.AddSorted(name, item);
          })
          .def("append", [](floatxfdata_ptr_t p, floatxfitembasedata_ptr_t item) { //
            char name_ch = 'A' + p->_transforms.size();
            std::string name(1, name_ch);
            p->_transforms.AddSorted(name, item);
          });
  type_codec->registerStdCodec<floatxfdata_ptr_t>(floatxfdata_type);
  /////////////////////////////////////////////////////////////////////////////
  // Helper: stringify EPlugRate so the DSL bind-rate validator can compare
  // against simple names ("uniform" / "varying1" / ...) instead of enum
  // values we'd otherwise have to expose separately.
  auto rate_name = [](EPlugRate r) -> std::string {
    switch (r) {
      case EPR_EVENT:    return "event";
      case EPR_UNIFORM:  return "uniform";
      case EPR_VARYING1: return "varying1";
      case EPR_VARYING2: return "varying2";
    }
    return "unknown";
  };
  auto inplugdata_type = //
      py::class_<InPlugData, ::ork::Object, inplugdata_ptr_t>(dfgmodule, "InPlugData")
          .def_property_readonly("rate", [rate_name](inplugdata_ptr_t p) -> std::string {
            return rate_name(p->_plugrate);
          })
          .def_property_readonly("name", [](inplugdata_ptr_t p) -> std::string {
            return p->_name;
          })
          // EDGE INTROSPECTION (E1) — the flowing-data type name (edge coloring/validation),
          // the owning module name, and this input's connected producer output (redraw a
          // loaded graph). _connectedOutput is the RAW physical edge (bypass-agnostic) so the
          // introspected topology matches what serialization writes.
          .def_property_readonly("type_name", [](inplugdata_ptr_t p) -> std::string {
            return dflow_demangle(p->GetDataTypeId());
          })
          .def_property_readonly("module_name", [](inplugdata_ptr_t p) -> std::string {
            return p->_parent_module ? p->_parent_module->_name : std::string();
          })
          .def_property_readonly("is_connected", [](inplugdata_ptr_t p) -> bool {
            return p->isConnected();
          })
          .def_property_readonly("connected_output", [](inplugdata_ptr_t p) -> outplugdata_ptr_t {
            return p->_connectedOutput; // null -> None
          })
          // E.6/2.12 — generic READ of a data plug's current value (symmetry with the
          // module.inputs __setattr__ pokes); viewer-side MaterialParamSink drains use it.
          .def_property_readonly("value", [](inplugdata_ptr_t p) -> py::object {
            if (auto f = std::dynamic_pointer_cast<inplugdata<FloatPlugTraits>>(p))
              return py::cast(*(f->_value));
            if (auto i = std::dynamic_pointer_cast<inplugdata<IntPlugTraits>>(p))
              return py::cast(*(i->_value));
            if (auto v2 = std::dynamic_pointer_cast<inplugdata<Vec2fPlugTraits>>(p))
              return py::cast(*(v2->_value));
            if (auto v3 = std::dynamic_pointer_cast<inplugdata<Vec3fPlugTraits>>(p))
              return py::cast(*(v3->_value));
            if (auto v4 = std::dynamic_pointer_cast<inplugdata<Vec4fPlugTraits>>(p))
              return py::cast(*(v4->_value));
            return py::none();
          })
          .def("__repr__", [](inplugdata_ptr_t p) -> std::string {
            auto clazz     = p->objectClass();
            auto clazzname = clazz->Name();
            return FormatString("InPlugData(%p:%s)", (void*)p.get(), clazzname.c_str());
          });
  type_codec->registerStdCodec<inplugdata_ptr_t>(inplugdata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto floatxfinplugdata_type = //
      py::class_<floatxfinplugdata_t, InPlugData, floatxfinplugdata_ptr_t>(dfgmodule, "FloatXfInPlugData")
          //.def(py::init<moduledata_ptr_t, EPlugRate, const char*>())
          .def(
              "__repr__",
              [](floatxfinplugdata_ptr_t p) -> std::string {
                auto clazz     = p->objectClass();
                auto clazzname = clazz->Name();
                return FormatString("FloatXfInPlugData(%p:%s)", (void*)p.get(), clazzname.c_str());
              })
          .def_property(
              "transformer",
              [](floatxfinplugdata_ptr_t p) -> object_ptr_t { //
                return p->_transformer;
              },
              [](floatxfinplugdata_ptr_t p, object_ptr_t transformer) { //
                p->_transformer = transformer;
              });
  type_codec->registerStdCodec<floatxfinplugdata_ptr_t>(floatxfinplugdata_type);

  /////////////////////////////////////////////////////////////////////////////
  auto outplugdata_type = //
      py::class_<OutPlugData, ::ork::Object, outplugdata_ptr_t>(dfgmodule, "OutPlugData")
          .def_property_readonly("rate", [rate_name](outplugdata_ptr_t p) -> std::string {
            return rate_name(p->_plugrate);
          })
          .def_property_readonly("name", [](outplugdata_ptr_t p) -> std::string {
            return p->_name;
          })
          // EDGE INTROSPECTION (E1) — flowing-data type name, owning module name, and the
          // fan-out: every input plug this output feeds, as (module, plug) pairs.
          .def_property_readonly("type_name", [](outplugdata_ptr_t p) -> std::string {
            return dflow_demangle(p->GetDataTypeId());
          })
          .def_property_readonly("module_name", [](outplugdata_ptr_t p) -> std::string {
            return p->_parent_module ? p->_parent_module->_name : std::string();
          })
          .def_property_readonly("connections", [](outplugdata_ptr_t p) -> py::list {
            py::list rval;
            for (auto inp : p->_connections) {
              py::dict d;
              d["module"] = inp->_parent_module ? inp->_parent_module->_name : std::string();
              d["plug"]   = inp->_name;
              rval.append(d);
            }
            return rval;
          })
          .def("__repr__", [](outplugdata_ptr_t p) -> std::string {
            auto clazz     = p->objectClass();
            auto clazzname = clazz->Name();
            return FormatString("OutPlugData(%p:%s)", (void*)p.get(), clazzname.c_str());
          });
  type_codec->registerStdCodec<outplugdata_ptr_t>(outplugdata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto graphdata_type = //
      py::class_<GraphData, ::ork::Object, graphdata_ptr_t>(dfgmodule, "GraphData")
          .def_static("createShared", []() -> graphdata_ptr_t { return std::make_shared<GraphData>(); })
          .def_property(
              "cacheable",
              [](graphdata_ptr_t g) -> bool { return g->_cacheable; },
              [](graphdata_ptr_t g, bool v) { g->_cacheable = v; })
          .def_property(
              "output_node",
              [](graphdata_ptr_t g) -> std::string { return g->_output_node; },
              [](graphdata_ptr_t g, std::string v) { g->_output_node = v; })
          .def_property_readonly(
              "num_modules",
              [](graphdata_ptr_t g) -> size_t { //
                return g->numModules();
              })
          ///////////////////////////////
          // EDITOR NODE LAYOUT (E1) — GRAPH-level module-name -> canvas position. Round-trips
          // with the graph JSON and NEVER enters per-node cook hashing (that hashes each
          // MODULE's reflected state; layout rides the graph, not the module).
          .def(
              "setNodePos",
              [](graphdata_ptr_t g, std::string named, float x, float y) { //
                g->_editor_layout[named] = fvec2(x, y);
              })
          .def(
              "nodePos",
              [](graphdata_ptr_t g, std::string named) -> py::object {
                auto it = g->_editor_layout.find(named);
                if (it == g->_editor_layout.end())
                  return py::none();
                return py::cast(it->second); // fvec2
              })
          .def(
              "clearNodePos",
              [](graphdata_ptr_t g, std::string named) { //
                g->_editor_layout.erase(named);
              })
          .def_property_readonly(
              "node_layout",
              [](graphdata_ptr_t g) -> py::dict {
                py::dict rval;
                for (const auto& item : g->_editor_layout)
                  rval[py::str(item.first)] = py::cast(item.second);
                return rval;
              })
          ///////////////////////////////
          // EDGE INTROSPECTION (E1) — every typed edge in the graph as a flat descriptor
          // list: (out_module, out_plug, out_type) -> (in_module, in_plug, in_type). Walks
          // each module's input plugs following the RAW _connectedOutput (bypass-agnostic,
          // matching the serialized edge set) — enough to redraw a loaded graph.
          .def(
              "edges",
              [](graphdata_ptr_t g) -> py::list {
                py::list rval;
                for (size_t im = 0; im < g->numModules(); im++) {
                  auto m = g->module(im);
                  if (not m)
                    continue;
                  int nin = m->numInputs();
                  for (int ip = 0; ip < nin; ip++) {
                    auto inp  = m->input(ip);
                    auto outp = inp->_connectedOutput;
                    if (not outp)
                      continue;
                    auto outmod = outp->_parent_module;
                    py::dict e;
                    e["out_module"] = outmod ? outmod->_name : std::string();
                    e["out_plug"]   = outp->_name;
                    e["out_type"]   = dflow_demangle(outp->GetDataTypeId());
                    e["in_module"]  = m->_name;
                    e["in_plug"]    = inp->_name;
                    e["in_type"]    = dflow_demangle(inp->GetDataTypeId());
                    rval.append(e);
                  }
                }
                return rval;
              })
          .def(
              "createGraphInst",
              [](graphdata_ptr_t g) -> graphinst_ptr_t { //
                return GraphData::createGraphInst(g);
              })
          ///////////////////////////////
          .def("addModule", [](graphdata_ptr_t g, dgmoduledata_ptr_t m, std::string named) { GraphData::addModule(g, named, m); })
          ///////////////////////////////
          .def(
              "findModule",
              [](graphdata_ptr_t g, std::string named) -> dgmoduledata_ptr_t { //
                return g->module(named);
              })
          ///////////////////////////////
          // Find the first module in the graph that is an instance of the
          // given Python module class (the same class object you'd pass to
          // graph.create). Returns None if no module matches. Used by the
          // HyperSyn DSL lowerer to resolve REQUIRE_EXISTING context-var
          // references (e.g. Expr.ptc.unit_age → find the user's Pool).
          .def(
              "findModuleByClass",
              [](graphdata_ptr_t g, py::object module_clazz) -> dgmoduledata_ptr_t {
                // module_clazz is a pybind11-wrapped C++ class (e.g.
                // particles.Pool). Construct an instance via its
                // createShared factory to get at the rtti::Class* — same
                // discriminant the module instances carry. (We don't keep
                // the temporary; just inspect its class.)
                auto create_shared = module_clazz.attr("createShared");
                auto probe         = py::cast<dgmoduledata_ptr_t>(create_shared());
                auto target_class  = probe->objectClass();
                for (size_t i = 0; i < g->numModules(); ++i) {
                  auto m = g->module(i);
                  if (m && m->objectClass() == target_class) {
                    return m;
                  }
                }
                return nullptr;
              })
          ///////////////////////////////
          ///////////////////////////////
          .def(
              "create",
              [type_codec](graphdata_ptr_t g, std::string named, py::object module_clazz) -> dgmoduledata_ptr_t {
                auto create_shared                = module_clazz.attr("createShared");
                dgmoduledata_ptr_t typed_instance = py::cast<dgmoduledata_ptr_t>(create_shared());
                OrkAssert(typed_instance);
                GraphData::addModule(g, named, typed_instance);
                bool has_trampoline = py::hasattr(module_clazz, "__dflow_trampoline");
                if (has_trampoline) {
                  auto as_pylambda = std::dynamic_pointer_cast<PyLambdaModuleData>(typed_instance);
                  as_pylambda->assignClass(module_clazz);
                }
                return typed_instance;
              })
          ///////////////////////////////
          .def(
              "connect", [](graphdata_ptr_t g, inplugdata_ptr_t input, outplugdata_ptr_t output) { g->safeConnect(input, output); })
          ///////////////////////////////
          .def("disconnect", [](graphdata_ptr_t g, inplugdata_ptr_t input) { g->disconnect(input); })
          .def("disconnect", [](graphdata_ptr_t g, outplugdata_ptr_t output) { g->disconnect(output); })
          ///////////////////////////////
          .def("__repr__", [](graphdata_ptr_t g) -> std::string { return FormatString("GraphData(%p)", (void*)g.get()); });
  type_codec->registerStdCodec<graphdata_ptr_t>(graphdata_type);
  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////
  // ContextVariableRegistry — exposes the DSL-author-name → (module class,
  // output plug, policy) mapping populated by each module's describeX().
  // The Python Expr factory uses this to build its namespace tree at import
  // time; the lowerer uses it to resolve ContextRef leaves at generatedflow().
  py::class_<dataflow::ContextVariableRegistry::Spec>(dfgmodule, "ContextVarSpec")
      .def_readonly("output_plug_name",
                    &dataflow::ContextVariableRegistry::Spec::_output_plug_name)
      .def_property_readonly("policy",
          [](const dataflow::ContextVariableRegistry::Spec& s) -> std::string {
            switch (s._policy) {
              case dataflow::ContextVariableRegistry::SINGLETON:        return "singleton";
              case dataflow::ContextVariableRegistry::REQUIRE_EXISTING: return "require_existing";
            }
            return "unknown";
          })
      .def_property_readonly("module_class_name",
          [](const dataflow::ContextVariableRegistry::Spec& s) -> std::string {
            return s._module_class ? std::string(s._module_class->Name().c_str()) : std::string();
          })
      ;
      // Note: spec.createIn / spec.findIn were intentionally NOT added.
      // sharedFactory() is only populated by describeX → which only runs
      // during Class::InitializeClasses → only triggered by
      // finalizeInitialization() inside lev2appinit (NOT coreappinit). The
      // HyperSyn emitter needs to work in headless contexts that don't init
      // lev2's GPU, so it uses the family-package-populated Python class
      // registry (ork.dflow._context_classes) and graphdata.create /
      // graphdata.findModuleByClass directly instead.

  auto ctxvar_module = dfgmodule.def_submodule(
      "context_variables", "HyperSyn DSL context variable registry");
  ctxvar_module.def("all_names", []() -> std::vector<std::string> {
    return dataflow::ContextVariableRegistry::instance().allNames();
  });
  ctxvar_module.def("lookup",
      [](const std::string& dsl_name) -> py::object {
        auto spec_opt = dataflow::ContextVariableRegistry::instance().lookup(dsl_name);
        if (!spec_opt) return py::none();
        return py::cast(*spec_opt);
      });
  /////////////////////////////////////////////////////////////////////////////
  auto context_type = //
      py::class_<dgcontext, dgcontext_ptr_t>(dfgmodule, "DgContext")
          .def_static("createShared", []() -> dgcontext_ptr_t { return std::make_shared<dgcontext>(); })
          .def(
              "createFloatRegisterBlock",
              [](dgcontext_ptr_t ctx, std::string blockname, int count) -> dgregisterblock_ptr_t {
                return ctx->createRegisters<float>(blockname, count);
              })
          .def("createVec2RegisterBlock", [](dgcontext_ptr_t ctx, std::string blockname, int count) -> dgregisterblock_ptr_t {
            return ctx->createRegisters<fvec2>(blockname, count);
          })
          .def("createVec3RegisterBlock", [](dgcontext_ptr_t ctx, std::string blockname, int count) -> dgregisterblock_ptr_t {
            return ctx->createRegisters<fvec3>(blockname, count);
          })
          .def("createVec4RegisterBlock", [](dgcontext_ptr_t ctx, std::string blockname, int count) -> dgregisterblock_ptr_t {
            return ctx->createRegisters<fvec4>(blockname, count);
          });
  type_codec->registerStdCodec<dgcontext_ptr_t>(context_type);
  /////////////////////////////////////////////////////////////////////////////
  auto sorter_type = //
      py::class_<DgSorter, dgsorter_ptr_t>(dfgmodule, "DgSorter")
          .def_static(
              "createShared",
              [](graphdata_ptr_t gdata, dgcontext_ptr_t ctx) -> dgsorter_ptr_t {
                return std::make_shared<DgSorter>(gdata.get(), ctx);
              })
          .def("generateTopology", [](dgsorter_ptr_t sorter) -> topology_ptr_t { return sorter->generateTopology(); });
  type_codec->registerStdCodec<dgsorter_ptr_t>(sorter_type);
  /////////////////////////////////////////////////////////////////////////////
  auto topology_type = //
      py::class_<Topology, topology_ptr_t>(dfgmodule, "Topology").def("__repr__", [](topology_ptr_t t) -> std::string {
        return FormatString("Topology(%p)", (void*)t.get());
      });
  type_codec->registerStdCodec<topology_ptr_t>(topology_type);
  /////////////////////////////////////////////////////////////////////////////
  auto regblock_type = //
      py::class_<DgRegisterBlock, dgregisterblock_ptr_t>(dfgmodule, "DgRegisterBlock")
          .def("__repr__", [](dgregisterblock_ptr_t b) -> std::string {
            fxstring<256> fxs;
            fxs.format("DgRegisterBlock(%p)", (void*)b.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<dgregisterblock_ptr_t>(regblock_type);
  /////////////////////////////////////////////////////////////////////////////
  auto graphinst_type = //
      py::class_<GraphInst, graphinst_ptr_t>(dfgmodule, "GraphInst")
          .def("bindTopology", [](graphinst_ptr_t g, topology_ptr_t t) { g->updateTopology(t); })
          .def("compute", [](graphinst_ptr_t g, ui::updatedata_ptr_t updata) { g->compute(updata); })
          // Restore the graphinst to a just-instantiated state — every module's
          // onReset hook fires (Globals clears its first-compute flag so the
          // next compute recaptures a fresh per-instance time origin; particle
          // pools release live particles; future modules clear their own
          // per-instance state). Used by ECS slot recycling.
          .def("reset", [](graphinst_ptr_t g) { g->reset(); })
          .def_property(
              "impl",
              [](graphinst_ptr_t g) -> py::object { //
                if (auto as_pyobj = g->_impl.tryAs<py::object>()) {
                  return as_pyobj.value();
                } else {
                  return py::none();
                }
              },
              [](graphinst_ptr_t g, py::object impl) { //
                g->_impl.set<py::object>(impl);
              })
          .def("__repr__", [](graphinst_ptr_t g) -> std::string { return FormatString("GraphInst(%p)", (void*)g.get()); });
  type_codec->registerStdCodec<graphinst_ptr_t>(graphinst_type);
  /////////////////////////////////////////////////////////////////////////////
  // NODE-TYPE ENUMERATION + CLASS-LEVEL PLUG SCHEMA (E1)
  //
  // reflection-first: the rtti class tree IS the node registry. moduleClasses()
  // walks the DgModuleData subtree (concrete/factory-bearing classes only) exposing
  // reflected PROPERTIES + their annotations. plugSpec() returns a class's plug schema
  // WITHOUT the caller instantiating a module — derived internally by building ONE
  // scratch ModuleData via the reflection factory (which runs reshapeIOs), reading its
  // _inputs/_outputs, then discarding it. Result cached per class (drift-proof: no
  // hand-maintained table; schema always tracks the code).
  /////////////////////////////////////////////////////////////////////////////

  // an ObjectProperty's scalar annotations as a python dict (non-scalar annotations —
  // e.g. the reshapeIOs functor — are skipped).
  auto anno_to_py = [](reflect::ObjectProperty* prop) -> py::dict {
    py::dict rval;
    for (auto& item : prop->_annotations) {
      const auto& key = item.first;
      auto val        = item.second;
      std::string keystr = key.c_str();
      if (auto s = val.tryAs<ConstString>())
        rval[py::str(keystr)] = std::string(s.value().c_str());
      else if (auto ss = val.tryAs<std::string>())
        rval[py::str(keystr)] = ss.value();
      else if (auto b = val.tryAs<bool>())
        rval[py::str(keystr)] = b.value();
      else if (auto i = val.tryAs<int>())
        rval[py::str(keystr)] = i.value();
      else if (auto f = val.tryAs<float>())
        rval[py::str(keystr)] = f.value();
      else if (auto d = val.tryAs<double>())
        rval[py::str(keystr)] = d.value();
      // else: non-scalar annotation — not surfaced.
    }
    return rval;
  };

  // coarse reflected-property type label for editor widget selection.
  auto prop_type_name = [](reflect::ObjectProperty* prop) -> std::string {
    if (dynamic_cast<reflect::ITyped<int>*>(prop))          return "int";
    if (dynamic_cast<reflect::ITyped<float>*>(prop))        return "float";
    if (dynamic_cast<reflect::ITyped<std::string>*>(prop))  return "string";
    if (dynamic_cast<reflect::ITyped<bool>*>(prop))         return "bool";
    if (dynamic_cast<reflect::ITypedArray<int>*>(prop))     return "int[]";
    if (dynamic_cast<reflect::ITypedArray<float>*>(prop))   return "float[]";
    if (dynamic_cast<reflect::IObjectArray*>(prop))         return "object[]";
    if (dynamic_cast<reflect::IObjectMap*>(prop))           return "object_map";
    return "other";
  };

  dfgmodule.def("moduleClasses", [anno_to_py, prop_type_name]() -> py::list {
    py::list rval;
    auto* base = rtti::Class::FindClass("dflow::DgModuleData");
    if (not base)
      return rval;
    std::stack<rtti::Class*> stk; // circular sibling lists -> iterative DFS
    stk.push(base);
    while (not stk.empty()) {
      auto* clazz = stk.top();
      stk.pop();
      if (clazz != base and clazz->hasFactory()) { // concrete/instantiable only (abstract bases skipped)
        py::dict d;
        std::string name = clazz->Name().c_str();
        d["name"]        = name;
        auto pos         = name.find("::"); // family tag = reflected-name namespace prefix (drift-proof)
        d["family"]      = (pos != std::string::npos) ? name.substr(0, pos) : std::string();
        py::list props;
        std::set<std::string> seen; // own props win over inherited on name collision
        auto* objclazz                   = dynamic_cast<object::ObjectClass*>(clazz);
        const reflect::Description* desc = objclazz ? &objclazz->Description() : nullptr;
        while (desc) {
          for (auto pitem : desc->properties()) {
            reflect::ObjectProperty* prop = pitem.second;
            std::string pn                = prop->_name;
            if (seen.count(pn))
              continue;
            seen.insert(pn);
            py::dict pd;
            pd["name"]        = pn;
            pd["type"]        = prop_type_name(prop);
            pd["annotations"] = anno_to_py(prop);
            props.append(pd);
          }
          desc = desc->parent();
        }
        d["properties"] = props;
        rval.append(d);
      }
      rtti::Class* first_child = clazz->FirstChild();
      rtti::Class* child       = first_child;
      while (child) {
        stk.push(child);
        child = (child->NextSibling() == first_child) ? nullptr : child->NextSibling();
      }
    }
    return rval;
  });

  // per-class plug schema, lazily derived + cached. A scratch instance is built ONCE per
  // class and discarded; failures are reported loudly and cached as "no schema" so the
  // enumeration never crashes and never retries a faulting class.
  struct PlugEntry {
    std::string _name, _type, _rate;
  };
  struct ModuleSchema {
    std::vector<PlugEntry> _inputs, _outputs;
    bool _built  = false;
    bool _failed = false;
  };
  auto schema_cache = std::make_shared<std::map<std::string, ModuleSchema>>();

  dfgmodule.def("plugSpec", [schema_cache, rate_name](const std::string& classname) -> py::object {
    auto& cache = *schema_cache;
    auto it     = cache.find(classname);
    if (it == cache.end()) {
      ModuleSchema sch;
      auto* clazz = rtti::Class::FindClass(classname);
      if (not clazz or not clazz->hasSharedFactory()) {
        printf("dflow.plugSpec: class <%s> is abstract/unregistered (no shared factory) — skipping\n", classname.c_str());
        sch._failed = true;
      } else {
        try {
          auto castable = clazz->sharedFactory()(); // runs createShared -> reshapeIOs
          auto mod      = std::dynamic_pointer_cast<DgModuleData>(castable);
          if (not mod) {
            printf("dflow.plugSpec: scratch instance of <%s> is not a DgModuleData — skipping\n", classname.c_str());
            sch._failed = true;
          } else {
            for (auto inp : mod->_inputs)
              sch._inputs.push_back({inp->_name, dflow_demangle(inp->GetDataTypeId()), rate_name(inp->_plugrate)});
            for (auto outp : mod->_outputs)
              sch._outputs.push_back({outp->_name, dflow_demangle(outp->GetDataTypeId()), rate_name(outp->_plugrate)});
            sch._built = true;
          }
        } catch (const std::exception& e) {
          printf("dflow.plugSpec: scratch construction FAILED for class <%s>: %s — skipping\n", classname.c_str(), e.what());
          sch._failed = true;
        } catch (...) {
          printf("dflow.plugSpec: scratch construction FAILED for class <%s> (unknown) — skipping\n", classname.c_str());
          sch._failed = true;
        }
      }
      it = cache.emplace(classname, sch).first;
    }
    const auto& sch = it->second;
    if (not sch._built)
      return py::none();
    auto emit = [](const std::vector<PlugEntry>& v) -> py::list {
      py::list l;
      for (auto& e : v) {
        py::dict d;
        d["name"] = e._name;
        d["type"] = e._type;
        d["rate"] = e._rate;
        l.append(d);
      }
      return l;
    };
    py::dict rval;
    rval["class"]   = classname;
    rval["inputs"]  = emit(sch._inputs);
    rval["outputs"] = emit(sch._outputs);
    return rval;
  });
  /////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
