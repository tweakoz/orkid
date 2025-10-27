////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"

#include <cstring>
#include <ork/math/cvector3.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include "_vdb_impl.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

///////////////////////////////////////

struct MicroMesh;
struct MicroMeshConnectivity;
using micromesh_ptr_t = std::shared_ptr<MicroMesh>;
using micromesh_connectivity_ptr_t = std::shared_ptr<MicroMeshConnectivity>;
using indexlist_t = std::vector<int>;

///////////////////////////////////////
// Helper functions for py::list or buffer protocol (numpy) support
///////////////////////////////////////

inline void loadVec3Data(std::vector<fvec3>& out_verts, py::object input) {
  // Try buffer protocol first (numpy arrays, etc.)
  if (py::isinstance<py::buffer>(input)) {
    py::buffer_info info = input.cast<py::buffer>().request();

    // Validate buffer format and shape
    if (info.format != py::format_descriptor<float>::format()) {
      throw std::runtime_error("Buffer must have float32 dtype");
    }
    if (info.ndim != 2 || info.shape[1] != 3) {
      throw std::runtime_error("Buffer must have shape (N, 3)");
    }

    size_t num_verts = info.shape[0];
    out_verts.reserve(num_verts);

    float* ptr = static_cast<float*>(info.ptr);

    // Check if we can use fast memcpy path:
    // - C-contiguous layout (natural strides)
    // - stride[0] == 3 * sizeof(float) (row stride is 3 floats)
    // - stride[1] == sizeof(float) (column stride is 1 float)
    bool is_c_contiguous = (info.strides[0] == 3 * sizeof(float)) &&
                           (info.strides[1] == sizeof(float));

    if (is_c_contiguous) {
      // Fast path: direct memcpy
      // Memory layout matches fvec3 array layout exactly
      size_t num_floats = num_verts * 3;
      size_t num_bytes = num_floats * sizeof(float);

      // Resize vector to make room
      out_verts.resize(num_verts);

      // Single memcpy operation
      std::memcpy(out_verts.data(), ptr, num_bytes);
    }
    else {
      // Slow path: handle non-contiguous or transposed arrays
      // This handles Fortran order, sliced views, custom strides, etc.
      size_t stride0 = info.strides[0] / sizeof(float);
      size_t stride1 = info.strides[1] / sizeof(float);

      for (size_t i = 0; i < num_verts; i++) {
        float x = ptr[i * stride0 + 0 * stride1];
        float y = ptr[i * stride0 + 1 * stride1];
        float z = ptr[i * stride0 + 2 * stride1];
        out_verts.push_back(fvec3(x, y, z));
      }
    }
  }
  // Fall back to py::list
  else if (py::isinstance<py::list>(input)) {
    py::list vert_list = input.cast<py::list>();
    for (const auto& vtx : vert_list) {
      out_verts.push_back(vtx.cast<fvec3>());
    }
  }
  else {
    throw std::runtime_error("Input must be a list of vec3 or numpy array with shape (N, 3)");
  }
}

///////////////////////////////////////

struct MicroMeshConnectivity {
  std::unordered_map<int,indexlist_t> _connectivity;
};

///////////////////////////////////////

using umesh_rprim_t = meshutil::rigidprim_V12N12B12T8C4_t;
using umesh_rprim_ptr_t = std::shared_ptr<umesh_rprim_t>;
struct MicroMesh {
  MicroMesh() {}
  MicroMesh(py::object vert_data, py::list face_list);

  // Update methods for reusing storage (support py::list or numpy arrays for vertex/normal data)
  void updateVertices(py::object vert_data);  // Only update vertices (topology unchanged)
  void updateFromLists(py::object vert_data, py::list face_list);  // Update everything
  void updateConnectivity();  // Recompute internal connectivity
  void updateNormals(py::object normal_data);  // Set normals from list or numpy array
  void computeNormals();  // Compute normals using internal connectivity

  micromesh_connectivity_ptr_t getConnectivity();  // Get connectivity (compute if needed)
  micromesh_ptr_t smoothed(micromesh_connectivity_ptr_t conn) const;
  void updateRigidPrim(umesh_rprim_ptr_t prim,
                       vdb_vec3grid_ptr_t colorgrid,
                       ctx_t context) const;

  std::vector<fvec3> _vertices;
  std::vector<fvec3> _colors;
  std::vector<fvec3> _normals;
  std::vector<indexlist_t> _tris;
  std::vector<indexlist_t> _quads;

  micromesh_connectivity_ptr_t _connectivity;
  bool _connectivity_dirty = true;
};

