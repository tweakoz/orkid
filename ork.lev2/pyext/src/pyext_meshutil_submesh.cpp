////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/meshutil/igl.h>


namespace ork::meshutil {
std::vector<submesh_ptr_t> submeshBulletConvexDecomposition(const submesh& inpsubmesh);
extern bool __enable_zero_area_face_check;
} // namespace ork::meshutil


///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
using namespace meshutil;
void pyinit_meshutil_submesh(py::module& module_meshutil) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  module_meshutil.def("enableZeroAreaFaceCheck", [](bool enable) { __enable_zero_area_face_check = enable; });
  /////////////////////////////////////////////////////////////////////////////////
  auto submesh_type =
      py::class_<submesh, submesh_ptr_t>(module_meshutil, "SubMesh")
          .def(py::init<>())
          .def_static(
              "createFromFrustum",
              [](dfrustum_ptr_t frus, py::kwargs kwargs) -> submesh_ptr_t { //
                bool projective_rect_uv = false;
                if (kwargs) {
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "projective_rect_uv") {
                      projective_rect_uv = py::cast<bool>(item.second);
                    }
                  }
                }
                return submeshFromFrustum(*frus, projective_rect_uv);
              })
          //////////////////////////////////////////////////////////////////
          // create a submesh from a python dictionary of vertices and faces
          //////////////////////////////////////////////////////////////////
          .def_static(
              "createFromDict",
              [](py::dict the_dict) -> submesh_ptr_t { //
                submesh_ptr_t rval = std::make_shared<submesh>();
                auto pyverts       = the_dict["vertices"].cast<py::list>();
                auto pyfaces       = the_dict["faces"].cast<py::list>();
                std::vector<vertex_ptr_t> inserted_vertices;
                for (auto py_vertex : pyverts) {
                  auto py_vertex_dict = py_vertex.cast<py::dict>();
                  vertex inp_vtx;
                  for (auto item : py_vertex_dict) {
                    auto key = item.first.cast<std::string>();
                    if (key[0] == 'p') {
                      inp_vtx.mPos = fvec3_to_dvec3(item.second.cast<fvec3>());
                    } else if (key[0] == 'n') {
                      inp_vtx.mNrm = fvec3_to_dvec3(item.second.cast<fvec3>());
                    } else if (key == "c0") {
                      inp_vtx.mCol[0] = item.second.cast<fvec4>();
                    } else if (key == "uv0") {
                      inp_vtx.mUV[0].mMapTexCoord = item.second.cast<fvec2>();
                    } else if (key == "b0") {
                      inp_vtx.mUV[0].mMapBiNormal = item.second.cast<fvec3>();
                    } else if (key == "t0") {
                      inp_vtx.mUV[0].mMapTangent = item.second.cast<fvec3>();
                    }
                  }
                  inserted_vertices.push_back(rval->mergeVertex(inp_vtx));
                }
                for (auto py_face : pyfaces) {
                  auto face = py_face.cast<py::list>();
                  std::vector<vertex_ptr_t> face_vertices;
                  for (auto item : face) {
                    int idx = item.cast<int>();
                    face_vertices.push_back(inserted_vertices[idx]);
                  }
                  rval->mergePoly(Polygon(face_vertices));
                }
                return rval;
              })
          //////////////////////////////////////////////////////////////////
          .def_property(
              "name",
              [](submesh_ptr_t submesh) -> std::string { return submesh->name; },
              [](submesh_ptr_t submesh, std::string n) { return submesh->name = n; })
          .def_property_readonly("hashed", [](submesh_ptr_t submesh) -> uint64_t { return submesh->hash(); })
          .def_property_readonly(
              "isConvexHull",
              [](submesh_ptr_t submesh) -> bool { //
                return submesh->isConvexHull();
              })
          .def_property_readonly(
              "vertices",
              [](submesh_ptr_t submesh) -> py::list {
                py::list pyl;
                submesh->visitAllVertices([&](vertex_ptr_t v) { pyl.append(v); });
                return pyl;
              })
          .def_property_readonly(
              "as_polygroup",
              [](submesh_ptr_t submesh) -> polygroup_ptr_t { //
                return submesh->asPolyGroup();
              })
          .def_property_readonly(
              "polys",
              [](submesh_ptr_t submesh) -> py::list {
                py::list pyl;
                submesh->visitAllPolys([&](merged_poly_ptr_t p) { pyl.append(p); });
                return pyl;
              })
          .def_property_readonly(
              "edges",
              [](submesh_ptr_t submesh) -> py::list {
                py::list pyl;
                for (auto item : submesh->allEdgesByVertexHash()) {
                  auto e = item.second;
                  pyl.append(e);
                }
                return pyl;
              })
          .def_property_readonly(
              "convexVolume",
              [](submesh_ptr_t submesh) -> float { //
                return submesh->convexVolume();
              })
          .def_property_readonly(
              "convexWindingOrder",
              [](submesh_ptr_t submesh) -> std::string { //
                return submeshConvexCheckWindingOrder(*submesh);
              })

