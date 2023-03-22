////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/meshutil/igl.h>

namespace ork::meshutil{
std::vector<submesh_ptr_t> submeshBulletConvexDecomposition(const submesh& inpsubmesh);
} // namespace ork::meshutil

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
using namespace meshutil;
void pyinit_meshutil_submesh(py::module& module_meshutil) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto submesh_type =
      py::class_<submesh, submesh_ptr_t>(module_meshutil, "SubMesh")
          .def(py::init<>())
          .def_static("createFromFrustum", [](frustum_ptr_t frus, bool projective_rect_uv=false) -> submesh_ptr_t { //
            return submeshFromFrustum(*frus,projective_rect_uv);
          })
          .def_property("name", [](submesh_ptr_t submesh) -> std::string {
            return submesh->name;
          },
          [](submesh_ptr_t submesh, std::string n) {            
            return submesh->name = n;
          })
          .def_property_readonly("isConvexHull", [](submesh_ptr_t submesh) -> bool {
            return submesh->isConvexHull();
          })
          .def_property_readonly("vertexpool", [](submesh_ptr_t submesh) -> vertexpool_ptr_t {            
            return submesh->_vtxpool;
          })
          .def_property_readonly("as_polyset", [](submesh_ptr_t submesh) -> polyset_ptr_t {            
              return submesh->asPolyset();
          })
          .def_property_readonly("polys", [](submesh_ptr_t submesh) -> py::list {            
              py::list pyl;
              for( auto item : submesh->_polymap ){
                auto p = item.second;
                pyl.append(p);
              }
              return pyl;
          })
          .def_property_readonly("edges", [](submesh_ptr_t submesh) -> py::list {            
              py::list pyl;
              for( auto item : submesh->_edgemap ){
                auto e = item.second;
                pyl.append(e);
              }
              return pyl;
          })
          .def_property_readonly("convexVolume", [](submesh_ptr_t submesh) -> float {            
            return submesh->convexVolume();
          })
          #if defined(ENABLE_IGL)
          .def("igl_test", [](submesh_ptr_t submesh) { return submesh->igl_test(); })
#endif //#if defined(ENABLE_IGL)
          .def(
              "copy",
              [](submesh_constptr_t inpsubmesh, py::kwargs kwargs) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                bool preserve_normals   = true;
                bool preserve_colors    = true;
                bool preserve_texcoords = true;
                if (kwargs) {
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "preserve_normals") {
                      preserve_normals = py::cast<bool>(item.second);
                    }
                    if (key == "preserve_colors") {
                      preserve_colors = py::cast<bool>(item.second);
                    }
                    if (key == "preserve_texcoords") {
                      preserve_texcoords = py::cast<bool>(item.second);
                    }
                  }
                }
                inpsubmesh->copy(*rval, preserve_normals,preserve_colors,preserve_texcoords);
                return rval;
              })
          .def(
              "convexDecomposition",
              [](submesh_constptr_t inpsubmesh) -> py::list {
                py::list pyl;
                auto outlist = submeshBulletConvexDecomposition(*inpsubmesh);
                for( auto i : outlist ){
                  pyl.append(i);
                }
                return pyl;
              })
          .def(
              "barycentricUVs",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                submeshBarycentricUV(*inpsubmesh, *rval);
                return rval;
              })
          .def(
              "triangulated",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                submeshTriangulate(*inpsubmesh, *rval);
                return rval;
              })
          .def(
              "quadulated",
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
              "slicedWithPlane",
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
          .def(
              "clippedWithPlane",
              [](submesh_constptr_t inpsubmesh,py::kwargs kwargs) -> py::dict {

                submesh_ptr_t res_front = std::make_shared<submesh>();
                submesh_ptr_t res_back = std::make_shared<submesh>();
                res_front->name = inpsubmesh->name + ".front";
                res_back->name = inpsubmesh->name + ".back";

                fplane3_ptr_t plane = nullptr;
                bool close_mesh = false;
                bool flip_orientation = false;

                for (auto item : kwargs) {
                  auto key = py::cast<std::string>(item.first);
                  if (key == "flip_orientation") {
                    flip_orientation = py::cast<bool>(item.second);
                  }
                  else if (key == "close_mesh") {
                    close_mesh = py::cast<bool>(item.second);
                  }
                  else if (key == "plane") {
                    plane = py::cast<fplane3_ptr_t>(item.second);
                  }
                  else{
                    OrkAssert(false);
                  }
                }

                submeshClipWithPlane(*inpsubmesh, //
                                     *plane, // 
                                     close_mesh, // 
                                     flip_orientation, // 
                                     *res_front, //
                                     *res_back);

                py::dict rval;
                rval["front"] = res_front;
                rval["back"] = res_back;
                return rval;
              })
          .def(
              "coplanarJoined",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                submeshJoinCoplanar(*inpsubmesh, *rval);
                return rval;
              })
