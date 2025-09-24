////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
void pyinit_gfx_shader(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto shaderasset_type = //
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
                else{
                  printf("FxShaderAsset::param() no param named<%s>\n",named.c_str());
                  fflush(stdout);
                }
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
              .def("computeShader", [](const fxshaderasset_ptr_t& shass,cstrref_t named) -> pyfxcomputeshader_ptr_t {
                auto sh = shass->GetFxShader();
                return pyfxcomputeshader_ptr_t(sh->findComputeShader(named));
              })
          .def("__repr__", [](const fxshaderasset_ptr_t& shass) -> std::string {
            auto sh = shass->GetFxShader();
            fxstring<256> fxs;
            fxs.format("FxShader(%p:%s)", sh, sh->mName.c_str());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<fxshaderasset_ptr_t>(shaderasset_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto computeshader_type = //
      py::class_<pyfxcomputeshader_ptr_t>(module_lev2, "FxComputeShader");
  type_codec->registerStdCodec<pyfxcomputeshader_ptr_t>(computeshader_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto param_type = //
      py::class_<pyfxparam_ptr_t>(module_lev2, "FxShaderParam")
          .def_property_readonly("name", [](pyfxparam_ptr_t& p) -> std::string { return p->_name; })
          .def("__repr__", [](pyfxparam_ptr_t& p) -> std::string {
            if(p.get()){
              return FormatString("FxShaderParam(%p:%s)", p.get(), p->_name.c_str());
            }
            return FormatString("FxShaderParam(nil)");
          });
  type_codec->registerStdCodec<pyfxparam_ptr_t>(param_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto uniblk_type = //
      py::class_<pyfxuniblk_ptr_t>(module_lev2, "FxUniformBlock")
          .def_property_readonly("name", [](pyfxuniblk_ptr_t& p) -> std::string { return p->_name; })
          .def("__repr__", [](pyfxuniblk_ptr_t& p) -> std::string {
            if(p.get()){
              return FormatString("FxUniformBlock(%p:%s)", p.get(), p->_name.c_str());
            }
            return FormatString("FxUniformBlock(nil)");
          });
  type_codec->registerStdCodec<pyfxuniblk_ptr_t>(uniblk_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto tek_type = //
      py::class_<pyfxtechnique_ptr_t>(module_lev2, "FxShaderTechnique")
          .def_property_readonly("name", [](pyfxtechnique_ptr_t& t) -> std::string { return t->_techniqueName; })
          .def("__repr__", [](pyfxtechnique_ptr_t& t) -> std::string {
            fxstring<256> fxs;
              std::string tekname = t.get() ? t->_techniqueName.c_str() : "nulltek";
              
        
            fxs.format("FxShaderTechnique(%p:%s)", t.get(), tekname.c_str());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<pyfxtechnique_ptr_t>(tek_type);
  /////////////////////////////////////////////////////////////////////////////////
}
} // namespace ork::lev2
