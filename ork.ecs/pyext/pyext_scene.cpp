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
  auto type_codec = python::typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto so_type = py::class_<SceneObject, sceneobject_ptr_t>(module_ecs, "SceneObject")
                     .def("__repr__", [](const sceneobject_ptr_t& sobj) -> std::string {
                       fxstring<256> fxs;
                       fxs.format("ecs::SceneObject(%p)", sobj.get());
                       return fxs.c_str();
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
                          .def_property_readonly("transform", [](spawndata_constptr_t spawndata) -> decompxf_const_ptr_t { return spawndata->transform(); });
  type_codec->registerStdCodec<spawndata_ptr_t>(sd_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SceneData, scenedata_ptr_t>(module_ecs, "SceneData")
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
      .def("declareSystem", [](scenedata_ptr_t scenedata, std::string name) { return scenedata->addSystemWithClassName(name); });

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
