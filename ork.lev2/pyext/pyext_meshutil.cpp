////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
using rigidprim_t = RigidPrimitive<SVtxV12N12B12T8C4>;
void pyinit_meshutil(py::module& module_lev2) {
  auto module_meshutil = module_lev2.def_submodule("meshutil", "Mesh operations");
  auto type_codec = python::TypeCodec::instance();
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
  py::class_<rigidprim_t>(module_meshutil, "RigidPrimitive")
      .def(py::init<>())
      .def(py::init([](submesh_ptr_t submesh, ctx_t context) {
        auto prim = std::unique_ptr<rigidprim_t>(new rigidprim_t);
        prim->fromSubMesh(*submesh, context.get());
        return prim;
      }))
      .def("createNode", [](rigidprim_t& prim, //
                            std::string named, //
                            scenegraph::layer_ptr_t layer, //
                            fxpipeline_ptr_t mtl_inst) -> scenegraph::drawable_node_ptr_t { // 
            auto node                                                 //
                = prim.createNode(named, layer, mtl_inst);
            //node->_userdata->template makeValueForKey<T>("_primitive") = prim; // hold on to reference
            return node;
      })
      .def("fromSubMesh", [](rigidprim_t& prim, submesh_ptr_t submesh, Context* context) { prim.fromSubMesh(*submesh, context); })
      .def("renderEML", [](rigidprim_t& prim, ctx_t context) { prim.renderEML(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  auto mesh_type = py::class_<Mesh, mesh_ptr_t>(module_meshutil, "Mesh") //
      .def(py::init<>())
      .def_property_readonly(
          "polygroups",                              //
          [](mesh_ptr_t the_mesh) -> submesh_lut_t { //
            return the_mesh->_submeshesByPolyGroup;
          })
      .def_property_readonly(
          "submesh_list",                              //
          [](mesh_ptr_t the_mesh) -> py::list { //
            py::list rval;
            for( auto item : the_mesh->_submeshesByPolyGroup ){
                auto subm = item.second;
                rval.append(subm);
            }
            return rval;
          })
      .def("readFromWavefrontObj", [](mesh_ptr_t the_mesh, std::string pth) { //
        the_mesh->ReadFromWavefrontObj(pth);
      });

  type_codec->registerStdCodec<mesh_ptr_t>(mesh_type);

    pyinit_meshutil_submesh(module_meshutil);
    pyinit_meshutil_component(module_meshutil);

    #if defined(ENABLE_IGL)
    pyinit_meshutil_igl(module_meshutil);
    #endif

  //////////////////////////////////////////////////////////////////////////////
} // void pyinit_meshutil(py::module& module_lev2) {

} // namespace ork::lev2
