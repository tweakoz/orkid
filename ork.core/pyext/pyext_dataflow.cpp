///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/plug_data.h>
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
  auto lambdamoduledata_type = //
      py::class_<LambdaModuleData, DgModuleData, lambdamoduledata_ptr_t>(dfgmodule, "LambdaModule")
          .def_static("createShared", []() -> lambdamoduledata_ptr_t { return LambdaModuleData::createShared(); })
          .def(
              "onCompute",
              [](lambdamoduledata_ptr_t m, py::object pylambda) { //
                m->_computeLambda = [m,pylambda](graphinst_ptr_t gi, //
                                                 ui::updatedata_ptr_t ud) { //
                  py::gil_scoped_acquire acquire;
                  pylambda(m,gi,ud);
                };
              })
          .def(
              "onLink",
              [](lambdamoduledata_ptr_t m, py::object pylambda) { //
                m->_linkLambda = [m,pylambda](graphinst_ptr_t gi) { //
                  py::gil_scoped_acquire acquire;
                  pylambda(m,gi);
                };
              })
          .def("__repr__", [](lambdamoduledata_ptr_t m) -> std::string {
            return FormatString("LambdaModuleData(%p)", (void*)m.get());
          });
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
          .def("createGraphInst", [](graphdata_ptr_t g) -> graphinst_ptr_t { //
            return GraphData::createGraphInst(g);
          })
          .def("addModule", [](graphdata_ptr_t g, dgmoduledata_ptr_t m, std::string named) { GraphData::addModule(g, named, m); })
          .def(
              "safeConnect",
              [](graphdata_ptr_t g, inplugdata_ptr_t input, outplugdata_ptr_t output) { g->safeConnect(input, output); })
          .def("disconnect", [](graphdata_ptr_t g, inplugdata_ptr_t input) { g->disconnect(input); })
          .def("disconnect", [](graphdata_ptr_t g, outplugdata_ptr_t output) { g->disconnect(output); })
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
          .def_property("impl", 
              [](graphinst_ptr_t g ) -> py::object { //
                if(auto as_pyobj = g->_impl.tryAs<py::object>()){
                  return as_pyobj.value();
                }
                else{
                  return py::none();
                }
              },
              [](graphinst_ptr_t g, py::object impl ) { //
                  g->_impl.set<py::object>(impl);
              })
          .def("__repr__", [](graphinst_ptr_t g) -> std::string { return FormatString("GraphInst(%p)", (void*)g.get()); });
  type_codec->registerStdCodec<graphinst_ptr_t>(graphinst_type);
  /////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
