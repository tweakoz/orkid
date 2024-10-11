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
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto material_type = //
      py::class_<GfxMaterial, material_ptr_t>(module_lev2, "Material")
          .def_property("name", 
            [](material_ptr_t material) -> std::string { //
              return material->mMaterialName; //
            }, 
            [](material_ptr_t material, std::string name) { //
              material->mMaterialName = name; //
            })
          .def_property_readonly(
              "rasterstate",                                    //
              [](material_ptr_t material) -> SRasterState&  { //
                return material->_rasterstate; 
              })
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
          })
          .def(
              "bindParam",                                                                    //
              [type_codec](material_ptr_t mtl, //
                 pyfxparam_ptr_t param, //
                 py::object inp_value) { //
                if( py::isinstance<CrcString>(inp_value) ){
                  mtl->bindParam(param.get(),py::cast<crcstring_ptr_t>(inp_value));
                }
                else if( py::isinstance<py::float_>(inp_value) ){
                  float fvalue = py::cast<float>(inp_value);
                  mtl->bindParam(param.get(),fvalue);
                }
                else if( py::isinstance<fvec2>(inp_value) ){
                  mtl->bindParam(param.get(),py::cast<fvec2>(inp_value));
                }
                else if( py::isinstance<fvec3>(inp_value) ){
                  mtl->bindParam(param.get(),py::cast<fvec3>(inp_value));
                }
                else if( py::isinstance<fvec4>(inp_value) ){
                  mtl->bindParam(param.get(),py::cast<fvec4>(inp_value));
                }
                else if( py::isinstance<Texture>(inp_value) ){
                  mtl->bindParam(param.get(),py::cast<texture_ptr_t>(inp_value));
                }
                else if( py::hasattr(inp_value, "__call__")){
                  auto holdname = FormatString("%s_held",param->_name.c_str());
                  mtl->_varmap.makeValueForKey<py::object>(holdname,inp_value);
                  FxPipeline::varval_generator_t L = [mtl,holdname,type_codec]() -> GfxMaterial::varval_t {
                    py::gil_scoped_acquire acquire;
                    auto held_inp_val = mtl->_varmap.typedValueForKey<py::object>(holdname);
                    py::object generated = held_inp_val.value()();
                    FxPipeline::varval_t asv = type_codec->decode(generated);
                    return asv;
                  };
                  mtl->bindParam(param.get(),L);
                }
                else{
                  py::print("Bad Param Type: ", inp_value);
                  OrkAssert(false);
                }
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
          )
          .def_property("stereo",
              [](fxpipelinepermutation_ptr_t permu) -> bool { //
                return permu->_stereo;
              },
              [](fxpipelinepermutation_ptr_t permu, bool stereo) { //
                permu->_stereo = stereo;
              }
          )
          .def_property("instanced",
              [](fxpipelinepermutation_ptr_t permu) -> bool { //
                return permu->_instanced;
              },
              [](fxpipelinepermutation_ptr_t permu, bool instanced) { //
                permu->_instanced = instanced;
              }
          )
          .def_property("skinned",
              [](fxpipelinepermutation_ptr_t permu) -> bool { //
                return permu->_skinned;
              },
              [](fxpipelinepermutation_ptr_t permu, bool skinned) { //
                permu->_skinned = skinned;
              }
          )
          .def_property("is_picking",
              [](fxpipelinepermutation_ptr_t permu) -> bool { //
                return permu->_is_picking;
              },
              [](fxpipelinepermutation_ptr_t permu, bool picking) { //
                permu->_is_picking = picking;
              }
          )
          .def_property("has_vtxcolors",
              [](fxpipelinepermutation_ptr_t permu) -> bool { //
                return permu->_has_vtxcolors;
              },
              [](fxpipelinepermutation_ptr_t permu, bool has_vtxcolors) { //
                permu->_has_vtxcolors = has_vtxcolors;
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
          .def_property("name", 
            [](fxpipeline_ptr_t pipeline) -> std::string { //
              return pipeline->_debugName; //
            }, 
            [](fxpipeline_ptr_t pipeline, std::string name) { //
              pipeline->_debugName = name; //
            })
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
                  auto holdname = FormatString("%s_held",param->_name.c_str());
                  pipeline->_vars->makeValueForKey<py::object>(holdname,inp_value);
                  FxPipeline::varval_generator_t L = [pipeline,holdname,type_codec]() -> FxPipeline::varval_t {
                    py::gil_scoped_acquire acquire;
                    auto cb = pipeline->_vars->typedValueForKey<py::object>(holdname);
                    py::object generated = cb.value()();
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
          .def("dump",[](fxpipeline_ptr_t pipeline){
            pipeline->dump();
          })
          .def_property(
              "technique",                                                                    //
              [](fxpipeline_ptr_t pipeline) -> pyfxtechnique_ptr_t { //
                return pyfxtechnique_ptr_t(pipeline->_technique);
              },
              [](fxpipeline_ptr_t pipeline, pyfxtechnique_ptr_t tek) { //
                pipeline->_technique = tek.get();
              })
          .def_property(
              "sharedMaterial",                                                                    //
              [](fxpipeline_ptr_t pipeline) -> material_ptr_t { //
                return pipeline->_sharedMaterial;
              },
              [](fxpipeline_ptr_t pipeline, material_ptr_t mtl) { //
                pipeline->_sharedMaterial = mtl;
              })
          .def_property(
              "debugPrint",                                                                    //
              [](fxpipeline_ptr_t pipeline) -> bool { //
                return pipeline->_debugPrint;
              },
              [](fxpipeline_ptr_t pipeline, bool val) { //
                pipeline->_debugPrint = val;
              })
          .def_property(
              "debugName",                                                                    //
              [](fxpipeline_ptr_t pipeline) -> std::string { //
                return pipeline->_debugName;
              },
              [](fxpipeline_ptr_t pipeline, std::string val) { //
                pipeline->_debugName = val;
              })
          /*.def(
              "__setattr__",                                                                    //
              [type_codec](fxpipeline_ptr_t pipeline, const std::string& key, py::object val) { //
                auto varmap_val = type_codec->decode(val);
                else if(key=="debugPrint"){
                  pipeline->_debugPrint = py::cast<bool>(val); 
                }
                else if(key=="debugName"){
                  pipeline->_debugName = py::cast<std::string>(val); 
                }
                else {
                  OrkAssert(false);
                  // instance->_vars.setValueForKey(key, varmap_val);
                  // auto keys = instance->_vars.dumpkeys();
                  // for (auto k : keys)
                  // deco::printe(fvec3::Yellow(), k + " ", false);
                  // printf("\n");
                }
              })*/
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
              [](freestyle_mtl_ptr_t m, pyfxtechnique_ptr_t tek, rcfd_ptr_t rcfd) { m->begin(tek.get(), rcfd); })
          .def("end", [](freestyle_mtl_ptr_t m, rcfd_ptr_t rcfd) { m->end(rcfd); })
          .def("dump", [](freestyle_mtl_ptr_t m) { m->dump(); })
          .def("__repr__", [](const freestyle_mtl_ptr_t m) -> std::string {
            return FormatString("FreestyleMaterial(%p:%s)", m.get(), m->mMaterialName.c_str());
          });
  type_codec->registerStdCodec<freestyle_mtl_ptr_t>(freestyle_type);

  /////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
} // namespace ork::lev2
