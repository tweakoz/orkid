////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/math/plane.hpp>
#include <ork/math/misc_math.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include "xatlas.h"
#include <deque>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

bool on_xatlas_progress( xatlas::ProgressCategory cat, //
                         int progress, //
                         void* user_data) { //
  printf("xatlas progress<%d>\n", progress);
  return true;
}

void submeshWithTextureUnwrap(const submesh& inpsubmesh, submesh& outsubmesh) {
  int inump = inpsubmesh.GetNumPolys();

  size_t sizeof_vertex = sizeof(vertex);

  std::vector<uint16_t> indices;
  for (auto p : inpsubmesh._orderedPolys) {
    int inumv = p->GetNumSides();
    OrkAssert(inumv == 3);
    for (int i = 0; i < inumv; i++) {
      auto v = p->_vertices[i];
      indices.push_back(v->_poolindex);
    }
  }
  std::vector<fvec3> vertices;
  for (auto v : inpsubmesh._vtxpool->_orderedVertices) {
    vertices.push_back(v->mPos);
  }

  auto vertex_base = (const uint8_t*)vertices.data();

  ///////////////////////////////////////
  // declare xatlas mesh format
  ///////////////////////////////////////

  xatlas::MeshDecl mesh_decl;

  mesh_decl.vertexCount          = vertices.size();
  mesh_decl.vertexPositionData   = (const void*)(vertex_base);
  mesh_decl.vertexPositionStride = sizeof(fvec3);
  mesh_decl.indexCount           = indices.size();
  mesh_decl.indexData            = indices.data();
  mesh_decl.indexFormat          = xatlas::IndexFormat::UInt16;

  ///////////////////////////////////////
  // add xatlas mesh
  ///////////////////////////////////////

  xatlas::Atlas* atlas = xatlas::Create();
  auto xatlas_status   = xatlas::AddMesh(atlas, mesh_decl, 1);
  OrkAssert(xatlas_status == xatlas::AddMeshError::Success);

  ///////////////////////////////////////
  // perform the xatlas charting
  ///////////////////////////////////////

  xatlas::ChartOptions chart_options;
  chart_options.maxIterations = 2;
  xatlas::ComputeCharts(atlas, chart_options);
  xatlas::SetProgressCallback(atlas, on_xatlas_progress);
  
  ///////////////////////////////////////
  // define packing options
  ///////////////////////////////////////

  xatlas::PackOptions pack_options;
  pack_options.padding = 0.0f;
  //pack_options.maxChartSize = 128; // squared
  pack_options.resolution = 8192; // squared
  pack_options.bruteForce = false; 
  xatlas::PackCharts(atlas, pack_options);

  ///////////////////////////////////////
  // debug output
  ///////////////////////////////////////

  printf("atlas->atlasCount<%d>\n", atlas->atlasCount);
  printf("atlas->meshCount<%d>\n", atlas->meshCount);
  printf("atlas->chartCount<%d>\n", atlas->chartCount);
  printf("atlas->width<%d>\n", atlas->width);
  printf("atlas->height<%d>\n", atlas->height);

  ///////////////////////////////////////

  uint32_t firstVertex = 0;
  for (uint32_t i = 0; i < atlas->meshCount; i++) {
    const xatlas::Mesh& mesh = atlas->meshes[i];
    std::vector<vertex_ptr_t> merged_vertices;
    std::map<int, int> chart_count;
    for (uint32_t v = 0; v < mesh.vertexCount; v++) {
      const xatlas::Vertex& vertex = mesh.vertexArray[v];
      int orig_vtx_index           = vertex.xref;
      auto temp_vtx                = *(inpsubmesh._vtxpool->_orderedVertices[orig_vtx_index]);
      float tu                     = vertex.uv[0] / atlas->width;
      float tv                     = vertex.uv[1] / atlas->height;
      int chart_index              = vertex.chartIndex;
      chart_count[chart_index]++; 
      temp_vtx.mUV[0].mMapTexCoord = fvec2(tu, tv);
      auto merged                  = outsubmesh.mergeVertex(temp_vtx);
      merged_vertices.push_back(merged);
    }
    for (auto item : chart_count) {
      printf("chart<%d> count<%d>\n", item.first, item.second);
    }
    for (uint32_t f = 0; f < mesh.indexCount; f += 3) {
      std::vector<vertex_ptr_t> face_vertices;
      for (uint32_t j = 0; j < 3; j++) {
        const uint32_t index = firstVertex + mesh.indexArray[f + j];
        face_vertices.push_back(merged_vertices[index]);
      }
      outsubmesh.mergePoly(face_vertices);
    }
    firstVertex += mesh.vertexCount;
  }
  xatlas::Destroy(atlas);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
