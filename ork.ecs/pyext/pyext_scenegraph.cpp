////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/ParticlesComponent.h>
#include <ork/ecs/HypermeshComponent.h>
#include <ork/lev2/gfx/hypermesh/hm_drawable.h>
// pyext is the right place for the full lev2 include — the pyext .so links
// lev2 anyway and is loaded by Python after lev2's dylib.
#include <ork/lev2/gfx/particle/drawable_data.h>

///////////////////////////////////////////////////////////////////////////////
using ctx_t = ork::python::unmanaged_ptr<::ork::lev2::Context>;
using pyparticlessys_ptr_t = ork::python::unmanaged_ptr<ork::ecs::ParticlesGlobalSystem>;
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
                         [](nodedef_ptr_t ndef, fvec4 val) { ndef->_modcolor = val; })
                     .def_property(
                         "drawable_asset_name",
                         [](nodedef_ptr_t ndef) -> std::string { return ndef->_drawable_asset_name; },
                         [](nodedef_ptr_t ndef, std::string val) { ndef->_drawable_asset_name = val; })
                     .def_property(
                         "envmap_path",
                         [](nodedef_ptr_t ndef) -> std::string { return ndef->_envmap_path; },
                         [](nodedef_ptr_t ndef, std::string val) { ndef->_envmap_path = val; });
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
          "drawable_asset_name",
          [](sgnodeitemdata_ptr_t nid) -> std::string { return nid->_drawable_asset_name; },
          [](sgnodeitemdata_ptr_t nid, std::string val) { nid->_drawable_asset_name = val; })
      .def_property(
          "envmap_path",
          [](sgnodeitemdata_ptr_t nid) -> std::string { return nid->_envmap_path; },
          [](sgnodeitemdata_ptr_t nid, std::string val) { nid->_envmap_path = val; })
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
      .def_property(
          "instance_node_name", // serializable instance pairing (matches the physics component's)
          [](const sgcomponentdata_ptr_t& sgcd) -> std::string { return sgcd->_instanceNodeName; },
          [](sgcomponentdata_ptr_t& sgcd, std::string val) { sgcd->_instanceNodeName = val; })
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
            if (kwargs.contains("drawable_asset_name")) {
              ndef->_drawable_asset_name = kwargs["drawable_asset_name"].cast<std::string>();
            }
            if (kwargs.contains("envmap_path")) {
              ndef->_envmap_path = kwargs["envmap_path"].cast<std::string>();
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
      // PBR2 Phase 0 — single skybox source field. Two forms:
      //   "asset://<name>" — cross-ref to an HdriToXirGenData; bake runs
      //                       and resolved .xir path lands in
      //                       _userParams["SkyboxTexPathStr"].
      //   anything else    — literal path string, passed through as-is.
      // Set by Scene.scenegraph(skybox_probe=wrapper) or directly via
      // self._sgsys.skybox_path = "ork_envmaps|cold4k".
      .def_property(
          "skybox_path",
          [](sgsystemdata_ptr_t sgsys) -> std::string { return sgsys->_skybox_path; },
          [](sgsystemdata_ptr_t sgsys, std::string v) { sgsys->_skybox_path = v; })
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
              // Route to _userParams (reflected, JSON-round-trippable),
              // not _internalParams (transient, runtime-only). Author
              // params from the Scene DSL must survive serialize.
              sgsys->setUserSceneParam(key_str, val_decoded);
            }
          })
      .def(
          "addPostFxNode",
          [](sgsystemdata_ptr_t sgsys, const std::string& name, lev2::compositorpostnode_ptr_t node) {
            sgsys->addPostFxNode(name, node);
          },
          R"doc(
        PBR2 P3.D — register a PostFxNode under a stable string key.
        Survives JSON round-trip via the reflected _postfx_nodes map.
        Pair with .postfx_order = "name1,name2,..." to set execution order.
       )doc")
      .def_property(
          "postfx_order",
          [](sgsystemdata_ptr_t sgsys) -> std::string { return sgsys->_postfx_order; },
          [](sgsystemdata_ptr_t sgsys, const std::string& v) { sgsys->_postfx_order = v; })
      .def(
          "appendPostFxOrder",
          [](sgsystemdata_ptr_t sgsys, const std::string& name) {
            sgsys->appendPostFxOrder(name);
          },
          R"doc(
        PBR2 P3.D — append a node name to _postfx_order, idempotent.
        Use from Scene DSL sub_calls to wire ordering without managing
        the comma-delimited string directly.
       )doc")
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
            if (kwargs.contains("drawable_asset_name")) {
              ndef->_drawable_asset_name = kwargs["drawable_asset_name"].cast<std::string>();
            }
            if (kwargs.contains("envmap_path")) {
              ndef->_envmap_path = kwargs["envmap_path"].cast<std::string>();
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
  // ParticlesComponentData — author-configurable component that hosts a
  // HyperSyn (or imperative) particle graph as an ECS entity. The author
  // builds a ParticlesDrawableData with its graphdata (typically via the
  // DSL's generatedflow()) and assigns it to .drawabledata; the system
  // creates the drawable + scenegraph node at stage time.
  py::class_<ParticlesComponentData, ComponentData, particlescomponentdata_ptr_t>(
      module_ecs, "ParticlesComponentData")
      .def("__repr__",
          [](const particlescomponentdata_ptr_t& pcd) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::ParticlesComponentData(%p)", pcd.get());
            return fxs.c_str();
          })
      .def_property("drawabledata",
          [](particlescomponentdata_ptr_t pcd) -> lev2::particles_drawable_data_ptr_t {
            return pcd->_drawabledata;
          },
          [](particlescomponentdata_ptr_t pcd, lev2::particles_drawable_data_ptr_t val) {
            pcd->_drawabledata = val;
          })
      .def_property("layername",
          [](particlescomponentdata_ptr_t pcd) -> std::string { return pcd->_layername; },
          [](particlescomponentdata_ptr_t pcd, std::string val) { pcd->_layername = val; })
      .def_property("nodename",
          [](particlescomponentdata_ptr_t pcd) -> std::string { return pcd->_nodename; },
          [](particlescomponentdata_ptr_t pcd, std::string val) { pcd->_nodename = val; })
      // Pool of N concurrent slots per component instance. Each START
      // event grabs the next FREE slot (or evicts the oldest if all
      // are busy). Default 1 = single-slot (legacy A1/A2 behavior).
      .def_property("pool_size",
          [](particlescomponentdata_ptr_t pcd) -> int { return pcd->_pool_size; },
          [](particlescomponentdata_ptr_t pcd, int val) { pcd->_pool_size = val; })
      // If > 0, RUNNING slots auto-transition to DRAINING after this
      // many seconds. 0 = manual STOP only.
      .def_property("duration",
          [](particlescomponentdata_ptr_t pcd) -> float { return pcd->_duration; },
          [](particlescomponentdata_ptr_t pcd, float val) { pcd->_duration = val; })
      .def_property("start_delay",
          [](particlescomponentdata_ptr_t pcd) -> float { return pcd->_start_delay; },
          [](particlescomponentdata_ptr_t pcd, float val) { pcd->_start_delay = val; })
      // DRAINING slots recycle to FREE after this many seconds of drain.
      // Should be ≥ the max particle lifespan in the graph.
      .def_property("drain_linger",
          [](particlescomponentdata_ptr_t pcd) -> float { return pcd->_drain_linger; },
          [](particlescomponentdata_ptr_t pcd, float val) { pcd->_drain_linger = val; })
      // M3 round-trip: AssetSystemData ParticleSystemGenData name. On
      // JSON load, the post-deserialize wire step looks this up,
      // materializes the named asset (instantiates the DSL class +
      // builds graphdata), and assigns the resulting
      // ParticlesDrawableData to .drawabledata.
      .def_property("particles_asset_name",
          [](particlescomponentdata_ptr_t pcd) -> std::string {
            return pcd->_particles_asset_name;
          },
          [](particlescomponentdata_ptr_t pcd, std::string val) {
            pcd->_particles_asset_name = val;
          });
  /////////////////////////////////////////////////////////////////////////////////
  // HypermeshComponentData (D.3) — author-configurable component hosting a HyperSyn
  // hypermesh graph as an ECS entity. The author builds a HypermeshDrawableData
  // (embedded graph + material asset ref + flags) and assigns it to .drawabledata;
  // the system creates the drawable + scenegraph node at stage time and wires the
  // material resolver against the AssetSystem registry.
  py::class_<HypermeshComponentData, ComponentData, hypermeshcomponentdata_ptr_t>(
      module_ecs, "HypermeshComponentData")
      .def("__repr__",
          [](const hypermeshcomponentdata_ptr_t& hcd) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::HypermeshComponentData(%p)", hcd.get());
            return fxs.c_str();
          })
      .def_property("drawabledata",
          [](hypermeshcomponentdata_ptr_t hcd) -> lev2::hypermesh::hypermesh_drawable_data_ptr_t {
            return hcd->_drawabledata;
          },
          [](hypermeshcomponentdata_ptr_t hcd, lev2::hypermesh::hypermesh_drawable_data_ptr_t val) {
            hcd->_drawabledata = val;
          })
      .def_property("layername",
          [](hypermeshcomponentdata_ptr_t hcd) -> std::string { return hcd->_layername; },
          [](hypermeshcomponentdata_ptr_t hcd, std::string val) { hcd->_layername = val; })
      .def_property("nodename",
          [](hypermeshcomponentdata_ptr_t hcd) -> std::string { return hcd->_nodename; },
          [](hypermeshcomponentdata_ptr_t hcd, std::string val) { hcd->_nodename = val; });
  /////////////////////////////////////////////////////////////////////////////////
  // HypermeshSystemData — minimal marker (auto-declared via the component's
  // DoRegisterWithScene; declarable explicitly via declareSystem("HypermeshSystem")).
  py::class_<HypermeshSystemData, SystemData, hypermesh_system_data_ptr_t>(
      module_ecs, "HypermeshSystemData")
      .def("__repr__",
          [](const hypermesh_system_data_ptr_t& d) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::HypermeshSystemData(%p)", d.get());
            return fxs.c_str();
          });
  /////////////////////////////////////////////////////////////////////////////////
  // ParticlesGlobalSystemData — minimal marker (no per-scene config in v0).
  // Authors declare it via ecsscene.declareSystem("ParticlesGlobalSystem")
  // if any ParticlesComponent uses it; SceneGraphSystem dep is auto-declared
  // through the component's DoRegisterWithScene.
  py::class_<ParticlesGlobalSystemData, SystemData, particles_global_system_data_ptr_t>(
      module_ecs, "ParticlesGlobalSystemData")
      .def("__repr__",
          [](const particles_global_system_data_ptr_t& d) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::ParticlesGlobalSystemData(%p)", d.get());
            return fxs.c_str();
          })
      .def_property(
          "parallel_compute",
          [](particles_global_system_data_ptr_t d) -> bool { return d->_parallel_compute; },
          [](particles_global_system_data_ptr_t d, bool v) { d->_parallel_compute = v; });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ParticlesGlobalSystem, pyparticlessys_ptr_t>(module_ecs, "ParticlesGlobalSystem")
      .def("__repr__",
          [](pyparticlessys_ptr_t s) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::ParticlesGlobalSystem(%p)", s.get());
            return fxs.c_str();
          });
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_scenegraph(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs