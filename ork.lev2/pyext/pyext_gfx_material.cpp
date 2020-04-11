#include "pyext.h"
#include <ork/kernel/string/deco.inl>

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
            fxs.format("GfxMaterial(%p:%s)", m, m->mMaterialName.c_str());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<material_ptr_t>(material_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto materialinst_type =                                                                 //
      py::class_<GfxMaterialInstance, materialinst_ptr_t>(module_lev2, "MaterialInstance") //
          .def(py::init<material_ptr_t>())
          .def(
              "__setitem__",                                                                      //
              [type_codec](materialinst_ptr_t instance, const std::string& key, py::object val) { //
                auto varmap_val = type_codec->decode(val);
                instance->_vars.setValueForKey(key, varmap_val);
                auto keys = instance->_vars.dumpkeys();
                for (auto k : keys)
                  deco::printe(fvec3::Yellow(), k + " ", false);
                printf("\n");
              })
          .def(
              "__getitem__",                                                                    //
              [type_codec](materialinst_ptr_t instance, const std::string& key) -> py::object { //
                OrkAssert(instance->_vars.hasKey(key));
                auto varmap_val = instance->_vars.valueForKey(key);
                return type_codec->encode(varmap_val);
              });
  type_codec->registerStdCodec<materialinst_ptr_t>(materialinst_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto freestyle_type = //
      py::class_<FreestyleMaterial, GfxMaterial, freestyle_mtl_ptr_t>(module_lev2, "FreestyleMaterial")
          .def(py::init<>())
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
          .def_property_readonly("shader", [](const freestyle_mtl_ptr_t m) -> pyfxshader_ptr_t { return pyfxshader_ptr_t(m->_shader); })
          .def("bindTechnique", [](freestyle_mtl_ptr_t m, const pyfxtechnique_ptr_t& tek) { m->bindTechnique(tek.get()); })
          .def("bindParamFloat", [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, float value) { m->bindParamFloat(p.get(), value); })
          .def("bindParamVec2", [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const fvec2& value) { m->bindParamVec2(p.get(), value); })
          .def("bindParamVec3", [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const fvec3& value) { m->bindParamVec3(p.get(), value); })
          .def("bindParamVec4", [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const fvec4& value) { m->bindParamVec4(p.get(), value); })
          .def(
              "bindParamMatrix3",
              [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const fmtx3& value) { m->bindParamMatrix(p.get(), value); })
          .def(
              "bindParamMatrix4",
              [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const fmtx4& value) { m->bindParamMatrix(p.get(), value); })
          .def(
              "bindParamTexture",
              [](freestyle_mtl_ptr_t m, pyfxparam_ptr_t& p, const tex_t& value) { m->bindParamCTex(p.get(), value.get()); })
          .def("begin", [](freestyle_mtl_ptr_t m, RenderContextFrameData& rcfd) { m->begin(rcfd); })
          .def("end", [](freestyle_mtl_ptr_t m, RenderContextFrameData& rcfd) { m->end(rcfd); })
          .def("__repr__", [](const freestyle_mtl_ptr_t m) -> std::string {
            fxstring<256> fxs;
            fxs.format("FreestyleMaterial(%p:%s)", m, m->mMaterialName.c_str());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<freestyle_mtl_ptr_t>(freestyle_type);
  /////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
} // namespace ork::lev2
