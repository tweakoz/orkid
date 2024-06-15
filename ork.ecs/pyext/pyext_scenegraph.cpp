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
      .def(
          "declareNodeOnLayer",
          [](sgcd_ptr_t sgcd, py::kwargs kwargs) { //
            decompxf_ptr_t xf = nullptr;
            std::string nodename;
            lev2::drawabledata_ptr_t d;
            std::string l;
            if (kwargs.contains("name")) {
              nodename = kwargs["name"].cast<std::string>();
            }
            if (kwargs.contains("drawable")) {
              d = kwargs["drawable"].cast<lev2::drawabledata_ptr_t>();
            }
            if (kwargs.contains("layer")) {
              l = kwargs["layer"].cast<std::string>();
            }
            if (kwargs.contains("transform")) {
              xf = kwargs["transform"].cast<decompxf_ptr_t>();
            }
            sgcd->declareNodeOnLayer(nodename, d, l, xf);
          },
          R"doc(
        Declares a node on a specified layer.

        Parameters:
        name (str): The name of the node.
        drawable (lev2::drawabledata_ptr_t): The drawable data associated with the node.
        layer (str): The name of the layer.
        transform (decompxf_ptr_t, optional): The transformation to be applied. Defaults to None.
     )doc");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SceneGraphSystemData, SystemData, sgsys_ptr_t>(module_ecs, "SceneGraphSystemData")
      .def(
          "__repr__",
          [](const sgsys_ptr_t& sgsys) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::SceneGraphSystemData(%p)", sgsys.get());
            return fxs.c_str();
          })
      .def(
          "declareLayer",
          [](const sgsys_ptr_t& sgsys, std::string name) { sgsys->declareLayer(name); },
          R"doc(
        Declares a layer into the SceneGraphsSystems' scenegraph.

        Parameters:
        name (str): The name of the layer.
     )doc")
     .def("declareParams", [type_codec](const sgsys_ptr_t& sgsys, py::dict param_dict) {
        for (auto& [key, value] : param_dict) {
          auto key_str = key.cast<std::string>();
          auto val_obj = py::reinterpret_borrow<py::object>(value);
          auto val_decoded = type_codec->decode(val_obj);
          sgsys->setInternalSceneParam(key_str,val_decoded);
        }
     });
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