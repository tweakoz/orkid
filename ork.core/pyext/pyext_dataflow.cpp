///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////
  auto dgmoduledata_type = //
      py::class_<DgModuleData, dgmoduledata_ptr_t>(dfgmodule, "DgModuleData")
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
  auto inplugdata_type = //
      py::class_<InPlugData, inplugdata_ptr_t>(dfgmodule, "InPlugData").def("__repr__", [](inplugdata_ptr_t p) -> std::string {
        auto clazz     = p->objectClass();
        auto clazzname = clazz->Name();
        return FormatString("InPlugData(%p:%s)", (void*)p.get(), clazzname.c_str());
      });
  type_codec->registerStdCodec<inplugdata_ptr_t>(inplugdata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto outplugdata_type = //
      py::class_<OutPlugData, outplugdata_ptr_t>(dfgmodule, "OutPlugData").def("__repr__", [](outplugdata_ptr_t p) -> std::string {
        auto clazz     = p->objectClass();
        auto clazzname = clazz->Name();
        return FormatString("OutPlugData(%p:%s)", (void*)p.get(), clazzname.c_str());
      });
  type_codec->registerStdCodec<outplugdata_ptr_t>(outplugdata_type);
  /////////////////////////////////////////////////////////////////////////////
  auto graphdata_type = //
      py::class_<GraphData, graphdata_ptr_t>(dfgmodule, "GraphData")
          .def_static("createShared", []() -> graphdata_ptr_t { return std::make_shared<GraphData>(); })
          .def(
              "createGraphInst",
              [](graphdata_ptr_t g) -> graphinst_ptr_t { //
                return GraphData::createGraphInst(g);
              })
          ///////////////////////////////
          .def("addModule", [](graphdata_ptr_t g, dgmoduledata_ptr_t m, std::string named) { GraphData::addModule(g, named, m); })
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
          .def_static("createShared", []() -> dgcontext_ptr_t { return std::make_shared<dgcontext>(); })
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
