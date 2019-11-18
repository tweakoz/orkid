///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyrigh 1996-2004, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid/license.html
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/meshutil/meshutil_fixedgrid.h>
#include <orktool/filter/gfx/meshutil/clusterizer.h>
#include "../meshutil/meshutil_stripper.h"

const bool gbFORCEDICE = true;
const int kDICESIZE = 512;

using namespace ork::tool;

///////////////////////////////////////////////////////////////////////////////
namespace ork::MeshUtil {
///////////////////////////////////////////////////////////////////////////////

XgmClusterizer::XgmClusterizer()
{

}

///////////////////////////////////////////////////////////////////////////////

XgmClusterizer::~XgmClusterizer()
{
	for( auto item : ClusterVect ) delete item;
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterizerStd::XgmClusterizerStd()
{
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterizerStd::~XgmClusterizerStd()
{
}

///////////////////////////////////////////////////////////////////////////////

bool XgmClusterizerStd::AddTriangle( const XgmClusterTri& Triangle, const ToolMaterialGroup* cmg )
{
	ColladaExportPolicy* policy = ColladaExportPolicy::GetContext();

	size_t iNumClusters = ClusterVect.size();

	bool bAdded = false;

	for( size_t i=0; i<iNumClusters; i++ )
	{
		XgmClusterBuilder *pClus = ClusterVect[i];
		bAdded = pClus->AddTriangle( Triangle );
		if( bAdded )
		{
			break;
		}
	}

	if( false == bAdded ) // start new cluster
	{
		XgmClusterBuilder *pNewCluster = 0;
		
		if( policy->mbIsSkinned && cmg->mMeshConfigurationFlags.mbSkinned )
		{
			pNewCluster = new XgmSkinnedClusterBuilder;
		}
		else
		{ 
			pNewCluster = new XgmRigidClusterBuilder;
		}

		ClusterVect.push_back( pNewCluster );
		return pNewCluster->AddTriangle( Triangle );
	}
	return bAdded;
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterizerDiced::XgmClusterizerDiced()
{
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterizerDiced::~XgmClusterizerDiced()
{
}

///////////////////////////////////////////////////////////////////////////////

void XgmClusterizerDiced::Begin()
{
}

///////////////////////////////////////////////////////////////////////////////

bool XgmClusterizerDiced::AddTriangle( const XgmClusterTri& Triangle, const ToolMaterialGroup* cmg )
{
	int iv0 = mPreDicedMesh.MergeVertex( Triangle.Vertex[0] );
	int iv1 = mPreDicedMesh.MergeVertex( Triangle.Vertex[1] );
	int iv2 = mPreDicedMesh.MergeVertex( Triangle.Vertex[2] );
	poly the_poly( iv0, iv1, iv2 );
	mPreDicedMesh.MergePoly( the_poly );
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void XgmClusterizerDiced::End()
{
	ColladaExportPolicy* policy = ColladaExportPolicy::GetContext();

	///////////////////////////////////////////////
	// compute ideal dice size
	///////////////////////////////////////////////

	AABox aab = mPreDicedMesh.GetAABox();
	fvec3 extents = aab.Max()-aab.Min();

#if 0

	int isize = 1<<20;
	bool bdone = false;

	while( false == bdone )
	{
		int idimX = int(extents.GetX())/isize;
		int idimY = int(extents.GetY())/isize;
		int idimZ = int(extents.GetZ())/isize;

		int inumnodes = idimX*idimY*idimZ;

		if( inumnodes > 16 )
		{
			bdone = true;
		}
		else
		{
			isize >>= 1;
			orkprintf( "idim<%d %d %d> dice size<%d>\n", idimX, idimY, idimZ, isize );
		}

	}
#else
	int isize = kDICESIZE;
	int idimX = int(extents.GetX())/isize;
	int idimY = int(extents.GetY())/isize;
	int idimZ = int(extents.GetZ())/isize;
	if( idimX==0 ) idimX=1;
	if( idimY==0 ) idimY=1;
	if( idimZ==0 ) idimZ=1;
	int inumnodes = idimX*idimY*idimZ;
#endif

	///////////////////////////////////////////////
	// END compute ideal dice size
	///////////////////////////////////////////////

	toolmesh DicedMesh;

	DicedMesh.SetMergeEdges(false);

	if( gbFORCEDICE || mPreDicedMesh.GetNumPolys() > 10000 )
	{
		float ftimeA = float(OldSchool::GetRef().GetLoResTime());

		GridGraph thegraph(isize);
		thegraph.BeginPreMerge();
			thegraph.PreMergeMesh( mPreDicedMesh );
		thegraph.EndPreMerge();
		thegraph.MergeMesh( mPreDicedMesh, DicedMesh );

		float ftimeB = float(OldSchool::GetRef().GetLoResTime());

		float ftime = (ftimeB-ftimeA);
		orkprintf( "<<PROFILE>> <<dicemesh %f seconds>>\n", ftime );

	}
	else
	{
		DicedMesh.MergeSubMesh(mPreDicedMesh);
	}
	int inumpacc = 0;

	const orklut<std::string, submesh* >& pgmap = DicedMesh.RefSubMeshLut();

	size_t inumgroups = pgmap.size();
	static int igroup = 0;

	float ftimeC = float(OldSchool::GetRef().GetLoResTime());
	for( orklut<std::string, submesh* >::const_iterator it=pgmap.begin(); it!=pgmap.end(); it++ )
	{
		const std::string& pgname = it->first;
		const submesh& pgrp = *it->second;
		int inumpolys = pgrp.GetNumPolys();

		inumpacc += inumpolys;

		XgmClusterBuilder* pNewCluster = new XgmRigidClusterBuilder;
		ClusterVect.push_back( pNewCluster );

		for( int ip=0; ip<inumpolys; ip++ )
		{
			const poly& ply = pgrp.RefPoly(ip);

			OrkAssert( ply.GetNumSides() == 3 );

			XgmClusterTri ClusTri;

			ClusTri.Vertex[0] = pgrp.RefVertexPool().GetVertex(ply.GetVertexID(0));
			ClusTri.Vertex[1] = pgrp.RefVertexPool().GetVertex(ply.GetVertexID(1));
			ClusTri.Vertex[2] = pgrp.RefVertexPool().GetVertex(ply.GetVertexID(2));

			bool bOK = pNewCluster->AddTriangle( ClusTri );

			if( false == bOK ) // cluster full, make new cluster
			{
				pNewCluster = new XgmRigidClusterBuilder;
				ClusterVect.push_back( pNewCluster );
				bOK = pNewCluster->AddTriangle( ClusTri );
				OrkAssert( bOK );
			}
		}
	}
	float ftimeD = float(OldSchool::GetRef().GetLoResTime());
	float ftime = (ftimeD-ftimeC);
	orkprintf( "<<PROFILE>> <<clusterize %f seconds>>\n", ftime );

	float favgpolyspergroup = float(inumpacc) / float(inumgroups);

	orkprintf( "dicer NumGroups<%d> AvgPolysPerGroup<%d>\n", inumgroups, int(favgpolyspergroup) );

	size_t inumclus = ClusterVect.size();
	for( size_t ic=0; ic<inumclus; ic++ )
	{
		const XgmClusterBuilder& clus = *ClusterVect[ic];
		AABox bbox = clus.mSubMesh.GetAABox();
		fvec3 vmin = bbox.Min();
		fvec3 vmax = bbox.Max();
		float fdist = (vmax-vmin).Mag();

		int inumv = (int) clus.mSubMesh.RefVertexPool().GetNumVertices();
		orkprintf( "clus<%d> inumv<%d> bbmin<%g %g %g> bbmax<%g %g %g> diag<%g>\n", ic, inumv, vmin.GetX(), vmin.GetY(), vmin.GetZ(), vmax.GetX(), vmax.GetY(), vmax.GetZ(), fdist );
	}

}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::MeshUtil
