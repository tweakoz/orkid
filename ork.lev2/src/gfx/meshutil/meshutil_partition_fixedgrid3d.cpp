////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/prop.h>
#include <ork/math/plane.hpp>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/meshutil/meshutil_fixedgrid.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

GridGraph::GridGraph(int fgsize)
    : kfixedgridsize(fgsize)
    , miDimX(0)
    , miDimY(0)
    , miDimZ(0)
    , miNumGrids(0)
    , miNumFilledGrids(0)
    , areamin(0.0)
    , areamax(0.0)
    , areaavg(0.0)
    , areatot(0.0)
    , totpolys(0) {
}

GridGraph::~GridGraph() {
  for (int ig = 0; ig < miNumGrids; ig++) {
    GridNode* pnode = mppGrids[ig];
    if (pnode) {
      delete pnode;
    }
  }
  delete[] mppGrids;
}

////////////////////////////////////////////////////

void GridGraph::BeginPreMerge() {
  vsize    = dvec3(0.0, 0.0, 0.0);
  vmin     = dvec3(0.0, 0.0, 0.0);
  vmax     = dvec3(0.0, 0.0, 0.0);
  areamax  = 0.0;
  areamin  = 0.0;
  areatot  = 0.0;
  areaavg  = 0.0;
  totpolys = 0;
  maab.BeginGrow();
}

////////////////////////////////////////////////////

void GridGraph::PreMergeMesh(const submesh& MeshIn) {

  ///////////////////////////////////////////
  // 1st step, calc some data
  //  min/max/avg/tot surface area
  //  aa bbox / extents
  ///////////////////////////////////////////

  double thisareamax = -std::numeric_limits<double>::max();
  double thisareamin = std::numeric_limits<double>::max();
  double thisareaavg = 0.0;

  MeshIn.visitAllPolys([&](merged_poly_const_ptr_t ply) {
    ///////////////////////////////
    double thisarea = ply->computeArea(dmtx4::Identity());
    thisareamax     = std::max(thisareamax, thisarea);
    thisareamin     = std::min(thisareamin, thisarea);
    thisareaavg += thisarea;
    ///////////////////////////////
    areatot += thisarea;
    areamin = std::min(areamin, thisarea);
    areamax = std::max(areamax, thisarea);
    //////////////////////////////
    totpolys++;
    //////////////////////////////
    int inumsides = ply->numVertices();
    for (int iv = 0; iv < inumsides; iv++) {
      int ivi         = ply->vertexID(iv);
      const vertex& v = *MeshIn.vertex(ivi);
      maab.Grow(dvec3_to_fvec3(v.mPos));
    }
  });
}

////////////////////////////////////////////////////

GridNode* GridGraph::GetGridNode(const GridAddr& addr) {
  OrkAssert(addr.ix >= 0);
  OrkAssert(addr.iy >= 0);
  OrkAssert(addr.iz >= 0);
  OrkAssert(addr.ix < miDimX);
  OrkAssert(addr.iy < miDimY);
  OrkAssert(addr.iz < miDimZ);
  int iaddr = addr.ix + (addr.iy * miDimX) + (addr.iz * miDimX * miDimY);
  return mppGrids[iaddr];
}

void GridGraph::SetGridNode(const GridAddr& addr, GridNode* node) {
  OrkAssert(addr.ix >= 0);
  OrkAssert(addr.iy >= 0);
  OrkAssert(addr.iz >= 0);
  OrkAssert(addr.ix < miDimX);
  OrkAssert(addr.iy < miDimY);
  OrkAssert(addr.iz < miDimZ);
  int iaddr       = addr.ix + (addr.iy * miDimX) + (addr.iz * miDimX * miDimY);
  mppGrids[iaddr] = node;
  miNumFilledGrids++;
}

////////////////////////////////////////////////////

GridAddr GridGraph::GetGridAddress(const dvec3& v) {
  dvec3 vxf = v.transform(mMtxWorldToGrid).xyz();

  GridAddr ret;
  ret.ix = int(vxf.x);
  ret.iy = int(vxf.y);
  ret.iz = int(vxf.z);

  return ret;
}

////////////////////////////////////////////////////

