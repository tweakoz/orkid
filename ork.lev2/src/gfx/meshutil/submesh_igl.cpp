////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/igl.h>
#include <iostream>

#include <igl/arap.h>
#include <igl/barycenter.h>
#include <igl/boundary_facets.h>
#include <igl/boundary_loop.h>
#include <igl/circulation.h>
#include <igl/collapse_edge.h>
#include <igl/copyleft/tetgen/tetrahedralize.h>
#include <igl/copyleft/tetgen/cdt.h>
#include <igl/copyleft/cgal/remesh_self_intersections.h> // GNU GPL (todo move to external executable?)
#include <igl/cotmatrix.h>
#include <igl/decimate.h>
#include <igl/doublearea.h>
#include <igl/edge_flaps.h>
#include <igl/exterior_edges.h>
#include <igl/flipped_triangles.h>
#include <igl/gaussian_curvature.h>
#include <igl/harmonic.h>
#include <igl/invert_diag.h>
#include <igl/is_edge_manifold.h>
#include <igl/is_irregular_vertex.h>
#include <igl/is_vertex_manifold.h>
#include <igl/lscm.h>
#include <igl/MappingEnergyType.h>
#include <igl/map_vertices_to_circle.h>
#include <igl/massmatrix.h>
#include <igl/orientable_patches.h>
#include <igl/PI.h>
#include <igl/per_vertex_normals.h>
#include <igl/per_face_normals.h>
#include <igl/per_corner_normals.h>
#include <igl/polygon_mesh_to_triangle_mesh.h>
#include <igl/principal_curvature.h>
#include <igl/randperm.h>
#include <igl/remove_unreferenced.h>
#include <igl/scaf.h>
#include <igl/shortest_edge_and_midpoint.h>
#include <igl/slice.h>
#include <igl/topological_hole_fill.h>
#include <igl/unique_edge_map.h>
#include <igl/unique_simplices.h>
#include <igl/winding_number.h>
#include <igl/copyleft/cgal/mesh_boolean.h>
#include <igl/MeshBooleanType.h>

#include <igl/embree/reorient_facets_raycast.h>
#include <igl/embree/ambient_occlusion.h>

#include <Eigen/Core>

