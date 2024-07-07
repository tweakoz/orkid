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
  auto nd_type = py::class_<NodeDef, nodedef_ptr_t>(module_ecs, "NodeDef")
                     .def(
                         "__repr__",
                         [](nodedef_ptr_t ndef) -> std::string {
                           fxstring<256> fxs;
                           fxs.format("ecs::NodeDef(%p)", ndef.get());
                           return fxs.c_str();
                         })
                     .def_property(
                         "nodename",
                         [](nodedef_ptr_t ndef) -> std::string { return ndef->_nodename; },
                         [](nodedef_ptr_t ndef, std::string val) { ndef->_nodename = val; })
                     .def_property(
                         "layername",
                         [](nodedef_ptr_t ndef) -> std::string { return ndef->_layername; },
                         [](nodedef_ptr_t ndef, std::string val) { ndef->_layername = val; })
                     .def_property(
                         "drawabledata",
                         [](nodedef_ptr_t ndef) -> lev2::drawabledata_ptr_t { return ndef->_drawabledata; },
                         [](nodedef_ptr_t ndef, lev2::drawabledata_ptr_t val) { ndef->_drawabledata = val; })
                     .def_property(
                         "transform",
                         [](nodedef_ptr_t ndef) -> decompxf_ptr_t { return ndef->_transform; },
                         [](nodedef_ptr_t ndef, decompxf_ptr_t val) { ndef->_transform = val; })
                     .def_property(
                         "modcolor",
                         [](nodedef_ptr_t ndef) -> fvec4 { return ndef->_modcolor; },
                         [](nodedef_ptr_t ndef, fvec4 val) { ndef->_modcolor = val; });
  type_codec->registerStdCodec<nodedef_ptr_t>(nd_type);

  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SceneGraphComponentData, ComponentData, sgcomponentdata_ptr_t>(module_ecs, "SceneGraphComponentData")
      .def(
          "__repr__",
          [](const sgcomponentdata_ptr_t& sgcd) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::SceneGraphComponentData(%p)", sgcd.get());
            return fxs.c_str();
          })
      .def(
          "declareNodeOnLayer",
          [](sgcomponentdata_ptr_t sgcd, py::kwargs kwargs) { //
            auto ndef = std::make_shared<NodeDef>();

            if (kwargs.contains("name")) {
              ndef->_nodename = kwargs["name"].cast<std::string>();
            }
            if (kwargs.contains("drawable")) {
              ndef->_drawabledata = kwargs["drawable"].cast<lev2::drawabledata_ptr_t>();
            }
            if (kwargs.contains("layer")) {
              ndef->_layername = kwargs["layer"].cast<std::string>();
            }
            if (kwargs.contains("transform")) {
              ndef->_transform = kwargs["transform"].cast<decompxf_ptr_t>();
            }
            if (kwargs.contains("modcolor")) {
              ndef->_modcolor = kwargs["modcolor"].cast<fvec4>();
            }
            sgcd->declareNodeOnLayer(ndef);
          },
          R"doc(
        Declares a node on a specified layer (scoped to component).

        Parameters:
        name (str): The name of the node.
        drawable (lev2::drawabledata_ptr_t): The drawable data associated with the node.
        layer (str): The name of the layer.
        transform (decompxf_ptr_t, optional): The transformation to be applied. Defaults to None.
     )doc")
      .def(
          "declareNodeInstance",
          [](sgcomponentdata_ptr_t sgcd, ::ork::lev2::scenegraph::node_instance_data_ptr_t nid) { //
            sgcd->_INSTANCEDATA = nid;
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SceneGraphSystemData, SystemData, sgsystemdata_ptr_t>(module_ecs, "SceneGraphSystemData")
      .def(
          "__repr__",
          [](sgsystemdata_ptr_t sgsys) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::SceneGraphSystemData(%p)", sgsys.get());
            return fxs.c_str();
          })
      .def(
          "declareLayer",
          [](sgsystemdata_ptr_t sgsys, std::string name) { sgsys->declareLayer(name); },
          R"doc(
        Declares a layer into the SceneGraphsSystems' scenegraph.

        Parameters:
        name (str): The name of the layer.
     )doc")
      .def(
          "declareParams",
          [type_codec](sgsystemdata_ptr_t sgsys, py::dict param_dict) {
            for (auto& [key, value] : param_dict) {
              auto key_str     = key.cast<std::string>();
              auto val_obj     = py::reinterpret_borrow<py::object>(value);
              auto val_decoded = type_codec->decode(val_obj);
              sgsys->setInternalSceneParam(key_str, val_decoded);
            }
          })
      .def(
          "declareNodeOnLayer",
          [](sgsystemdata_ptr_t sgsys, py::kwargs kwargs) { //
            auto ndef = std::make_shared<NodeDef>();

            if (kwargs.contains("name")) {
              ndef->_nodename = kwargs["name"].cast<std::string>();
            }
            if (kwargs.contains("drawable")) {
              ndef->_drawabledata = kwargs["drawable"].cast<lev2::drawabledata_ptr_t>();
            }
            if (kwargs.contains("layer")) {
              ndef->_layername = kwargs["layer"].cast<std::string>();
            }
            if (kwargs.contains("transform")) {
              ndef->_transform = kwargs["transform"].cast<decompxf_ptr_t>();
            }
            if (kwargs.contains("modcolor")) {
              ndef->_modcolor = kwargs["modcolor"].cast<fvec4>();
            }
            sgsys->declareNodeOnLayer(ndef);
          },
          R"doc(
        Declares a node on a specified layer (scoped to system).

        Parameters:
        name (str): The name of the node.
        drawable (lev2::drawabledata_ptr_t): The drawable data associated with the node.
        layer (str): The name of the layer.
        transform (decompxf_ptr_t, optional): The transformation to be applied. Defaults to None.
     )doc");
  /////////////////////////////////////////////////////////////////////////////////
  auto sgsys_type =
      py::class_<pysgsystem_ptr_t>(module_ecs, "SceneGraphSystem")
          .def(
              "__repr__",
              [](pysgsystem_ptr_t sgsys) -> std::string {
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