///////////////////////////////////////

inline MicroMesh::MicroMesh(py::object vert_data, py::list face_list) {
  updateFromLists(vert_data, face_list);
}

///////////////////////////////////////

inline void MicroMesh::updateVertices(py::object vert_data) {
  // Update only vertices, preserving topology
  _vertices.clear();
  _colors.clear();

  // Load vertices using helper (supports py::list or numpy)
  loadVec3Data(_vertices, vert_data);

  // Initialize colors
  _colors.resize(_vertices.size(), fvec3(1.0f, 1.0f, 1.0f));

  // Topology unchanged, connectivity still valid
}

///////////////////////////////////////

inline void MicroMesh::updateFromLists(py::object vert_data, py::list face_list) {
  // Clear and reuse existing storage
  _vertices.clear();
  _colors.clear();
  _tris.clear();
  _quads.clear();

  // Load vertices using helper (supports py::list or numpy)
  loadVec3Data(_vertices, vert_data);

  // Initialize colors
  _colors.resize(_vertices.size(), fvec3(1.0f, 1.0f, 1.0f));

  // Load faces
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

  // Topology changed, mark connectivity dirty
  _connectivity_dirty = true;
}

///////////////////////////////////////

inline void MicroMesh::updateConnectivity() {
  // Create if needed
  if (!_connectivity) {
    _connectivity = std::make_shared<MicroMeshConnectivity>();
  }

  // Clear existing connectivity and rebuild
  _connectivity->_connectivity.clear();

  // Build connectivity from triangles
  for( size_t itri=0; itri<_tris.size(); itri++ ){
    auto& tri = _tris[itri];
    for( size_t iv=0; iv<tri.size(); iv++ ){
      int iv0 = tri[(iv+0)%3];
      int iv1 = tri[(iv+1)%3];
      int iv2 = tri[(iv+2)%3];
      _connectivity->_connectivity[iv0].push_back(iv1);
      _connectivity->_connectivity[iv0].push_back(iv2);
    }
  }

  // Build connectivity from quads
  for( size_t iquad=0; iquad<_quads.size(); iquad++ ){
    auto& quad = _quads[iquad];
    for( size_t iv=0; iv<quad.size(); iv++ ){
      int iv0 = quad[(iv+0)%4];
      int iv1 = quad[(iv+1)%4];
      int iv2 = quad[(iv+2)%4];
      int iv3 = quad[(iv+3)%4];
      _connectivity->_connectivity[iv0].push_back(iv1);
      _connectivity->_connectivity[iv0].push_back(iv2);
      _connectivity->_connectivity[iv0].push_back(iv3);
    }
  }

  _connectivity_dirty = false;
}

///////////////////////////////////////

inline micromesh_connectivity_ptr_t MicroMesh::getConnectivity() {
  if (_connectivity_dirty || !_connectivity) {
    updateConnectivity();
  }
  return _connectivity;
}

///////////////////////////////////////

inline micromesh_ptr_t MicroMesh::smoothed(micromesh_connectivity_ptr_t conn) const {
  auto result = std::make_shared<MicroMesh>();
  size_t num_verts = _vertices.size();
  result->_vertices.reserve(num_verts);
  result->_colors.reserve(num_verts);
  for( size_t iv=0; iv<_vertices.size(); iv++ ){
    const auto& vtx = _vertices[iv];
    const auto& connlist = conn->_connectivity.at(iv);
    fvec3 sum;
    for( auto ivc : connlist ){
      sum += _vertices[ivc];
    }
    fvec3 avg = sum / float(connlist.size());
    result->_vertices.push_back(avg);
    result->_colors.push_back(_colors[iv]);
  }
  result->_tris = _tris;
  result->_quads = _quads;
  return result;
}

