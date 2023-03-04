////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
void pyinit_gfx_shader(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto shader_type = //
      py::class_<FxShaderAsset, fxshaderasset_ptr_t>(module_lev2, "FxShaderAsset")
          .def_property_readonly(
              "name",
              [](const fxshaderasset_ptr_t& shass) -> std::string { //
                auto sh = shass->GetFxShader();
                return sh->mName;
              })
          .def_property_readonly(
              "params",
              [](const fxshaderasset_ptr_t& shass) -> fxparammap_t {
                auto sh = shass->GetFxShader();
                fxparammap_t rval;
                for (auto item : sh->_parameterByName) {
                  rval[item.first] = pyfxparam_ptr_t(item.second);
                }
                return rval;
              })
          .def(
              "param",
              [](const fxshaderasset_ptr_t& shass, cstrref_t named) -> pyfxparam_ptr_t {
                auto sh = shass->GetFxShader();
                auto it = sh->_parameterByName.find(named);
                pyfxparam_ptr_t rval(nullptr);
                if (it != sh->_parameterByName.end())
                  rval = pyfxparam_ptr_t(it->second);
                return rval;
              })
          .def_property_readonly(
              "techniques",
              [](const fxshaderasset_ptr_t& shass) -> fxtechniquemap_t {
                auto sh = shass->GetFxShader();
                fxtechniquemap_t rval;
                for (auto item : sh->_techniques) {
                  rval[item.first] = pyfxtechnique_ptr_t(item.second);
                }
                return rval;
              })
          .def(
              "technique",
              [](const fxshaderasset_ptr_t& shass, cstrref_t named) -> pyfxtechnique_ptr_t {
                auto sh = shass->GetFxShader();
                auto it = sh->_techniques.find(named);
                pyfxtechnique_ptr_t rval(nullptr);
                if (it != sh->_techniques.end())
                  rval = pyfxtechnique_ptr_t(it->second);
                return rval;
              })
          .def("__repr__", [](const fxshaderasset_ptr_t& shass) -> std::string {
            auto sh = shass->GetFxShader();
            fxstring<256> fxs;
            fxs.format("FxShader(%p:%s)", sh, sh->mName.c_str());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<fxshaderasset_ptr_t>(shader_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto param_type = //
      py::class_<pyfxparam_ptr_t>(module_lev2, "FxShaderParam")
          .def_property_readonly("name", [](const pyfxparam_ptr_t& p) -> std::string { return p->_name; })
          .def("__repr__", [](const pyfxparam_ptr_t& p) -> std::string {
            if(p.get()){
              return FormatString("FxShaderParam(%p:%s)", p.get(), p->_name.c_str());
            }
            return FormatString("FxShaderParam(nil)");
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
