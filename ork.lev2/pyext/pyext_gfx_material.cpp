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
    fxinstance_ptr_t _materialinst;
  };
  using matinst_param_proxy_ptr_t = std::shared_ptr<matinst_param_proxy>;
  auto materialinst_params_type   =                                                               //
      py::class_<matinst_param_proxy, matinst_param_proxy_ptr_t>(module_lev2, "FxInstanceParams") //
          .def(
              "__repr__",
              [](matinst_param_proxy_ptr_t proxy) -> std::string {
                std::string output;
                output += FormatString("FxInstanceParams<%p>{\n", proxy.get());
                for (auto item : proxy->_materialinst->_params) {
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
                  proxy->_materialinst->_params[as_param.value()] = var_val;
                } else {
                  OrkAssert(false);
                }
              })
          .def(
              "__getitem__",                                                                //
              [type_codec](matinst_param_proxy_ptr_t proxy, py::object key) -> py::object { //
                auto var_key = type_codec->decode(key);
                if (auto as_param = var_key.tryAs<fxparam_constptr_t>()) {
                  auto it = proxy->_materialinst->_params.find(as_param.value());
                  if (it != proxy->_materialinst->_params.end()) {
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
  auto materialinst_type =                                                     //
      py::class_<FxStateInstance, fxinstance_ptr_t>(module_lev2, "FxInstance") //
          .def(
              "__setattr__",                                                                    //
              [type_codec](fxinstance_ptr_t instance, const std::string& key, py::object val) { //
                auto varmap_val = type_codec->decode(val);
                if (key == "technique")
                  instance->_technique = varmap_val.get<fxtechnique_constptr_t>();
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
              [type_codec](fxinstance_ptr_t instance, const std::string& key) -> py::object { //
                // OrkAssert(instance->_vars.hasKey(key));
                FxStateInstance::varval_t varval;
                // auto varmap_val = instance->_vars.valueForKey(key);
                if (key == "param") {
                  auto proxy           = std::make_shared<matinst_param_proxy>();
                  proxy->_materialinst = instance;
                  varval.set<matinst_param_proxy_ptr_t>(proxy);
                  return type_codec->encode(varval);
                } else if (key == "technique") {
                  varval.set<fxtechnique_constptr_t>(instance->_technique);
                  return type_codec->encode(varval);
                }
                return py::none(); // ;
              });
  type_codec->registerStdCodec<fxinstance_ptr_t>(materialinst_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto freestyle_type = //
      py::class_<FreestyleMaterial, GfxMaterial, freestyle_mtl_ptr_t>(module_lev2, "FreestyleMaterial")
          .def(py::init<>())
          .def(py::init([](ctx_t context, file::Path asset) -> freestyle_mtl_ptr_t { //
            auto rval = std::make_shared<FreestyleMaterial>();
            rval->gpuInit(context.get(), asset);
            return rval;
          }))
          .def(
              "createFxInstance",                                    //
              [](freestyle_mtl_ptr_t material) -> fxinstance_ptr_t { //
                FxStateInstanceConfig cfg;
                return std::make_shared<FxStateInstance>(cfg); // material
              })
          .def(
              "gpuInit",
              [](freestyle_mtl_ptr_t m, ctx_t& c, file::Path& path) {
                m->gpuInit(c.get(), path);
                m->_rasterstate.SetCullTest(ECULLTEST_OFF);
              })
          .def(
              "gpuInitFromShaderText",
              [](freestyle_mtl_ptr_t m, ctx_t& c, const std::string& name, const std::string& shadertext) {
                m->gpuInitFromShaderText(c.get(), name, shadertext);
                m->_rasterstate.SetCullTest(ECULLTEST_OFF);
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
