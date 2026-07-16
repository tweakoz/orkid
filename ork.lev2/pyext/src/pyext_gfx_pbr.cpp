////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/lev2/gfx/fx_pipeline.h>
#include <ork/python/gil_safe_pyobj.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_pbr(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto irrmap_type =
      py::class_<pbr::RadianceMaps, pbr::radiancemaps_ptr_t>(module_lev2, "RadianceMap")
          .def(py::init<>())
          .def_property_readonly("specular", [](pbr::radiancemaps_ptr_t m) -> texturearray_ptr_t { return m->_filtenvSpecularMapArray; })
          .def_property_readonly("diffuse", [](pbr::radiancemaps_ptr_t m) -> texture_ptr_t { return m->_filtenvDiffuseMap; })
          .def_property_readonly("brdf_ggx", [](pbr::radiancemaps_ptr_t m) -> texture_ptr_t { return m->_brdfIntegrationMapGGX; })
          .def_property_readonly(
              "loadRequest", [](pbr::radiancemaps_ptr_t m) -> asset::loadrequest_ptr_t { return m->_loadRequest; })
          .def("__repr__", [](pbr::radiancemaps_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("RadianceMap(%p)", d.get());
            return fxs.c_str();
          });
  /////////////////////////////////////////////////////////////////////////////////
  auto pbrcommon_type = //
      py::class_<pbr::CommonStuff, pbr::commonstuff_ptr_t>(module_lev2, "PbrCommon")
          .def_static(
              "requestRadianceMaps",
              [](py::object path) -> pbr::radiancemaps_ptr_t { //
                auto as_py_str = py::str(path);
                auto as_str    = as_py_str.cast<std::string>();
                // printf("requestRadianceMaps<%s>\n", as_str.c_str());
                return pbr::CommonStuff::requestRadianceMaps(as_str);
              })
          .def_static(
              "requestRadianceMapsAsync",
              [](py::object path) -> pbr::radiancemaps_ptr_t { //
                auto as_py_str = py::str(path);
                auto as_str    = as_py_str.cast<std::string>();
                return pbr::CommonStuff::requestRadianceMapsAsync(as_str);
              })
          .def_static(
              "requestRadianceMapsSync",
              [](py::object path, ctx_t ctx) -> pbr::radiancemaps_ptr_t { //
                auto as_py_str = py::str(path);
                auto as_str    = as_py_str.cast<std::string>();
                return pbr::CommonStuff::requestRadianceMapsSync(as_str, ctx.get());
              })
          .def_static(
              "makeProceduralRadianceMaps",
              [](ctx_t ctx) -> pbr::radiancemaps_ptr_t { //
                return pbr::CommonStuff::makeProceduralRadianceMaps(ctx.get());
              })
          .def_static(
              "updateRadianceMapsGradient",
              [](pbr::radiancemaps_ptr_t maps, py::list stops, ctx_t ctx) {
                std::vector<std::pair<float, fvec3>> cpp_stops;
                cpp_stops.reserve(stops.size());
                for (auto handle : stops) {
                  auto tup = handle.cast<py::tuple>();
                  cpp_stops.emplace_back(tup[0].cast<float>(), tup[1].cast<fvec3>());
                }
                pbr::CommonStuff::updateRadianceMapsGradient(maps, cpp_stops, ctx.get());
              })
          .def(py::init<>())
          .def_property(
              "RadianceMaps",
              [](pbr::commonstuff_ptr_t pbc) -> pbr::radiancemaps_ptr_t { //
                return pbc->_radiance_maps;                                //
              },
              [](pbr::commonstuff_ptr_t pbc, pbr::radiancemaps_ptr_t v) { //
                pbc->_radiance_maps = v;                                   //
              })
          .def(
              "requestSkyboxTexture",
              [](pbr::commonstuff_ptr_t pbc, std::string path) { //
                auto load_req = std::make_shared<asset::LoadRequest>(path);
                pbc->requestAndRefSkyboxTexture(load_req);

              })
          .def(
              "requestAndRefSkyboxTexture",
              [](pbr::commonstuff_ptr_t pbc, std::string path) -> asset::loadrequest_ptr_t { //
                auto load_req = std::make_shared<asset::LoadRequest>(path);
                pbc->requestAndRefSkyboxTexture(load_req);

                return load_req;
              })
          .def(
              "refilterSkybox",
              [](pbr::commonstuff_ptr_t pbc, std::string raw_source_path) { // MT2 §2.6 in-scene refilter
                pbc->refilterSkybox(ork::file::Path(raw_source_path));
              })
          .def(
              "setBRDF",
              [](pbr::commonstuff_ptr_t pbc, crcstring_ptr_t fmt) { //
                pbc->_brdftype = fmt->hashed();
              })
          .def_property(
              "environmentIntensity",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_environmentIntensity; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_environmentIntensity = v; })
          .def_property(
              "environmentMipBias",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_environmentMipBias; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_environmentMipBias = v; })
          .def_property(
              "environmentMipScale",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_environmentMipScale; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_environmentMipScale = v; })
          .def_property(
              "ambientLevel",
              [](pbr::commonstuff_ptr_t pbc) -> fvec3 { return pbc->_ambientLevel; },
              [](pbr::commonstuff_ptr_t pbc, fvec3 v) { pbc->_ambientLevel = v; })
          .def_property(
              "diffuseLevel",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_diffuseLevel; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_diffuseLevel = v; })
          .def_property(
              "specularLevel",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_specularLevel; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_specularLevel = v; })
          .def_property(
              "specularMipBias",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_specularMipBias; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_specularMipBias = v; })
          .def_property(
              "skyboxLevel",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_skyboxLevel; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_skyboxLevel = v; })
          .def_property(   // disable the skybox BACKGROUND draw while keeping the env loaded for IBL
              "enable_skybox",
              [](pbr::commonstuff_ptr_t pbc) -> bool { return pbc->_enable_skybox; },
              [](pbr::commonstuff_ptr_t pbc, bool v) { pbc->_enable_skybox = v; })
          .def_property(
              "depthFogDistance",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_depthFogDistance; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_depthFogDistance = v; })
          .def_property(
              "depthFogPower",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_depthFogPower; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_depthFogPower = v; })
          .def_property(
              "roughnessPower",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_roughnessPower; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_roughnessPower = v; })
          .def_property(
              "useDepthPrepass",
              [](pbr::commonstuff_ptr_t pbc) -> bool { return pbc->_useDepthPrepass; },
              [](pbr::commonstuff_ptr_t pbc, bool v) { pbc->_useDepthPrepass = v; })
          .def_property(
              "useFloatColorBuffer",
              [](pbr::commonstuff_ptr_t pbc) -> bool { return pbc->_useFloatColorBuffer; },
              [](pbr::commonstuff_ptr_t pbc, bool v) { pbc->_useFloatColorBuffer = v; })
          .def_property(
              "enable_SSSS",
              [](pbr::commonstuff_ptr_t pbc) -> bool { return pbc->_enable_SSSS; },
              [](pbr::commonstuff_ptr_t pbc, bool v) { pbc->_enable_SSSS = v; })
          .def_property(
              "ssaoNumSamples",
              [](pbr::commonstuff_ptr_t pbc) -> int { return pbc->_ssaoNumSamples; },
              [](pbr::commonstuff_ptr_t pbc, int v) { pbc->_ssaoNumSamples = v; })
          .def_property(
              "ssaoRadius",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_ssaoRadius; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_ssaoRadius = v; })
          .def_property(
              "dppZBias",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_dppZbias; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_dppZbias = v; })
          .def("__repr__", [](pbr::commonstuff_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("PbrCommon(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<pbr::commonstuff_ptr_t>(pbrcommon_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pbr_type = //
      py::class_<PBRMaterial, GfxMaterial, pbrmaterial_ptr_t>(module_lev2, "PBRMaterial")
          .def(py::init<>())
          .def(
              "__repr__",
              [](pbrmaterial_ptr_t m) -> std::string {
                return FormatString("PBRMaterial(%p:%s)", m.get(), m->mMaterialName.c_str());
              })
          .def("clone", [](pbrmaterial_ptr_t m) -> pbrmaterial_ptr_t { return m->clone(); })
          .def("addBasicStateLambdaToPipeline", [](pbrmaterial_ptr_t m, fxpipeline_ptr_t pipe) { m->addBasicStateLambda(pipe); })
          .def("addLightingLambdaToPipeline", [](pbrmaterial_ptr_t m, fxpipeline_ptr_t pipe) { m->addBasicStateLambda(pipe); })
          .def("addBasicStateLambda", [](pbrmaterial_ptr_t m) { m->addBasicStateLambda(); })
          .def("addLightingLambda", [](pbrmaterial_ptr_t m) { m->addLightingLambda(); })
          .def(
              "setActiveLightMap",
              [](pbrmaterial_ptr_t m, std::string name, fvec3 color) { //
                m->setActiveLightMap(name, color);
              })
          .def_property_readonly(
              "fxcache",                                              //
              [](pbrmaterial_ptr_t m) -> fxpipelinecache_constptr_t { //
                return m->pipelineCache();                            // material
              })
          .def_property_readonly("freestyle", [](pbrmaterial_ptr_t m) -> freestyle_mtl_ptr_t { return m->_as_freestyle; })
          .def("gpuInit", [](pbrmaterial_ptr_t m, ctx_t& c) { m->gpuInit(c.get()); })
          .def_static("brdfIntegrationMap", [](ctx_t& c, std::string type) -> texture_ptr_t {
            return PBRMaterial::brdfIntegrationMap(c.get(), type);
          })
          .def(
              "assignImages",
              [](pbrmaterial_ptr_t m,
                 ctx_t context,
                 py::kwargs kwa) { //
                image_ptr_t color_tex, normal_tex, mtlruf_tex, emissive_tex, ambocc_tex;

                bool doConform = false;

                if (kwa.contains("color"))
                  color_tex = kwa["color"].cast<image_ptr_t>();
                if (kwa.contains("normal"))
                  normal_tex = kwa["normal"].cast<image_ptr_t>();
                if (kwa.contains("mtlruf"))
                  mtlruf_tex = kwa["mtlruf"].cast<image_ptr_t>();
                if (kwa.contains("emissive"))
                  emissive_tex = kwa["emissive"].cast<image_ptr_t>();
                if (kwa.contains("ambocc"))
                  ambocc_tex = kwa["ambocc"].cast<image_ptr_t>();
                if (kwa.contains("doConform")) {
                  doConform = kwa["doConform"].cast<bool>();
                }
                m->assignImages(context.get(), color_tex, normal_tex, mtlruf_tex, emissive_tex, ambocc_tex, doConform);
              })
          .def_property(
              "instanceMatricesOnly", // set BEFORE gpuInit: instanced pipeline uses the matrices-only
              [](pbrmaterial_ptr_t m) -> bool { return m->_instanceMatricesOnly; }, // dynamic block (no per-inst color)
              [](pbrmaterial_ptr_t m, bool v) { m->_instanceMatricesOnly = v; })
          .def_property(
              "metallicFactor",
              [](pbrmaterial_ptr_t m) -> float { //
                return m->_metallicFactor;
              },
              [](pbrmaterial_ptr_t m, float v) { //
                m->_metallicFactor = v;
              })
          .def_property(
              "roughnessFactor",
              [](pbrmaterial_ptr_t m) -> float { //
                return m->_roughnessFactor;
              },
              [](pbrmaterial_ptr_t m, float v) { //
                m->_roughnessFactor = v;
              })
          .def_property(
              "baseColor",
              [](pbrmaterial_ptr_t m) -> fvec4 { //
                return m->_baseColor;
              },
              [](pbrmaterial_ptr_t m, fvec4 v) { //
                m->_baseColor = v;
              })
          .def_property(
              "texColor",
              [](pbrmaterial_ptr_t m) -> texture_ptr_t { //
                return m->_texColor;
              },
              [](pbrmaterial_ptr_t m, texture_ptr_t v) { //
                m->_texColor = v;
              })
          .def_property(
              "texNormal",
              [](pbrmaterial_ptr_t m) -> texture_ptr_t { //
                return m->_texNormal;
              },
              [](pbrmaterial_ptr_t m, texture_ptr_t v) { //
                m->_texNormal = v;
              })
          .def_property(
              "texMtlRuf",
              [](pbrmaterial_ptr_t m) -> texture_ptr_t { //
                return m->_texMtlRuf;
              },
              [](pbrmaterial_ptr_t m, texture_ptr_t v) { //
                m->_texMtlRuf = v;
              })
          .def_property(
              "texEmissive",
              [](pbrmaterial_ptr_t m) -> texture_ptr_t { //
                return m->_texEmissive;
              },
              [](pbrmaterial_ptr_t m, texture_ptr_t v) { //
                m->_texEmissive = v;
              })
          .def_property_readonly(
              "colorMapName",
              [](pbrmaterial_ptr_t m) -> std::string { //
                return m->_colorMapName;
              })
          .def_property_readonly(
              "normalMapName",
              [](pbrmaterial_ptr_t m) -> std::string { //
                return m->_normalMapName;
              })
          .def_property_readonly(
              "mtlRufMapName",
              [](pbrmaterial_ptr_t m) -> std::string { //
                return m->_mtlRufMapName;
              })
          .def_property_readonly(
              "amboccMapName",
              [](pbrmaterial_ptr_t m) -> std::string { //
                return m->_amboccMapName;
              })
          .def_property_readonly(
              "emissiveMapName",
              [](pbrmaterial_ptr_t m) -> std::string { //
                return m->_emissiveMapName;
              })
          .def_property(
              "shaderpath",
              [](pbrmaterial_ptr_t m) -> std::string { //
                return m->_shaderpath.c_str();
              },
              [](pbrmaterial_ptr_t m, std::string p) { //
                printf("PBRMaterial<%p> shaderpath<%s>\n", (void*)m.get(), p.c_str());
                m->_shaderpath = p;
              })
          // GEOV2 Phase 3 — resolve + bind a uniform_block-member param by name
          // (a generated ptex3d surface's bindable params). Binds on the material
          // (_bound_params), which propagate into every pipeline; the internal
          // _as_freestyle shares _shader so it resolves the handle.
          .def(
              "param",
              [](pbrmaterial_ptr_t m, std::string named) -> pyfxparam_ptr_t {
                return pyfxparam_ptr_t(m->_as_freestyle ? m->_as_freestyle->param(named) : nullptr);
              })
          // FxShaderParam-HANDLE overload (restored — the original surface; the
          // name:str form below resolves through _as_freestyle and is ptex3d-
          // only, a handle from ANY shader source binds directly). Same deferred
          // _bound_params contract; LIVE post-creation via the 2.12 rebind stamp.
          .def(
              "bindParam",
              [type_codec](pbrmaterial_ptr_t m, pyfxparam_ptr_t par, py::object value) {
                if (not par)
                  return;
                if (py::hasattr(value, "__call__")) {
                  auto safe = ork::python::gil_safe_pyobj(value);
                  FxPipeline::varval_generator_t gen = [safe, type_codec]() -> FxPipeline::varval_t {
                    py::gil_scoped_acquire acquire;
                    auto fn = safe.valueAs<py::object>();
                    py::object out = (*fn)();
                    return type_codec->decode(out);
                  };
                  m->bindParam(par.get(), FxPipeline::varval_t(gen));
                } else {
                  m->bindParam(par.get(), type_codec->decode(value));
                }
              },
              py::arg("param"),
              py::arg("value"))
          .def(
              "bindParam",
              [type_codec](pbrmaterial_ptr_t m, std::string named, py::object value) {
                if (not m->_as_freestyle)
                  return;
                auto par = m->_as_freestyle->param(named);
                if (not par)
                  return;
                // A 0-arg callable binds a LIVE value re-evaluated every draw
                // (the generator is carried into each pipeline at creation and
                // FxPipeline::_set_typed_param evaluates it per-draw). Captured
                // GIL-safe so off-thread pipeline copies/teardown are safe.
                if (py::hasattr(value, "__call__")) {
                  auto safe = ork::python::gil_safe_pyobj(value);
                  FxPipeline::varval_generator_t gen = [safe, type_codec]() -> FxPipeline::varval_t {
                    py::gil_scoped_acquire acquire;
                    auto fn = safe.valueAs<py::object>();
                    py::object out = (*fn)();
                    return type_codec->decode(out);
                  };
                  m->bindParam(par, FxPipeline::varval_t(gen));
                } else {
                  m->bindParam(par, type_codec->decode(value));
                }
              },
              py::arg("name"),
              py::arg("value"))
          .def_property(
              "doubleSided",
              [](pbrmaterial_ptr_t m) -> bool { //
                return m->_doubleSided;
              },
              [](pbrmaterial_ptr_t m, bool p) { //
                m->_doubleSided = p;
              })
          .def_property(
              "alphaBlend",
              [](pbrmaterial_ptr_t m) -> bool { return m->_alphaBlend; },
              [](pbrmaterial_ptr_t m, bool v) { m->_alphaBlend = v; })
          .def_property(
              "pbrcommon",
              [](pbrmaterial_ptr_t mtl) -> pbr::commonstuff_ptr_t { //
                return mtl->_commonOverride;
              },
              [](pbrmaterial_ptr_t mtl, pbr::commonstuff_ptr_t irr) { //
                mtl->_commonOverride = irr;
              })
          .def_property_readonly("texArrayCNMREA", [](pbrmaterial_ptr_t m) -> texturearray_ptr_t { return m->_texArrayCNMREA; })
          .def("setColorImage", [](pbrmaterial_ptr_t mtl, ctx_t context, image_ptr_t img) {
            auto txi   = context->TXI();
            auto array = mtl->_texArrayCNMREA;
            auto slice = array->slice(0);
            txi->updateTextureArraySlice(slice.get(), img);
          })
          //////////////////////////////////////////////////////////////////////
          // PBR2 Phase 2 — 8 glTF KHR lobe properties (flag + factor pairs),
          // 4 color vec3s, and 3 secondary scalars. All exposed under
          // glTF-spec naming so Scene DSL kwargs map cleanly.
          //////////////////////////////////////////////////////////////////////
#define _PBR2_LOBE_PROP_BOOL(pyname, member)                                       \
  .def_property(pyname,                                                            \
                [](pbrmaterial_ptr_t m) -> bool { return m->member; },             \
                [](pbrmaterial_ptr_t m, bool v) { m->member = v; })
#define _PBR2_LOBE_PROP_FLOAT(pyname, member)                                      \
  .def_property(pyname,                                                            \
                [](pbrmaterial_ptr_t m) -> float { return m->member; },            \
                [](pbrmaterial_ptr_t m, float v) { m->member = v; })
#define _PBR2_LOBE_PROP_VEC3(pyname, member)                                       \
  .def_property(pyname,                                                            \
                [](pbrmaterial_ptr_t m) -> fvec3 { return m->member; },            \
                [](pbrmaterial_ptr_t m, fvec3 v) { m->member = v; })
          _PBR2_LOBE_PROP_BOOL("has_transmission",            _hasTransmission)
          _PBR2_LOBE_PROP_FLOAT("transmission_factor",        _transmissionFactor)
          _PBR2_LOBE_PROP_BOOL("has_transmission_roughness",  _hasTransmissionRoughness)
          _PBR2_LOBE_PROP_FLOAT("transmission_roughness",     _transmissionRoughness)
          _PBR2_LOBE_PROP_BOOL("has_ior",                     _hasIor)
          _PBR2_LOBE_PROP_FLOAT("ior",                        _ior)
          _PBR2_LOBE_PROP_BOOL("has_volume",                  _hasVolume)
          _PBR2_LOBE_PROP_FLOAT("volume_thickness_factor",    _volumeThicknessFactor)
          _PBR2_LOBE_PROP_BOOL("has_diffuse_transmission",    _hasDiffuseTransmission)
          _PBR2_LOBE_PROP_FLOAT("diffuse_transmission_factor", _diffuseTransmissionFactor)
          _PBR2_LOBE_PROP_BOOL("has_specular",                _hasSpecular)
          _PBR2_LOBE_PROP_FLOAT("specular_factor",            _specularFactor)
          _PBR2_LOBE_PROP_BOOL("has_clearcoat",               _hasClearcoat)
          _PBR2_LOBE_PROP_FLOAT("clearcoat_factor",           _clearcoatFactor)
          _PBR2_LOBE_PROP_BOOL("has_sheen",                   _hasSheen)
          _PBR2_LOBE_PROP_FLOAT("sheen_factor",               _sheenFactor)
          _PBR2_LOBE_PROP_BOOL("has_iridescence",             _hasIridescence)
          _PBR2_LOBE_PROP_FLOAT("iridescence_factor",         _iridescenceFactor)
          _PBR2_LOBE_PROP_VEC3("sheen_color",                 _sheenColor)
          _PBR2_LOBE_PROP_VEC3("specular_color",              _specularColor)
          _PBR2_LOBE_PROP_VEC3("attenuation_color",           _attenuationColor)
          _PBR2_LOBE_PROP_VEC3("diffuse_transmission_color",  _diffuseTransmissionColor)
          _PBR2_LOBE_PROP_FLOAT("clearcoat_roughness",        _clearcoatRoughness)
          _PBR2_LOBE_PROP_FLOAT("sheen_roughness",            _sheenRoughness)
          _PBR2_LOBE_PROP_FLOAT("attenuation_distance",       _attenuationDistance)
          // PBR2 Phase 3 (P3.D) — subsurface scattering.
          _PBR2_LOBE_PROP_BOOL("has_subsurface",              _hasSubsurface)
          _PBR2_LOBE_PROP_VEC3("subsurface_color",            _subsurfaceColor)
          _PBR2_LOBE_PROP_VEC3("subsurface_radius",           _subsurfaceRadius)
          _PBR2_LOBE_PROP_FLOAT("subsurface_factor",          _subsurfaceFactor)
          ;
#undef _PBR2_LOBE_PROP_BOOL
#undef _PBR2_LOBE_PROP_FLOAT
#undef _PBR2_LOBE_PROP_VEC3
  type_codec->registerStdCodec<pbrmaterial_ptr_t>(pbr_type);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