void GridGraph::GetCuttingPlanes(
    const GridAddr& addr,
    dplane3& topplane,
    dplane3& botplane,
    dplane3& lftplane,
    dplane3& rgtplane,
    dplane3& frnplane,
    dplane3& bakplane) {
  const int ix = addr.ix;
  const int iy = addr.iy;
  const int iz = addr.iz;

  double fx0 = double(ix * kfixedgridsize) + vmin.x;
  double fy0 = double(iy * kfixedgridsize) + vmin.y;
  double fz0 = double(iz * kfixedgridsize) + vmin.z;

  double fx1 = fx0 + kfixedgridsize;
  double fy1 = fy0 + kfixedgridsize;
  double fz1 = fz0 + kfixedgridsize;

  static const dvec3 vn_top(0.0, -1.0, 0.0);
  static const dvec3 vn_bot(0.0, 1.0, 0.0);
  static const dvec3 vn_lft(1.0, 0.0, 0.0);
  static const dvec3 vn_rgt(-1.0, 0.0, 0.0);
  static const dvec3 vn_bak(0.0, 0.0, -1.0);
  static const dvec3 vn_frn(0.0, 0.0, 1.0);

  dvec3 vp_top(0.0, fy1, 0.0);
  dvec3 vp_bot(0.0, fy0, 0.0);
  dvec3 vp_lft(fx0, 0.0, 0.0);
  dvec3 vp_rgt(fx1, 0.0, 0.0);
  dvec3 vp_frn(0.0, 0.0, fz0);
  dvec3 vp_bak(0.0, 0.0, fz1);

  topplane.CalcFromNormalAndOrigin(vn_top, vp_top);
  botplane.CalcFromNormalAndOrigin(vn_bot, vp_bot);
  lftplane.CalcFromNormalAndOrigin(vn_lft, vp_lft);
  rgtplane.CalcFromNormalAndOrigin(vn_rgt, vp_rgt);
  frnplane.CalcFromNormalAndOrigin(vn_frn, vp_frn);
  bakplane.CalcFromNormalAndOrigin(vn_bak, vp_bak);
}

////////////////////////////////////////////////////

void GridGraph::EndPreMerge() {
  maab.EndGrow();

  /////////////////////////////
  vmin  = fvec3_to_dvec3(maab.Min());
  vmax  = fvec3_to_dvec3(maab.Max());
  vctr  = (vmin + vmax) * 0.5;
  vsize = vmax - vmin;
  /////////////////////////////
  areaavg = areatot / double(totpolys);
  /////////////////////////////

  orkprintf(
      "fg3d aabb min<%f,%f,%f> max<%f,%f,%f> siz<%f,%f,%f>\n",
      vmin.x,
      vmin.y,
      vmin.z,
      vmax.x,
      vmax.y,
      vmax.z,
      vsize.x,
      vsize.y,
      vsize.z);

  orkprintf("MaxPolyArea<%f> MinPolyArea<%f> AvgPolyArea<%f> TotPolyArea<%f>\n", areamax, areamin, areaavg, areatot);

  ///////////////////////////////////////////

  miDimX = int(vsize.x);
  miDimY = int(vsize.y);
  miDimZ = int(vsize.z);

  int icntX = miDimX / kfixedgridsize;
  int icntY = miDimY / kfixedgridsize;
  int icntZ = miDimZ / kfixedgridsize;

  if ((miDimX % kfixedgridsize) != 0)
    icntX++;
  if ((miDimY % kfixedgridsize) != 0)
    icntY++;
  if ((miDimZ % kfixedgridsize) != 0)
    icntZ++;

  // int icntY = 0; while( miDimY!=0 ) { icntY++; miDimY>>=1; }
  // int icntZ = 0; while( miDimZ!=0 ) { icntZ++; miDimZ>>=1; }

  double fscal = 1.0 / kfixedgridsize;

  miDimX = icntX; //(miDimX*icntX)/kfixedgridsize;
  miDimY = icntY; //(miDimX*icntY)/kfixedgridsize;
  miDimZ = icntZ; //(miDimX*icntZ)/kfixedgridsize;

  miDimX = (miDimX > 0) ? miDimX : 1;
  miDimY = (miDimY > 0) ? miDimY : 1;
  miDimZ = (miDimZ > 0) ? miDimZ : 1;

  ///////////////////////////////////////////////

  dmtx4 MatS;
  dmtx4 MatT;

  MatS.scale(fscal, fscal, fscal);
  MatT.setTranslation(-vmin);
  mMtxWorldToGrid = dmtx4::multiply_ltor(MatT,MatS);

  dvec3 vtest_min = vmin.transform(mMtxWorldToGrid).xyz();
  dvec3 vtest_max = vmax.transform(mMtxWorldToGrid).xyz();
  dvec3 vtest_ctr = vctr.transform(mMtxWorldToGrid).xyz();

  ///////////////////////////////////////////////

  miNumGrids = (miDimX * miDimY * miDimZ);
  mppGrids   = new GridNode*[miNumGrids];
  for (int i = 0; i < miNumGrids; i++)
    mppGrids[i] = 0;
}