void MicroMesh::updateRigidPrim(umesh_rprim_ptr_t prim,
                                vdb_vec3grid_ptr_t colorgrid,
                                ctx_t context) const {
  ////////////////////////////////////////////
  int num_verts = _vertices.size();
  int num_tris = _tris.size();
  int num_quads = _quads.size();
  int num_indices_required = num_tris * 3 + num_quads * 6;
  ////////////////////////////////////////////
  auto GBI = context->GBI();
  prim->_gpuClusters.clear();
  auto cluster        = std::make_shared<umesh_rprim_t::PrimGroupCluster>();
  auto vtxbuf         = std::make_shared<lev2::StaticVertexBuffer<SVtxV12N12B12T8C4>>(num_verts, 0);
  auto idxbuf         = std::make_shared<lev2::StaticIndexBuffer<uint32_t>>(num_indices_required);
  cluster->_vtxbuffer = vtxbuf;
  auto PG             = std::make_shared<umesh_rprim_t::PrimitiveGroup>();
  cluster->_primgroups.push_back(PG);
  PG->_primtype  = lev2::PrimitiveType::TRIANGLES;
  PG->_idxbuffer = idxbuf;
  prim->_gpuClusters.push_back(cluster);
  //////////////////////////////////////////////////////////////
  // Use cached normals (must call updateNormals() before this)
  const auto& normals = _normals;
  //////////////////////////////////////////////////////////////
  auto vtxptr            = GBI->LockVB(*vtxbuf.get(), 0, num_verts);
  auto typed_vertex_base = (SVtxV12N12B12T8C4*)vtxptr;
  int ivtx = 0;
  fvec3 updir(0.0f, 1.0f, 0.0f);
  for (size_t ivtx = 0; ivtx < num_verts; ivtx++) {
    auto& vertex_out     = typed_vertex_base[ivtx];
    vertex_out._position = _vertices[ivtx];
    vertex_out._normal   = normals[ivtx];
    // compute binormal (towards up direction)
    fvec3 binormal = vertex_out._normal.crossWith(updir);
    fvec3 x2 = binormal.crossWith(vertex_out._normal);
    vertex_out._binormal = x2;
    vertex_out._color = _colors[ivtx].ARGBU32();
  }
  if(colorgrid){
    auto accessor = colorgrid->getConstAccessor();
    using sampler_t = openvdb::tools::GridSampler<openvdb::Vec3SGrid::ConstAccessor,openvdb::tools::BoxSampler>;
    auto color_sampler = sampler_t(accessor,colorgrid->transform());
    for (size_t ivtx = 0; ivtx < num_verts; ivtx++) {
      auto& vertex_out     = typed_vertex_base[ivtx];
      const auto& pos = _vertices[ivtx];
      auto coord_w = openvdb::Vec3f(pos.x, pos.y, pos.z);
      auto coord_i = colorgrid->worldToIndex(coord_w);
      auto coord_ii = openvdb::Coord(coord_i.x(), coord_i.y(), coord_i.z());

      //auto color = openvdb::tools::PointSampler::sample(colorgrid->tree(), coord_i);
      auto color = color_sampler.wsSample(coord_w);
      //auto color = accessor.getValue(coord_ii);
      if(0)printf("pos<%f %f %f> coord_w<%f %f %f> coord_i<%f %f %f> color<%f %f %f>\n",
             pos.x, pos.y, pos.z,
             coord_w.x(), coord_w.y(), coord_w.z(),
             coord_i.x(), coord_i.y(), coord_i.z(),
             color.x(), color.y(), color.z());
      //printf("color %d <%f %f %f>\n", ivtx, color.x(), color.y(), color.z());
      auto as_orkv3 = fvec3(color.x(), color.y(), color.z());
      vertex_out._color = as_orkv3.ABGRU32();
    }
  }
  //////////////////////////////////////////////////////////////
  int oidx              = 0;
  auto idxptr           = GBI->LockIB(*idxbuf.get(), 0, num_indices_required);
  auto typed_indices = (uint32_t*)idxptr;

  for( auto t : _tris ){
    typed_indices[oidx++] = t[0];
    typed_indices[oidx++] = t[1];
    typed_indices[oidx++] = t[2];
  }
  for( auto q : _quads ){
    typed_indices[oidx++] = q[0];
    typed_indices[oidx++] = q[1];
    typed_indices[oidx++] = q[2];
    typed_indices[oidx++] = q[0];
    typed_indices[oidx++] = q[2];
    typed_indices[oidx++] = q[3];
  }
  OrkAssert(oidx == num_indices_required);
  // printf("oidx<%d> num_indices_required<%d>\n", oidx, num_indices_required);
  GBI->UnLockIB(*idxbuf.get());
  GBI->UnLockVB(*vtxbuf.get());
  //////////////////////////////////////////////////////////////
  // prim->fromData(primdata, context.get());
}

///////////////////////////////////////

inline void MicroMesh::updateNormals(py::object normal_data) {
  // Update normals from pre-generated list or numpy array
  _normals.clear();

  // Load normals using helper (supports py::list or numpy)
  loadVec3Data(_normals, normal_data);
}

///////////////////////////////////////

inline void MicroMesh::computeNormals() {
  // Ensure connectivity is up to date
  auto conn = getConnectivity();

  _normals.clear();
  _normals.reserve(_vertices.size());

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
    _normals.push_back(normalize(sum));
  }
}

} //namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
