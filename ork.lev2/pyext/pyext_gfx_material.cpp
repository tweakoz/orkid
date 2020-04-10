#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

struct PythonTypeCodec {
  //////////////////////////////////
  ork::varmap::val_t decode(const py::object& val) const {
    ork::varmap::val_t rval;
    auto type = val.get_type();
    if (type.is(_int_type)) {
      rval.Set<int>(val.cast<int>());
    } else if (type.is(_float_type)) {
      rval.Set<float>(val.cast<float>());
    } else if (type.is(_str_type)) {
      rval.Set<std::string>(val.cast<std::string>());
    } else {
      OrkAssert(false);
    }
    return rval;
  }
  //////////////////////////////////
  PythonTypeCodec() {
    _builtins   = py::module::import("builtins");
    _int_type   = _builtins.attr("int");
    _float_type = _builtins.attr("float");
    _str_type   = _builtins.attr("str");
  }
  //////////////////////////////////
  static std::shared_ptr<PythonTypeCodec> instance() {
    static auto _instance = std::make_shared<PythonTypeCodec>();
    return _instance;
  }
  //////////////////////////////////
  py::module _builtins;
  py::object _int_type;
  py::object _float_type;
  py::object _str_type;
};

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
void pyinit_gfx_material(py::module& module_lev2) {
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<GfxMaterial, material_ptr_t>(module_lev2, "Material")
      .def_property("name", &GfxMaterial::GetName, &GfxMaterial::SetName)
      .def("__repr__", [](material_ptr_t m) -> std::string {
        fxstring<64> fxs;
        fxs.format("GfxMaterial(%p:%s)", m, m->mMaterialName.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<GfxMaterialInstance, materialinst_ptr_t>(module_lev2, "MaterialInstance") //
      .def(py::init<material_ptr_t>())
      .def("__setitem__", [](materialinst_ptr_t instance, const std::string& key, py::object val) { //
        auto type_codec = PythonTypeCodec::instance();
        auto varmap_val = type_codec->decode(val);
        instance->_vars.setValueForKey(key, varmap_val);
      });
  /////////////////////////////////////////////////////////////////////////////////
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
      .def_property_readonly("shader", [](const freestyle_mtl_ptr_t m) -> fxshader_t { return fxshader_t(m->_shader); })
      .def("bindTechnique", [](freestyle_mtl_ptr_t m, const fxtechnique_t& tek) { m->bindTechnique(tek.get()); })
      .def("bindParamFloat", [](freestyle_mtl_ptr_t m, fxparam_t& p, float value) { m->bindParamFloat(p.get(), value); })
      .def("bindParamVec2", [](freestyle_mtl_ptr_t m, fxparam_t& p, const fvec2& value) { m->bindParamVec2(p.get(), value); })
      .def("bindParamVec3", [](freestyle_mtl_ptr_t m, fxparam_t& p, const fvec3& value) { m->bindParamVec3(p.get(), value); })
      .def("bindParamVec4", [](freestyle_mtl_ptr_t m, fxparam_t& p, const fvec4& value) { m->bindParamVec4(p.get(), value); })
      .def("bindParamMatrix3", [](freestyle_mtl_ptr_t m, fxparam_t& p, const fmtx3& value) { m->bindParamMatrix(p.get(), value); })
      .def("bindParamMatrix4", [](freestyle_mtl_ptr_t m, fxparam_t& p, const fmtx4& value) { m->bindParamMatrix(p.get(), value); })
      .def(
          "bindParamTexture",
          [](freestyle_mtl_ptr_t m, fxparam_t& p, const tex_t& value) { m->bindParamCTex(p.get(), value.get()); })
      .def("begin", [](freestyle_mtl_ptr_t m, RenderContextFrameData& rcfd) { m->begin(rcfd); })
      .def("end", [](freestyle_mtl_ptr_t m, RenderContextFrameData& rcfd) { m->end(rcfd); })
      .def("__repr__", [](const freestyle_mtl_ptr_t m) -> std::string {
        fxstring<256> fxs;
        fxs.format("FreestyleMaterial(%p:%s)", m, m->mMaterialName.c_str());
        return fxs.c_str();
      });
}
} // namespace ork::lev2
