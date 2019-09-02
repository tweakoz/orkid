///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <unittest++/UnitTest++.h>

namespace ork { namespace MeshUtil {

TEST(MergeVertex)
{
	toolmesh testmesh;

	MeshUtil::vertex vtx1;

	// Set only portions
	vtx1.mPos.SetXYZ(0.0f, 0.0f, 0.0f);
	vtx1.mNrm.SetXYZ(0.0f, 0.0f, 0.0f);
	vtx1.mUV[0].mMapTexCoord = fvec2(0.0f, 0.0f);
	vtx1.mCol[0].SetXYZ(0.0f, 0.0f, 0.0f);
	vtx1.mCol[1].SetXYZ(0.0f, 0.0f, 0.0f);

	MeshUtil::submesh& group = testmesh.MergeSubMesh( "default" );

	int iv1 = group.MergeVertex(vtx1);

	MeshUtil::vertex vtx2;

	// Set only portions
	vtx2.mPos.SetXYZ(0.0f, 0.0f, 0.0f);
	vtx2.mNrm.SetXYZ(0.0f, 0.0f, 0.0f);
	vtx2.mUV[0].mMapTexCoord = fvec2(0.0f, 0.0f);
	vtx2.mCol[0].SetXYZ(0.0f, 0.0f, 0.0f);
	vtx2.mCol[1].SetXYZ(0.0f, 0.0f, 0.0f);

	vtx2.mJointNames[0] = "test";
	vtx2.mJointNames[0].clear();

	int iv2 = group.MergeVertex(vtx2);

	CHECK_EQUAL(iv1, iv2);
}

} }
