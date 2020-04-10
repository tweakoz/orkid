#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
void pyinit_gfx_shader(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fxshader_t>(module_lev2, "FxShader")
      .def(py::init<>())
      .def_property_readonly("name", [](const fxshader_t& sh) -> std::string { return sh->mName; })
      .def_property_readonly(
          "params",
          [](const fxshader_t& sh) -> fxparammap_t {
            fxparammap_t rval;
            for (auto item : sh->_parameterByName) {
              // python has no concept of const
              //  so we must cast away constness
              rval[item.first] = fxparam_t(const_cast<FxShaderParam*>(item.second));
            }
            return rval;
          })
      .def(
          "param",
          [](const fxshader_t& sh, cstrref_t named) -> fxparam_t {
            auto it = sh->_parameterByName.find(named);
            fxparam_t rval(nullptr);
            if (it != sh->_parameterByName.end())
              rval = fxparam_t(const_cast<FxShaderParam*>(it->second));
            return rval;
          })
      .def_property_readonly(
          "techniques",
          [](const fxshader_t& sh) -> fxtechniquemap_t {
            fxtechniquemap_t rval;
            for (auto item : sh->_techniques) {
              // python has no concept of const
              //  so we must cast away constness
              rval[item.first] = fxtechnique_t(const_cast<FxShaderTechnique*>(item.second));
            }
            return rval;
          })
      .def(
          "technique",
          [](const fxshader_t& sh, cstrref_t named) -> fxtechnique_t {
            auto it = sh->_techniques.find(named);
            fxtechnique_t rval(nullptr);
            if (it != sh->_techniques.end())
              rval = fxtechnique_t(const_cast<FxShaderTechnique*>(it->second));
            return rval;
          })
      .def("__repr__", [](const fxshader_t& sh) -> std::string {
        fxstring<256> fxs;
        fxs.format("FxShader(%p:%s)", sh.get(), sh->mName.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fxparam_t>(module_lev2, "FxShaderParam")
      .def_property_readonly("name", [](const fxparam_t& p) -> std::string { return p->_name; })
      .def("__repr__", [](const fxparam_t& p) -> std::string {
        fxstring<256> fxs;
        fxs.format("FxShader(%p:%s)", p.get(), p->_name.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  auto tek_type = //
      py::class_<fxtechnique_t>(module_lev2, "FxShaderTechnique")
          .def_property_readonly("name", [](const fxtechnique_t& t) -> std::string { return t->mTechniqueName; })
          .def("__repr__", [](const fxtechnique_t& t) -> std::string {
            fxstring<256> fxs;
            fxs.format("FxShaderTechnique(%p:%s)", t.get(), t->mTechniqueName.c_str());
            return fxs.c_str();
          });
  /////////////////////////////////////////////////////////////////////////////////
  type_codec->registerDecoder(
      tek_type,                                                  //
      [](const py::object& inpval, ork::varmap::val_t& outval) { //
        auto tek = inpval.cast<fxtechnique_t>();
        // printf("GOT TEK<%p:%s>\n", tek.get(), tek->mTechniqueName.c_str());
        outval.Set<fxtechnique_t>(tek);
      });
}
} // namespace ork::lev2
