#include "pyext.h"
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
using rigidprim_t = meshutil::RigidPrimitive<SVtxV12N12B12T8C4>;
void pyinit_meshutil(py::module& module_lev2) {
  auto meshutil = module_lev2.def_submodule("meshutil", "Mesh operations");
  meshutil.def("triangulate", [](const meshutil::submesh& inpsubmesh, meshutil::submesh& outsubmesh) {
    meshutil::submeshTriangulate(inpsubmesh, outsubmesh);
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<rigidprim_t>(meshutil, "RigidPrimitive")
      .def(py::init<>())
      .def(py::init([](meshutil::submesh& submesh, ctx_t context) {
        auto prim = std::unique_ptr<rigidprim_t>(new rigidprim_t);
        prim->fromSubMesh(submesh, context.get());
        return prim;
      }))
      .def(
          "fromSubMesh",
          [](rigidprim_t& prim, const meshutil::submesh& submesh, Context* context) { prim.fromSubMesh(submesh, context); })
      .def("draw", [](rigidprim_t& prim, ctx_t context) { prim.draw(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<meshutil::submesh>(meshutil, "SubMesh")
      .def(py::init<>())
      .def("numPolys", [](const meshutil::submesh& submesh, int numsides = 0) -> int { return submesh.GetNumPolys(numsides); })
      .def("numVertices", [](const meshutil::submesh& submesh) -> int { return submesh._vtxpool.GetNumVertices(); })
      .def(
          "writeObj",
          [](const meshutil::submesh& submesh, const std::string& outpath) { return submeshWriteObj(submesh, outpath); })
      .def(
          "addQuad",
          [](meshutil::submesh& submesh,
             fvec3 p0,
             fvec3 p1,
             fvec3 p2,
             fvec3 p3,
             fvec2 uv0,
             fvec2 uv1,
             fvec2 uv2,
             fvec2 uv3,
             fvec4 c) { return submesh.addQuad(p0, p1, p2, p3, uv0, uv1, uv2, uv3, c); })
      .def(
          "addQuad",
          [](meshutil::submesh& submesh,
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
  py::class_<meshutil::vertexpool>(meshutil, "VertexPool").def(py::init<>());
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<meshutil::poly>(meshutil, "Poly").def(py::init<>());
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<meshutil::edge>(meshutil, "Edge").def(py::init<>());
}

} // namespace ork::lev2
