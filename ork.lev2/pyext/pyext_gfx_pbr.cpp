////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_pbr(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto pbrcommon_type = //
      py::class_<pbr::CommonStuff, pbr::commonstuff_ptr_t>(module_lev2, "PbrCommon")
          .def(py::init<>())
          .def("requestSkyboxTexture",[](pbr::commonstuff_ptr_t pbc, std::string path){
            pbc->requestSkyboxTexture(path);
          })
          .def_property("environmentIntensity",
            [](pbr::commonstuff_ptr_t pbc) -> float {
              return pbc->_environmentIntensity;
            },
            [](pbr::commonstuff_ptr_t pbc, float v) {
              pbc->_environmentIntensity = v;
            })
          .def_property("environmentMipBias",
            [](pbr::commonstuff_ptr_t pbc) -> float {
              return pbc->_environmentMipBias;
            },
            [](pbr::commonstuff_ptr_t pbc, float v) {
              pbc->_environmentMipBias = v;
            })
          .def_property("environmentMipScale",
            [](pbr::commonstuff_ptr_t pbc) -> float {
              return pbc->_environmentMipScale;
            },
            [](pbr::commonstuff_ptr_t pbc, float v) {
              pbc->_environmentMipScale = v;
            })
          .def_property("ambientLevel",
            [](pbr::commonstuff_ptr_t pbc) -> fvec3 {
              return pbc->_ambientLevel;
            },
            [](pbr::commonstuff_ptr_t pbc, fvec3 v) {
              pbc->_ambientLevel = v;
            })
          .def_property("diffuseLevel",
            [](pbr::commonstuff_ptr_t pbc) -> float {
              return pbc->_diffuseLevel;
            },
            [](pbr::commonstuff_ptr_t pbc, float v) {
              pbc->_diffuseLevel = v;
            })
          .def_property("specularLevel",
            [](pbr::commonstuff_ptr_t pbc) -> float {
              return pbc->_specularLevel;
            },
            [](pbr::commonstuff_ptr_t pbc, float v) {
              pbc->_specularLevel = v;
            })
          .def_property("specularMipBias",
            [](pbr::commonstuff_ptr_t pbc) -> float {
              return pbc->_specularMipBias;
            },
            [](pbr::commonstuff_ptr_t pbc, float v) {
              pbc->_specularMipBias = v;
            })
          .def_property("skyboxLevel",
            [](pbr::commonstuff_ptr_t pbc) -> float {
              return pbc->_skyboxLevel;
            },
            [](pbr::commonstuff_ptr_t pbc, float v) {
              pbc->_skyboxLevel = v;
            })
          .def_property("depthFogDistance",
            [](pbr::commonstuff_ptr_t pbc) -> float {
              return pbc->_depthFogDistance;
            },
            [](pbr::commonstuff_ptr_t pbc, float v) {
              pbc->_depthFogDistance = v;
            })
          .def_property("depthFogPower",
            [](pbr::commonstuff_ptr_t pbc) -> float {
              return pbc->_depthFogPower;
            },
            [](pbr::commonstuff_ptr_t pbc, float v) {
              pbc->_depthFogPower = v;
            })
          .def("__repr__", [](pbr::commonstuff_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("PbrCommon(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<pbr::commonstuff_ptr_t>(pbrcommon_type);

}

///////////////////////////////////////////////////////////////////////////////

} //namespace ork::lev2 {