namespace ork::meshutil {
//////////////////////////////////////////////////////////////////////////////
iglmesh_ptr_t submesh::toIglMesh(int numsides) const {
  return std::make_shared<IglMesh>(*this, numsides);
}
//////////////////////////////////////////////////////////////////////////////
IglMesh::IglMesh(const Eigen::MatrixXd& verts, const Eigen::MatrixXi& faces)
    : _verts(verts)
    , _faces(faces) {
  OrkAssert(_verts.cols() == 3); // make sure we have vec3's
}
//////////////////////////////////////////////////////////////////////////////
IglMesh::IglMesh(const submesh& inp_submesh, int numsides)
    : _verts(inp_submesh._vtxpool.GetNumVertices(), 3)
    , _faces(inp_submesh.GetNumPolys(numsides), numsides) {
  size_t numverts = inp_submesh._vtxpool.GetNumVertices();
  size_t numfaces = inp_submesh.GetNumPolys(numsides);
  _verts          = Eigen::MatrixXd(numverts, 3);
  _normals        = Eigen::MatrixXd(numverts, 3);
  _binormals      = Eigen::MatrixXd(numverts, 3);
  _tangents       = Eigen::MatrixXd(numverts, 3);
  _uvs            = Eigen::MatrixXd(numverts, 2);
  _colors         = Eigen::MatrixXd(numverts, 4);
  _faces          = Eigen::MatrixXi(numfaces, numsides);
  ///////////////////////////////////////////////
  // fill in vertices
  ///////////////////////////////////////////////
  for (int v = 0; v < numverts; v++) {
    const auto& inpvtx = inp_submesh._vtxpool.GetVertex(v);
    const auto& inpuv  = inpvtx.mUV[0];
    _verts.row(v) << inpvtx.mPos.x, inpvtx.mPos.y, inpvtx.mPos.z;
    _normals.row(v) << inpvtx.mNrm.x, inpvtx.mNrm.y, inpvtx.mNrm.z;
    _binormals.row(v) << inpuv.mMapBiNormal.x, inpuv.mMapBiNormal.y, inpuv.mMapBiNormal.z;
    _tangents.row(v) << inpuv.mMapTangent.x, inpuv.mMapTangent.y, inpuv.mMapTangent.z;
    _uvs.row(v) << inpuv.mMapTexCoord.x, inpuv.mMapTexCoord.y;
    _colors.row(v) << inpvtx.mCol[0].x, inpvtx.mCol[0].y, inpvtx.mCol[0].z, inpvtx.mCol[0].w;
  }
  ///////////////////////////////////////////////
  // fill in faces
  ///////////////////////////////////////////////
  orkvector<int> face_indices;
  inp_submesh.FindNSidedPolys(face_indices, numsides);
  for (int f = 0; f < face_indices.size(); f++) {
    const auto& face = inp_submesh.RefPoly(f);
    switch (numsides) {
      case 3:
        _faces.row(f) <<         //
            face.GetVertexID(0), // tri index 0
            face.GetVertexID(1), // tri index 1
            face.GetVertexID(2); // tri index 2
        break;
      case 4:
        _faces.row(f) <<         //
            face.GetVertexID(0), // tri index 0
            face.GetVertexID(1), // tri index 1
            face.GetVertexID(2), // tri index 2
            face.GetVertexID(3); // tri index 3
        break;
      default:
        OrkAssert(false);
        break;
    }
  }
}
//////////////////////////////////////////////////////////////////////////////
size_t IglMesh::numFaces() const {
  return _faces.rows();
}
//////////////////////////////////////////////////////////////////////////////
size_t IglMesh::numVertices() const {
  return _verts.rows();
}
//////////////////////////////////////////////////////////////////////////////
unique_edges_ptr_t IglMesh::uniqueEdges() const {
  auto rval = std::make_shared<UniqueEdges>();
  igl::unique_edge_map(_faces, rval->E, rval->uE, rval->EMAP, rval->_ue2e);
  rval->_count = rval->uE.rows();
  return rval;
}
//////////////////////////////////////////////////////////////////////////////
manifold_extraction_ptr_t IglMesh::extractManifolds() const {
  auto ue   = uniqueEdges();
  auto rval = std::make_shared<ManifoldExtraction>();
  // Compute patches (F,EMAP,uE2E) --> (P)
  rval->_numpatches = igl::extract_manifold_patches(
      _faces, //
      ue->EMAP,
      ue->_ue2e,
      rval->P);
  // Compute cells (V,F,P,E,uE,EMAP) -> (per_patch_cells)
  rval->_numcells = igl::copyleft::cgal::extract_cells(
      _verts, //
      _faces,
      rval->P,
      ue->E,
      ue->uE,
      ue->_ue2e,
      ue->EMAP,
      rval->per_patch_cells);
  return rval;
}
//////////////////////////////////////////////////////////////////////////////
size_t IglMesh::numEdges() const {
  return uniqueEdges()->_count;
}
//////////////////////////////////////////////////////////////////////////////
size_t IglMesh::sidesPerFace() const {
  return _faces.cols();
}
//////////////////////////////////////////////////////////////////////////////
bool IglMesh::isVertexManifold() const {
  Eigen::MatrixXi B;
  return igl::is_vertex_manifold(_faces, B);
}
//////////////////////////////////////////////////////////////////////////////
bool IglMesh::isEdgeManifold() const {
  return igl::is_edge_manifold(_faces);
}
//////////////////////////////////////////////////////////////////////////////
double IglMesh::averageEdgeLength() const {
  return igl::avg_edge_length(_verts, _faces);
}
//////////////////////////////////////////////////////////////////////////////
fvec4 IglMesh::computeAreaStatistics() const {
  Eigen::VectorXd area;
  igl::doublearea(_verts, _faces, area);
  area              = area.array() / 2;
  double area_avg   = area.mean();
  double area_min   = area.minCoeff() / area_avg;
  double area_max   = area.maxCoeff() / area_avg;
  double area_sigma = sqrt(((area.array() - area_avg) / area_avg).square().mean());
  return fvec4(area_min, area_max, area_avg, area_sigma);
}
//////////////////////////////////////////////////////////////////////////////
fvec4 IglMesh::computeAngleStatistics() const {
  Eigen::MatrixXd angles;
  igl::internal_angles(_verts, _faces, angles);
  angles             = 360.0 * (angles / (2 * igl::PI)); // Convert to degrees
  double angle_avg   = angles.mean();
  double angle_min   = angles.minCoeff();
  double angle_max   = angles.maxCoeff();
  double angle_sigma = sqrt((angles.array() - angle_avg).square().mean());
  return fvec4(angle_min, angle_max, angle_avg, angle_sigma);
}
//////////////////////////////////////////////////////////////////////////////
size_t IglMesh::countIrregularVertices() const {
  // Count the number of irregular vertices, the border is ignored
  auto irregular                = igl::is_irregular_vertex(_verts, _faces);
  size_t vertex_count           = _verts.rows();
  size_t irregular_vertex_count = std::count(
      irregular.begin(), //
      irregular.end(),
      true);
  return irregular_vertex_count;
}
//////////////////////////////////////////////////////////////////////////////
size_t IglMesh::genus() const {

  // an orientable 2-manifold mesh M with g“handles” (i.e., genus)
  // has Euler-Poincaré characteristic χ(M) = V-E+F = 2(1-g)

  // 2(1-g)=v-e+f     : 2-2g == Euler-Poincaré characteristic
  //-2g=v-e+f-2
  // 2g=-v+e-f+2
  // 2g=2+e-f-v
  // g = (2+e-f-v)/2
  const size_t this_genus = (2 + numEdges() - numFaces() - numVertices()) / 2;
  return this_genus;
}
//////////////////////////////////////////////////////////////////////////////
submesh_ptr_t IglMesh::toSubMesh() const {
  auto subm           = std::make_shared<submesh>();
  size_t numFaces     = this->numFaces();
  size_t sidesPerFace = this->sidesPerFace();
  OrkAssert(sidesPerFace == 3 or sidesPerFace == 4);
  /////////////////////////////////////////////
  std::vector<vertex> submeshverts;
  submeshverts.resize(numFaces * sidesPerFace);
  /////////////////////////////////////////////
  OrkAssert(_verts.cols() == 3); // make sure we have vec3's
  size_t numVerts     = _verts.rows();
  size_t numNormals   = _normals.rows();
  size_t numBinormals = _binormals.rows();
  size_t numTangents  = _tangents.rows();
  size_t numUvs       = _uvs.rows();
  size_t numColors    = _colors.rows();
  auto generateVertex = [&](int faceindex, int facevtxindex) -> vertex { //
    vertex outv;
    const Eigen::MatrixXi& face = _faces.row(faceindex);
    int per_vert_index          = face(facevtxindex);
    /////////////////////////////////////////////
    // position
    /////////////////////////////////////////////
    auto inp_pos = _verts.row(per_vert_index);
    outv.mPos    = fvec3(inp_pos(0), inp_pos(1), inp_pos(2));
    /////////////////////////////////////////////
    // normal
    /////////////////////////////////////////////
    auto donormal = [&](int index) {
      OrkAssert(_normals.cols() == 3);
      auto inp  = _normals.row(index);
      outv.mNrm = fvec3(inp(0), inp(1), inp(2));
    };
    if (numNormals == numVerts) // per vertex
      donormal(per_vert_index);
    else if (numNormals == numFaces) // per face
      donormal(faceindex);
    else if (numNormals == 0) {
    } // no normals
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    // binormal
    /////////////////////////////////////////////
    auto dobinormal = [&](int index) {
      OrkAssert(_binormals.cols() == 3);
      auto inp                 = _binormals.row(index);
      outv.mUV[0].mMapBiNormal = fvec3(inp(0), inp(1), inp(2));
    };
    if (numBinormals == numVerts) // per vertex
      dobinormal(per_vert_index);
    else if (numBinormals == numFaces) // per face
      dobinormal(faceindex);
    else if (numBinormals == 0) {
    } // no binormals
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    // tangent
    /////////////////////////////////////////////
    auto dotangent = [&](int index) {
      OrkAssert(_tangents.cols() == 3);
      auto inp                = _tangents.row(index);
      outv.mUV[0].mMapTangent = fvec3(inp(0), inp(1), inp(2));
    };
    if (numTangents == numVerts) // per vertex
      dotangent(per_vert_index);
    else if (numTangents == numFaces) // per face
      dotangent(faceindex);
    else if (numTangents == 0) {
    } // no tangents
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    // texturecoord
    /////////////////////////////////////////////
    auto dotexcoord = [&](int index) {
      OrkAssert(_uvs.cols() == 2);
      auto inp                 = _uvs.row(index);
      outv.mUV[0].mMapTexCoord = fvec2(inp(0), inp(1));
    };
    if (numUvs == numVerts) // per vertex
      dotexcoord(per_vert_index);
    else if (numUvs == numFaces) // per face
      dotexcoord(faceindex);
    else if (numUvs == 0) {
    } // no texcoords
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    // color
    /////////////////////////////////////////////
    auto docolor = [&](int index) {
      auto inp = _colors.row(index);
      switch (_colors.cols()) {
        case 1: // luminance
          outv.mCol[0] = fvec4(inp(0), inp(0), inp(0), 1);
          break;
        case 3: // rgb
          outv.mCol[0] = fvec4(inp(0), inp(1), inp(2), 1);
          break;
        case 4: // rgba
          outv.mCol[0] = fvec4(inp(0), inp(1), inp(2), inp(3));
          break;
        default:
          OrkAssert(false);
          break;
      }
    };
    if (numColors == numVerts)
      docolor(per_vert_index);
    else if (numColors == numFaces)
      docolor(faceindex);
    else if (numColors == 1)
      docolor(0);
    else if (numColors == 0)
      outv.mCol[0] = fvec4(1, 1, 1, 1);
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    return outv;
  }; // auto generateVertex = [&](int faceindex, int facevtxindex) -> vertex { //
  /////////////////////////////////////////////
  for (int f = 0; f < numFaces; f++) {
    switch (sidesPerFace) {
      case 3: {
        auto o0 = subm->newMergeVertex(generateVertex(f, 0));
        auto o1 = subm->newMergeVertex(generateVertex(f, 1));
        auto o2 = subm->newMergeVertex(generateVertex(f, 2));
        subm->MergePoly(poly(o0, o1, o2));
        break;
      }
      case 4: {
        auto o0 = subm->newMergeVertex(generateVertex(f, 0));
        auto o1 = subm->newMergeVertex(generateVertex(f, 1));
        auto o2 = subm->newMergeVertex(generateVertex(f, 2));
        auto o3 = subm->newMergeVertex(generateVertex(f, 3));
        subm->MergePoly(poly(o0, o1, o2, o3));
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  }
  return subm;
}
//////////////////////////////////////////////////////////////////////////////
iglmesh_ptr_t IglMesh::triangulated() const {
  auto rval = std::make_shared<IglMesh>();
  igl::polygon_mesh_to_triangle_mesh(_faces, rval->_faces);
  rval->_verts = _verts;
  return rval;
}
//////////////////////////////////////////////////////////////////////////////
iglmesh_ptr_t IglMesh::decimated(float amount) const {
  auto rval = std::make_shared<IglMesh>();
  Eigen::MatrixXd V;
  Eigen::MatrixXi F;
  Eigen::MatrixXd OV = _verts;
  Eigen::MatrixXi OF = _faces;
  // Prepare array-based edge data structures and priority queue
  Eigen::VectorXi EMAP;
  Eigen::MatrixXi E, EF, EI;
  typedef std::set<std::pair<double, int>> PriorityQueue;
  PriorityQueue Q;
  std::vector<PriorityQueue::iterator> Qit;
  // If an edge were collapsed, we'd collapse it to these points:
  Eigen::MatrixXd C;
  int num_collapsed;
  { // prep
    F = OF;
    V = OV;
    igl::edge_flaps(F, E, EMAP, EF, EI);
    Qit.resize(E.rows());

    C.resize(E.rows(), V.cols());
    Eigen::VectorXd costs(E.rows());
    Q.clear();
    for (int e = 0; e < E.rows(); e++) {
      double cost = e;
      Eigen::RowVectorXd p(1, 3);
      igl::shortest_edge_and_midpoint(e, V, F, E, EMAP, EF, EI, cost, p);
      C.row(e) = p;
      Qit[e]   = Q.insert(std::pair<double, int>(cost, e)).first;
    }
    num_collapsed = 0;
  }
  { // decimate
    while (not Q.empty()) {
      bool something_collapsed = false;
      // collapse edge
      const int max_iter = std::ceil(amount * Q.size());
      for (int j = 0; j < max_iter; j++) {
        if (not igl::collapse_edge(
                igl::shortest_edge_and_midpoint, //
                V,
                F,
                E,
                EMAP,
                EF,
                EI,
                Q,
                Qit,
                C)) {
          break;
        }
        something_collapsed = true;
        num_collapsed++;
      }
    }
  }
  rval->_verts = V;
  rval->_faces = F;
  return rval;
}
//////////////////////////////////////////////////////////////////////////////
Eigen::MatrixXd IglMesh::computeFaceNormals() const {
  Eigen::MatrixXd rval;
  igl::per_face_normals(_verts, _faces, rval);
  return rval;
}
Eigen::MatrixXd IglMesh::computeVertexNormals() const {
  Eigen::MatrixXd rval;
  igl::per_vertex_normals(_verts, _faces, rval);
  return rval;
}
Eigen::MatrixXd IglMesh::computeCornerNormals(float dihedral_angle) const {
  Eigen::MatrixXd rval;
  igl::per_corner_normals(_verts, _faces, dihedral_angle, rval);
  return rval;
}
iglprinciplecurvature_ptr_t IglMesh::computePrincipleCurvature() const {
  auto rval = std::make_shared<IglPrincipleCurvature>();
  // Alternative discrete mean curvature
  Eigen::MatrixXd HN;
  Eigen::SparseMatrix<double> L, M, Minv;
  igl::cotmatrix(_verts, _faces, L);
  igl::massmatrix(_verts, _faces, igl::MASSMATRIX_TYPE_VORONOI, M);
  igl::invert_diag(M, Minv);
  // Laplace-Beltrami of position
  HN = -Minv * (L * _verts);
  // Extract magnitude as mean curvature
  rval->H = HN.rowwise().norm();

  // Compute curvature directions via quadric fitting
  igl::principal_curvature(_verts, _faces, rval->PD1, rval->PD2, rval->PV1, rval->PV2);
  // mean curvature
  rval->H = 0.5 * (rval->PV1 + rval->PV2);
  return rval;
}

Eigen::VectorXd IglMesh::computeGaussianCurvature() const {
  Eigen::VectorXd rval;
  igl::gaussian_curvature(_verts, _faces, rval);
  // Compute mass matrix
  Eigen::SparseMatrix<double> M, Minv;
  igl::massmatrix(_verts, _faces, igl::MASSMATRIX_TYPE_DEFAULT, M);
  igl::invert_diag(M, Minv);
  // Divide by area to get integral average
  rval = (Minv * rval).eval();
  return rval;
}

//////////////////////////////////////////////////////////////////////////////
Eigen::VectorXd IglMesh::ambientOcclusion(int numsamples) const {
  Eigen::VectorXd AO;
  Eigen::MatrixXd N;
  igl::per_vertex_normals(_verts, _faces, N);
  igl::embree::ambient_occlusion(_verts, _faces, _verts, N, numsamples, AO);
  AO = 1.0 - AO.array();
  return AO;
}
//////////////////////////////////////////////////////////////////////////////

Eigen::MatrixXd IglMesh::parameterizeHarmonic() const {
  Eigen::MatrixXd harmonic_uvs;
  // Find the open boundary
  Eigen::VectorXi bnd;
  igl::boundary_loop(_faces, bnd);

  // Map the boundary to a circle, preserving edge proportions
  Eigen::MatrixXd bnd_uv;
  igl::map_vertices_to_circle(_verts, bnd, bnd_uv);

  // Harmonic parametrization for the internal vertices
  igl::harmonic(_verts, _faces, bnd, bnd_uv, 1, harmonic_uvs);
  return harmonic_uvs;
}
//////////////////////////////////////////////////////////////////////////////
Eigen::MatrixXd IglMesh::parameterizeLCSM() {

  Eigen::MatrixXd lcsm_uvs;
  // Fix two points on the boundary
  Eigen::VectorXi bnd, b(2, 1);
  igl::boundary_loop(_faces, bnd);
  b(0) = bnd(0);
  b(1) = bnd(bnd.size() / 2);
  Eigen::MatrixXd bc(2, 2);
  bc << 0, 0, 1, 0;

  // LSCM parametrization
  igl::lscm(_verts, _faces, b, bc, lcsm_uvs);
  return lcsm_uvs;
}
//////////////////////////////////////////////////////////////////////////////
iglmesh_ptr_t IglMesh::cleaned() const {
  auto rval = std::make_shared<IglMesh>(_verts, _faces);
  using namespace Eigen;
  using namespace igl;
  using namespace igl::copyleft::tetgen;
  using namespace igl::copyleft::cgal;

  ////////////////////////////////////////////////////////////////////////
  auto V = _verts;
  auto F = _faces;
  MatrixXd CV; // clean output verts
  MatrixXi CF; // clean output faces
  VectorXi IM;
  MatrixXi _1;
  VectorXi _2;
  ////////////////////////////////////////////////////////////////////////
  // check manifold status
  ////////////////////////////////////////////////////////////////////////
  // OrkAssert(isVertexManifold() and isEdgeManifold());
  ////////////////////////////////////////////////////////////////////////
  // remesh_self_intersections
  ////////////////////////////////////////////////////////////////////////
  remesh_self_intersections(V, F, {false, false, false}, CV, CF, _1, _2, IM);
  std::for_each(CF.data(), CF.data() + CF.size(), [&IM](int& a) { a = IM(a); });
  // validate_IM(V,CV,IM);
  std::cout << "clean: remove_unreferenced" << std::endl;
  {
    MatrixXi oldCF = CF;
    unique_simplices(oldCF, CF);
  }
  MatrixXd oldCV = CV;
  MatrixXi oldCF = CF;
  VectorXi nIM;
  remove_unreferenced(oldCV, oldCF, CV, CF, nIM);
  std::for_each(IM.data(), IM.data() + IM.size(), [&nIM](int& a) { a = a >= 0 ? nIM(a) : a; });
  ////////////////////////////////////////////////////////////////////////
  // tetrahedralize
  ////////////////////////////////////////////////////////////////////////
  MatrixXd TV;
  MatrixXi TT;
  {
    MatrixXi _1;
    // c  convex hull
    // Y  no boundary steiners
    // p  polygon input
    // T1e-16  sometimes helps tetgen
    // cout << "clean: tetrahedralize" << endl;
    // writeOBJ("CVCF.obj", CV, CF);
    CDTParam params;
    params.flags            = "CYT1e-16";
    params.use_bounding_box = true;
    if (cdt(CV, CF, params, TV, TT, _1) != 0) {
      OrkAssert(false);
    }
    // writeMESH("TVTT.mesh",TV,TT,MatrixXi());
  }
  ////////////////////////////////////////////////////////////////////////
  {
    MatrixXd BC;
    barycenter(TV, TT, BC);
    VectorXd W;
    // cout << "clean: winding_number" << endl;
    winding_number(V, F, BC, W);
    W                   = W.array().abs();
    const double thresh = 0.5;
    const int count     = (W.array() > thresh).cast<int>().sum();
    MatrixXi CT(count, TT.cols());
    int c = 0;
    for (int t = 0; t < TT.rows(); t++) {
      if (W(t) > thresh) {
        CT.row(c++) = TT.row(t);
      }
    }
    //////
    assert(c == count);
    boundary_facets(CT, CF);
    // writeMESH("CVCTCF.mesh",TV,CT,CF);
    // cout << "clean: remove_unreferenced" << endl;
    // Force all original vertices to be referenced
    MatrixXi FF = F;
    std::for_each(FF.data(), FF.data() + FF.size(), [&IM](int& a) { a = IM(a); });
    int ncf = CF.rows();
    MatrixXi ref(ncf + FF.rows(), 3);
    ref << CF, FF;
    VectorXi nIM;
    remove_unreferenced(TV, ref, CV, CF, nIM);
    // Only keep boundary faces
    CF.conservativeResize(ncf, 3);
    // cout << "clean: IM.minCoeff(): " << IM.minCoeff() << endl;
    // reindex nIM through IM
    std::for_each(IM.data(), IM.data() + IM.size(), [&nIM](int& a) { a = a >= 0 ? nIM(a) : a; });
  }
  ////////////////////////////////////////////////////////////////////////
  // output
  ////////////////////////////////////////////////////////////////////////
  rval->_verts = CV;
  rval->_faces = CF;
  return rval;
}
//////////////////////////////////////////////////////////////////////////////
iglmesh_ptr_t IglMesh::reOriented() const {
  auto rval = std::make_shared<IglMesh>(_verts, _faces);
  std::vector<Eigen::VectorXi> C(2);
  std::vector<Eigen::MatrixXi> FF(2);
  // Compute patches
  for (int pass = 0; pass < 2; pass++) {
    Eigen::VectorXi I;
    igl::embree::reorient_facets_raycast(
        rval->_verts,
        rval->_faces, //
        rval->_faces.rows() * 100,
        10,
        pass == 1,
        false,
        false,
        I,
        C[pass]);
    // apply reorientation
    FF[pass].conservativeResize(
        rval->_faces.rows(), //
        rval->_faces.cols());
    for (int i = 0; i < I.rows(); i++) {
      if (I(i)) {
        FF[pass].row(i) = (rval->_faces.row(i).reverse()).eval();
      } else {
        FF[pass].row(i) = rval->_faces.row(i);
      }
    }
  }
  constexpr int selector = 0; // 0==patchwise, 1==facetwise
  rval->_faces           = FF[selector];
  return rval;
}
//////////////////////////////////////////////////////////////////////////////
iglmesh_ptr_t IglMesh::parameterizedSCAF(int numiters, double scale, double bias) const {

  Eigen::MatrixXd V = _verts;
  Eigen::MatrixXi F = _faces;
  igl::SCAFData scaf_data;

  Eigen::MatrixXd bnd_uv, uv_init;

  Eigen::VectorXd M;
  igl::doublearea(V, F, M);
  std::vector<std::vector<int>> all_bnds;
  igl::boundary_loop(F, all_bnds);

  printf("numbnds<%zu>\n", all_bnds.size());

  // Heuristic primary boundary choice: longest
  auto primary_bnd = std::max_element(
      all_bnds.begin(), all_bnds.end(), [](const std::vector<int>& a, const std::vector<int>& b) { return a.size() < b.size(); });

  OrkAssert(primary_bnd != all_bnds.end()); // see https://github.com/libigl/libigl/issues/873

  Eigen::VectorXi bnd = Eigen::Map<Eigen::VectorXi>(primary_bnd->data(), primary_bnd->size());

  igl::map_vertices_to_circle(V, bnd, bnd_uv);
  bnd_uv *= sqrt(M.sum() / (2 * igl::PI));
  if (all_bnds.size() == 1) {
    if (bnd.rows() == V.rows()) // case: all vertex on boundary
    {
      uv_init.resize(V.rows(), 2);
      for (int i = 0; i < bnd.rows(); i++)
        uv_init.row(bnd(i)) = bnd_uv.row(i);
    } else {
      igl::harmonic(V, F, bnd, bnd_uv, 1, uv_init);
      if (igl::flipped_triangles(uv_init, F).size() != 0)
        igl::harmonic(F, bnd, bnd_uv, 1, uv_init); // fallback uniform laplacian
    }
  } else {
    // if there is a hole, fill it and erase additional vertices.
    all_bnds.erase(primary_bnd);
    Eigen::MatrixXi F_filled;
    igl::topological_hole_fill(F, bnd, all_bnds, F_filled);
    igl::harmonic(F_filled, bnd, bnd_uv, 1, uv_init);
    uv_init.conservativeResize(V.rows(), 2);
  }

  Eigen::VectorXi b;
  Eigen::MatrixXd bc;
  igl::scaf_precompute(V, F, uv_init, scaf_data, igl::MappingEnergyType::SYMMETRIC_DIRICHLET, b, bc, 0);

  //_verts = V;
  //_faces = F;

  igl::scaf_solve(scaf_data, numiters);

  auto rval                           = std::make_shared<IglMesh>(V, F);
  double uv_scale                     = 0.2 * 0.5 * scale;
  double uv_bias                      = 0.5 + bias;
  Eigen::MatrixXd scaledandbiased_uvs = uv_scale * scaf_data.w_uv.topRows(V.rows());
  size_t num_uvs                      = scaledandbiased_uvs.rows();
  for (size_t i = 0; i < num_uvs; i++) {
    scaledandbiased_uvs(i, 0) = scaledandbiased_uvs(i, 0) + uv_bias; // U
    scaledandbiased_uvs(i, 1) = scaledandbiased_uvs(i, 1) + uv_bias; // V
  }
  rval->_uvs = scaledandbiased_uvs;
  return rval;
}
//////////////////////////////////////////////////////////////////////////////
void submesh::igl_test() {
  auto trimesh = submesh();
  submeshTriangulate(*this, trimesh);
}
//////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
