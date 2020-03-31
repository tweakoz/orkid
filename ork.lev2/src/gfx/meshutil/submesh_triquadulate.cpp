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
    const poly& ply = inpmesh.mMergedPolys[ip];

    int inumv = ply.GetNumSides();

    switch (inumv) {
      case 3: {
        int idx0         = ply.miVertices[0];
        int idx1         = ply.miVertices[1];
        int idx2         = ply.miVertices[2];
        const vertex& v0 = inpmesh.mvpool.VertexPool[idx0];
        const vertex& v1 = inpmesh.mvpool.VertexPool[idx1];
        const vertex& v2 = inpmesh.mvpool.VertexPool[idx2];
        int imerged0     = outmesh.mvpool.MergeVertex(v0);
        int imerged1     = outmesh.mvpool.MergeVertex(v1);
        int imerged2     = outmesh.mvpool.MergeVertex(v2);
        outmesh.MergePoly(poly(imerged0, imerged1, imerged2));
        break;
      }
      case 4: {
        int idx0         = ply.miVertices[0];
        int idx1         = ply.miVertices[1];
        int idx2         = ply.miVertices[2];
        int idx3         = ply.miVertices[3];
        const vertex& v0 = inpmesh.mvpool.VertexPool[idx0];
        const vertex& v1 = inpmesh.mvpool.VertexPool[idx1];
        const vertex& v2 = inpmesh.mvpool.VertexPool[idx2];
        const vertex& v3 = inpmesh.mvpool.VertexPool[idx3];
        int imerged0     = outmesh.mvpool.MergeVertex(v0);
        int imerged1     = outmesh.mvpool.MergeVertex(v1);
        int imerged2     = outmesh.mvpool.MergeVertex(v2);
        int imerged3     = outmesh.mvpool.MergeVertex(v3);
        outmesh.MergePoly(poly(imerged0, imerged1, imerged2));
        outmesh.MergePoly(poly(imerged2, imerged3, imerged0));
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
  int ici[6];
  fvec4 VPos[6];

  ///////////////////////////////////////

  int inumtri = inpmesh.GetNumPolys(3);

  for (int ip = 0; ip < inumtri; ip++) {
    bool basquad = false;

    const poly& inpoly = inpmesh.RefPoly(ip);

    ici[0] = inpoly.miVertices[0];
    ici[1] = inpoly.miVertices[1];
    ici[2] = inpoly.miVertices[2];

    VPos[0] = inpmesh.mvpool.VertexPool[ici[0]].mPos;
    VPos[1] = inpmesh.mvpool.VertexPool[ici[1]].mPos;
    VPos[2] = inpmesh.mvpool.VertexPool[ici[2]].mPos;
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
    inpmesh.GetConnectedPolys(edge(ici[0], ici[1]), ConnectedPolySetA);
    inpmesh.GetConnectedPolys(edge(ici[1], ici[2]), ConnectedPolySetB);
    inpmesh.GetConnectedPolys(edge(ici[2], ici[0]), ConnectedPolySetC);

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

        ici[3] = ply.miVertices[0];
        ici[4] = ply.miVertices[1];
        ici[5] = ply.miVertices[2];

        VPos[3] = inpmesh.mvpool.VertexPool[ici[3]].mPos;
        VPos[4] = inpmesh.mvpool.VertexPool[ici[4]].mPos;
        VPos[5] = inpmesh.mvpool.VertexPool[ici[5]].mPos;

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
              int idx = ici[it];

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
                  int i0 = ici[icorner0];
                  int i1 = ici[ilo0];
                  int i2 = ici[ilo1];
                  int i3 = ici[icorner1];

                  //////////////////////////////////////
                  // ensure good winding order

                  fplane3 P3;
                  P3.CalcPlaneFromTriangle(VPos[icorner0], VPos[ilo0], VPos[ilo1]);

                  float fdot = P3.n.Dot(P0.n);

                  //////////////////////////////////////

                  if ((float(1.0f) - fdot) < float(0.001f)) {
                    outmesh.MergePoly(poly(i0, i1, i3, i2));
                    basquad = true;
                  } else if ((float(1.0f) + fdot) < float(0.001f)) {
                    outmesh.MergePoly(poly(i0, i2, i3, i1));
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
