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
#include <igl/cotmatrix.h>
#include <igl/massmatrix.h>
#include <igl/invert_diag.h>
#include <iostream>

namespace ork::meshutil {
//////////////////////////////////////////////////////////////////////////////
iglmesh_ptr_t submesh::toIglMesh() const {
  auto trimesh = submesh();
  submeshTriangulate(*this, trimesh);
  return std::make_shared<IglMesh>(trimesh, 3);
}
//////////////////////////////////////////////////////////////////////////////
IglMesh::IglMesh(const submesh& inp_submesh, int numsides)
    : _numvertices(inp_submesh._vtxpool.GetNumVertices())
    , _numfaces(inp_submesh.GetNumPolys(numsides))
    , _verts(_numvertices, 3)
    , _faces(_numfaces, numsides) {
  printf("_numvertices<%d>\n", _numvertices);
  printf("_numfaces<%d>\n", _numfaces);
  _verts = Eigen::MatrixXd(_numvertices, 3);
  _faces = Eigen::MatrixXi(_numfaces, numsides);
  ///////////////////////////////////////////////
  // fill in vertices
  ///////////////////////////////////////////////
  for (int v = 0; v < _numvertices; v++) {
    auto pos = inp_submesh._vtxpool.GetVertex(v).mPos;
    _verts.row(v) << pos.x, pos.y, pos.z;
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
  ///////////////////////////////////////////////
  std::cout << _verts << std::endl;
  std::cout << _faces << std::endl;
}

//////////////////////////////////////////////////////////////////////////////

void submesh::igl_test() {

  auto trimesh = submesh();
  submeshTriangulate(*this, trimesh);
  auto iglmesh_tris = IglMesh(trimesh, 3);

  ///////////////////////////////////////////////
  // Eigen::SparseMatrix<double> L;
  // igl::cotmatrix(
  //  mesh_verts, //
  // mesh_faces,
  // L);
  // std::cout << L << std::endl;
}

} // namespace ork::meshutil
