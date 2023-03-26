////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/config.h>
#if defined(ENABLE_IGL)

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/igl.h>
#include <iostream>

#include <Eigen/Core>

#include <igl/embree/ambient_occlusion.h>

namespace ork::meshutil {

//////////////////////////////////////////////////////////////////////////////
Eigen::VectorXd IglMesh::ambientOcclusion(int numsamples) const {
  Eigen::VectorXd AO;
  Eigen::MatrixXd N = computeVertexNormals();
  igl::embree::ambient_occlusion(_verts, _faces, _verts, N, numsamples, AO);
  AO = 1.0 - AO.array();
  return AO;
}

} // namespace ork::meshutil

#endif