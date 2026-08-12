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
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/lev2/gfx/renderer/probe_sh.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_lighting(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto lmd_type_t = py::class_<LightManagerData, lightmanagerdata_ptr_t>(module_lev2, "LightManagerData");
  type_codec->registerStdCodec<lightmanagerdata_ptr_t>(lmd_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto lm_type_t = py::class_<LightManager, lightmanager_ptr_t>(module_lev2, "LightManager");
  lm_type_t.def_property(
      "spot_cookies_color",                             //
      [](lightmanager_ptr_t lm) -> texturearray_ptr_t { //
        return lm->_cookies_spot_color;
      },
      [](lightmanager_ptr_t lm, texturearray_ptr_t tex) { //
        lm->_cookies_spot_color = tex;
      });
  lm_type_t.def_property(
      "spot_cookies_depth",                             //
      [](lightmanager_ptr_t lm) -> texturearray_ptr_t { //
        return lm->_cookies_spot_depth;
      },
      [](lightmanager_ptr_t lm, texturearray_ptr_t tex) { //
        lm->_cookies_spot_depth = tex;
      });
      lm_type_t.def("gpuInit", [](lightmanager_ptr_t lm, ctx_t ctx) { //
        lm->gpuInit(ctx.get());
      });
  lm_type_t.def("allocateDepthSlice", [](lightmanager_ptr_t lm) -> texturearraysliceref_ptr_t {
    return lm->allocateDepthSlice();
  });
  lm_type_t.def("allocateColorSlice", [](lightmanager_ptr_t lm, std::string path) -> texturearraysliceref_ptr_t {
    return lm->allocateColorSlice(path);
  });
  type_codec->registerStdCodec<lightmanager_ptr_t>(lm_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto lc_type_t = py::class_<LightCollector, lightcollector_ptr_t>(module_lev2, "LightCollector");
  type_codec->registerStdCodec<lightcollector_ptr_t>(lc_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto lg_type_t = py::class_<LightingGroup, lightinggroup_ptr_t>(module_lev2, "LightingGroup");
  type_codec->registerStdCodec<lightinggroup_ptr_t>(lg_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<LightData, DrawableData, lightdata_ptr_t>(module_lev2, "LightData")
      .def_property(
          "color",                                 //
          [](lightdata_ptr_t lightdata) -> fvec3 { //
            return lightdata->mColor;
          },
          [](lightdata_ptr_t lightdata, fvec3 color) { //
            lightdata->mColor = color;
          })
      .def_property(
          "intensity",                             //
          [](lightdata_ptr_t lightdata) -> float { //
            return lightdata->_intensity;
          },
          [](lightdata_ptr_t lightdata, float v) { //
            lightdata->_intensity = v;
          })
      .def_property(
          "priority",                              //
          [](lightdata_ptr_t lightdata) -> float { //
            return lightdata->_priority;
          },
          [](lightdata_ptr_t lightdata, float v) { //
            lightdata->_priority = v;
          })
      .def_property(
          "sky_body",                            // 0 none, 1 sun, 2 moon
          [](lightdata_ptr_t lightdata) -> int { //
            return lightdata->_skyBody;
          },
          [](lightdata_ptr_t lightdata, int v) { //
            lightdata->_skyBody = v;
          })
      .def_property(
          "shadowCaster",                          //
          [](lightdata_ptr_t lightdata) -> bool {  //
            return lightdata->mbShadowCaster;
          },
          [](lightdata_ptr_t lightdata, bool v) {  //
            lightdata->mbShadowCaster = v;
          })
      .def_property(
          "shadowBias",                            //
          [](lightdata_ptr_t lightdata) -> float { //
            return lightdata->mShadowBias;
          },
          [](lightdata_ptr_t lightdata, float bias) { //
            lightdata->mShadowBias = bias;
          })
      .def_property(
          "shadowMapSize",                       //
          [](lightdata_ptr_t lightdata) -> int { //
            return lightdata->_shadowMapSize;
          },
          [](lightdata_ptr_t lightdata, int size) { //
            lightdata->_shadowMapSize = size;
          });
  py::class_<PointLightData, LightData, pointlightdata_ptr_t>(module_lev2, "PointLightData")
      .def(py::init<>())
      .def_property(
          "radius",                                      //
          [](pointlightdata_ptr_t lightdata) -> float {  //
            return lightdata->_radius;
          },
          [](pointlightdata_ptr_t lightdata, float v) {  //
            lightdata->_radius = v;
          })
      .def_property(
          "falloff",                                     //
          [](pointlightdata_ptr_t lightdata) -> float {  //
            return lightdata->_falloff;
          },
          [](pointlightdata_ptr_t lightdata, float v) {  //
            lightdata->_falloff = v;
          })
      .def(
          "createNode",                      //
          [](pointlightdata_ptr_t lightdata, //
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::lightnode_ptr_t { //
            auto xfgen = [] -> fmtx4 { return fmtx4(); };
            auto light = std::make_shared<PointLight>(xfgen, lightdata.get());
            return layer->createLightNode(named, light);
          });
  py::class_<DirectionalLightData, LightData, directionallightdata_ptr_t>(module_lev2, "DirectionalLightData")
      .def(py::init<>())
      .def_property(
          "shadowCascadeCount",                        //
          [](directionallightdata_ptr_t lightdata) -> int { //
            return lightdata->_shadowCascadeCount;
          },
          [](directionallightdata_ptr_t lightdata, int v) { //
            lightdata->_shadowCascadeCount = v;
          })
      .def_property(
          "shadowMaxDistance",                         //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_shadowMaxDistance;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_shadowMaxDistance = v;
          })
      .def_property(
          "pcfDither",                                 //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_pcfDither;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_pcfDither = v;
          })
      .def_property(
          "shadowSnapshotInterval",                          //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_shadowSnapshotInterval;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_shadowSnapshotInterval = v;
          })
      .def_property(
          "shadowRefreshAngleDeg",                            //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_shadowRefreshAngleDeg;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_shadowRefreshAngleDeg = v;
          })
      .def_property(
          "shadowRefreshDistance",                            //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_shadowRefreshDistance;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_shadowRefreshDistance = v;
          })
      .def_property(
          "shadowRefreshMaxSecs",                             //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_shadowRefreshMaxSecs;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_shadowRefreshMaxSecs = v;
          })
      .def_property(
          "shadowBandRadius",                                 //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_shadowBandRadius;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_shadowBandRadius = v;
          })
      .def_property(
          "shadowBandRatio",                                  //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_shadowBandRatio;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_shadowBandRatio = v;
          })
      .def_property(
          "shadowBandResRatio",                               //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_shadowBandResRatio;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_shadowBandResRatio = v;
          })
      .def_property(
          "shadowJitterTexels",                               //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_shadowJitterTexels;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_shadowJitterTexels = v;
          })
      .def_property(
          "shadowSnapshotBandsPerFrame",                    //
          [](directionallightdata_ptr_t lightdata) -> int { //
            return lightdata->_shadowSnapshotBandsPerFrame;
          },
          [](directionallightdata_ptr_t lightdata, int v) { //
            lightdata->_shadowSnapshotBandsPerFrame = v;
          })
      .def_property(
          "shadowCrossfadeFrames",                          //
          [](directionallightdata_ptr_t lightdata) -> int { //
            return lightdata->_shadowCrossfadeFrames;
          },
          [](directionallightdata_ptr_t lightdata, int v) { //
            lightdata->_shadowCrossfadeFrames = v;
          })
      .def_property(
          "shadowCrossfadeSecs",                              //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_shadowCrossfadeSecs;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_shadowCrossfadeSecs = v;
          })
      .def_property(
          "cloudShadowStrength",                              //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_cloudShadowStrength;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_cloudShadowStrength = v;
          })
      .def_property(
          "cloudShadowExtent",                                //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_cloudShadowExtent;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_cloudShadowExtent = v;
          })
      .def_property(
          "cloudShadowSoftness",                              //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_cloudShadowSoftness;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_cloudShadowSoftness = v;
          })
      .def_property(
          "cloudShadowDepth",                                 //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_cloudShadowDepth;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_cloudShadowDepth = v;
          })
      .def_property(
          "cloudShadowMapSize",                             //
          [](directionallightdata_ptr_t lightdata) -> int { //
            return lightdata->_cloudShadowMapSize;
          },
          [](directionallightdata_ptr_t lightdata, int v) { //
            lightdata->_cloudShadowMapSize = v;
          })
      .def_property(
          "cloudShadowRefreshFrames",                       //
          [](directionallightdata_ptr_t lightdata) -> int { //
            return lightdata->_cloudShadowRefreshFrames;
          },
          [](directionallightdata_ptr_t lightdata, int v) { //
            lightdata->_cloudShadowRefreshFrames = v;
          })
      .def_property(
          "cloudExtinction",                                  //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_cloudExtinction;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_cloudExtinction = v;
          })
      .def_property(
          "cloudDiscSoftness",                                //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_cloudDiscSoftness;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_cloudDiscSoftness = v;
          })
      .def_property(
          "cloudShadowIblWeight",                             //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_cloudShadowIblWeight;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_cloudShadowIblWeight = v;
          })
      .def_property(
          "cascadeShadowIblWeight",                           //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_cascadeShadowIblWeight;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_cascadeShadowIblWeight = v;
          })
      .def_property(
          "cascadeShadowFloor",                               //
          [](directionallightdata_ptr_t lightdata) -> float { //
            return lightdata->_cascadeShadowFloor;
          },
          [](directionallightdata_ptr_t lightdata, float v) { //
            lightdata->_cascadeShadowFloor = v;
          })
      // CULLSETS — the named caster-family sets and the per-band subscription,
      // as authored text (see DirectionalLightData). Both empty = the one
      // implicit all-families set.
      .def_property(
          "shadowCullSets",                                         //
          [](directionallightdata_ptr_t lightdata) -> std::string { //
            return lightdata->_shadowCullSets;
          },
          [](directionallightdata_ptr_t lightdata, std::string v) { //
            lightdata->_shadowCullSets = v;
          })
      .def_property(
          "shadowBandCullSets",                                     //
          [](directionallightdata_ptr_t lightdata) -> std::string { //
            return lightdata->_shadowBandCullSets;
          },
          [](directionallightdata_ptr_t lightdata, std::string v) { //
            lightdata->_shadowBandCullSets = v;
          })
      .def(
          "createNode",                           //
          [](directionallightdata_ptr_t lightdata, //
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::lightnode_ptr_t { //
            auto xfgen = [] -> fmtx4 { return fmtx4(); };
            auto light = std::make_shared<DirectionalLight>(xfgen, lightdata.get());
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
          })
      .def_property(
          "cookiePath",                                          //
          [](spotlightdata_ptr_t d) -> std::string { return d->_cookiePath.c_str(); },
          [](spotlightdata_ptr_t d, std::string p) { d->_cookiePath = file::Path(p.c_str()); })
      .def(
          "createNode",                      //
          [](spotlightdata_ptr_t lightdata, //
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::lightnode_ptr_t { //
            auto xfgen = [] -> fmtx4 { return fmtx4(); };
            auto light = std::make_shared<SpotLight>(xfgen, lightdata.get());
            return layer->createLightNode(named, light);
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Light, light_ptr_t>(module_lev2, "Light")
      .def_property_readonly(
          "matrix",                        //
          [](light_ptr_t light) -> fmtx4 { //
            return light->worldMatrix();
          })
      .def_property(
          "colorCookie",                         //
          [](light_ptr_t light) -> texturearraysliceref_ptr_t { //
            return light->_cookieColor;
          },
          [](light_ptr_t light, texturearraysliceref_ptr_t tex) { //
            light->_cookieColor = tex;
          })
          .def_property(
            "depthCookie",                         //
            [](light_ptr_t light) -> texturearraysliceref_ptr_t { //
              return light->_cookieDepth;
            },
            [](light_ptr_t light, texturearraysliceref_ptr_t tex) { //
              light->_cookieDepth = tex;
            })
        .def_property(
          "RadianceCookie",                                  //
          [](light_ptr_t light) -> pbr::radiancemaps_ptr_t { //
            return light->_RadianceCookie;
          },
          [](light_ptr_t light, pbr::radiancemaps_ptr_t tex) { //
            light->_RadianceCookie = tex;
          })
      .def_property(
          "shadowCaster",                 //
          [](light_ptr_t light) -> bool { //
            return light->_castsShadows;
          },
          [](light_ptr_t light, bool val) { //
            light->_castsShadows = val;
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<PointLight, Light, pointlight_ptr_t>(module_lev2, "PointLight");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<DirectionalLight, Light, directionallight_ptr_t>(module_lev2, "DirectionalLight")
      .def("lookAt", &DirectionalLight::lookAt);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SpotLight, Light, spotlight_ptr_t>(module_lev2, "SpotLight")
      .def("lookAt", &SpotLight::lookAt)
      .def("setViewProj", &SpotLight::setViewProj)
      .def("setOrthoViewProj", &SpotLight::setOrthoViewProj)
      .def("setPerspectiveViewProj", &SpotLight::setPerspectiveViewProj)
      .def_readonly("are_matrices_explicit", &SpotLight::_matrices_explicit)
      .def_readonly("explicit_ortho_left",   &SpotLight::_explicit_ortho_left)
      .def_readonly("explicit_ortho_right",  &SpotLight::_explicit_ortho_right)
      .def_readonly("explicit_ortho_top",    &SpotLight::_explicit_ortho_top)
      .def_readonly("explicit_ortho_bottom", &SpotLight::_explicit_ortho_bottom)
      .def_readonly("explicit_persp_fovy_rad", &SpotLight::_explicit_persp_fovy_rad)
      .def_readonly("explicit_persp_aspect",   &SpotLight::_explicit_persp_aspect)
      .def_readonly("explicit_near", &SpotLight::_explicit_near)
      .def_readonly("explicit_far",  &SpotLight::_explicit_far)
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
  py::class_<DynamicPointLight, PointLight, dynamicpointlight_ptr_t>(module_lev2, "DynamicPointLight")
      .def(py::init<>())
      .def_property_readonly(
          "data",
          [](dynamicpointlight_ptr_t light) -> pointlightdata_ptr_t {
            return light->_inlineData;
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<DynamicDirectionalLight, DirectionalLight, dynamicdirectionallight_ptr_t>(module_lev2, "DynamicDirectionalLight")
      .def(py::init<>())
      .def_property_readonly(
          "data",                                                                //
          [](dynamicdirectionallight_ptr_t light) -> directionallightdata_ptr_t { //
            return light->_inlineData;
          });
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
                     .def("invalidate", [](lightprobe_ptr_t probe) { probe->_dirty = true; })
                     .def_property(
                         "imageDim",                         //
                         [](lightprobe_ptr_t probe) -> int { //
                           return probe->dim();
                         },
                         [](lightprobe_ptr_t probe, int dim) { //
                           probe->_dim = dim;
                         })
                     .def_property(
                         "worldMatrix",                        //
                         [](lightprobe_ptr_t probe) -> fmtx4 { //
                           return probe->_worldMatrix;
                         },
                         [](lightprobe_ptr_t probe, fmtx4 mtx) { //
                           probe->_worldMatrix = mtx;
                         })
                     .def_property(
                         "name",                                     //
                         [](lightprobe_ptr_t probe) -> std::string { //
                           return probe->_name;
                         },
                         [](lightprobe_ptr_t probe, std::string name) { //
                           probe->_name = name;
                         })
                     .def_property(
                         "type",                                         //
                         [](lightprobe_ptr_t probe) -> crcstring_ptr_t { //
                           return std::make_shared<CrcString>(uint64_t(probe->_type));
                         },
                         [](lightprobe_ptr_t probe, crcstring_ptr_t t) { //
                           probe->_type = LightProbeType(t->hashed());
                         })
                     .def_property(
                         "active",
                         [](lightprobe_ptr_t probe) -> bool { return probe->_active; },
                         [](lightprobe_ptr_t probe, bool v) { probe->_active = v; })
                     .def_property(
                         "activationMode",
                         [](lightprobe_ptr_t probe) -> crcstring_ptr_t {
                           return std::make_shared<CrcString>(uint64_t(probe->_activationMode));
                         },
                         [](lightprobe_ptr_t probe, crcstring_ptr_t m) {
                           probe->_activationMode = ProbeActivationMode(m->hashed());
                         })
                     .def_property_readonly("shSlot", [](lightprobe_ptr_t probe) -> int { return probe->_shSlot; })
                     .def("shCoefficients", [](lightprobe_ptr_t probe, ctx_t ctx) -> py::list {
                       // staged GPU readback of the probe's L2 coefficients (9 x vec3,
                       // radiance integrals). Empty list until the probe has been
                       // captured+projected at least once.
                       py::list rval;
                       if (probe->_shProjector and probe->_shSlot >= 0) {
                         fvec3 coeffs[kProbeSHCoeffs];
                         if (probe->_shProjector->readback(ctx.get(), probe->_shSlot, coeffs)) {
                           for (int i = 0; i < kProbeSHCoeffs; i++)
                             rval.append(coeffs[i]);
                         }
                       }
                       return rval;
                     })
                     .def("exportEquirectangular", [](lightprobe_ptr_t probe, ctx_t ctx, fquat& qrot, py::object path) {
                       auto path_as_str = py::str(path);
                       auto path_as_std = path_as_str.cast<std::string>();
                       probe->exportEquirectangular(ctx.get(), qrot, path_as_std);
                     });
  type_codec->registerStdCodec<lightprobe_ptr_t>(probe_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto shproj_t = py::class_<ProbeSHProjector, probeshprojector_ptr_t>(module_lev2, "ProbeSHProjector")
                      .def(py::init<>())
                      .def(
                          "project",
                          [](probeshprojector_ptr_t proj, ctx_t ctx, texture_ptr_t cubetex, int face_dim, int slot) {
                            proj->project(ctx.get(), cubetex, face_dim, slot);
                          })
                      .def("coefficients", [](probeshprojector_ptr_t proj, ctx_t ctx, int slot) -> py::list {
                        py::list rval;
                        fvec3 coeffs[kProbeSHCoeffs];
                        if (proj->readback(ctx.get(), slot, coeffs)) {
                          for (int i = 0; i < kProbeSHCoeffs; i++)
                            rval.append(coeffs[i]);
                        }
                        return rval;
                      });
  type_codec->registerStdCodec<probeshprojector_ptr_t>(shproj_t);
  /////////////////////////////////////////////////////////////////////////////////
  module_lev2.def("computeAmbientOcclusion", [](int numsamples, meshutil::mesh_ptr_t model, ctx_t ctx) {
    computeAmbientOcclusion(numsamples, model, ctx.get());
  });
  module_lev2.def("computeLightMaps", [](meshutil::mesh_ptr_t model, ctx_t ctx) { computeLightMaps(model, ctx.get()); });
}
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
