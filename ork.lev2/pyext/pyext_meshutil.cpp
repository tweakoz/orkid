#include "pyext.h"
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <pybind11/eigen.h>
#include <ork/lev2/gfx/meshutil/igl.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
using namespace meshutil;
using rigidprim_t = RigidPrimitive<SVtxV12N12B12T8C4>;
void pyinit_meshutil(py::module& module_lev2) {
  auto meshutil = module_lev2.def_submodule("meshutil", "Mesh operations");
  meshutil.def("triangulate", [](const submesh& inpsubmesh, submesh& outsubmesh) { submeshTriangulate(inpsubmesh, outsubmesh); });
  py::class_<IglMesh, iglmesh_ptr_t>(meshutil, "IglMesh") //
      .def(
          "__repr__",
          [](iglmesh_ptr_t iglmesh) -> std::string {
            fxstring<64> fxs;
            fxs.format("IglMesh(%p)", iglmesh.get());
            return fxs.c_str();
          })
      .def_property(
          "vertices",
          [](iglmesh_ptr_t iglm) -> Eigen::MatrixXd { //
            return iglm->_verts;
          },
          [](iglmesh_ptr_t iglm, Eigen::MatrixXd inp) { //
            iglm->_verts = inp;
          })
      .def_property(
          "faces",
          [](iglmesh_ptr_t iglm) -> Eigen::MatrixXi { //
            return iglm->_faces;
          },
          [](iglmesh_ptr_t iglm, Eigen::MatrixXi inp) { //
            iglm->_faces = inp;
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<rigidprim_t>(meshutil, "RigidPrimitive")
      .def(py::init<>())
      .def(py::init([](submesh& submesh, ctx_t context) {
        auto prim = std::unique_ptr<rigidprim_t>(new rigidprim_t);
        prim->fromSubMesh(submesh, context.get());
        return prim;
      }))
      .def("fromSubMesh", [](rigidprim_t& prim, const submesh& submesh, Context* context) { prim.fromSubMesh(submesh, context); })
      .def("renderEML", [](rigidprim_t& prim, ctx_t context) { prim.renderEML(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<submesh>(meshutil, "SubMesh")
      .def(py::init<>())
      .def("igl_test", [](submesh& submesh) { return submesh.igl_test(); })
      .def("toIglMesh", [](submesh& submesh) -> iglmesh_ptr_t { return submesh.toIglMesh(); })
      .def("numPolys", [](const submesh& submesh, int numsides = 0) -> int { return submesh.GetNumPolys(numsides); })
      .def("numVertices", [](const submesh& submesh) -> int { return submesh._vtxpool.GetNumVertices(); })
      .def("writeObj", [](const submesh& submesh, const std::string& outpath) { return submeshWriteObj(submesh, outpath); })
      .def("addQuad", [](submesh& submesh, fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3) { return submesh.addQuad(p0, p1, p2, p3); })
      .def(
          "addQuad",
          [](submesh& submesh, fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3, fvec4 c) { return submesh.addQuad(p0, p1, p2, p3, c); })
      .def(
          "addQuad",
          [](submesh& submesh, fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3, fvec2 uv0, fvec2 uv1, fvec2 uv2, fvec2 uv3, fvec4 c) {
            return submesh.addQuad(p0, p1, p2, p3, uv0, uv1, uv2, uv3, c);
          })
      .def(
          "addQuad",
          [](submesh& submesh,
             fvec3 p0,
             fvec3 p1,
             fvec3 p2,
             fvec3 p3,
             fvec3 n0,
             fvec3 n1,
             fvec3 n2,
             fvec3 n3,
             fvec2 uv0,
             fvec2 uv1,
             fvec2 uv2,
             fvec2 uv3,
             fvec4 c) { return submesh.addQuad(p0, p1, p2, p3, n0, n1, n2, n3, uv0, uv1, uv2, uv3, c); });

  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vertexpool>(meshutil, "VertexPool").def(py::init<>());
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<poly>(meshutil, "Poly").def(py::init<>());
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<edge>(meshutil, "Edge").def(py::init<>());
}

} // namespace ork::lev2
