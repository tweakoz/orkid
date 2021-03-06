////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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
        auto m0 = outmesh._vtxpool.newMergeVertex(*v0);
        auto m1 = outmesh._vtxpool.newMergeVertex(*v1);
        auto m2 = outmesh._vtxpool.newMergeVertex(*v2);
        outmesh.MergePoly(poly(m0, m1, m2));
        break;
      }
      case 4: {
        auto v0 = ply._vertices[0];
        auto v1 = ply._vertices[1];
        auto v2 = ply._vertices[2];
        auto v3 = ply._vertices[3];
        auto m0 = outmesh._vtxpool.newMergeVertex(*v0);
        auto m1 = outmesh._vtxpool.newMergeVertex(*v1);
        auto m2 = outmesh._vtxpool.newMergeVertex(*v2);
        auto m3 = outmesh._vtxpool.newMergeVertex(*v3);
        outmesh.MergePoly(poly(m0, m1, m2));
        outmesh.MergePoly(poly(m2, m3, m0));
        break;
      }
      default:
        OrkAssert(false);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void submeshTrianglesToQuads(const submesh& inpmesh, submesh& outmesh) {
  ///////////////////////////////////////

  fplane3 P0, P1;
  vertex_ptr_t ici[6];
  fvec4 VPos[6];

  ///////////////////////////////////////

  int inumtri = inpmesh.GetNumPolys(3);

  for (int ip = 0; ip < inumtri; ip++) {
    bool basquad = false;

    const poly& inpoly = inpmesh.RefPoly(ip);

    auto v0 = inpoly._vertices[0];
    auto v1 = inpoly._vertices[1];
    auto v2 = inpoly._vertices[2];

    VPos[0] = v0->mPos;
    VPos[1] = v1->mPos;
    VPos[2] = v2->mPos;
    P0.CalcPlaneFromTriangle(VPos[0], VPos[1], VPos[2]);
    // fvec4 VArea012[3] = { VPos[0],VPos[1],VPos[2] };

    float fArea012 = fvec4::CalcTriArea(VPos[0], VPos[1], VPos[2], P0.GetNormal());

    /*if( 0 != _isnan( fArea012 ) )
    {
        fArea012 = 0.0f;
    }*/

    /////////////////////////
    // get other triangles connected to this via any of its edges

    orkset<int> ConnectedPolySet;
    orkset<int> ConnectedPolySetA;
    orkset<int> ConnectedPolySetB;
    orkset<int> ConnectedPolySetC;
    inpmesh.GetConnectedPolys(edge(v0, v1), ConnectedPolySetA);
    inpmesh.GetConnectedPolys(edge(v1, v2), ConnectedPolySetB);
    inpmesh.GetConnectedPolys(edge(v2, v0), ConnectedPolySetC);

    for (orkset<int>::iterator it = ConnectedPolySetA.begin(); it != ConnectedPolySetA.end(); it++) {
      ConnectedPolySet.insert(*it);
    }
    for (orkset<int>::iterator it = ConnectedPolySetB.begin(); it != ConnectedPolySetB.end(); it++) {
      ConnectedPolySet.insert(*it);
    }
    for (orkset<int>::iterator it = ConnectedPolySetC.begin(); it != ConnectedPolySetC.end(); it++) {
      ConnectedPolySet.insert(*it);
    }

    /////////////////////////////////////////////////////////////////////////////
    // for each connected poly, test if it matches our quad critera

    for (orkset<int>::iterator it = ConnectedPolySet.begin(); it != ConnectedPolySet.end(); it++) {
      int iotherpoly = *it;

      const poly& ply = inpmesh.RefPoly(iotherpoly);

      int inumsides = ply.GetNumSides();

      if ((inumsides == 3) && (iotherpoly != ip)) // if not the same triangle
      {
        ////////////////////////////////////////

        IndexTestContext itestctx;

        auto v3 = ply._vertices[0];
        auto v4 = ply._vertices[1];
        auto v5 = ply._vertices[2];

        VPos[3] = v3->mPos;
        VPos[4] = v4->mPos;
        VPos[5] = v5->mPos;

        P1.CalcPlaneFromTriangle(VPos[3], VPos[4], VPos[5]);
        // fvec4 VArea345[3] = { VPos[3],VPos[4],VPos[5] };

        float fArea345 = fvec4::CalcTriArea(VPos[3], VPos[4], VPos[5], P1.GetNormal());

        /*if( 0 != _isnan( fArea345 ) )
        {
            fArea345 = 0.0f;
        }*/

        float DelArea = fabs(fArea012 - fArea345);
        float AvgArea = (fArea012 + fArea345) * float(0.5f);

        if (P0.IsCoPlanar(P1)) {
          if ((DelArea / AvgArea) < float(0.01f)) // are the areas within 1% of each other ?
          {
            ////////////////////////////////////////////
            // count number of unique indices in the 2 triangles, for quads this should be 4

            orkset<int> TestForQuadIdxSet;
            std::multimap<int, int> TestForQuadIdxMap;

            for (int it = 0; it < 6; it++) {
              int idx = ici[it]->_poolindex;

              TestForQuadIdxMap.insert(std::make_pair(idx, it));
              TestForQuadIdxSet.insert(idx);
            }

            int inumidxinset = (int)TestForQuadIdxSet.size();

            ////////////////////////////////////////////

            if (4 == inumidxinset) // its a quad with a shared edge
            {
              ////////////////////////////////////////////
              // find the shared edges

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

              for (int ii = 0; ii < 6; ii++) {
                if (itestctx.PairedIndicesCombined.find(ii) == itestctx.PairedIndicesCombined.end()) {
                  itestctx.CornerIndices.insert(ii);
                }
              }

              ////////////////////////////////////////////
              // test if the quad is rectangular, if so add it

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

                // AD are corners

                fvec4 VDelAC = (VPos[icorner0] - VPos[ilo0]).Normal();
                fvec4 VDelAB = (VPos[icorner0] - VPos[ilo1]).Normal();
                fvec4 VDelDC = (VPos[icorner1] - VPos[ilo0]).Normal();
                fvec4 VDelBD = (VPos[ilo1] - VPos[icorner1]).Normal();

                float fdotACBD = VDelAC.Dot(VDelBD); // quad is at least a parallelogram if ang(V02) == ang(V31)
                float fdotACAB = VDelAC.Dot(VDelAB); // quad is rectangular if V01 is perpendicular to V02
                float fdotDCBD = VDelDC.Dot(VDelBD); // quad is rectangular if V01 is perpendicular to V02

                // make sure its a rectangular quad by comparing edge directions

                if ((fdotACBD > float(0.999f)) && (fabs(fdotACAB) < float(0.02f)) && (fabs(fdotDCBD) < float(0.02f))) {
                  auto v0 = ici[icorner0];
                  auto v1 = ici[ilo0];
                  auto v2 = ici[ilo1];
                  auto v3 = ici[icorner1];

                  //////////////////////////////////////
                  // ensure good winding order

                  fplane3 P3;
                  P3.CalcPlaneFromTriangle(VPos[icorner0], VPos[ilo0], VPos[ilo1]);

                  float fdot = P3.n.Dot(P0.n);

                  //////////////////////////////////////

                  if ((float(1.0f) - fdot) < float(0.001f)) {
                    outmesh.MergePoly(poly(v0, v1, v3, v2));
                    basquad = true;
                  } else if ((float(1.0f) + fdot) < float(0.001f)) {
                    outmesh.MergePoly(poly(v0, v2, v3, v1));
                    basquad = true;
                  }
                }
              }
            }
          }
        }
      }

    } // for( set<int>::iterator it=ConnectedPolySet.begin(); it!=ConnectedPolySet.end(); it++ )

    if (false == basquad) {
      outmesh.MergePoly(poly(ici[0], ici[1], ici[2]));
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
