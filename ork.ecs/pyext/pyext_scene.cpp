////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/scene.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_scene(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto so_type = py::class_<SceneObject, Object, sceneobject_ptr_t>(module_ecs, "SceneObject")
                     .def("__repr__", [](const sceneobject_ptr_t& sobj) -> std::string {
                       fxstring<256> fxs;
                       fxs.format("ecs::SceneObject(%p)", sobj.get());
                       return fxs.c_str();
                     })
                     .def_property("name",
                         [](const sceneobject_ptr_t& sobj) -> std::string {
                           return sobj->GetName().c_str();
                         },
                         [](sceneobject_ptr_t& sobj, std::string name) {
                           sobj->SetName(name.c_str());
                         });
  type_codec->registerStdCodec<sceneobject_ptr_t>(so_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto sdo_type = py::class_<SceneDagObject, SceneObject, scenedagobject_ptr_t>(module_ecs, "SceneDagObject")
                      .def("__repr__", [](const scenedagobject_ptr_t& sobj) -> std::string {
                        fxstring<256> fxs;
                        fxs.format("ecs::SceneDagObject(%p)", sobj.get());
                        return fxs.c_str();
                      });
  type_codec->registerStdCodec<scenedagobject_ptr_t>(sdo_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto sd_type = py::class_<SpawnData, SceneDagObject, spawndata_ptr_t>(module_ecs, "SpawnData")
                     .def(
                         "__repr__",
                         [](const spawndata_constptr_t& sobj) -> std::string {
                           fxstring<256> fxs;
                           fxs.format("ecs::SpawnData(%p)", sobj.get());
                           return fxs.c_str();
                         })
                     .def("onSpawn", [type_codec](spawndata_ptr_t sobj, py::function pyfn) { //
                        script_cb_t cb = [pyfn,type_codec](const evdata_t& rdata) {
                          py::gil_scoped_acquire acquire;
                          auto encoded = type_codec->encode64(rdata);
                          pyfn(encoded);
                        };
                        sobj->_onSpawn = cb;
                      },
                      // docstring
                      "Set the onSpawn callback for this SpawnData object.\n"
                      "  The callback will be called when the object is spawned.\n"
                      "  The callback will be passed an entity pointer.\n"
                      "  this point should only be accessed from update thread.\n"
                     )
                     .def_property(
                         "archetype",
                         [](spawndata_constptr_t spawndata) -> archetype_ptr_t { 
                            return std::const_pointer_cast<Archetype>(spawndata->_archetype); 
                         },
                         [](spawndata_ptr_t spawndata, archetype_ptr_t arch) { 
                          spawndata->_archetype = arch; 
                         })
                         .def_property(
                          "autospawn",
                          [](spawndata_constptr_t spawndata) -> bool { 
                              return spawndata->_autospawn; 
                          },
                          [](spawndata_ptr_t spawndata, bool val) { 
                            spawndata->_autospawn = val; 
                          })
                          .def_property_readonly("transform", [](spawndata_ptr_t spawndata) -> decompxf_ptr_t { return spawndata->transform(); })
                          .def_property(
                            "spawnCount",
                            [](spawndata_constptr_t sd) -> int { return sd->_spawnCount; },
                            [](spawndata_ptr_t sd, int val) { sd->_spawnCount = val; })
                          .def_property(
                            "spawnInterval",
                            [](spawndata_constptr_t sd) -> float { return sd->_spawnInterval; },
                            [](spawndata_ptr_t sd, float val) { sd->_spawnInterval = val; })
                          .def_property(
                            "stochasticInterval",
                            [](spawndata_constptr_t sd) -> float { return sd->_stochasticInterval; },
                            [](spawndata_ptr_t sd, float val) { sd->_stochasticInterval = val; })
                          .def_property(
                            "positionRandomRadius",
                            [](spawndata_constptr_t sd) -> fvec3 { return sd->_positionRandomRadius; },
                            [](spawndata_ptr_t sd, fvec3 val) { sd->_positionRandomRadius = val; })
                          .def_property(
                            "minDistance",
                            [](spawndata_constptr_t sd) -> fvec3 { return sd->_minDistance; },
                            [](spawndata_ptr_t sd, fvec3 val) { sd->_minDistance = val; })
                          .def_property(
                            "initialDirection",
                            [](spawndata_constptr_t sd) -> fvec3 { return sd->_initialDirection; },
                            [](spawndata_ptr_t sd, fvec3 val) { sd->_initialDirection = val; })
                          .def_property(
                            "initialSpeed",
                            [](spawndata_constptr_t sd) -> float { return sd->_initialSpeed; },
                            [](spawndata_ptr_t sd, float val) { sd->_initialSpeed = val; })
                          .def_property(
                            "directionRandomize",
                            [](spawndata_constptr_t sd) -> float { return sd->_directionRandomize; },
                            [](spawndata_ptr_t sd, float val) { sd->_directionRandomize = val; })
                          .def_property(
                            "lifetimeMin",
                            [](spawndata_constptr_t sd) -> float { return sd->_lifetimeMin; },
                            [](spawndata_ptr_t sd, float val) { sd->_lifetimeMin = val; })
                          .def_property(
                            "lifetimeMax",
                            [](spawndata_constptr_t sd) -> float { return sd->_lifetimeMax; },
                            [](spawndata_ptr_t sd, float val) { sd->_lifetimeMax = val; });
  type_codec->registerStdCodec<spawndata_ptr_t>(sd_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SceneData, Object, scenedata_ptr_t>(module_ecs, "SceneData")
      .def(py::init<>())
      .def(
          "__repr__",
          [](const scenedata_ptr_t& scenedata) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::SceneData(%p)", scenedata.get());
            return fxs.c_str();
          })
      .def("addSceneObject", [](scenedata_ptr_t scenedata, sceneobject_ptr_t sobj) { return scenedata->AddSceneObject(sobj); })
      .def("addSceneGraphSystem", [](scenedata_ptr_t scenedata) { return scenedata->getTypedSystemData<SceneGraphSystemData>(); })
      .def("declareSpawner", [](scenedata_ptr_t scenedata, std::string named ) { //
        auto psname = AddPooledString(named.c_str());
        return scenedata->createSceneObject<SpawnData>(psname);
        })
      .def(
          "declareArchetype",
          [](scenedata_ptr_t scenedata, std::string named) {
            auto psname = AddPooledString(named.c_str());
            return scenedata->createSceneObject<Archetype>(psname);
          })
      //
      .def("declareSystem", [](scenedata_ptr_t scenedata, std::string name) { return scenedata->addSystemWithClassName(name); })
      //
      .def_property_readonly("archetypes", [](scenedata_ptr_t scenedata) -> py::list {
        py::list result;
        for (auto& item : scenedata->_sceneObjects) {
          if (auto as_arch = std::dynamic_pointer_cast<Archetype>(item.second)) {
            result.append(as_arch);
          }
        }
        return result;
      })
      .def_property_readonly("spawners", [](scenedata_ptr_t scenedata) -> py::list {
        py::list result;
        for (auto& item : scenedata->_sceneObjects) {
          if (auto as_sp = std::dynamic_pointer_cast<SpawnData>(item.second)) {
            result.append(as_sp);
          }
        }
        return result;
      })
      .def_property_readonly("systemDatas", [](scenedata_ptr_t scenedata) -> py::list {
        py::list result;
        for (auto& item : scenedata->getSystemDatas()) {
          result.append(item.second);
        }
        return result;
      })
      .def("removeSceneObject", [](scenedata_ptr_t scenedata, sceneobject_ptr_t sobj) {
        scenedata->RemoveSceneObject(sobj);
      })
      .def("renameSceneObject", [](scenedata_ptr_t scenedata, sceneobject_ptr_t sobj, std::string newname) -> bool {
        return scenedata->RenameSceneObject(sobj, newname.c_str());
      })
      .def("removeSystem", [](scenedata_ptr_t scenedata, std::string name) {
        auto& lut = scenedata->_systemDatas;
        auto it = lut.find(name);
        if (it != lut.end()) {
          lut.erase(it);
        }
      })
      .def("findSceneObject", [](scenedata_ptr_t scenedata, std::string name) -> sceneobject_ptr_t {
        auto psname = AddPooledString(name.c_str());
        return scenedata->findSceneObjectByName(psname);
      })
      .def("generateSceneGraphParams", [](scenedata_ptr_t scenedata) -> varmap::varmap_ptr_t {
        return scenedata->generateSceneGraphParams();
      });

  /////////////////////////////////////////////////////////////////////////////////

  /*pyinit_gfx_material(module_lev2);
  pyinit_gfx_shader(module_lev2);
  /////////////////////////////////////////////////////////////////////////////////
  auto refresh_policy_type = //
      py::enum_<ERefreshPolicy>(module_lev2, "RefreshPolicy")
          .value("RefreshFastest", EREFRESH_FASTEST)
          .value("RefreshWhenDirty", EREFRESH_WHENDIRTY)
          .value("RefreshFixedFPS", EREFRESH_FIXEDFPS)
          .export_values();
  type_codec->registerStdCodec<ERefreshPolicy>(refresh_policy_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto gfxenv_type = //
      py::class_<GfxEnv>(module_lev2, "GfxEnv")
          .def_readonly_static("ref", &GfxEnv::GetRef())
          .def("loadingContext", [](const GfxEnv& e) -> ctx_t { return ctx_t(GfxEnv::GetRef().loadingContext()); })
          .def("__repr__", [](const GfxEnv& e) -> std::string {
            fxstring<64> fxs;
            fxs.format("GfxEnv(%p)", &e);
            return fxs.c_str();
          });*/
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_scene(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
