////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_lighting(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto lmd_type_t = py::class_<LightManagerData, lightmanagerdata_ptr_t>(module_lev2, "LightManagerData");
  type_codec->registerStdCodec<lightmanagerdata_ptr_t>(lmd_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto lm_type_t = py::class_<LightManager, lightmanager_ptr_t>(module_lev2, "LightManager");
  type_codec->registerStdCodec<lightmanager_ptr_t>(lm_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto lc_type_t = py::class_<LightCollector, lightcollector_ptr_t>(module_lev2, "LightCollector");
  type_codec->registerStdCodec<lightcollector_ptr_t>(lc_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto lg_type_t = py::class_<LightingGroup, lightinggroup_ptr_t>(module_lev2, "LightingGroup");
  type_codec->registerStdCodec<lightinggroup_ptr_t>(lg_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<LightData, lightdata_ptr_t>(module_lev2, "LightData")
      .def_property(
          "color",                                 //
          [](lightdata_ptr_t lightdata) -> fvec3 { //
            return lightdata->mColor;
          },
          [](lightdata_ptr_t lightdata, fvec3 color) { //
            lightdata->mColor = color;
          });
  py::class_<PointLightData, LightData, pointlightdata_ptr_t>(module_lev2, "PointLightData")
      .def(py::init<>())
      .def(
          "createNode",                      //
          [](pointlightdata_ptr_t lightdata, //
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::lightnode_ptr_t { //
            auto xfgen = []() -> fmtx4 { return fmtx4(); };
            auto light = std::make_shared<PointLight>(xfgen, lightdata.get());
            return layer->createLightNode(named, light);
          });
  py::class_<SpotLightData, LightData, spotlightdata_ptr_t>(module_lev2, "SpotLightData");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Light, light_ptr_t>(module_lev2, "Light")
      .def_property(
          "matrix",                              //
          [](light_ptr_t light) -> fmtx4_ptr_t { //
            auto copy = std::make_shared<fmtx4>(light->worldMatrix());
            return copy;
          },
          [](light_ptr_t light, fmtx4_ptr_t mtx) { //
            light->worldMatrix() = *mtx.get();
          });
  py::class_<PointLight, Light, pointlight_ptr_t>(module_lev2, "PointLight");
  py::class_<SpotLight, Light, spotlight_ptr_t>(module_lev2, "SpotLight");
  /////////////////////////////////////////////////////////////////////////////////
  module_lev2.def("computeAmbientOcclusion", [](int numsamples, meshutil::mesh_ptr_t model, ctx_t ctx) {
    computeAmbientOcclusion(numsamples, model, ctx.get());
  });
  module_lev2.def("computeLightMaps", [](meshutil::mesh_ptr_t model, ctx_t ctx){
    computeLightMaps(model, ctx.get());

  });
}
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2 {
