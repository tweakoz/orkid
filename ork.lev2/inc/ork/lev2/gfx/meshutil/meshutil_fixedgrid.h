////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

struct mupoly_clip_adapter {
  typedef vertex VertexType;

  orkvector<VertexType> mVerts;

  mupoly_clip_adapter() {
  }

  void AddVertex(const VertexType& v) {
    mVerts.push_back(v);
  }
  const VertexType& GetVertex(int idx) const {
    return mVerts[idx];
  }

  size_t GetNumVertices() const {
    return mVerts.size();
  }
};

class GridGraph {
public:
  ////////////////////////////////////////////////////

  int miDimX;
  int miDimY;
  int miDimZ;
  int miNumGrids;
  int miNumFilledGrids;

  GridNode** mppGrids;

  fvec3 vsize;
  fvec3 vmin;
  fvec3 vmax;
  fvec3 vctr;
  AABox maab;
  float areamax;
  float areamin;
  float areaavg;
  float areatot;
  int totpolys;
  fmtx4 mMtxWorldToGrid;
  const int kfixedgridsize;

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
