#pragma once

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <ork/lev2/gfx/meshutil/submesh.h>

namespace ork::meshutil {

  struct IglPrincipleCurvature {
    Eigen::VectorXd H; // mean curvature (magnitude)
    Eigen::MatrixXd PD1,PD2; // directions
    Eigen::VectorXd PV1,PV2;
  };
  using iglprinciplecurvature_ptr_t = std::shared_ptr<IglPrincipleCurvature>;


  struct IglMesh {
    IglMesh(const submesh& inp_submesh, int numsides);
    IglMesh(const Eigen::MatrixXd& verts, const Eigen::MatrixXi& faces);
    Eigen::MatrixXd computeFaceNormals() const;
    Eigen::MatrixXd computeVertexNormals() const;
    Eigen::MatrixXd computeCornerNormals(float dihedral_angle) const;
    Eigen::VectorXd computeGaussianCurvature() const;
    iglprinciplecurvature_ptr_t computePrincipleCurvature() const;
    Eigen::MatrixXd parameterizeHarmonic() const;
    Eigen::MatrixXd parameterizeLCSM() const;
    fvec4 computeAreaStatistics() const;
    fvec4 computeAngleStatistics() const;
    size_t countIrregularVertices() const;
    double averageEdgeLength() const;

    iglmesh_ptr_t reOriented() const;
    Eigen::VectorXd ambientOcclusion(int numsamples) const;

    int _numvertices;
    submesh_ptr_t toSubMesh() const;
    int _numfaces;
    int _sidesPerFace;
    Eigen::MatrixXd _verts;
    Eigen::MatrixXi _faces;
    Eigen::MatrixXd _normals;
    Eigen::MatrixXd _binormals;
    Eigen::MatrixXd _tangents;
    Eigen::MatrixXd _colors;
    Eigen::MatrixXd _uvs;
  };

} //namespace ork::meshutil {
