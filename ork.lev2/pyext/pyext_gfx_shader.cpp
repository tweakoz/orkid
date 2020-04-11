#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
void pyinit_gfx_shader(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto shader_type = //
      py::class_<pyfxshader_ptr_t>(module_lev2, "FxShader")
          .def(py::init<>())
          .def_property_readonly("name", [](const pyfxshader_ptr_t& sh) -> std::string { return sh->mName; })
          .def_property_readonly(
              "params",
              [](const pyfxshader_ptr_t& sh) -> fxparammap_t {
                fxparammap_t rval;
                for (auto item : sh->_parameterByName) {
                  rval[item.first] = pyfxparam_ptr_t(item.second);
                }
                return rval;
              })
          .def(
              "param",
              [](const pyfxshader_ptr_t& sh, cstrref_t named) -> pyfxparam_ptr_t {
                auto it = sh->_parameterByName.find(named);
                pyfxparam_ptr_t rval(nullptr);
                if (it != sh->_parameterByName.end())
                  rval = pyfxparam_ptr_t(it->second);
                return rval;
              })
          .def_property_readonly(
              "techniques",
              [](const pyfxshader_ptr_t& sh) -> fxtechniquemap_t {
                fxtechniquemap_t rval;
                for (auto item : sh->_techniques) {
                  rval[item.first] = pyfxtechnique_ptr_t(item.second);
                }
                return rval;
              })
          .def(
              "technique",
              [](const pyfxshader_ptr_t& sh, cstrref_t named) -> pyfxtechnique_ptr_t {
                auto it = sh->_techniques.find(named);
                pyfxtechnique_ptr_t rval(nullptr);
                if (it != sh->_techniques.end())
                  rval = pyfxtechnique_ptr_t(it->second);
                return rval;
              })
          .def("__repr__", [](const pyfxshader_ptr_t& sh) -> std::string {
            fxstring<256> fxs;
            fxs.format("FxShader(%p:%s)", sh.get(), sh->mName.c_str());
            return fxs.c_str();
          });
  type_codec->registerRawPtrCodec<pyfxshader_ptr_t, fxshader_constptr_t>(shader_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto param_type = //
      py::class_<pyfxparam_ptr_t>(module_lev2, "FxShaderParam")
          .def_property_readonly("name", [](const pyfxparam_ptr_t& p) -> std::string { return p->_name; })
          .def("__repr__", [](const pyfxparam_ptr_t& p) -> std::string {
            fxstring<256> fxs;
            fxs.format("FxShader(%p:%s)", p.get(), p->_name.c_str());
            return fxs.c_str();
          });
  type_codec->registerRawPtrCodec<pyfxparam_ptr_t, fxparam_constptr_t>(param_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto tek_type = //
      py::class_<pyfxtechnique_ptr_t>(module_lev2, "FxShaderTechnique")
          .def_property_readonly("name", [](const pyfxtechnique_ptr_t& t) -> std::string { return t->mTechniqueName; })
          .def("__repr__", [](const pyfxtechnique_ptr_t& t) -> std::string {
            fxstring<256> fxs;
            fxs.format("FxShaderTechnique(%p:%s)", t.get(), t->mTechniqueName.c_str());
            return fxs.c_str();
          });
  type_codec->registerRawPtrCodec<pyfxtechnique_ptr_t, fxtechnique_constptr_t>(tek_type);
  /////////////////////////////////////////////////////////////////////////////////
}
} // namespace ork::lev2