#if defined(ENABLE_IGL)
          .def("igl_test", [](submesh_ptr_t submesh) { return submesh->igl_test(); })
#endif //#if defined(ENABLE_IGL)
          .def("dumpPolys", [](submesh_ptr_t submesh, std::string hdr) { submesh->dumpPolys(hdr); })
          .def("clearPolys", [](submesh_ptr_t submesh) { submesh->clearPolys(); })
          .def(
              "copy",
              [](submesh_constptr_t inpsubmesh, py::kwargs kwargs) -> submesh_ptr_t {
                submesh_ptr_t rval      = std::make_shared<submesh>();
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
                inpsubmesh->copy(*rval, preserve_normals, preserve_colors, preserve_texcoords);
                return rval;
              })
          .def(
              "positionsOnly",
              [](submesh_constptr_t inpsubmesh, py::kwargs kwargs) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                inpsubmesh->copy(*rval, false, false, false);
                return rval;
              })
          .def(
              "convexDecomposition",
              [](submesh_constptr_t inpsubmesh) -> py::list {
                py::list pyl;
                auto outlist = submeshBulletConvexDecomposition(*inpsubmesh);
                for (auto i : outlist) {
                  pyl.append(i);
                }
                return pyl;
              })
          .def(
              "withBarycentricUVs",
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
              "prune",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                submeshPrune(*inpsubmesh, *rval);
                return rval;
              })
          .def(
              "xatlas",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                submesh_xatlas(*inpsubmesh,*rval);
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
              [](submesh_constptr_t inpsubmesh, py::kwargs kwargs) -> py::dict {
                submesh_ptr_t res_front = std::make_shared<submesh>();
                submesh_ptr_t res_back  = std::make_shared<submesh>();
                submesh_ptr_t res_isect = std::make_shared<submesh>();

                dplane3 as_dplane;

                for (auto item : kwargs) {
                  auto key = py::cast<std::string>(item.first);
                  if (key == "plane") {
                    as_dplane = py::cast<dplane3>(item.second);
                  } else {
                    OrkAssert(false);
                  }
                }

                submeshSliceWithPlane(*inpsubmesh, as_dplane, *res_front, *res_back, *res_isect);
                py::dict rval;
                rval["front"]      = res_front;
                rval["back"]       = res_back;
                rval["intersects"] = res_isect;
                return rval;
              })
          .def(
              "clippedWithPlane",
              [](submesh_constptr_t inpsubmesh, py::kwargs kwargs) -> submesh_ptr_t {
                submesh_ptr_t res_front = std::make_shared<submesh>();
                res_front->name         = inpsubmesh->name + ".front";

                dplane3 as_dplane;
                bool close_mesh       = false;
                bool flip_orientation = false;
                bool debug            = false;

                for (auto item : kwargs) {
                  auto key = py::cast<std::string>(item.first);
                  if (key == "flip_orientation") {
                    flip_orientation = py::cast<bool>(item.second);
                  } else if (key == "close_mesh") {
                    close_mesh = py::cast<bool>(item.second);
                  } else if (key == "plane") {
                    as_dplane = py::cast<dplane3>(item.second);
                  } else if (key == "debug") {
                    debug = py::cast<bool>(item.second);
                  } else {
                    OrkAssert(false);
                  }
                }

                submeshClipWithPlane(
                    *inpsubmesh,      //
                    as_dplane,        //
                    close_mesh,       //
                    flip_orientation, //
                    *res_front,       //
                    debug);

                return res_front;
              })
          .def(
              "withFaceNormals",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t res_faced = std::make_shared<submesh>();
                submeshWithFaceNormals(*inpsubmesh, *res_faced);
                return res_faced;
              })
          .def(
              "withFaceNormalsAndBinormals",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t res_faced = std::make_shared<submesh>();
                submeshWithFaceNormalsAndBinormals(*inpsubmesh, *res_faced);
                return res_faced;
              })
          .def(
              "withSmoothedNormals",
              [](submesh_constptr_t inpsubmesh, float threshold_radians) -> submesh_ptr_t {
                submesh_ptr_t res_smoothed = std::make_shared<submesh>();
                submeshWithSmoothNormals(*inpsubmesh, *res_smoothed, threshold_radians);
                return res_smoothed;
              })
          .def(
              "withVertexColorsFromNormals",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t res_faced = std::make_shared<submesh>();
                submeshWithVertexColorsFromNormals(*inpsubmesh, *res_faced);
                return res_faced;
              })
          .def(
              "withTextureBasis",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t res_basis = std::make_shared<submesh>();
                submeshWithTextureBasis(*inpsubmesh, *res_basis);
                return res_basis;
              })
          .def(
              "withTextureUnwrap",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t res_basis = std::make_shared<submesh>();
                submeshWithTextureUnwrap(*inpsubmesh, *res_basis);
                return res_basis;
              })
          .def(
              "coplanarJoined",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                submeshJoinCoplanar(*inpsubmesh, *rval);
                return rval;
              })
          .def(
              "convexHull",
              [](submesh_constptr_t inpsubmesh, int steps = 0) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                submeshConvexHull(*inpsubmesh, *rval, steps);
                return rval;
              })
          .def(
              "withWindingOrderFixed",
              [](submesh_constptr_t inpsubmesh, bool inside_out) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                submeshFixWindingOrder(*inpsubmesh, *rval, inside_out);
                return rval;
              })
          .def(
              "repaired",
              [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
                submesh_ptr_t rval = std::make_shared<submesh>();
                submeshJoinCoplanar(*inpsubmesh, *rval);
                submesh_ptr_t rval2 = std::make_shared<submesh>();
                submeshTriangulate(*rval, *rval2);
                submesh_ptr_t rval3 = std::make_shared<submesh>();
                submeshFixWindingOrder(*rval2, *rval3, false);
                return rval3;
              })
