////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <pybind11/eigen.h>
#include <ork/lev2/gfx/meshutil/igl.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
submesh_ptr_t submeshFromEigen(
    const Eigen::MatrixXd& verts, //
    const Eigen::MatrixXi& faces,
    const Eigen::MatrixXd& uvs,
    const Eigen::MatrixXd& colors,
    const Eigen::MatrixXd& normals,
    const Eigen::MatrixXd& binormals,
    const Eigen::MatrixXd& tangents);
}
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {

void pyinit_meshutil_igl(py::module& module_meshutil);
void pyinit_meshutil_submesh(py::module& module_meshutil);
void pyinit_meshutil_component(py::module& module_meshutil);

using namespace meshutil;
void pyinit_meshutil(py::module& module_lev2) {
  auto module_meshutil = module_lev2.def_submodule("meshutil", "Mesh operations");
  auto type_codec      = python::pb11_typecodec_t::instance();
  //////////////////////////////////////////////////////////////////////////////
  module_meshutil.def("submeshFromNumPy", [](py::kwargs kwargs) -> submesh_ptr_t {
    if (kwargs) {
      Eigen::MatrixXd verts;
      Eigen::MatrixXi faces;
      Eigen::MatrixXd uvs;
      Eigen::MatrixXd colors;
      Eigen::MatrixXd normals;
      Eigen::MatrixXd binormals;
      Eigen::MatrixXd tangents;
      for (auto item : kwargs) {
        auto key = py::cast<std::string>(item.first);
        if (key == "vertices") {
          verts = py::cast<Eigen::MatrixXd>(item.second);
        } else if (key == "faces") {
          faces = py::cast<Eigen::MatrixXi>(item.second);
        } else if (key == "uvs") {
          uvs = py::cast<Eigen::MatrixXd>(item.second);
        } else if (key == "colors") {
          colors = py::cast<Eigen::MatrixXd>(item.second);
        } else if (key == "normals") {
          normals = py::cast<Eigen::MatrixXd>(item.second);
        } else if (key == "binormals") {
          binormals = py::cast<Eigen::MatrixXd>(item.second);
        } else if (key == "tangents") {
          tangents = py::cast<Eigen::MatrixXd>(item.second);
        }
      } // for (auto item : kwargs) {
      auto rval = submeshFromEigen(
          verts, //
          faces,
          uvs,
          colors,
          normals,
          binormals,
          tangents);
      return rval;
    } else {
      return nullptr;
    } // namespace ork::lev2
  });
/////////////////////////////////////////////////////////////////////////////////
  auto meshxf_type = //
    py::class_<MeshTransformerPipe,mesh_transformer_pipe_ptr_t> //
    (module_meshutil, "MeshTransformerPipe") //
    .def(py::init([](py::list mtf_array) -> mesh_transformer_pipe_ptr_t {
      auto rval = std::make_shared<MeshTransformerPipe>();
      for( auto item : mtf_array ){
        // item should be a python function that takes a mesh_ptr_t and returns a mesh_ptr_t
        auto pyfn = py::cast<py::function>(item);
        auto xf = [pyfn](mesh_ptr_t inp) -> mesh_ptr_t {
          OrkAssert(false);
          py::object xf_rval;
          {
            py::gil_scoped_acquire acquire;
            xf_rval = pyfn(inp);
            OrkAssert(py::isinstance<mesh_ptr_t>(xf_rval));
          }
          return py::cast<mesh_ptr_t>(xf_rval);
        };
        rval->_transformers.push_back(xf);
      }
      return rval;
    }));
  type_codec->registerStdCodec<mesh_transformer_pipe_ptr_t>(meshxf_type);
/////////////////////////////////////////////////////////////////////////////////
  auto mesh_type = //
    py::class_<Mesh, mesh_ptr_t> //
    (module_meshutil, "Mesh") //
      .def(py::init<>())
      .def_property_readonly(
         "polygroups",                              //
         [](mesh_ptr_t the_mesh) -> submesh_lut_t { //
           return the_mesh->_submeshesByPolyGroup;
         })
      .def_property_readonly(
         "submesh_list",                       //
         [](mesh_ptr_t the_mesh) -> py::list { //
           py::list rval;
           for (auto item : the_mesh->_submeshesByPolyGroup) {
             auto subm = item.second;
             rval.append(subm);
           }
           return rval;
         })
      .def(
         "readFromWavefrontObj",
         [](mesh_ptr_t the_mesh, std::string pth) { //
           the_mesh->ReadFromWavefrontObj(pth);
         })
      .def(
         "readFromXGM",
         [](mesh_ptr_t the_mesh, std::string pth) { //
           the_mesh->ReadFromXGM(pth);
         })
      .def("readFromAssimp", [](mesh_ptr_t the_mesh, std::string pth) { //
       the_mesh->readFromAssimp(file::Path(pth));
      });

  type_codec->registerStdCodec<mesh_ptr_t>(mesh_type);
  /////////////////////////////////////////////////////////////////////////////
  pyinit_meshutil_submesh(module_meshutil);
  pyinit_meshutil_component(module_meshutil);
  /////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_IGL)
  pyinit_meshutil_igl(module_meshutil);
#endif

  //////////////////////////////////////////////////////////////////////////////
} // void pyinit_meshutil(py::module& module_lev2) {

} // namespace ork::lev2
