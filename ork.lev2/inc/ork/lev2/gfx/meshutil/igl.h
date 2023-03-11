////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2021, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#if defined(__APPLE__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdtor-name"
#endif

#include <ork/lev2/config.h>

#if defined(ENABLE_IGL)

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <ork/lev2/gfx/meshutil/submesh.h>

namespace ork::meshutil {

enum class BooleanOperation : uint64_t  {
  CrcEnum(PLUS),
  CrcEnum(INTERSECTION),
  CrcEnum(MINUS),
  CrcEnum(XOR),
  CrcEnum(RESOLVE),
};

struct IglPrincipleCurvature {
  Eigen::VectorXd H;        // mean curvature (magnitude)
  Eigen::MatrixXd PD1, PD2; // directions
  Eigen::VectorXd PV1, PV2;
};
using iglprinciplecurvature_ptr_t = std::shared_ptr<IglPrincipleCurvature>;

struct UniqueEdges {
  Eigen::MatrixXi E, uE, EMAP;
  std::vector<std::vector<size_t>> _ue2e;
  size_t _count = 0;
};
using unique_edges_ptr_t = std::shared_ptr<UniqueEdges>;

struct ManifoldExtraction {
  Eigen::VectorXi P;
  Eigen::MatrixXi per_patch_cells;
  size_t _numpatches = 0;
  size_t _numcells   = 0;
};
using manifold_extraction_ptr_t = std::shared_ptr<ManifoldExtraction>;

struct IglMesh {
  IglMesh() {
  }
  IglMesh(const submesh& inp_submesh, int numsides);
  IglMesh(const Eigen::MatrixXd& verts, const Eigen::MatrixXi& faces);
  ////////////////////////////////
  size_t numFaces() const;
  size_t numEdges() const;
  size_t sidesPerFace() const;
  size_t numVertices() const;
  bool isVertexManifold() const;
  bool isEdgeManifold() const;
  size_t genus() const;
  fvec4 computeAreaStatistics() const;
  fvec4 computeAngleStatistics() const;
  size_t countIrregularVertices() const;
  double averageEdgeLength() const;
  unique_edges_ptr_t uniqueEdges() const;
  manifold_extraction_ptr_t extractManifolds() const;
  ////////////////////////////////
  Eigen::MatrixXd computeFaceNormals() const;
  Eigen::MatrixXd computeVertexNormals() const;
  Eigen::MatrixXd computeCornerNormals(float dihedral_angle) const;
  Eigen::MatrixXd parameterizeHarmonic() const;
  Eigen::MatrixXd parameterizeLCSM();
  Eigen::VectorXd ambientOcclusion(int numsamples) const;
  Eigen::VectorXd computeGaussianCurvature() const;
  iglprinciplecurvature_ptr_t computePrincipleCurvature() const;
  ////////////////////////////////
  iglmesh_ptr_t decimated(float amount) const;
  iglmesh_ptr_t parameterizedSCAF(int numiters, double scale, double bias) const;
  iglmesh_ptr_t cleaned() const;
  iglmesh_ptr_t reOriented() const;
  iglmesh_ptr_t triangulated() const;
  submesh_ptr_t toSubMesh() const;
  bool booleanOf( iglmesh_constptr_t a, BooleanOperation operation, iglmesh_constptr_t b );
  ////////////////////////////////
  Eigen::MatrixXd _verts;
  Eigen::MatrixXi _faces;
  Eigen::MatrixXd _normals;
  Eigen::MatrixXd _binormals;
  Eigen::MatrixXd _tangents;
  Eigen::MatrixXd _colors;
  Eigen::MatrixXd _uvs;
};

} // namespace ork::meshutil

#endif
