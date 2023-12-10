////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <utpp/UnitTest++.h>

namespace ork::meshutil {

TEST(MergeVertex) {
  Mesh testmesh;

  vertex vtx1;

  // Set only portions
  vtx1.mPos = dvec3(0, 0, 0);
  vtx1.mNrm = dvec3(0, 0, 0);
  vtx1.mUV[0].mMapTexCoord = fvec2(0.0f, 0.0f);
  vtx1.mCol[0].setXYZ(0.0f, 0.0f, 0.0f);
  vtx1.mCol[1].setXYZ(0.0f, 0.0f, 0.0f);

  submesh& group = testmesh.MergeSubMesh("default");

  auto v1 = group.mergeVertex(vtx1);

  vertex vtx2;

  // Set only portions
  vtx2.mPos = dvec3(0, 0, 0);
  vtx2.mNrm = dvec3(0, 0, 0);
  vtx2.mUV[0].mMapTexCoord = fvec2(0.0f, 0.0f);
  vtx2.mCol[0].setXYZ(0.0f, 0.0f, 0.0f);
  vtx2.mCol[1].setXYZ(0.0f, 0.0f, 0.0f);

  vtx2._jointpaths[0] = "test";
  vtx2._jointpaths[0].clear();

  auto v2 = group.mergeVertex(vtx2);

  CHECK_EQUAL(v1, v2);
}

} // namespace ork::meshutil