#if defined(ENABLE_IGL)
          .def("toIglMesh", [](submesh_ptr_t submesh, int numsides) -> iglmesh_ptr_t { return submesh->toIglMesh(numsides); })
#endif //#if defined(ENABLE_IGL)
          .def("numPolys", [](submesh_constptr_t submesh, int numsides) -> int { return submesh->GetNumPolys(numsides); })
          .def("numVertices", [](submesh_constptr_t submesh) -> int { return submesh->_vtxpool->GetNumVertices(); })
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
              "makeVertex",
              [](submesh_ptr_t submesh, py::kwargs kwargs) ->  vertex_ptr_t{
                auto vin = std::make_shared<vertex>();
                for (auto item : kwargs) {
                  auto key = py::cast<std::string>(item.first);
                  if (key == "position") {
                    vin->mPos = py::cast<fvec3>(item.second);
                  }
                  else if (key == "normal") {
                    vin->mNrm = py::cast<fvec3>(item.second);
                  }
                  else if (key == "color0") {
                    vin->mCol[0] = py::cast<fvec4>(item.second);
                  }
                  else{
                    OrkAssert(false);
                  }
                }
                return submesh->mergeVertex(*vin);
              })
          .def(
              "mergeVertex",
              [](submesh_ptr_t submesh, vertex_constptr_t vin) ->  vertex_ptr_t{
                return submesh->mergeVertex(*vin);
              })
          .def(
              "mergePoly",
              [](submesh_ptr_t submesh, poly_ptr_t pin) ->  poly_ptr_t{
                return submesh->mergePoly(*pin);
              })
          .def(
              "mergePolySet",
              [](submesh_ptr_t submesh, polyset_ptr_t psetin) {
                submesh->mergePolySet(*psetin);
              })
          .def(
              "makeTriangle",
              [](submesh_ptr_t submesh, vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc) ->  poly_ptr_t{
                return submesh->mergeTriangle(va,vb,vc);
              })
          .def(
              "makeQuad",
              [](submesh_ptr_t submesh, vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc, vertex_ptr_t vd) ->  poly_ptr_t{
                return submesh->mergeQuad(va,vb,vc,vd);
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
            rval += FormatString("  num_vertices<%d>\n", (int)sm->_vtxpool->_vtxmap.size());
            rval += FormatString("  num_polys<%d>\n", (int)sm->_polymap.size());
            rval += FormatString("  num_triangles<%d>\n", (int)sm->GetNumPolys(3));
            rval += FormatString("  num_quads<%d>\n", (int)sm->GetNumPolys(4));
            rval += FormatString("  num_edges<%d>\n", (int)sm->_edgemap.size());
            return rval;
          });
  type_codec->registerStdCodec<submesh_ptr_t>(submesh_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto polyset_type = py::class_<PolySet, polyset_ptr_t>(module_meshutil, "PolySet")
          .def(py::init<>())
          .def_property_readonly("numpolys", [](polyset_ptr_t pset) -> int {            
              return (int) pset->_polys.size();
          })
          .def_property_readonly("polys", [](polyset_ptr_t pset) -> py::list {            
              py::list pyl;
              for( auto p : pset->_polys ){
                pyl.append(p);
              }
              return pyl;
          })
          .def(
              "splitByPlane",
              [type_codec](polyset_ptr_t pset) -> py::dict {
                py::dict rval;
                auto polys_by_plane = pset->splitByPlane();
                for( auto item : polys_by_plane ){
                  uint64_t key = item.first;
                  polyset_ptr_t val = item.second;
                  rval[type_codec->encode(key)] = type_codec->encode(val);
                }
                OrkAssert(rval.size()>0);
                return rval;
              })
          .def("splitByIsland",[](polyset_ptr_t pset) -> py::list {
            py::list pyl;
            auto islands = pset->splitByIsland();
            for( auto island : islands ){
              pyl.append(island);
            }
            return pyl;
          });
  type_codec->registerStdCodec<polyset_ptr_t>(polyset_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto island_type = py::class_<Island, PolySet, island_ptr_t>(module_meshutil, "Island")
          .def(py::init<>())
          .def("boundaryEdges",[](island_ptr_t island) -> py::list {
            py::list pyl;
            auto edges = island->boundaryEdges();
            for( auto edge : edges ){
              pyl.append(edge);
            }
            return pyl;
          })
          .def("boundaryLoop",[](island_ptr_t island) -> py::list {
            py::list pyl;
            auto edges = island->boundaryLoop();
            for( auto edge : edges ){
              pyl.append(edge);
            }
            return pyl;
          });
  type_codec->registerStdCodec<island_ptr_t>(island_type);
  /////////////////////////////////////////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
