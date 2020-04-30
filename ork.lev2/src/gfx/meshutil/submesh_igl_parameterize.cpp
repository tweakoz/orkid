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

#include <igl/boundary_loop.h>
#include <igl/lscm.h>
#include <igl/MappingEnergyType.h>
#include <igl/map_vertices_to_circle.h>
#include <igl/harmonic.h>
#include <igl/flipped_triangles.h>
#include <igl/topological_hole_fill.h>
#include <igl/scaf.h>

namespace ork::meshutil {
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
} // namespace ork::meshutil
