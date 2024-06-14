////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_scenegraph(py::module& module_ecs) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SceneGraphComponentData, ComponentData, sgcd_ptr_t>(module_ecs, "SceneGraphComponentData")
      .def(
          "__repr__",
          [](const sgcd_ptr_t& sgcd) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::SceneGraphComponentData(%p)", sgcd.get());
            return fxs.c_str();
          })
      .def("declareNodeOnLayer", [](sgcd_ptr_t sgcd, std::string nodename, lev2::drawabledata_ptr_t d, std::string l) {
        sgcd->createNodeOnLayer(nodename, d, l);
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SceneGraphSystemData, SystemData, sgsys_ptr_t>(module_ecs, "SceneGraphSystemData")
      .def(
          "__repr__",
          [](const sgsys_ptr_t& sgsys) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::SceneGraphSystemData(%p)", sgsys.get());
            return fxs.c_str();
          })
      .def("declareLayer", [](const sgsys_ptr_t& sgsys, std::string name) { sgsys->declareLayer(name); });
  /////////////////////////////////////////////////////////////////////////////////
  auto sgsys_type =
      py::class_<pysgsystem_ptr_t>(module_ecs, "SceneGraphSystem")
          .def(
              "__repr__",
              [](const pysgsystem_ptr_t& sgsys) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::SceneGraphSystem(%p)", sgsys.get());
                return fxs.c_str();
              })
          .def_property_readonly(
              "defaultLayer", [](pysgsystem_ptr_t sgsys) -> lev2::scenegraph::layer_ptr_t { return sgsys->_default_layer; })
          .def_property_readonly("defaultCamera", [](pysgsystem_ptr_t sgsys) -> lev2::cameradata_ptr_t { return sgsys->_camera; });
  type_codec->registerRawPtrCodec<pysgsystem_ptr_t, SceneGraphSystem*>(sgsys_type);
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_scenegraph(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs