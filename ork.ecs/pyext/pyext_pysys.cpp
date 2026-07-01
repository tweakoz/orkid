////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/pysys/PythonComponent.h>
#include <ork/ecs/datatable.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_pysys(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto pyc_type =
      py::class_<PythonComponentData, ComponentData, pycompdata_ptr_t>(module_ecs, "PythonComponentData")
          .def(
              "__repr__",
              [](pycompdata_ptr_t pycdata) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::PythonComponentData(%p)", pycdata.get());
                return fxs.c_str();
              })
      .def(
          "declareNodeInstance",
          [](pycompdata_ptr_t pycdata, ::ork::lev2::scenegraph::node_instance_data_ptr_t nid) { //
            pycdata->_INSTANCEDATA = nid;
          })
      .def_property(
          "scriptFile",
          [](pycompdata_ptr_t pycdata) -> std::string { //
            return pycdata->GetPath().c_str();
          },
          [](pycompdata_ptr_t pycdata, std::string path) { //
            pycdata->SetPath(file::Path(path.c_str()));
          });
  type_codec->registerStdCodec<pycompdata_ptr_t>(pyc_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pysys_type = py::class_<PythonSystemData, SystemData, pysysdata_ptr_t>(module_ecs, "PythonSystemData")
                          .def(
                              "__repr__",
                              [](pysysdata_ptr_t sysdata) -> std::string {
                                fxstring<256> fxs;
                                fxs.format("ecs::PythonSystemData(%p)", sysdata.get());
                                return fxs.c_str();
                              })
                          .def_property(
                              "systemUpdateScript",
                              [=](pysysdata_ptr_t sysdata) -> std::string { //
                                return sysdata->_sceneScriptPath.c_str();
                              },
                              [=](pysysdata_ptr_t sysdata, std::string path) { //
                                sysdata->_sceneScriptPath = path;
                              })
                          .def_property(
                              "systemScripts",
                              [=](pysysdata_ptr_t sysdata) -> std::vector<std::string> { // ADDITIONAL composable scripts
                                std::vector<std::string> out;
                                const std::string& csv = sysdata->_sceneScriptPathsCSV;
                                size_t start = 0;
                                while (start < csv.size()) {
                                  size_t sep      = csv.find(';', start);
                                  std::string one = (sep == std::string::npos) ? csv.substr(start) : csv.substr(start, sep - start);
                                  start           = (sep == std::string::npos) ? csv.size() : sep + 1;
                                  if (not one.empty())
                                    out.push_back(one);
                                }
                                return out;
                              },
                              [=](pysysdata_ptr_t sysdata, std::vector<std::string> paths) {
                                std::string csv;
                                for (auto& p : paths) {
                                  if (not csv.empty())
                                    csv += ";";
                                  csv += p;
                                }
                                sysdata->_sceneScriptPathsCSV = csv;
                              });
  type_codec->registerStdCodec<pysysdata_ptr_t>(pysys_type);
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_system(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
