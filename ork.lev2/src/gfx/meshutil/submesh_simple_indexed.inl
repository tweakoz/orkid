////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>

namespace ork::meshutil {

  struct SimpleIndexedMesh {
    std::vector<fvec3> _positions;
    std::vector<fvec3> _normals;
    std::vector<fvec2> _texcoords;
    std::vector<uint32_t> _triangle_indices;
  };

  SimpleIndexedMesh submeshToSimpleIndexedMesh(const submesh& inpsubmesh ) {
    submesh as_tris;
    submeshTriangulate(inpsubmesh, as_tris);
    submesh temp;
    std::unordered_map<vertex_const_ptr_t,vertex_ptr_t> used_vertices;
    as_tris.visitAllPolys([&](poly_const_ptr_t p) {
      p->visitVertices([&](vertex_const_ptr_t v) {
          auto it = used_vertices.find(v);
          if(it==used_vertices.end()){
              auto merged = temp.mergeVertex(*v);
              used_vertices[v] = merged;
          }
      });
    }); // visit all polys       
    as_tris.visitAllPolys([&](poly_const_ptr_t p) {
      std::vector<vertex_ptr_t> merged_vertices;
      p->visitVertices([&](vertex_const_ptr_t v) {
          auto it = used_vertices.find(v);
          OrkAssert(it!=used_vertices.end());
          merged_vertices.push_back(it->second);
      });
      temp.mergePoly(merged_vertices);
    }); // visit all polys

    SimpleIndexedMesh outmesh;
    temp.visitAllVertices([&](vertex_const_ptr_t v) {
      outmesh._positions.push_back(dvec3_to_fvec3(v->mPos));
      outmesh._normals.push_back(dvec3_to_fvec3(v->mNrm));
      outmesh._texcoords.push_back(v->mUV[0].mMapTexCoord);
    });
    temp.visitAllPolys([&](poly_const_ptr_t p) {
      if(p->numVertices()==3) {
        p->visitVertices([&](vertex_const_ptr_t v) {
          outmesh._triangle_indices.push_back(v->_poolindex);
        });
      }
      else {
        OrkAssert(false);
      }
    });

    return outmesh;
  }

} //namespace ork::meshutil {
