////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace meshutil {
///////////////////////////////////////////////////////////////////////////////

void submeshTriangulate(const submesh& inpmesh, submesh& outmesh) {
  int inump = inpmesh.GetNumPolys();

  for (int ip = 0; ip < inump; ip++) {
    const poly& ply = *inpmesh._orderedPolys[ip];

    int inumv = ply.GetNumSides();

    switch (inumv) {
      case 0:
      case 1:
      case 2:
        OrkAssert(false);
        break;
      case 3: {
        auto v0 = ply._vertices[0];
        auto v1 = ply._vertices[1];
        auto v2 = ply._vertices[2];
        auto m0 = outmesh._vtxpool->mergeVertex(*v0);
        auto m1 = outmesh._vtxpool->mergeVertex(*v1);
        auto m2 = outmesh._vtxpool->mergeVertex(*v2);
        outmesh.mergeTriangle(m0, m1, m2);
        break;
      }
      case 4: {
        auto v0 = ply._vertices[0];
        auto v1 = ply._vertices[1];
        auto v2 = ply._vertices[2];
        auto v3 = ply._vertices[3];
        auto m0 = outmesh._vtxpool->mergeVertex(*v0);
        auto m1 = outmesh._vtxpool->mergeVertex(*v1);
        auto m2 = outmesh._vtxpool->mergeVertex(*v2);
        auto m3 = outmesh._vtxpool->mergeVertex(*v3);

        poly TP0(m0,m1,m2);
        poly TP1(m1,m2,m3);

        double maxtp0 = TP0.maxEdgeLength();
        double maxtp1 = TP1.maxEdgeLength();

        if( maxtp0<(maxtp1*0.9) ){
          outmesh.mergeTriangle(m0, m1, m2);
          outmesh.mergeTriangle(m2, m3, m0);
        }
        else{
          outmesh.mergeTriangle(m1, m2, m3);
          outmesh.mergeTriangle(m3, m0, m1);
        }
        break;
      }
      default: {
        dvec3 c = ply.centerOfMass();
        vertex vc;
        vc.mPos = c;
        auto mc = outmesh._vtxpool->mergeVertex(vc);
        for( int i=0; i<inumv; i++ ){
          auto v0 = ply._vertices[i];
          auto v1 = ply._vertices[(i+1)%inumv];
          auto m0 = outmesh._vtxpool->mergeVertex(*v0);
          auto m1 = outmesh._vtxpool->mergeVertex(*v1);
          outmesh.mergeTriangle(mc, m0, m1);
        }
        break;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void submeshTrianglesToQuads(const submesh& inpmesh, //
                             submesh& outmesh, //
                             double area_tolerance, //
                             bool exclude_non_coplanar, //
                             bool exclude_non_rectangular ) { //

  ///////////////////////////////////////

  dplane3 P0, P1;
  vertex_ptr_t ici[6];
  dvec3 VPos[6];

  ///////////////////////////////////////

  int inumtri = inpmesh.GetNumPolys(3);

  auto inp_edgemap = inpmesh.allEdgesByVertexHash();

  for (int ip = 0; ip < inumtri; ip++) {
    bool was_quadified = false;

    const poly& inpoly = inpmesh.RefPoly(ip);

    auto v0 = inpoly._vertices[0];
    auto v1 = inpoly._vertices[1];
    auto v2 = inpoly._vertices[2];

    ici[0] = outmesh.mergeVertex(*v0);
    ici[1] = outmesh.mergeVertex(*v1);
    ici[2] = outmesh.mergeVertex(*v2);

    VPos[0] = v0->mPos;
    VPos[1] = v1->mPos;
    VPos[2] = v2->mPos;
    P0.CalcPlaneFromTriangle(VPos[0], VPos[1], VPos[2]);
    // dvec3 VArea012[3] = { VPos[0],VPos[1],VPos[2] };

    double fArea012 = dvec3::calcTriangularArea(VPos[0], VPos[1], VPos[2], P0.GetNormal());

    /*if( 0 != _isnan( fArea012 ) )
    {
        fArea012 = 0.0f;
    }*/

    /////////////////////////
    // get other triangles connected to this via any of its edges
    /////////////////////////

    orkset<int> ConnectedPolySet;

    for ( int conpoly : inpmesh.connectedPolys(edge(v0, v1)) ) {
      ConnectedPolySet.insert(conpoly);
    }
    for (int conpoly : inpmesh.connectedPolys(edge(v1, v2))) {
      ConnectedPolySet.insert(conpoly);
    }
    for (int conpoly : inpmesh.connectedPolys(edge(v2, v0))) {
      ConnectedPolySet.insert(conpoly);
    }

    /////////////////////////////////////////////////////////////////////////////
    // for each connected poly, test if it matches our quad critera
    /////////////////////////////////////////////////////////////////////////////

    for ( int iotherpoly : ConnectedPolySet ) { //

      const poly& other_poly = inpmesh.RefPoly(iotherpoly);

      int inumsides = other_poly.GetNumSides();

      if ((inumsides == 3) && (iotherpoly != ip)) { // if not the same triangle

        ////////////////////////////////////////

        IndexTestContext itestctx;

        auto v3 = other_poly._vertices[0];
        auto v4 = other_poly._vertices[1];
        auto v5 = other_poly._vertices[2];

        VPos[3] = v3->mPos;
        VPos[4] = v4->mPos;
        VPos[5] = v5->mPos;
        ici[3] = outmesh.mergeVertex(*v3);
        ici[4] = outmesh.mergeVertex(*v4);
        ici[5] = outmesh.mergeVertex(*v5);

        P1.CalcPlaneFromTriangle(VPos[3], VPos[4], VPos[5]);
        // dvec3 VArea345[3] = { VPos[3],VPos[4],VPos[5] };

        bool coplanar_check = P0.IsCoPlanar(P1);

        if (coplanar_check or (not exclude_non_coplanar)) {

          double fArea345 = dvec3::calcTriangularArea(VPos[3], VPos[4], VPos[5], P1.GetNormal());

          /*if( 0 != _isnan( fArea345 ) )
          {
              fArea345 = 0.0f;
          }*/

          double DelArea = fabs(fArea012 - fArea345);
          double AvgArea = (fArea012 + fArea345) * double(0.5f);
          bool area_checkA = (DelArea / AvgArea) < area_tolerance;
          bool area_checkB = (AvgArea / DelArea) < area_tolerance;

          if (area_checkA or area_checkB) { 

            ////////////////////////////////////////////
            // count number of unique indices in the 2 triangles, for quads this should be 4
            ////////////////////////////////////////////

            orkset<int> TestForQuadIdxSet;
            std::multimap<int, int> TestForQuadIdxMap;

            for (int it = 0; it < 6; it++) {
              int idx = ici[it]->_poolindex;

              TestForQuadIdxMap.insert(std::make_pair(idx, it));
              TestForQuadIdxSet.insert(idx);
            }

            int inumidxinset = (int) TestForQuadIdxSet.size();

            ////////////////////////////////////////////

            if (4 == inumidxinset) { // is it a quad with a shared edge ?

              ////////////////////////////////////////////
              // find the shared edges
              ////////////////////////////////////////////

              int imc = 0;

              for (int idx : TestForQuadIdxSet) {

                int matches =
                    (int)std::count_if(TestForQuadIdxMap.begin(), TestForQuadIdxMap.end(), [=](const std::pair<int, int>& pr) {
                      return (pr.first == idx);
                    });

                if (2 == matches) {
                  OrkAssert(imc < 2);
                  itestctx.iset  = imc++;
                  itestctx.itest = idx;
                  for (auto item : TestForQuadIdxMap) {
                    if (item.first == itestctx.itest) {
                      itestctx.PairedIndices[itestctx.iset].insert(item.second);
                      itestctx.PairedIndicesCombined.insert(item.second);
                    }
                  }
                }
              }

              ////////////////////////////////////////////
              // find corner edges (unshared)
              ////////////////////////////////////////////

              for (int ii = 0; ii < 6; ii++) {
                if (itestctx.PairedIndicesCombined.find(ii) == itestctx.PairedIndicesCombined.end()) {
                  itestctx.CornerIndices.insert(ii);
                }
              }

              ////////////////////////////////////////////
              // test if the quad is rectangular, if so add it
              ////////////////////////////////////////////

              if ((itestctx.CornerIndices.size() == 2) && (itestctx.PairedIndicesCombined.size() == 4)) {
                orkset<int>::iterator itcorner = itestctx.CornerIndices.begin();
                orkset<int>::iterator itpair0  = itestctx.PairedIndices[0].begin();
                orkset<int>::iterator itpair1  = itestctx.PairedIndices[1].begin();

                int icorner0 = (*itcorner++);
                int ilo0     = (*itpair0++);
                int ihi0     = (*itpair0);

                int icorner1 = (*itcorner);
                int ilo1     = (*itpair1++);
                int ihi1     = (*itpair1);

                ////////////////////////////////////////////
                // AD are corners
                ////////////////////////////////////////////

                dvec3 VDelAC = (VPos[icorner0] - VPos[ilo0]).normalized();
                dvec3 VDelAB = (VPos[icorner0] - VPos[ilo1]).normalized();
                dvec3 VDelDC = (VPos[icorner1] - VPos[ilo0]).normalized();
                dvec3 VDelBD = (VPos[ilo1] - VPos[icorner1]).normalized();

                double fdotACBD = VDelAC.dotWith(VDelBD); // quad is at least a parallelogram if ang(V02) == ang(V31)
                double fdotACAB = VDelAC.dotWith(VDelAB); // quad is rectangular if V01 is perpendicular to V02
                double fdotDCBD = VDelDC.dotWith(VDelBD); // quad is rectangular if V01 is perpendicular to V02

                ////////////////////////////////////////////
                // make sure its a rectangular quad by comparing edge directions
                ////////////////////////////////////////////

                bool rect_check = (fdotACBD > double(0.999f)) && (fabs(fdotACAB) < double(0.02f)) && (fabs(fdotDCBD) < double(0.02f));

                if(rect_check or (not exclude_non_rectangular) ) {
                  auto v0 = ici[icorner0];
                  auto v1 = ici[ilo0];
                  auto v2 = ici[ilo1];
                  auto v3 = ici[icorner1];

                  //////////////////////////////////////
                  // ensure good winding order
                  ////////////////////////////////////////////

                  dplane3 P3;
                  P3.CalcPlaneFromTriangle(VPos[icorner0], VPos[ilo0], VPos[ilo1]);

                  double fdot = P3.n.dotWith(P0.n);

                  //////////////////////////////////////

                  if ((double(1.0f) - fdot) < double(0.001f)) {
                    outmesh.mergeQuad(v0, v1, v3, v2);
                    was_quadified = true;
                  } else if ((double(1.0f) + fdot) < double(0.001f)) {
                    outmesh.mergeQuad(v0, v2, v3, v1);
                    was_quadified = true;
                  }
                }
              }
            }
          }
        }
      }

    } // for( set<int>::iterator it=ConnectedPolySet.begin(); it!=ConnectedPolySet.end(); it++ )

    ////////////////////////////////////////////
    // if not quadified, add the triangle
    ////////////////////////////////////////////

    if (false == was_quadified) {
      outmesh.mergeTriangle(ici[0], ici[1], ici[2]);
    }

  } // for (int ip = 0; ip < inumtri; ip++) {

}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
