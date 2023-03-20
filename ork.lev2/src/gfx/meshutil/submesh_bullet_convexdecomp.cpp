////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/math/plane.hpp>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <deque>

//#include <BulletCollision/CollisionShapes/btShapeHull.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <ConvexDecomposition/ConvexDecomposition.h>
#include <HACD/hacdHACD.h>
//#include <VHACD/vhacdVHACD.h>

/*
void calcConvexDecomposition(unsigned int vcount,
                             const float *vertices,
                             unsigned int tcount,
                             const unsigned int *indices,
                             ConvexDecompInterface *callback,
                             float masterVolume,
                             unsigned int depth);

*/
///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

using decomp_int_t = unsigned int;
using decomp_constint_t = const unsigned int;

struct MyConvexDecomposition : public ConvexDecomposition::ConvexDecompInterface {
  
  void ConvexDecompResult(ConvexDecomposition::ConvexResult &result) final {

    decomp_int_t vcount = result.mHullVcount;
    const float* vertices = result.mHullVertices;
    decomp_int_t num_triangles = result.mHullTcount;
    decomp_constint_t* hullindices = result.mHullIndices;

    float hullvolume = result.mHullVolume; 

    const auto& obb_sides = result.mOBBSides;         // float[3] the width, height and breadth of the best fit OBB
    const auto& obb_center = result.mOBBCenter;       // float[3] the center of the OBB
    const auto& obb_orient = result.mOBBOrientation;  // float[4] the quaternion rotation of the OBB.
    const auto& obb_xform = result.mOBBTransform;     // float[16] the 4x4 transform of the OBB.
    float obb_volume = result.mOBBVolume;             // the volume of the OBB

    printf( "Convex Hull Result vcount<%d> num_triangles<%d> volume<%g>\n", int(vcount), int(num_triangles), hullvolume );

    float sph_radius = result.mSphereRadius;          // radius and center of best fit sphere
    const auto& sph_center = result.mSphereCenter;    // float[3];
    float sph_volume = result.mSphereVolume;


    auto out_mesh = std::make_shared<submesh>();
    std::vector<vertex_ptr_t> out_verts;
    for( int tri_index=0; tri_index<num_triangles; tri_index++ ){
      int i0 = hullindices[tri_index*3+0];
      int i1 = hullindices[tri_index*3+1];
      int i2 = hullindices[tri_index*3+2];

      auto inpv0 = vertices+(i0*3);
      auto inpv1 = vertices+(i1*3);
      auto inpv2 = vertices+(i2*3);
      auto v0 = std::make_shared<vertex>();
      v0->mPos.x = inpv0[0];
      v0->mPos.y = inpv0[1];
      v0->mPos.z = inpv0[2];
      auto v1 = std::make_shared<vertex>();
      v1->mPos.x = inpv1[0];
      v1->mPos.y = inpv1[1];
      v1->mPos.z = inpv1[2];
      auto v2 = std::make_shared<vertex>();
      v2->mPos.x = inpv2[0];
      v2->mPos.y = inpv2[1];
      v2->mPos.z = inpv2[2];

      auto merged_v0 = out_mesh->mergeVertex(*v0);
      auto merged_v1 = out_mesh->mergeVertex(*v1);
      auto merged_v2 = out_mesh->mergeVertex(*v2);

      out_mesh->mergeTriangle(merged_v0,merged_v1,merged_v2);
    }
    _output_submeshes.push_back(out_mesh);
  }

  std::vector<submesh_ptr_t> _output_submeshes;

};

std::vector<submesh_ptr_t> submeshBulletConvexDecomposition(const submesh& inpsubmesh){

  submesh triangulated;
  submeshTriangulate(inpsubmesh,triangulated);

  
  std::vector<fvec3> _tempverts;
  std::vector<decomp_int_t> _tempindices;
  for( auto v : triangulated._vtxpool->_orderedVertices ){
    _tempverts.push_back(v->mPos);
  }
  for( auto p : triangulated._orderedPolys ){
    _tempindices.push_back(p->_vertices[0]->_poolindex);
    _tempindices.push_back(p->_vertices[1]->_poolindex);
    _tempindices.push_back(p->_vertices[2]->_poolindex);
  }
  MyConvexDecomposition interface;

  if(0){
    /*
    ConvexDecomposition::DecompDesc desc;
    desc.mVcount = _tempverts.size();
    desc.mVertices = (const float*) _tempverts.data();
    desc.mTcount = triangulated._orderedPolys.size();
    desc.mIndices = (decomp_int_t*) _tempindices.data();
    desc.mCallback = & interface;
    desc.mDepth = 7; // a maximum of 10, generally not over 7.

    int hull_count = ConvexDecomposition::performConvexDecomposition(desc);
    */
  }
  else{
    float masterVolume = 1.0f;
    decomp_int_t depth = 7; // a maximum of 10, generally not over 7.
    calcConvexDecomposition((decomp_int_t) _tempverts.size(), // vcount
                          (const float*) _tempverts.data(), // vertices
                          triangulated._orderedPolys.size(), // tcount
                          (const decomp_int_t*) _tempindices.data(), // indices
                          & interface, // callback
                          masterVolume, // mastervolume
                          depth // depth
                          );
  }

  return interface._output_submeshes;

}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
