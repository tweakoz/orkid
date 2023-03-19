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

class MyConvexDecomposition : public ConvexDecomposition::ConvexDecompInterface {
  
  void ConvexDecompResult(ConvexDecomposition::ConvexResult &result) final {

    decomp_int_t vcount = result.mHullVcount;
    const float* vertices = result.mHullVertices;
    decomp_int_t hulltcount = result.mHullTcount;
    decomp_constint_t* hullindices = result.mHullIndices;

    float hullvolume = result.mHullVolume; 

    const auto& obb_sides = result.mOBBSides;         // float[3] the width, height and breadth of the best fit OBB
    const auto& obb_center = result.mOBBCenter;       // float[3] the center of the OBB
    const auto& obb_orient = result.mOBBOrientation;  // float[4] the quaternion rotation of the OBB.
    const auto& obb_xform = result.mOBBTransform;     // float[16] the 4x4 transform of the OBB.
    float obb_volume = result.mOBBVolume;             // the volume of the OBB

    printf( "Convex Hull Result vcount<%d> tcount<%d> volume<%g>\n", int(vcount), int(hulltcount), hullvolume );

    float sph_radius = result.mSphereRadius;          // radius and center of best fit sphere
    const auto& sph_center = result.mSphereCenter;    // float[3];
    float sph_volume = result.mSphereVolume;

  }
};

std::vector<submesh_ptr_t> submeshBulletConvexDecomposition(const submesh& inpsubmesh){

  submesh triangulated;
  submeshTriangulate(inpsubmesh,triangulated);

  std::vector<submesh_ptr_t> convex_output_submeshes;
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

  return convex_output_submeshes;

}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
