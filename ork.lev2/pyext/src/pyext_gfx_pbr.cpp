////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_pbr(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto irrmap_type =
      py::class_<pbr::IrradianceMaps, pbr::irradiancemaps_ptr_t>(module_lev2, "IrradianceMap")
          .def(py::init<>())
          .def_property_readonly("specular", [](pbr::irradiancemaps_ptr_t m) -> texture_ptr_t { return m->_filtenvSpecularMap; })
          .def_property_readonly("diffuse", [](pbr::irradiancemaps_ptr_t m) -> texture_ptr_t { return m->_filtenvDiffuseMap; })
          .def_property_readonly("brdf", [](pbr::irradiancemaps_ptr_t m) -> texture_ptr_t { return m->_brdfIntegrationMap; })
          .def_property_readonly(
              "loadRequest", [](pbr::irradiancemaps_ptr_t m) -> asset::loadrequest_ptr_t { return m->_loadRequest; })
          .def("__repr__", [](pbr::irradiancemaps_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("IrradianceMap(%p)", d.get());
            return fxs.c_str();
          });
  /////////////////////////////////////////////////////////////////////////////////
  auto pbrcommon_type = //
      py::class_<pbr::CommonStuff, pbr::commonstuff_ptr_t>(module_lev2, "PbrCommon")
          .def_static(
              "requestIrradianceMaps",
              [](py::object path) -> pbr::irradiancemaps_ptr_t { //
                auto as_py_str = py::str(path);
                auto as_str    = as_py_str.cast<std::string>();
                printf("requestIrradianceMaps<%s>\n", as_str.c_str());
                return pbr::CommonStuff::requestIrradianceMaps(as_str);
               })
          .def(py::init<>())
          .def_property("irradianceMaps", 
             [](pbr::commonstuff_ptr_t pbc) -> pbr::irradiancemaps_ptr_t { //
               return pbc->_irradianceMaps; //
             },
              [](pbr::commonstuff_ptr_t pbc, pbr::irradiancemaps_ptr_t v) { //
                pbc->_irradianceMaps = v; //
              })
          .def("requestSkyboxTexture", [](pbr::commonstuff_ptr_t pbc, std::string path) { //
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
          .def_property(
              "depthFogDistance",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_depthFogDistance; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_depthFogDistance = v; })
          .def_property(
              "depthFogPower",
              [](pbr::commonstuff_ptr_t pbc) -> float { return pbc->_depthFogPower; },
              [](pbr::commonstuff_ptr_t pbc, float v) { pbc->_depthFogPower = v; })
          .def_property(
              "useDepthPrepass",
              [](pbr::commonstuff_ptr_t pbc) -> bool { return pbc->_useDepthPrepass; },
              [](pbr::commonstuff_ptr_t pbc, bool v) { pbc->_useDepthPrepass = v; })
          .def_property(
              "useFloatColorBuffer",
              [](pbr::commonstuff_ptr_t pbc) -> bool { return pbc->_useFloatColorBuffer; },
              [](pbr::commonstuff_ptr_t pbc, bool v) { pbc->_useFloatColorBuffer = v; })
          .def_property(
              "ssaoNumSamples",
              [](pbr::commonstuff_ptr_t pbc) -> int { return pbc->_ssaoNumSamples; },
              [](pbr::commonstuff_ptr_t pbc, int v) { pbc->_ssaoNumSamples = v; })
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
          .def_property_readonly(
              "fxcache",                                              //
              [](pbrmaterial_ptr_t m) -> fxpipelinecache_constptr_t { //
                return m->pipelineCache();                            // material
              })
          .def_property_readonly("freestyle", [](pbrmaterial_ptr_t m) -> freestyle_mtl_ptr_t { return m->_as_freestyle; })
          .def("gpuInit", [](pbrmaterial_ptr_t m, ctx_t& c) { m->gpuInit(c.get()); })
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
          .def_property(
              "doubleSided",
              [](pbrmaterial_ptr_t m) -> bool { //
                return m->_doubleSided;
              },
              [](pbrmaterial_ptr_t m, bool p) { //
                m->_doubleSided = p;
              })
          .def_property(
              "blending",
              [](pbrmaterial_ptr_t m) -> crcstring_ptr_t { //
                auto blending = m->_rasterstate._blending;
                auto crcstr   = std::make_shared<CrcString>(uint64_t(blending));
                return crcstr;
              },
              [](pbrmaterial_ptr_t m, crcstring_ptr_t ctest) { //
                m->_rasterstate._blending = Blending(ctest->hashed());
              })
          .def_property(
              "pbrcommon",
              [](pbrmaterial_ptr_t mtl) -> pbr::commonstuff_ptr_t { //
                return mtl->_commonOverride;
              },
              [](pbrmaterial_ptr_t mtl, pbr::commonstuff_ptr_t irr) { //
                mtl->_commonOverride = irr;
              });
  type_codec->registerStdCodec<pbrmaterial_ptr_t>(pbr_type);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
