////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/fx_pipeline.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
void pyinit_gfx_material(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto material_type = //
      py::class_<GfxMaterial, material_ptr_t>(module_lev2, "Material")
          .def_property("name", &GfxMaterial::GetName, &GfxMaterial::SetName)
          /*.def_property(
              "rasterstate",                                    //
              [](material_ptr_t material) -> SRasterState  { //
                return material->_rasterstate; 
              },
              [](material_ptr_t material, SRasterState rstate)  { //
                material->_rasterstate=rstate; 
              })*/
          .def("__repr__", [](material_ptr_t m) -> std::string {
            fxstring<64> fxs;
            fxs.format("GfxMaterial(%p:%s)", m.get(), m->mMaterialName.c_str());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<material_ptr_t>(material_type);
  /////////////////////////////////////////////////////////////////////////////////
  // materialinst params proxy
  /////////////////////////////////////////////////////////////////////////////////
  struct matinst_param_proxy {
    fxpipeline_ptr_t _pipeline;
  };
  using matinst_param_proxy_ptr_t = std::shared_ptr<matinst_param_proxy>;
  auto materialinst_params_type   =                                                               //
      py::class_<matinst_param_proxy, matinst_param_proxy_ptr_t>(module_lev2, "FxPipelineParams") //
          .def(
              "__repr__",
              [](matinst_param_proxy_ptr_t proxy) -> std::string {
                std::string output;
                output += FormatString("FxPipelineParams<%p>{\n", proxy.get());
                for (auto item : proxy->_pipeline->_params) {
                  const auto& k = item.first;
                  const auto& v = item.second;
                  auto vstr     = v.typestr();
                  output += FormatString("  param(%s): valtype(%s),\n", k->_name.c_str(), vstr.c_str());
                }
                output += "}\n";
                return output.c_str();
              })
          .def(
              "__setitem__",                                                                  //
              [type_codec](matinst_param_proxy_ptr_t proxy, py::object key, py::object val) { //
                auto var_key = type_codec->decode(key);
                auto var_val = type_codec->decode(val);
                if (auto as_param = var_key.tryAs<fxparam_constptr_t>()) {
                  proxy->_pipeline->_params[as_param.value()] = var_val;
                } else {
                  OrkAssert(false);
                }
              })
          .def(
              "__getitem__",                                                                //
              [type_codec](matinst_param_proxy_ptr_t proxy, py::object key) -> py::object { //
                auto var_key = type_codec->decode(key);
                if (auto as_param = var_key.tryAs<fxparam_constptr_t>()) {
                  auto it = proxy->_pipeline->_params.find(as_param.value());
                  if (it != proxy->_pipeline->_params.end()) {
                    auto var_val = it->second;
                    return type_codec->encode(var_val);
                  }
                } else {
                  OrkAssert(false);
                }
                return py::none();
              });

  type_codec->registerStdCodec<matinst_param_proxy_ptr_t>(materialinst_params_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pipelinepermu_type =                                                     //
      py::class_<FxPipelinePermutation, fxpipelinepermutation_ptr_t>(module_lev2, "FxPipelinePermutation") //
          .def(py::init<>())
          .def_property("rendering_model",
              [](fxpipelinepermutation_ptr_t permu) -> uint32_t { //
                return permu->_rendering_model;
              },
              [](fxpipelinepermutation_ptr_t permu, std::string model) { //
                permu->_rendering_model = CrcString(model.c_str()).hashed();
              }
          )
          .def_property("technique",
              [](fxpipelinepermutation_ptr_t permu) -> pyfxtechnique_ptr_t { //
                return permu->_forced_technique;
              },
              [](fxpipelinepermutation_ptr_t permu, pyfxtechnique_ptr_t tek) { //
                permu->_forced_technique = tek.get();
              }
          );
  type_codec->registerStdCodec<fxpipelinepermutation_ptr_t>(pipelinepermu_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pipelinecache_type =                                                     //
      py::class_<FxPipelineCache, fxpipelinecache_ptr_t>(module_lev2, "FxPipelineCache") //
          .def(
              "findPipeline",                                                                    //
              [](fxpipelinecache_ptr_t cache, rcid_ptr_t RCID) -> fxpipeline_ptr_t { //
                return cache->findPipeline(*RCID);
              })
          .def(
              "findPipeline",                                                                    //
              [](fxpipelinecache_ptr_t cache, 
                 fxpipelinepermutation_ptr_t permu ) -> fxpipeline_ptr_t { //
                return cache->findPipeline(*permu);
              });
  type_codec->registerStdCodec<fxpipelinecache_ptr_t>(pipelinecache_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pipeline_type =                                                     //
      py::class_<FxPipeline, fxpipeline_ptr_t>(module_lev2, "FxPipeline") //
          .def(
              "bindParam",                                                                    //
              [type_codec](fxpipeline_ptr_t pipeline, //
                 pyfxparam_ptr_t param, //
                 py::object inp_value) { //
                if( py::isinstance<CrcString>(inp_value) ){
                  pipeline->bindParam(param.get(),py::cast<crcstring_ptr_t>(inp_value));
                }
                else if( py::isinstance<py::float_>(inp_value) ){
                  float fvalue = py::cast<float>(inp_value);
                  pipeline->bindParam(param.get(),fvalue);
                }
                else if( py::isinstance<fvec2>(inp_value) ){
                  pipeline->bindParam(param.get(),py::cast<fvec2>(inp_value));
                }
                else if( py::isinstance<fvec3>(inp_value) ){
                  pipeline->bindParam(param.get(),py::cast<fvec3>(inp_value));
                }
                else if( py::isinstance<fvec4>(inp_value) ){
                  pipeline->bindParam(param.get(),py::cast<fvec4>(inp_value));
                }
                else if( py::isinstance<Texture>(inp_value) ){
                  pipeline->bindParam(param.get(),py::cast<texture_ptr_t>(inp_value));
                }
                else if( py::hasattr(inp_value, "__call__")){
                  FxPipeline::varval_generator_t L = [inp_value,type_codec]() -> FxPipeline::varval_t {
                    py::gil_scoped_acquire acquire;
                    py::object generated = inp_value();
                    FxPipeline::varval_t asv = type_codec->decode(generated);
                    return asv;
                  };
                  pipeline->bindParam(param.get(),L);
                }
                else{
                  py::print("Bad Param Type: ", inp_value);
                  OrkAssert(false);
                }
              })
          .def(
              "wrappedDrawCall",                                                                    //
              [](fxpipeline_ptr_t pipeline, //
                 rcid_ptr_t rcid, //
                 py::object method
                 ){
                auto py_callback = method.cast<pybind11::object>();
                pipeline->wrappedDrawCall(*rcid,[py_callback](){
                  py::gil_scoped_acquire acquire;
                  py_callback();
                });
              })
          .def(
              "__setattr__",                                                                    //
              [type_codec](fxpipeline_ptr_t pipeline, const std::string& key, py::object val) { //
                auto varmap_val = type_codec->decode(val);
                if (key == "technique")
                  pipeline->_technique = varmap_val.get<fxtechnique_constptr_t>();
                else if(key=="sharedMaterial"){
                  pipeline->_sharedMaterial = py::cast<material_ptr_t>(val); 
                }
                else {
                  OrkAssert(false);
                  // instance->_vars.setValueForKey(key, varmap_val);
                  // auto keys = instance->_vars.dumpkeys();
                  // for (auto k : keys)
                  // deco::printe(fvec3::Yellow(), k + " ", false);
                  // printf("\n");
                }
              })
          .def(
              "__getattr__",                                                                  //
              [type_codec](fxpipeline_ptr_t pipeline, const std::string& key) -> py::object { //
                // OrkAssert(pipeline->_vars.hasKey(key));
                FxPipeline::varval_t varval;
                // auto varmap_val = pipeline->_vars.valueForKey(key);
                if (key == "param") {
                  auto proxy           = std::make_shared<matinst_param_proxy>();
                  proxy->_pipeline = pipeline;
                  varval.set<matinst_param_proxy_ptr_t>(proxy);
                  return type_codec->encode(varval);
                } else if (key == "technique") {
                  varval.set<fxtechnique_constptr_t>(pipeline->_technique);
                  return type_codec->encode(varval);
                }
                else if(key=="sharedMaterial"){
                  return type_codec->encode(pipeline->_sharedMaterial); 
                }
                return py::none(); // ;
              });
  type_codec->registerStdCodec<fxpipeline_ptr_t>(pipeline_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto freestyle_type = //
      py::class_<FreestyleMaterial, GfxMaterial, freestyle_mtl_ptr_t>(module_lev2, "FreestyleMaterial")
          .def(py::init<>())
          .def(py::init([](ctx_t context, file::Path asset) -> freestyle_mtl_ptr_t { //
            auto rval = std::make_shared<FreestyleMaterial>();
            rval->gpuInit(context.get(), asset);
            return rval;
          }))
          .def_property_readonly(
              "fxcache",                                    //
              [](freestyle_mtl_ptr_t material) -> fxpipelinecache_constptr_t { //
                return material->pipelineCache(); // material
              })
          .def_property_readonly(
              "rasterstate",                                    //
              [](freestyle_mtl_ptr_t material) -> SRasterState&  { //
                return material->_rasterstate; 
              })
          .def(
              "gpuInit",
              [](freestyle_mtl_ptr_t m, ctx_t& c, file::Path& path) {
                m->gpuInit(c.get(), path);
                m->_rasterstate.SetCullTest(ECullTest::OFF);
              })
          .def(
              "gpuInitFromShaderText",
              [](freestyle_mtl_ptr_t m, ctx_t& c, const std::string& name, const std::string& shadertext) {
                m->gpuInitFromShaderText(c.get(), name, shadertext);
                m->_rasterstate.SetCullTest(ECullTest::OFF);
              })
          .def_property_readonly(
              "shader",
              [](const freestyle_mtl_ptr_t m) -> fxshaderasset_ptr_t { //
                return fxshaderasset_ptr_t(m->_shaderasset);
              })
          .def("technique", [](freestyle_mtl_ptr_t m, std::string name) -> pyfxtechnique_ptr_t { return pyfxtechnique_ptr_t(m->technique(name)); })
          .def("param", [](freestyle_mtl_ptr_t m, std::string name) -> pyfxparam_ptr_t { return pyfxparam_ptr_t(m->param(name)); })
          .def("bindParamFloat", [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, float value) { m->bindParamFloat(p.get(), value); })
          .def(
              "bindParamVec2",
              [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const fvec2& value) { m->bindParamVec2(p.get(), value); })
          .def(
              "bindParamVec3",
              [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const fvec3& value) { m->bindParamVec3(p.get(), value); })
          .def(
              "bindParamVec4",
              [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const fvec4& value) { m->bindParamVec4(p.get(), value); })
          .def(
              "bindParamMatrix3",
              [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const fmtx3& value) { m->bindParamMatrix(p.get(), value); })
          .def(
              "bindParamMatrix4",
              [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const fmtx4& value) { m->bindParamMatrix(p.get(), value); })
          .def(
              "bindParamTexture",
              [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const texture_ptr_t& value) { m->bindParamCTex(p.get(), value.get()); })
          .def(
              "begin",
              [](freestyle_mtl_ptr_t m, pyfxtechnique_ptr_t tek, RenderContextFrameData& rcfd) { m->begin(tek.get(), rcfd); })
          .def("end", [](freestyle_mtl_ptr_t m, RenderContextFrameData& rcfd) { m->end(rcfd); })
          .def("__repr__", [](const freestyle_mtl_ptr_t m) -> std::string {
            return FormatString("FreestyleMaterial(%p:%s)", m.get(), m->mMaterialName.c_str());
          });
  type_codec->registerStdCodec<freestyle_mtl_ptr_t>(freestyle_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pbr_type = //
      py::class_<PBRMaterial, GfxMaterial, pbrmaterial_ptr_t>(module_lev2, "PBRMaterial")
          .def(py::init<>())
          .def("__repr__", [](pbrmaterial_ptr_t m) -> std::string {
            return FormatString("PBRMaterial(%p:%s)", m.get(), m->mMaterialName.c_str());
          })
          .def("clone", [](pbrmaterial_ptr_t m) -> pbrmaterial_ptr_t {
            return m->clone();
          })
          .def("addBasicStateLambdaToPipeline", [](pbrmaterial_ptr_t m, fxpipeline_ptr_t pipe) {
            m->addBasicStateLambda(pipe);
          })
          .def_property_readonly(
              "fxcache",                                    //
              [](pbrmaterial_ptr_t m) -> fxpipelinecache_constptr_t { //
                return m->pipelineCache(); // material
              })
          .def_property_readonly(
              "freestyle",
              [](pbrmaterial_ptr_t m) -> freestyle_mtl_ptr_t {
                return m->_as_freestyle;
              })
          .def(
              "gpuInit",
              [](pbrmaterial_ptr_t m, ctx_t& c) {
                m->gpuInit(c.get());
              })
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
              [](pbrmaterial_ptr_t m, std::string p)  { //
                printf( "PBRMaterial<%p> shaderpath<%s>\n", (void*) m.get(), p.c_str() );
                m->_shaderpath = p;
              });
  type_codec->registerStdCodec<pbrmaterial_ptr_t>(pbr_type);
  /////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
} // namespace ork::lev2
