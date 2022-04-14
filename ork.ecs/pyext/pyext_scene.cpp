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
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SceneObject,sceneobject_ptr_t>(module_ecs, "SceneObject")
      .def(
          "__repr__",
          [](const sceneobject_ptr_t& sobj) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::SceneObject(%p)", sobj.get());
            return fxs.c_str();
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SceneData,scenedata_ptr_t>(module_ecs, "SceneData")
      .def(py::init<>())
      .def(
          "__repr__",
          [](const scenedata_ptr_t& scenedata) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::SceneData(%p)", scenedata.get());
            return fxs.c_str();
          })
    .def("addSceneObject", [](scenedata_ptr_t& scenedata, sceneobject_ptr_t& sobj) {
        return scenedata->AddSceneObject(sobj);
      })
      .def("addSceneGraphSystem", [](scenedata_ptr_t& scenedata) {
        return scenedata->getTypedSystemData<SceneGraphSystemData>();
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
} // namespace ork::ecs {