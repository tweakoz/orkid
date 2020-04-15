#pragma once

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <ork/lev2/gfx/meshutil/submesh.h>

namespace ork::meshutil {
  struct IglMesh {
    IglMesh(const submesh& inp_submesh, int numsides);
    int _numvertices;
    int _numfaces;
    Eigen::MatrixXd _verts;
    Eigen::MatrixXi _faces;
  };

} //namespace ork::meshutil {
