////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <utpp/UnitTest++.h>

namespace ork::meshutil {

TEST(MergeVertex) {
  Mesh testmesh;

  vertex vtx1;

  // Set only portions
  vtx1.mPos = fvec3(0.0f, 0.0f, 0.0f);
  vtx1.mNrm = fvec3(0.0f, 0.0f, 0.0f);
  vtx1.mUV[0].mMapTexCoord = fvec2(0.0f, 0.0f);
  vtx1.mCol[0].setXYZ(0.0f, 0.0f, 0.0f);
  vtx1.mCol[1].setXYZ(0.0f, 0.0f, 0.0f);

  submesh& group = testmesh.MergeSubMesh("default");

  auto v1 = group.newMergeVertex(vtx1);

  vertex vtx2;

  // Set only portions
  vtx2.mPos = fvec3(0.0f, 0.0f, 0.0f);
  vtx2.mNrm = fvec3(0.0f, 0.0f, 0.0f);
  vtx2.mUV[0].mMapTexCoord = fvec2(0.0f, 0.0f);
  vtx2.mCol[0].setXYZ(0.0f, 0.0f, 0.0f);
  vtx2.mCol[1].setXYZ(0.0f, 0.0f, 0.0f);

  vtx2.mJointNames[0] = "test";
  vtx2.mJointNames[0].clear();

  auto v2 = group.newMergeVertex(vtx2);

  CHECK_EQUAL(v1, v2);
}

} // namespace ork::meshutil
