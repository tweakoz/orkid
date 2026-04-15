////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////
using ctx_t = ork::python::unmanaged_ptr<::ork::lev2::Context>;
///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_scenegraph(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
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
  auto snid_type = py::class_<SceneGraphNodeItemData, ork::Object, sgnodeitemdata_ptr_t>(module_ecs, "SceneGraphNodeItemData")
      .def_property(
          "nodename",
          [](sgnodeitemdata_ptr_t nid) -> std::string { return nid->_nodename; },
          [](sgnodeitemdata_ptr_t nid, std::string val) { nid->_nodename = val; })
      .def_property(
          "layername",
          [](sgnodeitemdata_ptr_t nid) -> std::string { return nid->_layername; },
          [](sgnodeitemdata_ptr_t nid, std::string val) { nid->_layername = val; })
      .def_property(
          "drawabledata",
          [](sgnodeitemdata_ptr_t nid) -> lev2::drawabledata_ptr_t { return nid->_drawabledata; },
          [](sgnodeitemdata_ptr_t nid, lev2::drawabledata_ptr_t val) { nid->_drawabledata = val; })
      .def_property(
          "transform",
          [](sgnodeitemdata_ptr_t nid) -> decompxf_ptr_t { return nid->_xfoverride; },
          [](sgnodeitemdata_ptr_t nid, decompxf_ptr_t val) { nid->_xfoverride = val; })
      .def_property(
          "modcolor",
          [](sgnodeitemdata_ptr_t nid) -> fvec4 { return nid->_modcolor; },
          [](sgnodeitemdata_ptr_t nid, fvec4 val) { nid->_modcolor = val; })
      .def_property(
          "drawableClassName",
          [](sgnodeitemdata_ptr_t nid) -> std::string {
            if (nid->_drawabledata) {
              return nid->_drawabledata->GetClass()->Name();
            }
            return std::string("(none)");
          },
          [](sgnodeitemdata_ptr_t nid, std::string classname) {
            auto* clazz = (ork::object::ObjectClass*) ork::rtti::Class::FindClass(classname);
            if (clazz && clazz->hasFactory()) {
              nid->_drawabledata = std::dynamic_pointer_cast<lev2::DrawableData>(clazz->createShared());
            }
          });
  type_codec->registerStdCodec<sgnodeitemdata_ptr_t>(snid_type);

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
            if (kwargs.contains("layers")) {
              auto layer_list = kwargs["layers"].cast<py::list>();
              for (auto item : layer_list) {
                ndef->_multilayers.push_back(item.cast<std::string>());
              }
              //ndef->_layername
            }
            if (kwargs.contains("transform")) {
              ndef->_transform = kwargs["transform"].cast<decompxf_ptr_t>();
            }
            if (kwargs.contains("modcolor")) {
              ndef->_modcolor = kwargs["modcolor"].cast<fvec4>();
            }
            if (kwargs.contains("skip_auto_dpp")) {
              ndef->_skipAutoDepthPrepass = kwargs["skip_auto_dpp"].cast<bool>();
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
          })
      .def_property_readonly(
          "nodedatas",
          [](sgcomponentdata_ptr_t sgcd) -> py::dict {
            py::dict result;
            for (auto& [key, val] : sgcd->_nodedatas) {
              result[py::str(key)] = val;
            }
            return result;
          })
      .def(
          "addNode",
          [](sgcomponentdata_ptr_t sgcd, std::string name, std::string layer) -> sgnodeitemdata_ptr_t {
            auto nid = std::make_shared<SceneGraphNodeItemData>();
            nid->_nodename = name;
            nid->_layername = layer;
            sgcd->_nodedatas[name] = nid;
            return nid;
          })
      .def(
          "removeNode",
          [](sgcomponentdata_ptr_t sgcd, std::string name) {
            sgcd->_nodedatas.erase(name);
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
      .def_property_readonly(
          "declaredLayers",
          [](sgsystemdata_ptr_t sgsys) -> std::vector<std::string> {
            return sgsys->declaredLayers();
          })
      .def(
          "clearDeclaredLayers",
          [](sgsystemdata_ptr_t sgsys) {
            sgsys->clearDeclaredLayers();
          })
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
            if (kwargs.contains("layers")) {
              auto layer_list = kwargs["layers"].cast<py::list>();
              for (auto item : layer_list) {
                ndef->_multilayers.push_back(item.cast<std::string>());
              }
            }
            if (kwargs.contains("transform")) {
              ndef->_transform = kwargs["transform"].cast<decompxf_ptr_t>();
            }
            if (kwargs.contains("modcolor")) {
              ndef->_modcolor = kwargs["modcolor"].cast<fvec4>();
            }
            if (kwargs.contains("skip_auto_dpp")) {
              ndef->_skipAutoDepthPrepass = kwargs["skip_auto_dpp"].cast<bool>();
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
      py::class_<SceneGraphSystem,pysgsystem_ptr_t>(module_ecs, "SceneGraphSystem")
          .def(
              "__repr__",
              [](pysgsystem_ptr_t sgsys) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::SceneGraphSystem(%p)", sgsys.get());
                return fxs.c_str();
              })
          .def_property_readonly(
              "defaultLayer", [](pysgsystem_ptr_t sgsys) -> lev2::scenegraph::layer_ptr_t { return sgsys->_default_layer; })
          .def_property_readonly("defaultCamera", [](pysgsystem_ptr_t sgsys) -> lev2::cameradata_ptr_t { return sgsys->_camera; })
          .def_property_readonly(
              "scene",
              [](pysgsystem_ptr_t sgsys) -> lev2::scenegraph::scene_ptr_t {
                return sgsys->_scene;
              })
          .def(
              "reloadDrawableData",
              [](pysgsystem_ptr_t sgsys, lev2::drawabledata_ptr_t data) {
                sgsys->reloadDrawableData(data);
              })
          .def(
              "processRenderOps",
              [](pysgsystem_ptr_t sgsys) {
                sgsys->processRenderOps();
              })
          .def(
              "initializeForEditMode",
              [](pysgsystem_ptr_t sgsys, ctx_t ctx) {
                sgsys->initializeForEditMode(ctx.get());
              })
          .def_readwrite("autodraw", &SceneGraphSystem::_autodraw)
          .def_readwrite("autoupdate", &SceneGraphSystem::_autoupdate);
  type_codec->registerStdCodec<pysgsystem_ptr_t>(sgsys_type);
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_scenegraph(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs