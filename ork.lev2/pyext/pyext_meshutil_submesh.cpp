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
namespace ork::lev2 {
using namespace meshutil;
using rigidprim_t = RigidPrimitive<SVtxV12N12B12T8C4>;
void pyinit_meshutil_submesh(py::module& module_meshutil) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto submesh_type =
      py::class_<submesh, submesh_ptr_t>(module_meshutil, "SubMesh")
          .def(py::init<>())
#if defined(ENABLE_IGL)
          .def("igl_test", [](submesh_ptr_t submesh) { return submesh->igl_test(); })
#endif //#if defined(ENABLE_IGL)
          .def(
              "triangulate",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                submeshTriangulate(*inpsubmesh, *rval);
                return rval;
              })
          .def(
              "quadulate",
              [](submesh_constptr_t inpsubmesh, py::kwargs kwargs) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                if (kwargs) {
                  float area_tolerance         = 100.0f;
                  bool exclude_non_coplanar    = false;
                  bool exclude_non_rectangular = false;
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "area_tolerance") {
                      area_tolerance = py::cast<float>(item.second);
                    }
                    if (key == "exclude_non_coplanar") {
                      exclude_non_coplanar = py::cast<bool>(item.second);
                    }
                    if (key == "exclude_non_rectangular") {
                      exclude_non_rectangular = py::cast<bool>(item.second);
                    }
                  }
                  submeshTrianglesToQuads(*inpsubmesh, *rval, area_tolerance, exclude_non_coplanar, exclude_non_rectangular);
                } else {
                  submeshTrianglesToQuads(*inpsubmesh, *rval);
                }
                return rval;
              })
          .def(
              "sliceWithPlane",
              [](submesh_constptr_t inpsubmesh, 
                 fplane3_ptr_t plane ) -> py::dict {
                submesh_ptr_t res_front = std::make_shared<submesh>();
                submesh_ptr_t res_back = std::make_shared<submesh>();
                submesh_ptr_t res_isect = std::make_shared<submesh>();
                submeshSliceWithPlane(*inpsubmesh,*plane,*res_front,*res_back,*res_isect);
                py::dict rval;
                rval["front"] = res_front;
                rval["back"] = res_back;
                rval["intersects"] = res_isect;
                return rval;
              })
#if defined(ENABLE_IGL)
          .def("toIglMesh", [](submesh_ptr_t submesh, int numsides) -> iglmesh_ptr_t { return submesh->toIglMesh(numsides); })
#endif //#if defined(ENABLE_IGL)
          .def("numPolys", [](submesh_constptr_t submesh, int numsides) -> int { return submesh->GetNumPolys(numsides); })
          .def("numVertices", [](submesh_constptr_t submesh) -> int { return submesh->_vtxpool.GetNumVertices(); })
          .def(
              "writeWavefrontObj",
              [](submesh_constptr_t submesh, const std::string& outpath) { return submeshWriteObj(*submesh, outpath); })
          .def(
              "addQuad",
              [](submesh_ptr_t submesh, fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3) { return submesh->addQuad(p0, p1, p2, p3); })
          .def(
              "addQuad",
              [](submesh_ptr_t submesh, fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3, fvec4 c) {
                return submesh->addQuad(p0, p1, p2, p3, c);
              })
          .def(
              "addQuad",
              [](submesh_ptr_t submesh,
                 fvec3 p0,
                 fvec3 p1,
                 fvec3 p2,
                 fvec3 p3,
                 fvec2 uv0,
                 fvec2 uv1,
                 fvec2 uv2,
                 fvec2 uv3,
                 fvec4 c) { return submesh->addQuad(p0, p1, p2, p3, uv0, uv1, uv2, uv3, c); })
          .def(
              "addQuad",
              [](submesh_ptr_t submesh,
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
                 fvec4 c) { return submesh->addQuad(p0, p1, p2, p3, n0, n1, n2, n3, uv0, uv1, uv2, uv3, c); })
          .def("__repr__", [](submesh_ptr_t sm) -> std::string {
            std::string rval = FormatString("Submesh<%p>\n", (void*)sm.get());
            rval += FormatString("  num_verticess<%d>\n", (int)sm->_vtxpool._vtxmap.size());
            rval += FormatString("  num_polys<%d>\n", (int)sm->_polymap.size());
            rval += FormatString("  num_triangles<%d>\n", (int)sm->GetNumPolys(3));
            rval += FormatString("  num_quads<%d>\n", (int)sm->GetNumPolys(4));
            rval += FormatString("  num_edges<%d>\n", (int)sm->_edgemap.size());
            return rval;
          });

  type_codec->registerStdCodec<submesh_ptr_t>(submesh_type);
}

/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
