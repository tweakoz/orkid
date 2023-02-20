////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/fxstate_instance.h>

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
  auto fxcachepermu_type =                                                     //
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
  type_codec->registerStdCodec<fxpipelinepermutation_ptr_t>(fxcachepermu_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto fxcache_type =                                                     //
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
  type_codec->registerStdCodec<fxpipelinecache_ptr_t>(fxcache_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto materialinst_type =                                                     //
      py::class_<FxPipeline, fxpipeline_ptr_t>(module_lev2, "FxPipeline") //
          .def(
              "bindParam",                                                                    //
              [](fxpipeline_ptr_t pipeline, //
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
                else{
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
  type_codec->registerStdCodec<fxpipeline_ptr_t>(materialinst_type);
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
              [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const tex_t& value) { m->bindParamCTex(p.get(), value.get()); })
          .def(
              "begin",
              [](freestyle_mtl_ptr_t m, pyfxtechnique_ptr_t tek, RenderContextFrameData& rcfd) { m->begin(tek.get(), rcfd); })
          .def("end", [](freestyle_mtl_ptr_t m, RenderContextFrameData& rcfd) { m->end(rcfd); })
          .def("__repr__", [](const freestyle_mtl_ptr_t m) -> std::string {
            fxstring<256> fxs;
            fxs.format("FreestyleMaterial(%p:%s)", m.get(), m->mMaterialName.c_str());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<freestyle_mtl_ptr_t>(freestyle_type);
  /////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
} // namespace ork::lev2