////////////////////////////////////////////////////

void GridGraph::MergeMesh(const submesh& MeshIn, Mesh& MeshOut) {

  int inumgtot = 0;

  static int ginuminners = 0;
  static int ginumouters = 0;

  dplane3 topplane, bottomplane, leftplane, rightplane, frontplane, backplane;

  int inumpolys = 0;
  MeshIn.visitAllPolys([&](merged_poly_const_ptr_t ply) {
    inumpolys++;
    ginumouters++;

    /////////////////////////////////
    // Compute AABB of polygon
    /////////////////////////////////
    AABox thaab;
    thaab.BeginGrow();
    for (int iv = 0; iv < ply->numVertices(); iv++) {
      const dvec3& vpos = MeshIn.vertex(ply->vertexID(iv))->mPos;
      thaab.Grow(dvec3_to_fvec3(vpos));
    }
    thaab.EndGrow();

    dvec3 pntA = MeshIn.vertex(ply->vertexID(0))->mPos;
    dvec3 pntB = MeshIn.vertex(ply->vertexID(1))->mPos;
    dvec3 pntC = MeshIn.vertex(ply->vertexID(2))->mPos;
    dvec3 dirA = (pntA - pntB).normalized();
    dvec3 dirB = (pntA - pntC).normalized();
    dvec3 pnrm = dirA.crossWith(dirB);
    dvec3 pctr = (pntA + pntB + pntC) * 0.333333333f;
    dplane3 PolyPlane(pnrm, pctr);

    /////////////////////////////////
    dvec3 polymin = fvec3_to_dvec3(thaab.Min());
    dvec3 polymax = fvec3_to_dvec3(thaab.Max());
    /////////////////////////////////

    const GridAddr ga_min = GetGridAddress(polymin);
    const GridAddr ga_max = GetGridAddress(polymax);

    int igW = ga_max.ix - ga_min.ix;
    int igH = ga_max.iy - ga_min.iy;
    int igD = ga_max.iz - ga_min.iz;

    int inumg = igW * igH * igD;
    inumgtot += inumg;

    GridAddr abs_nodeaddr;
    GridAddr rel_nodeaddr;

    for (int igx = ga_min.ix; igx <= ga_max.ix; igx++) {
      rel_nodeaddr.ix = igx - ga_min.ix;
      abs_nodeaddr.ix = igx;

      for (int igy = ga_min.iy; igy <= ga_max.iy; igy++) {
        rel_nodeaddr.iy = igy - ga_min.iy;
        abs_nodeaddr.iy = igy;

        for (int igz = ga_min.iz; igz <= ga_max.iz; igz++) {
          rel_nodeaddr.iz = igz - ga_min.iz;
          abs_nodeaddr.iz = igz;

          /////////////////////////////////////////////////

          GetCuttingPlanes(abs_nodeaddr, topplane, bottomplane, leftplane, rightplane, frontplane, backplane);

          mupoly_clip_adapter poly_top, poly_bot;
          mupoly_clip_adapter poly_lft, poly_rgt;
          mupoly_clip_adapter poly_fnt, poly_bak;
          mupoly_clip_adapter srcp;

          int inumv = ply->numVertices();
          for (int iv = 0; iv < inumv; iv++) {
            srcp.AddVertex(*MeshIn.vertex(ply->vertexID(iv)));
          }

          ginuminners++;

          if (topplane.ClipPoly(srcp, poly_top))
            if (bottomplane.ClipPoly(poly_top, poly_bot))
              if (leftplane.ClipPoly(poly_bot, poly_lft))
                if (rightplane.ClipPoly(poly_lft, poly_rgt))
                  if (frontplane.ClipPoly(poly_rgt, poly_fnt))
                    if (backplane.ClipPoly(poly_fnt, poly_bak)) {
                      GridNode* node = GetGridNode(abs_nodeaddr);

                      if (0 == node) {
                        node               = new GridNode;
                        node->GridNodeName = CreateFormattedString("fg_%d_%d_%d", igx, igy, igz);
                        SetGridNode(abs_nodeaddr, node);
                      }

                      const char* NodeName = node->GridNodeName.c_str();

                      int inumclippedverts = (int)poly_bak.GetNumVertices();

                      submesh& node_submesh = MeshOut.MergeSubMesh(NodeName);

                      node_submesh.annotations() = MeshIn.annotations();

                      /////////////////////////////////////////////////////////
                      // debug output (wavefront obj fmt)
                      /////////////////////////////////////////////////////////

                      if (0) {
                        for (int ic = 0; ic < inumclippedverts; ic++) {
                          const dvec3& pos = poly_bak.GetVertex(ic).Pos();
                          PropTypeString pts;
                          PropType<dvec3>::ToString(pos, pts);

                          orkprintf("v %s\n", ic, pts.c_str());
                        }
                      }

                      /////////////////////////////////////////////////////////

                      // const char* NodeName = node->GridNodeName.c_str();
                      switch (inumclippedverts) {
                        case 5: {
                          auto v0 = node_submesh.mergeVertex(poly_bak.GetVertex(0));
                          auto v1 = node_submesh.mergeVertex(poly_bak.GetVertex(1));
                          auto v2 = node_submesh.mergeVertex(poly_bak.GetVertex(2));
                          auto v3 = node_submesh.mergeVertex(poly_bak.GetVertex(3));
                          auto v4 = node_submesh.mergeVertex(poly_bak.GetVertex(4));

                          Polygon polya(v0, v1, v2);
                          Polygon polyb(v0, v2, v4);
                          Polygon polyc(v4, v2, v3);
                          polya.SetAnnoMap(ply->GetAnnoMap());
                          polyb.SetAnnoMap(ply->GetAnnoMap());
                          polyc.SetAnnoMap(ply->GetAnnoMap());

                          node_submesh.mergePoly(polya);
                          node_submesh.mergePoly(polyb);
                          node_submesh.mergePoly(polyc);
                          break;
                        }
                        case 3:
                        case 4:
                        case 6:
                        case 7:
                        case 8: {
                          auto v0 = node_submesh.mergeVertex(poly_bak.GetVertex(0));

                          for (int ic = 2; ic < inumclippedverts; ic++) {
                            auto va = node_submesh.mergeVertex(poly_bak.GetVertex(ic - 1));
                            auto vb = node_submesh.mergeVertex(poly_bak.GetVertex(ic));
                            Polygon polya(v0, va, vb);
                            polya.SetAnnoMap(ply->GetAnnoMap());
                            node_submesh.mergePoly(polya);
                          }
                          break;
                        }
#if 0 // not yet
							case 4:
							{
								auto v0 = node_submesh.mergeVertex( poly_bak.GetVertex(0) );
								auto v1 = node_submesh.mergeVertex( poly_bak.GetVertex(1) );
								auto v2 = node_submesh.mergeVertex( poly_bak.GetVertex(2) );
								auto v3 = node_submesh.mergeVertex( poly_bak.GetVertex(3) );
								poly polya( v0, v1, v2, v3 );
								polya.SetAnnoMap(ply.GetAnnoMap());
								node_submesh.mergePoly( polya );
								break;
							}
#endif
                        default:
                          OrkAssert(false);
                          break;
                      }
                    }
        }
      }
    }
  });
  double fng = double(inumgtot) / double(inumpolys);
  orkprintf("mergemesh nodetouces avg<%d> tot<%d>\n", int(fng), inumgtot);
  orkprintf("NumFilledGrids<%d> numinners<%d> numouters<%d>\n", miNumFilledGrids, ginuminners, ginumouters);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
