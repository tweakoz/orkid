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

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

void submeshWithFaceNormals(const submesh& inpsubmesh, submesh& outsubmesh){
  int inump = inpsubmesh.GetNumPolys();

  for (int ip = 0; ip < inump; ip++) {
    const poly& ply = *inpsubmesh._orderedPolys[ip];
    fvec3 N = ply.ComputeNormal();

    int inumv = ply.GetNumSides();
    std::vector<vertex_ptr_t> merged_vertices;
    for( int i=0; i<inumv; i++ ){
        auto inp_v0 = *ply._vertices[i];
        inp_v0.mNrm = N;
        auto out_v = outsubmesh._vtxpool->mergeVertex(inp_v0);
        merged_vertices.push_back(out_v);
    }
    outsubmesh.mergePoly(merged_vertices);
  }
}
void submeshWithSmoothNormals(const submesh& inpsubmesh, submesh& outsubmesh, float threshold_radians){
  int inump = inpsubmesh.GetNumPolys();

  threshold_radians *= 0.5f;

  for (int ip = 0; ip < inump; ip++) {
    const poly& ply = *inpsubmesh._orderedPolys[ip];
    fvec3 N = ply.ComputeNormal();

    int inumv = ply.GetNumSides();
    std::vector<vertex_ptr_t> merged_vertices;
    for( int i=0; i<inumv; i++ ){
        auto inp_v0 = ply._vertices[i];
        auto polys = inpsubmesh.polysConnectedTo(inp_v0);
        fvec3 Naccum;
        int ncount = 0;
        for( auto p : polys ){
         fvec3 ON = p->ComputeNormal();
         float angle = N.angle(ON);
         printf( "angle<%g> threshold<%g>\n", angle, threshold_radians);
         if( angle <= threshold_radians ){
           Naccum += ON;
           ncount++;
         }  
        }
        if(ncount==0){
            Naccum = N;
        }
        else{
            Naccum *= (1.0f/float(ncount));
        }
        auto copy_v0 = *inp_v0;
        copy_v0.mNrm = Naccum;
        auto out_v = outsubmesh._vtxpool->mergeVertex(copy_v0);
        merged_vertices.push_back(out_v);
    }
    outsubmesh.mergePoly(merged_vertices);
  }
}


///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
