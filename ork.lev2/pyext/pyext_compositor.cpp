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

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_compositor(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto compositorpassdata_type = //
      py::class_<CompositingPassData, compositingpassdata_ptr_t>(module_lev2, "CompositingPassData")
          .def(py::init<>())
          .def_property("cameramatrices",
            [](compositingpassdata_ptr_t cpd) -> cameramatrices_ptr_t {
              return cpd->_shared_cameraMatrices;
            },
            [](compositingpassdata_ptr_t cpd, cameramatrices_ptr_t m){
              cpd->setSharedCameraMatrices(m);
            }
          )
          .def("__repr__", [](compositingpassdata_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingPassData(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositingpassdata_ptr_t>(compositorpassdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<RenderPresetContext>(module_lev2, "RenderPresetContext");
  /////////////////////////////////////////////////////////////////////////////////
  auto compositorrnode_type = //
      py::class_<RenderCompositingNode, compositorrendernode_ptr_t>(module_lev2, "RenderCompositingNode")
          .def_property("layers",
            [](compositorrendernode_ptr_t rnode) -> std::string {
              return rnode->_layers;
            },
            [](compositorrendernode_ptr_t rnode, std::string l){
              rnode->_layers = l;
            }
          )
          .def("__repr__", [](compositorrendernode_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("RenderCompositingNode(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositorrendernode_ptr_t>(compositorrnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto compositoronode_type = //
      py::class_<OutputCompositingNode, compositoroutnode_ptr_t>(module_lev2, "OutputCompositingNode")
          .def("__repr__", [](compositoroutnode_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("OutputCompositingNode(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositoroutnode_ptr_t>(compositoronode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto compositordata_type = //
      py::class_<CompositingData, compositordata_ptr_t>(module_lev2, "CompositingData")
          .def(py::init<>())
          .def("__repr__", [](compositordata_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingData(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositordata_ptr_t>(compositordata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto compositorimpl_type = //
      py::class_<CompositingImpl, compositorimpl_ptr_t>(module_lev2, "CompositingImpl")
          .def(py::init([](compositordata_ptr_t cdata) -> compositorimpl_ptr_t { //
            return std::make_shared<CompositingImpl>(cdata);
          }))
          .def("pushCPD", 
               [](compositorimpl_ptr_t ci, 
                  compositingpassdata_ptr_t cpd) {
                ci->pushCPD(*cpd);
          })
          .def("popCPD", 
               [](compositorimpl_ptr_t ci) {
                ci->popCPD();
          })
          .def("__repr__", [](compositorimpl_ptr_t i) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingImpl(%p)", i.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositorimpl_ptr_t>(compositorimpl_type);
  /////////////////////////////////////////////////////////////////////////////////

}
} //namespace ork::lev2 {
