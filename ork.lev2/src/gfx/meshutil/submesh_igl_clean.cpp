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

#include <Eigen/Core>

#include <igl/copyleft/cgal/remesh_self_intersections.h> // GNU GPL (todo move to external executable?)
#include <igl/copyleft/cgal/extract_cells.h>             // GNU GPL (todo move to external executable?)
#include <igl/copyleft/tetgen/tetrahedralize.h>
#include <igl/copyleft/tetgen/cdt.h>
#include <igl/remove_unreferenced.h>
#include <igl/unique_simplices.h>
#include <igl/barycenter.h>
#include <igl/winding_number.h>
#include <igl/extract_manifold_patches.h>
#include <igl/embree/reorient_facets_raycast.h>

namespace ork::meshutil {
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
} // namespace ork::meshutil
