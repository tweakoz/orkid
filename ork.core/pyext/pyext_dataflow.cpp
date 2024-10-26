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
///////////////////////////////////////////////////////////////////////////////
namespace ork {
using namespace dataflow;
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
                auto m         = proxy->_module;
                auto input     = m->inputNamed(key);
                auto clazzname = input->objectClass()->Name();
                if (0)
                  printf("inputNamed<%s:%s>\n", key.c_str(), clazzname.c_str());
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
                OrkAssert(input);
                auto decoded_value = type_codec->decode(value);

                ////////////////////////////////////////////////
                // TODO a cleaner way to do this would be nice..
                //   lots of permutations
                ////////////////////////////////////////////////

                auto set_float = [input](float value) {
                  auto finplug = std::dynamic_pointer_cast<inplugdata<FloatPlugTraits>>(input);
                  if (finplug) {
                    finplug->setValue(value);
                  } else {
                    auto xfinplug = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(input);
                    if (xfinplug) {
                      xfinplug->setValue(value);
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

                if (auto as_float = decoded_value.tryAs<float>()) {
                  set_float(as_float.value());
                } else if (auto as_int = decoded_value.tryAs<int>()) {
                  set_float(as_int.value());
                } else if (auto as_fvec3 = decoded_value.tryAs<fvec3>()) {
                  set_fvec3(as_fvec3.value());
                } else if (auto as_fvec3_ptr = decoded_value.tryAs<fvec3_ptr_t>()) {
                  set_fvec3(*as_fvec3_ptr.value());
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
              });
  type_codec->registerStdCodec<moduledata_ptr_t>(moduledata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto dgmoduledata_type = //
      py::class_<DgModuleData, ModuleData, dgmoduledata_ptr_t>(dfgmodule, "DgModuleData")
          .def_static("createShared", [] -> dgmoduledata_ptr_t { return DgModuleData::createShared(); })
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
          .def_static("createShared", [] -> lambdamoduledata_ptr_t { return LambdaModuleData::createShared(); })
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
          .def_static("__dflow_trampoline", [] -> bool { return true; })
          .def_static("createShared", [] -> pylambdamoduledata_ptr_t { return std::make_shared<PyLambdaModuleData>(); })
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
  auto inplugdata_type = //
      py::class_<InPlugData, ::ork::Object, inplugdata_ptr_t>(dfgmodule, "InPlugData")
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
          .def("__repr__", [](outplugdata_ptr_t p) -> std::string {
            auto clazz     = p->objectClass();
            auto clazzname = clazz->Name();
            return FormatString("OutPlugData(%p:%s)", (void*)p.get(), clazzname.c_str());
          });
  type_codec->registerStdCodec<outplugdata_ptr_t>(outplugdata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto graphdata_type = //
      py::class_<GraphData, ::ork::Object, graphdata_ptr_t>(dfgmodule, "GraphData")
          .def_static("createShared", [] -> graphdata_ptr_t { return std::make_shared<GraphData>(); })
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
  auto context_type = //
      py::class_<dgcontext, dgcontext_ptr_t>(dfgmodule, "DgContext")
          .def_static("createShared", [] -> dgcontext_ptr_t { return std::make_shared<dgcontext>(); })
          .def(
              "createFloatRegisterBlock",
              [](dgcontext_ptr_t ctx, std::string blockname, int count) -> dgregisterblock_ptr_t {
                return ctx->createRegisters<float>(blockname, count);
              })
          .def("createVec3RegisterBlock", [](dgcontext_ptr_t ctx, std::string blockname, int count) -> dgregisterblock_ptr_t {
            return ctx->createRegisters<fvec3>(blockname, count);
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
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
