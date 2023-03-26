////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/meshutil/meshutil.h>

namespace ork { namespace meshutil {
///////////////////////////////////////////////////////////////////////////////

struct GridNode {
  // Mesh NodeMesh;
  std::string GridNodeName;
};

struct GridAddr {
  int ix, iy, iz;
  GridAddr()
      : ix(0)
      , iy(0)
      , iz(0) {
  }
};

class GridGraph {
public:
  ////////////////////////////////////////////////////

  GridNode** mppGrids = nullptr;

  const int kfixedgridsize;

  int miDimX;
  int miDimY;
  int miDimZ;
  int miNumGrids;
  int miNumFilledGrids;

  float areamin;
  float areamax;
  float areaavg;
  float areatot;
  int totpolys;

  fvec3 vsize;
  fvec3 vmin;
  fvec3 vmax;
  fvec3 vctr;
  AABox maab;
  fmtx4 mMtxWorldToGrid;

  ////////////////////////////////////////////////////

  GridGraph(const int fgsize = 256);
  ~GridGraph();
  void BeginPreMerge();
  void PreMergeMesh(const submesh& MeshIn);
  GridNode* GetGridNode(const GridAddr& addr);
  void SetGridNode(const GridAddr& addr, GridNode* node);
  GridAddr GetGridAddress(const fvec3& v);
  void GetCuttingPlanes(
      const GridAddr& addr,
      fplane3& topplane,
      fplane3& botplane,
      fplane3& lftplane,
      fplane3& rgtplane,
      fplane3& frnplane,
      fplane3& bakplane);

  void EndPreMerge();
  void MergeMesh(const submesh& MeshIn, Mesh& MeshOut);

  ////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::meshutil