#if defined(ENABLE_IGL)
          .def("toIglMesh", [](submesh_ptr_t submesh, int numsides) -> iglmesh_ptr_t { return submesh->toIglMesh(numsides); })
#endif //#if defined(ENABLE_IGL)
          .def("numPolys", [](submesh_constptr_t submesh, int numsides) -> int { return submesh->numPolys(numsides); })
          .def("numVertices", [](submesh_constptr_t submesh) -> int { return submesh->numVertices(); })
          .def(
              "edgesForPoly",
              [](submesh_constptr_t submesh, merged_poly_ptr_t p) -> py::list { //
                py::list rval;
                auto edges = submesh->edgesForPoly(p);
                for (auto e : edges)
                  rval.append(e);
                return rval;
              })
          .def(
              "writeWavefrontObj",
              [](submesh_constptr_t submesh, const std::string& outpath) { return submeshWriteObj(*submesh, outpath); })
          .def(
              "addQuad",
              [](submesh_ptr_t submesh, dvec3 p0, dvec3 p1, dvec3 p2, dvec3 p3) { return submesh->addQuad(p0, p1, p2, p3); })
          .def(
              "addQuad",
              [](submesh_ptr_t submesh, dvec3 p0, dvec3 p1, dvec3 p2, dvec3 p3, dvec4 c) {
                return submesh->addQuad(p0, p1, p2, p3, c);
              })
          .def(
              "makeVertex",
              [](submesh_ptr_t submesh, py::kwargs kwargs) -> vertex_ptr_t {
                auto vin = std::make_shared<vertex>();
                for (auto item : kwargs) {
                  auto key = py::cast<std::string>(item.first);
                  if (key == "position") {
                    vin->mPos = py::cast<dvec3>(item.second);
                  } else if (key == "normal") {
                    vin->mNrm = py::cast<dvec3>(item.second);
                  } else if (key == "color0") {
                    vin->mCol[0] = py::cast<fvec4>(item.second);
                  } else if (key == "uvc0") {
                    vin->mUV[0] = py::cast<uvmapcoord>(item.second);
                  } else {
                    OrkAssert(false);
                  }
                }
                return submesh->mergeVertex(*vin);
              })
          .def(
              "mergeVertex",
              [](submesh_ptr_t submesh, vertex_const_ptr_t vin) -> vertex_ptr_t { return submesh->mergeVertex(*vin); })
          .def("mergePoly", [](submesh_ptr_t submesh, poly_ptr_t pin) -> merged_poly_ptr_t { return submesh->mergePoly(*pin); })
          .def("mergePolyGroup", [](submesh_ptr_t submesh, polygroup_ptr_t psetin) { submesh->mergePolyGroup(*psetin); })
          .def("mergeSubMesh", [](submesh_ptr_t submesh, submesh_ptr_t subm2) { submesh->MergeSubMesh(*subm2); })
          .def(
              "makeTriangle",
              [](submesh_ptr_t submesh, vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc) -> merged_poly_ptr_t {
                return submesh->mergeTriangle(va, vb, vc);
              })
          .def(
              "makeQuad",
              [](submesh_ptr_t submesh, vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc, vertex_ptr_t vd) -> merged_poly_ptr_t {
                return submesh->mergeQuad(va, vb, vc, vd);
              })
          .def(
              "addQuad",
              [](submesh_ptr_t submesh,
                 dvec3 p0,
                 dvec3 p1,
                 dvec3 p2,
                 dvec3 p3,
                 dvec2 uv0,
                 dvec2 uv1,
                 dvec2 uv2,
                 dvec2 uv3,
                 dvec4 c) { return submesh->addQuad(p0, p1, p2, p3, uv0, uv1, uv2, uv3, c); })
          .def(
              "addQuad",
              [](submesh_ptr_t submesh,
                 dvec3 p0,
                 dvec3 p1,
                 dvec3 p2,
                 dvec3 p3,
                 dvec3 n0,
                 dvec3 n1,
                 dvec3 n2,
                 dvec3 n3,
                 dvec2 uv0,
                 dvec2 uv1,
                 dvec2 uv2,
                 dvec2 uv3,
                 dvec4 c) { return submesh->addQuad(p0, p1, p2, p3, n0, n1, n2, n3, uv0, uv1, uv2, uv3, c); })
          .def("__repr__", [](submesh_ptr_t sm) -> std::string {
            std::string rval = FormatString("Submesh<%p>\n", (void*)sm.get());
            rval += FormatString("  num_vertices<%d>\n", (int)sm->numVertices());
            rval += FormatString("  num_polys<%d>\n", (int)sm->numPolys());
            rval += FormatString("  num_triangles<%d>\n", (int)sm->numPolys(3));
            rval += FormatString("  num_quads<%d>\n", (int)sm->numPolys(4));
            rval += FormatString("  num_edges<%d>\n", (int)sm->allEdgesByVertexHash().size());
            return rval;
          });
  type_codec->registerStdCodec<submesh_ptr_t>(submesh_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto polyset_type = py::class_<PolyGroup, polygroup_ptr_t>(module_meshutil, "PolyGroup")
                          .def(py::init<>())
                          .def_property_readonly("numpolys", [](polygroup_ptr_t pset) -> int { return (int)pset->_polys.size(); })
                          .def_property_readonly(
                              "polys",
                              [](polygroup_ptr_t pset) -> py::list {
                                py::list pyl;
                                for (auto p : pset->_polys) {
                                  pyl.append(p);
                                }
                                return pyl;
                              })
                          .def(
                              "splitByPlane",
                              [type_codec](polygroup_ptr_t pset) -> py::dict {
                                py::dict rval;
                                auto polys_by_plane = pset->splitByPlane();
                                for (auto item : polys_by_plane) {
                                  uint64_t key                  = item.first;
                                  polygroup_ptr_t val           = item.second;
                                  rval[type_codec->encode(key)] = type_codec->encode(val);
                                }
                                OrkAssert(rval.size() > 0);
                                return rval;
                              })
                          .def("splitByIsland", [](polygroup_ptr_t pset) -> py::list {
                            py::list pyl;
                            auto islands = pset->splitByIsland();
                            for (auto island : islands) {
                              pyl.append(island);
                            }
                            return pyl;
                          });
  type_codec->registerStdCodec<polygroup_ptr_t>(polyset_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto island_type = py::class_<Island, PolyGroup, island_ptr_t>(module_meshutil, "PolyIsland")
                         .def(py::init<>())
                         .def(
                             "boundaryEdges",
                             [](island_ptr_t island) -> py::list {
                               py::list pyl;
                               auto edges = island->boundaryEdges();
                               for (auto edge : edges) {
                                 pyl.append(edge);
                               }
                               return pyl;
                             })
                         .def("boundaryLoop", [](island_ptr_t island) -> py::list {
                           py::list pyl;
                           auto edges = island->boundaryLoop();
                           for (auto edge : edges) {
                             pyl.append(edge);
                           }
                           return pyl;
                         });
  type_codec->registerStdCodec<island_ptr_t>(island_type);
  /////////////////////////////////////////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
