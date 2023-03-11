////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
      case 3: {
        auto v0 = ply._vertices[0];
        auto v1 = ply._vertices[1];
        auto v2 = ply._vertices[2];
        auto m0 = outmesh._vtxpool->mergeVertex(*v0);
        auto m1 = outmesh._vtxpool->mergeVertex(*v1);
        auto m2 = outmesh._vtxpool->mergeVertex(*v2);
        outmesh.MergePoly(poly(m0, m1, m2));
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
        outmesh.MergePoly(poly(m0, m1, m2));
        outmesh.MergePoly(poly(m2, m3, m0));
        break;
      }
      case 5: {
        auto v0 = ply._vertices[0];
        auto v1 = ply._vertices[1];
        auto v2 = ply._vertices[2];
        auto v3 = ply._vertices[3];
        auto v4 = ply._vertices[4];
        auto m0 = outmesh._vtxpool->mergeVertex(*v0);
        auto m1 = outmesh._vtxpool->mergeVertex(*v1);
        auto m2 = outmesh._vtxpool->mergeVertex(*v2);
        auto m3 = outmesh._vtxpool->mergeVertex(*v3);
        auto m4 = outmesh._vtxpool->mergeVertex(*v4);
        outmesh.MergePoly(poly(m0, m1, m2));
        outmesh.MergePoly(poly(m2, m3, m0));
        outmesh.MergePoly(poly(m0, m3, m4));
        break;
      }
      default:
        OrkAssert(false);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void submeshTrianglesToQuads(const submesh& inpmesh, //
                             submesh& outmesh, //
                             float area_tolerance, //
                             bool exclude_non_coplanar, //
                             bool exclude_non_rectangular ) { //

  ///////////////////////////////////////

  fplane3 P0, P1;
  vertex_ptr_t ici[6];
  fvec4 VPos[6];

  ///////////////////////////////////////

  int inumtri = inpmesh.GetNumPolys(3);

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
    // fvec4 VArea012[3] = { VPos[0],VPos[1],VPos[2] };

    float fArea012 = fvec4::calcTriangularArea(VPos[0], VPos[1], VPos[2], P0.GetNormal());

    /*if( 0 != _isnan( fArea012 ) )
    {
        fArea012 = 0.0f;
    }*/

    /////////////////////////
    // get other triangles connected to this via any of its edges
    /////////////////////////

    orkset<int> ConnectedPolySet;
    orkset<int> ConnectedPolySetA;
    orkset<int> ConnectedPolySetB;
    orkset<int> ConnectedPolySetC;
    inpmesh.GetConnectedPolys(edge(v0, v1), ConnectedPolySetA);
    inpmesh.GetConnectedPolys(edge(v1, v2), ConnectedPolySetB);
    inpmesh.GetConnectedPolys(edge(v2, v0), ConnectedPolySetC);

    for ( int conpoly : ConnectedPolySetA ) {
      ConnectedPolySet.insert(conpoly);
    }
    for (int conpoly : ConnectedPolySetB) {
      ConnectedPolySet.insert(conpoly);
    }
    for (int conpoly : ConnectedPolySetC) {
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
        // fvec4 VArea345[3] = { VPos[3],VPos[4],VPos[5] };

        bool coplanar_check = P0.IsCoPlanar(P1);

        if (coplanar_check or (not exclude_non_coplanar)) {

          float fArea345 = fvec4::calcTriangularArea(VPos[3], VPos[4], VPos[5], P1.GetNormal());

          /*if( 0 != _isnan( fArea345 ) )
          {
              fArea345 = 0.0f;
          }*/

          float DelArea = fabs(fArea012 - fArea345);
          float AvgArea = (fArea012 + fArea345) * float(0.5f);
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

                fvec4 VDelAC = (VPos[icorner0] - VPos[ilo0]).normalized();
                fvec4 VDelAB = (VPos[icorner0] - VPos[ilo1]).normalized();
                fvec4 VDelDC = (VPos[icorner1] - VPos[ilo0]).normalized();
                fvec4 VDelBD = (VPos[ilo1] - VPos[icorner1]).normalized();

                float fdotACBD = VDelAC.dotWith(VDelBD); // quad is at least a parallelogram if ang(V02) == ang(V31)
                float fdotACAB = VDelAC.dotWith(VDelAB); // quad is rectangular if V01 is perpendicular to V02
                float fdotDCBD = VDelDC.dotWith(VDelBD); // quad is rectangular if V01 is perpendicular to V02

                ////////////////////////////////////////////
                // make sure its a rectangular quad by comparing edge directions
                ////////////////////////////////////////////

                bool rect_check = (fdotACBD > float(0.999f)) && (fabs(fdotACAB) < float(0.02f)) && (fabs(fdotDCBD) < float(0.02f));

                if(rect_check or (not exclude_non_rectangular) ) {
                  auto v0 = ici[icorner0];
                  auto v1 = ici[ilo0];
                  auto v2 = ici[ilo1];
                  auto v3 = ici[icorner1];

                  //////////////////////////////////////
                  // ensure good winding order
                  ////////////////////////////////////////////

                  fplane3 P3;
                  P3.CalcPlaneFromTriangle(VPos[icorner0], VPos[ilo0], VPos[ilo1]);

                  float fdot = P3.n.dotWith(P0.n);

                  //////////////////////////////////////

                  if ((float(1.0f) - fdot) < float(0.001f)) {
                    outmesh.MergePoly(poly(v0, v1, v3, v2));
                    was_quadified = true;
                  } else if ((float(1.0f) + fdot) < float(0.001f)) {
                    outmesh.MergePoly(poly(v0, v2, v3, v1));
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
      auto p = poly(ici[0], ici[1], ici[2]);
      if(p.GetNumSides()==3){
        outmesh.MergePoly(p);
      }
    }

  } // for (int ip = 0; ip < inumtri; ip++) {

}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
