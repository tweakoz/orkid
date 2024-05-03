////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/math/plane.hpp>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <deque>
#include "xatlas.h"
#include "submesh_simple_indexed.inl"

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

void submesh_xatlas(const submesh& inpsubmesh, submesh& outsubmesh) {

  auto smesh = submeshToSimpleIndexedMesh(inpsubmesh);

  printf( "num_positions<%zu>\n", smesh._positions.size() );
  printf( "num_normals<%zu>\n", smesh._normals.size() );
  printf( "num_texcoords<%zu>\n", smesh._texcoords.size() );
  printf( "num_triangle_indices<%zu>\n", smesh._triangle_indices.size() );

  xatlas::SetPrint(printf,true);

  xatlas::Atlas *atlas = xatlas::Create();
  
  xatlas::MeshDecl decl;

  decl.vertexCount = (int)smesh._positions.size();
  decl.vertexPositionData = smesh._positions.data();
  decl.vertexPositionStride = sizeof(float) * 3;
  decl.vertexNormalData = smesh._normals.data();
  decl.vertexNormalStride = sizeof(float) * 3;
  decl.vertexUvData = smesh._texcoords.data();
  decl.vertexUvStride = sizeof(float) * 2;
  decl.indexCount = (int)smesh._triangle_indices.size();
  decl.indexData = smesh._triangle_indices.data();
  decl.indexFormat = xatlas::IndexFormat::UInt32;
  xatlas::AddMeshError error = xatlas::AddMesh(atlas, decl);
  if (error != xatlas::AddMeshError::Success) {
    xatlas::Destroy(atlas);
    OrkAssert(false);
  }
  xatlas::AddMeshJoin(atlas);
  xatlas::ChartOptions chartOptions;
  xatlas::PackOptions packOptions;

  chartOptions.maxIterations = 10;
  int RESOLUTION = 4096;
  packOptions.resolution = RESOLUTION;

  xatlas::ComputeCharts(atlas, chartOptions);
  xatlas::PackCharts(atlas, packOptions);

  int num_charts = atlas->chartCount;
  int num_meshes = atlas->meshCount;
  printf( "num_charts<%d>\n", num_charts );
  printf( "num_meshes<%d>\n", num_meshes );

  for( int im=0; im<num_meshes; im++ ) {
    const auto& mesh = atlas->meshes[im];
    int num_charts = mesh.chartCount;
    printf( "  mesh<%d> num_charts<%d>\n", im, num_charts );
    for( int ich=0; ich<num_charts; ich++ ) {
      const auto& chart = mesh.chartArray[ich];
      int num_faces = chart.faceCount;
      int atlas_id = chart.atlasIndex;
      std::string atlas_type;
      switch( chart.type ) {
        case xatlas::ChartType::Planar: atlas_type = "Planar"; break;
        case xatlas::ChartType::Ortho: atlas_type = "Ortho"; break;
        case xatlas::ChartType::LSCM: atlas_type = "LSCM"; break;
        case xatlas::ChartType::Piecewise: atlas_type = "Piecewise"; break;
        case xatlas::ChartType::Invalid: atlas_type = "Invalid"; break;
        default:
          OrkAssert(false);
          break;
      }
      printf( "    chart<%d> atlas_id<%d> atlas_type<%s> num_faces<%d>\n", ich, atlas_id,atlas_type.c_str(), num_faces );
    }

    // pack into outsubmesh

    float inv_res = 1.0f / float(RESOLUTION);
    for( int it=0; it<mesh.indexCount; it+=3 ) {
      int idx0 = mesh.indexArray[it+0];
      int idx1 = mesh.indexArray[it+1];
      int idx2 = mesh.indexArray[it+2];

      const auto& avtx0 = mesh.vertexArray[idx0];
      const auto& avtx1 = mesh.vertexArray[idx1];
      const auto& avtx2 = mesh.vertexArray[idx2];

      const auto& pos0 = smesh._positions[avtx0.xref];
      const auto& pos1 = smesh._positions[avtx1.xref];
      const auto& pos2 = smesh._positions[avtx2.xref];

      const auto& nrm0 = smesh._normals[avtx0.xref];
      const auto& nrm1 = smesh._normals[avtx1.xref];
      const auto& nrm2 = smesh._normals[avtx2.xref];

      const auto& uva0 = smesh._texcoords[avtx0.xref];
      const auto& uva1 = smesh._texcoords[avtx1.xref];
      const auto& uva2 = smesh._texcoords[avtx2.xref];

      fvec2 uvb0 = fvec2( avtx0.uv[0], avtx0.uv[1] ) * inv_res;
      fvec2 uvb1 = fvec2( avtx1.uv[0], avtx1.uv[1] ) * inv_res;
      fvec2 uvb2 = fvec2( avtx2.uv[0], avtx2.uv[1] ) * inv_res;

      vertex vtxout0, vtxout1, vtxout2;

      //printf( "uvb0.x,uvb0.y<%f,%f>\n", uvb0.x, uvb0.y );
      //printf( "uvb1.x,uvb1.y<%f,%f>\n", uvb1.x, uvb1.y );
      //printf( "uvb2.x,uvb2.y<%f,%f>\n", uvb2.x, uvb2.y );

      vtxout0.mPos = dvec3(pos0.x,pos0.y,pos0.z);
      vtxout0.mNrm = dvec3(nrm0.x,nrm0.y,nrm0.z);
      vtxout0.mUV[1].mMapTexCoord = fvec2(uva0.x,uva0.y);
      vtxout0.mUV[0].mMapTexCoord = fvec2(uvb0.x,uvb0.y);
      vtxout0.miNumUvs = 2;

      vtxout1.mPos = dvec3(pos1.x,pos1.y,pos1.z);
      vtxout1.mNrm = dvec3(nrm1.x,nrm1.y,nrm1.z);
      vtxout1.mUV[1].mMapTexCoord = fvec2(uva1.x,uva1.y);
      vtxout1.mUV[0].mMapTexCoord = fvec2(uvb1.x,uvb1.y);
      vtxout1.miNumUvs = 2;

      vtxout2.mPos = dvec3(pos2.x,pos2.y,pos2.z);
      vtxout2.mNrm = dvec3(nrm2.x,nrm2.y,nrm2.z);
      vtxout2.mUV[1].mMapTexCoord = fvec2(uva2.x,uva2.y);
      vtxout2.mUV[0].mMapTexCoord = fvec2(uvb2.x,uvb2.y);
      vtxout2.miNumUvs = 2;

      auto vout0 = outsubmesh.mergeVertex(vtxout0);
      auto vout1 = outsubmesh.mergeVertex(vtxout1);
      auto vout2 = outsubmesh.mergeVertex(vtxout2);

      printf( "vout0.uv0<%f,%f>\n", vout0->mUV[0].mMapTexCoord.x, vout0->mUV[0].mMapTexCoord.y );

      auto p = Polygon(vout0,vout1,vout2);
      outsubmesh.mergePoly(p);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
