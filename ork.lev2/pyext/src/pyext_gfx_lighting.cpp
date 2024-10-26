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
  auto type_codec = python::pb11_typecodec_t::instance();
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
          })
          .def_property("shadowBias",                                 //
          [](lightdata_ptr_t lightdata) -> float { //
            return lightdata->mShadowBias;
          },
          [](lightdata_ptr_t lightdata, float bias) { //
            lightdata->mShadowBias = bias;
          })
          .def_property("shadowMapSize",                                 //
          [](lightdata_ptr_t lightdata) -> int { //
            return lightdata->_shadowMapSize;
          },
          [](lightdata_ptr_t lightdata, int size) { //
            lightdata->_shadowMapSize = size;
          });
  py::class_<PointLightData, LightData, pointlightdata_ptr_t>(module_lev2, "PointLightData")
      .def(py::init<>())
      .def(
          "createNode",                      //
          [](pointlightdata_ptr_t lightdata, //
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::lightnode_ptr_t { //
            auto xfgen = [] -> fmtx4 { return fmtx4(); };
            auto light = std::make_shared<PointLight>(xfgen, lightdata.get());
            return layer->createLightNode(named, light);
          });
  py::class_<SpotLightData, LightData, spotlightdata_ptr_t>(module_lev2, "SpotLightData")
      .def(py::init<>())
      .def_property(
          "fovy",                                      //
          [](spotlightdata_ptr_t lightdata) -> float { //
            return lightdata->mFovy;
          },
          [](spotlightdata_ptr_t lightdata, float fovy) { //
            lightdata->mFovy = fovy;
          })
      .def_property(
          "range",                                     //
          [](spotlightdata_ptr_t lightdata) -> float { //
            return lightdata->mRange;
          },
          [](spotlightdata_ptr_t lightdata, float range) { //
            lightdata->mRange = range;
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Light, light_ptr_t>(module_lev2, "Light")
      .def_property_readonly(
          "matrix",                              //
          [](light_ptr_t light) -> fmtx4 { //
            return light->worldMatrix();
          })
      .def_property(
          "cookieTexture",                         //
          [](light_ptr_t light) -> texture_ptr_t { //
            return light->_cookieTexture;
          },
          [](light_ptr_t light, texture_ptr_t tex) { //
            light->_cookieTexture = tex;
          })
      .def_property(
          "irradianceCookie",                                  //
          [](light_ptr_t light) -> pbr::irradiancemaps_ptr_t { //
            return light->_irradianceCookie;
          },
          [](light_ptr_t light, pbr::irradiancemaps_ptr_t tex) { //
            light->_irradianceCookie = tex;
          })
      .def_property(
          "shadowCaster",                     //
          [](light_ptr_t light) -> bool { //
            return light->_castsShadows;
          },
          [](light_ptr_t light, bool val) { //
            light->_castsShadows = val;
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<PointLight, Light, pointlight_ptr_t>(module_lev2, "PointLight");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<DirectionalLight, Light, directionallight_ptr_t>(module_lev2, "DirectionalLight");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SpotLight, Light, spotlight_ptr_t>(module_lev2, "SpotLight")
      .def("lookAt", &SpotLight::lookAt)
      .def("affectsSphere", &SpotLight::AffectsSphere)
      .def("affectsAABox", &SpotLight::AffectsAABox)
      .def_property_readonly(
          "shadowMatrix",                      //
          [](spotlight_ptr_t light) -> fmtx4 { //
            return light->shadowMatrix();
          })
      .def_property_readonly(
          "projectionMatrix",                  //
          [](spotlight_ptr_t light) -> fmtx4 { //
            return light->mProjectionMatrix;
          })
      .def_property_readonly(
          "viewMatrix",                        //
          [](spotlight_ptr_t light) -> fmtx4 { //
            return light->mViewMatrix;
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<DynamicPointLight, PointLight, dynamicpointlight_ptr_t>(module_lev2, "DynamicPointLight");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<DynamicDirectionalLight, DirectionalLight, dynamicdirectionallight_ptr_t>(module_lev2, "DynamicDirectionalLight");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<DynamicSpotLight, SpotLight, dynamicspotlight_ptr_t>(module_lev2, "DynamicSpotLight")
      .def(py::init<>())
      .def_property_readonly(
          "data",                                                   //
          [](dynamicspotlight_ptr_t light) -> spotlightdata_ptr_t { //
            return light->_inlineData;
          });
  /////////////////////////////////////////////////////////////////////////////////
  auto probe_t = py::class_<LightProbe, lightprobe_ptr_t>(module_lev2, "LightProbe")
    .def(py::init<>())
    .def("invalidate", [](lightprobe_ptr_t probe) { probe->_dirty=true; })
    .def_property("imageDim",                                //
          [](lightprobe_ptr_t probe) -> int { //
            return probe->_dim;
          },
          [](lightprobe_ptr_t probe, int dim) { //
            probe->_dim = dim;
          })
          .def_property("worldMatrix",                                //
          [](lightprobe_ptr_t probe) -> fmtx4 { //
            return probe->_worldMatrix;
          },
          [](lightprobe_ptr_t probe, fmtx4 mtx) { //
            probe->_worldMatrix = mtx;
          })
          .def_property("name",                                //
          [](lightprobe_ptr_t probe) -> std::string { //
            return probe->_name;
          },
          [](lightprobe_ptr_t probe, std::string name) { //
            probe->_name = name;
          })
          .def_property("type",                                //
          [](lightprobe_ptr_t probe) -> crcstring_ptr_t { //
            return std::make_shared<CrcString>(uint64_t(probe->_type));
          },
          [](lightprobe_ptr_t probe, crcstring_ptr_t t) { //
            probe->_type = LightProbeType(t->hashed());
          });
  type_codec->registerStdCodec<lightprobe_ptr_t>(probe_t);
  /////////////////////////////////////////////////////////////////////////////////
  module_lev2.def("computeAmbientOcclusion", [](int numsamples, meshutil::mesh_ptr_t model, ctx_t ctx) {
    computeAmbientOcclusion(numsamples, model, ctx.get());
  });
  module_lev2.def("computeLightMaps", [](meshutil::mesh_ptr_t model, ctx_t ctx) {
    computeLightMaps(model, ctx.get());
  });
}
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
