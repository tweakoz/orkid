////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/util/logger.h>

template class ork::orklut<std::string, ork::meshutil::submesh_ptr_t>;

namespace ork::meshutil {

////////////////////////////////////////////////////////////////

void submesh_fromUvSphere(float radius, size_t num_u, size_t num_v, submesh& outsubmesh){
  outsubmesh.clear();

  // generate uv sphere with num_u iterations on U and num_v iterations on V
  //  generate positions
  //  generate normals
  //  generate binormals
  //  generate tangents
  //  generate UV's -> uv0

  // Create vertices
  for (size_t v = 0; v <= num_v; v++) {
    float v_ratio = float(v) / float(num_v);
    float phi = v_ratio * M_PI; // 0 to PI
    
    float cos_phi = cos(phi);
    float sin_phi = sin(phi);
    
    for (size_t u = 0; u <= num_u; u++) {
      float u_ratio = float(u) / float(num_u);
      float theta = u_ratio * 2.0f * M_PI; // 0 to 2PI
      
      float cos_theta = cos(theta);
      float sin_theta = sin(theta);
      
      // Position
      dvec3 pos;
      pos.x = radius * sin_phi * cos_theta;
      pos.y = radius * cos_phi;
      pos.z = radius * sin_phi * sin_theta;
      
      // Normal (normalized position)
      dvec3 nrm = pos.normalized();
      
      // UV coordinates
      fvec2 uv;
      uv.x = u_ratio;
      uv.y = 1.0f - v_ratio; // Flip V coordinate to match standard UV mapping
      
      // Binormal and tangent 
      fvec3 binormal = fvec3(-sin_theta, 0.0f, cos_theta);
      fvec3 tangent = fvec3(-cos_theta * sin_phi, cos_phi, -sin_theta * sin_phi);
      // Normalize binormal and tangent
      binormal = binormal.normalized();
      tangent = tangent.normalized();
      // Create vertex
      vertex vtx;
      vtx.mPos = pos;
      vtx.mNrm = nrm;
      vtx.mUV[0].mMapTexCoord = uv;
      vtx.mUV[0].mMapBiNormal = binormal;
      vtx.mUV[0].mMapTangent = tangent;
      vtx.miNumUvs = 1;
      
      // Add vertex to submesh
      outsubmesh.mergeVertex(vtx);
    }
  }
  
  // Create faces (quads)
  for (size_t v = 0; v < num_v; v++) {
    for (size_t u = 0; u < num_u; u++) {
      size_t v0 = v * (num_u + 1) + u;
      size_t v1 = v0 + 1;
      size_t v2 = (v + 1) * (num_u + 1) + u + 1;
      size_t v3 = (v + 1) * (num_u + 1) + u;
      
      auto vert0 = outsubmesh.vertex(v0);
      auto vert1 = outsubmesh.vertex(v1);
      auto vert2 = outsubmesh.vertex(v2);
      auto vert3 = outsubmesh.vertex(v3);
      
      outsubmesh.mergeQuad(vert0, vert1, vert2, vert3);
    }
  }
}

////////////////////////////////////////////////////////////////

void submesh_fromIcoSphere(float radius, size_t num_subdivs, submesh& outsubmesh){
  OrkAssert( num_subdivs<10 );

  // generate ico-sphere with num_subdivs subdivisions
  //  num_subdivs==0 : num_tris=20
  //  num_subdivs==1 : num_tris=20
  //  num_subdivs==2 : num_tris=80
  //  num_subdivs==3 : num_tris=320
  //  num_subdivs==4 : num_tris=1280
  //  ...
  //  generate positions
  //  generate normals
  //  generate binormals
  //  generate tangents
  //  generate UV's -> uv0

  outsubmesh.clear();

    

}

////////////////////////////////////////////////////////////////

} //namespace ork::meshutil {
