////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"

#include <ork/math/cvector3.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

///////////////////////////////////////

struct MicroMesh;
struct MicroMeshConnectivity;
using micromesh_ptr_t = std::shared_ptr<MicroMesh>;
using micromesh_connectivity_ptr_t = std::shared_ptr<MicroMeshConnectivity>;
using indexlist_t = std::vector<int>;

///////////////////////////////////////

struct MicroMeshConnectivity {
  std::unordered_map<int,indexlist_t> _connectivity;
};

///////////////////////////////////////

struct MicroMesh {
  MicroMesh(py::list vert_list, py::list face_list);
  micromesh_connectivity_ptr_t computeVertexConnectivity() const;
  micromesh_ptr_t smoothed(micromesh_connectivity_ptr_t conn) const;
  std::vector<fvec3> computeNormals(micromesh_connectivity_ptr_t conn) const;
  std::vector<fvec3> _vertices;
  std::vector<indexlist_t> _tris;
  std::vector<indexlist_t> _quads;
};

///////////////////////////////////////

inline MicroMesh::MicroMesh(py::list vert_list, py::list face_list) {
  for( const auto& vtx : vert_list ){
    _vertices.push_back(vtx.cast<fvec3>());
  }
  bool done_with_faces     = false;
  int iidx                 = 0;
  size_t numface_values = face_list.size();
  while (not done_with_faces) {
    done_with_faces = (iidx >= numface_values);
    if(not done_with_faces) {
      int face_size = face_list[iidx++].cast<int>();
      //printf("iidx<%d> fac<%d> max<%zu>\n",iidx, face_size, numface_values);
      switch (face_size) {
        case 3: {
          auto& out_tri = _tris.emplace_back();
          out_tri.push_back(face_list[iidx+2].cast<int>());
          out_tri.push_back(face_list[iidx+1].cast<int>());
          out_tri.push_back(face_list[iidx+0].cast<int>());
          iidx += 3;
          break;
        }
        case 4: {
          auto& out_quad = _quads.emplace_back();
          out_quad.push_back(face_list[iidx+3].cast<int>());
          out_quad.push_back(face_list[iidx+2].cast<int>());
          out_quad.push_back(face_list[iidx+1].cast<int>());
          out_quad.push_back(face_list[iidx+0].cast<int>());
          iidx += 4;
          break;
        }
        default:
          OrkAssert(false);
      }
    }
  }
}

///////////////////////////////////////

inline micromesh_connectivity_ptr_t MicroMesh::computeVertexConnectivity() const {
      auto conn = std::make_shared<MicroMeshConnectivity>();
      for( size_t itri=0; itri<_tris.size(); itri++ ){
        auto& tri = _tris[itri];
        for( size_t iv=0; iv<tri.size(); iv++ ){
          int iv0 = tri[iv];
          int iv1 = tri[(iv+1)%3];
          int iv2 = tri[(iv+2)%3];
          conn->_connectivity[iv0].push_back(iv1);
          conn->_connectivity[iv0].push_back(iv2);
        }
      }
      for( size_t iquad=0; iquad<_quads.size(); iquad++ ){
        auto& quad = _quads[iquad];
        for( size_t iv=0; iv<quad.size(); iv++ ){
          int iv0 = quad[iv];
          int iv1 = quad[(iv+1)%4];
          int iv2 = quad[(iv+2)%4];
          int iv3 = quad[(iv+3)%4];
          conn->_connectivity[iv0].push_back(iv1);
          conn->_connectivity[iv0].push_back(iv2);
          conn->_connectivity[iv0].push_back(iv3);
        }
      }
      return conn;
}

///////////////////////////////////////

inline micromesh_ptr_t MicroMesh::smoothed(micromesh_connectivity_ptr_t conn) const {
  auto result = std::make_shared<MicroMesh>(py::list(),py::list());
  for( size_t iv=0; iv<_vertices.size(); iv++ ){
    const auto& vtx = _vertices[iv];
    const auto& connlist = conn->_connectivity.at(iv);
    fvec3 sum;
    for( auto ivc : connlist ){
      sum += _vertices[ivc];
    }
    fvec3 avg = sum / float(connlist.size());
    result->_vertices.push_back(avg);
  }
  result->_tris = _tris;
  result->_quads = _quads;
  return result;
}

///////////////////////////////////////

inline std::vector<fvec3> MicroMesh::computeNormals(micromesh_connectivity_ptr_t conn) const {
  std::vector<fvec3> normals;
  for( size_t iv=0; iv<_vertices.size(); iv++ ){
    const auto& vtx = _vertices[iv];
    const auto& connlist = conn->_connectivity.at(iv);
    fvec3 sum;
    int num_conns = connlist.size();
    for( int ic=0; ic<num_conns; ic++ ){
      int iva = connlist[ic];
      int ivb = connlist[(ic+1)%num_conns];
      auto edge0 = (_vertices[iva] - vtx).normalized();
      auto edge1 = (_vertices[ivb] - vtx).normalized();
      sum += edge0.crossWith(edge1);
    }
    fvec3 normal = normalize(sum);
    normals.push_back(normal);
  }
  return normals;
}

} //namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
