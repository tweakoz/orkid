////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/config.h>
#if defined(ENABLE_IGL)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/igl.h>
#include <iostream>

#include <Eigen/Core>
#include <igl/gaussian_curvature.h>
#include <igl/principal_curvature.h>
#include <igl/cotmatrix.h>
#include <igl/massmatrix.h>
#include <igl/invert_diag.h>

namespace ork::meshutil {
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

} // namespace ork::meshutil

#pragma GCC diagnostic pop

#endif