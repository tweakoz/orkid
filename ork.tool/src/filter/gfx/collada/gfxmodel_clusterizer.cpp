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

const bool gbFORCEDICE = true;
const int kDICESIZE = 512;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
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

bool XgmClusterizerStd::AddTriangle( const XgmClusterTri& Triangle, const SColladaMatGroup* cmg )
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

bool XgmClusterizerDiced::AddTriangle( const XgmClusterTri& Triangle, const SColladaMatGroup* cmg )
{
	int iv0 = mPreDicedMesh.MergeVertex( Triangle.Vertex[0] );
	int iv1 = mPreDicedMesh.MergeVertex( Triangle.Vertex[1] );
	int iv2 = mPreDicedMesh.MergeVertex( Triangle.Vertex[2] );
	ork::MeshUtil::poly the_poly( iv0, iv1, iv2 );
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

	ork::MeshUtil::toolmesh DicedMesh;

	DicedMesh.SetMergeEdges(false);

	if( gbFORCEDICE || mPreDicedMesh.GetNumPolys() > 10000 )
	{
		float ftimeA = float(CSystem::GetRef().GetLoResTime());

		ork::MeshUtil::GridGraph thegraph(isize);
		thegraph.BeginPreMerge();
			thegraph.PreMergeMesh( mPreDicedMesh );
		thegraph.EndPreMerge();
		thegraph.MergeMesh( mPreDicedMesh, DicedMesh );

		float ftimeB = float(CSystem::GetRef().GetLoResTime());

		float ftime = (ftimeB-ftimeA);
		orkprintf( "<<PROFILE>> <<dicemesh %f seconds>>\n", ftime );

	}
	else
	{
		DicedMesh.MergeSubMesh(mPreDicedMesh);
	}
	int inumpacc = 0;

	const orklut<std::string, ork::MeshUtil::submesh* >& pgmap = DicedMesh.RefSubMeshLut();

	size_t inumgroups = pgmap.size();
	static int igroup = 0;

	float ftimeC = float(CSystem::GetRef().GetLoResTime());
	for( orklut<std::string, ork::MeshUtil::submesh* >::const_iterator it=pgmap.begin(); it!=pgmap.end(); it++ )
	{
		const std::string& pgname = it->first;
		const ork::MeshUtil::submesh& pgrp = *it->second;
		int inumpolys = pgrp.GetNumPolys();

		inumpacc += inumpolys;

		XgmClusterBuilder* pNewCluster = new XgmRigidClusterBuilder;
		ClusterVect.push_back( pNewCluster );

		for( int ip=0; ip<inumpolys; ip++ )
		{
			const ork::MeshUtil::poly& ply = pgrp.RefPoly(ip);

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
	float ftimeD = float(CSystem::GetRef().GetLoResTime());
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

XgmClusterBuilder::XgmClusterBuilder() : mpVertexBuffer(NULL)
{
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterBuilder::~XgmClusterBuilder()
{

}

///////////////////////////////////////////////////////////////////////////////

int XgmSkinnedClusterBuilder::FindNewBoneIndex( const std::string & BoneName )
{	int rval = -1;
	orkmap<std::string,int>::const_iterator itBONE = mmBoneRegMap.find( BoneName );
	if( mmBoneRegMap.end()!=itBONE )
	{	rval = (*itBONE).second;
	}
//	OrkAssert( rval>=0 );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool XgmSkinnedClusterBuilder::AddTriangle( const XgmClusterTri& Triangle )
{
	///////////////////////////////////////
	// make sure triangle will absolutely fit in the vertex buffer
	///////////////////////////////////////

	size_t ivcount = mSubMesh.RefVertexPool().GetNumVertices();

	static const size_t kvtresh = (2<<16)-4;

	if( ivcount > kvtresh )
	{
		return false;
	}

	///////////////////////////////////////
	// make sure triangle will absolutely fit in the vertex buffer
	///////////////////////////////////////

	bool bAddTriangle = false;
	const int kMaxBonesPerCluster = ColladaExportPolicy::GetContext()->miNumBonesPerCluster;
	orkset<std::string> AddThisRun;
	for( int i=0; i<3; i++ )
	{	int inumw = Triangle.Vertex[i].miNumWeights;
		for( int iw=0; iw<inumw; iw++ )
		{	const std::string & BoneName = Triangle.Vertex[i].mJointNames[iw];
			bool IsBoneResidentInClusterAlready = mmBoneRegMap.find(BoneName) != mmBoneRegMap.end();
			if( IsBoneResidentInClusterAlready )
			{
			}
			else if( AddThisRun.find(BoneName) == AddThisRun.end() )
			{
				AddThisRun.insert(BoneName);
			}
		}
	}
	size_t NumBonesToAllocate = AddThisRun.size();
	if( 0 == NumBonesToAllocate )
	{	bAddTriangle=true;
	}
	else
	{	size_t NumBonesAlreadyAllocated = mmBoneRegMap.size();
		size_t NumBonesFreeInCluster = (size_t) kMaxBonesPerCluster - NumBonesAlreadyAllocated;
		if( NumBonesFreeInCluster <= 0 )
		{	//orkprintf( "Current Cluster [%08x] Is Full\n", this );
			return false;
		}
		else if( NumBonesToAllocate <= NumBonesFreeInCluster )
		{	for( orkset<std::string>::const_iterator it = AddThisRun.begin(); it!=AddThisRun.end(); it++ )
			{	const std::string & BoneName = *it;
				int iBoneREG = (int) mmBoneRegMap.size();
				//orkprintf( "SKIN: <Cluster %08x> <Adding New Bone %d> <Reg%02d> <Bone:%s>\n", this, AddThisRun.size(), iBoneREG, BoneName.c_str() );
				if( mmBoneRegMap.find(BoneName) == mmBoneRegMap.end() )
				{	std::pair<std::string,int> NewBone(BoneName,iBoneREG);
					mmBoneRegMap.insert(NewBone);
					//orkprintf( "Cluster[%08x] Adding BoneRec [Reg%02d:Bone:%s]\n", this, iBoneREG, BoneName.c_str() );
				}
			}
			bAddTriangle=true;
		}
	}
	if( bAddTriangle )
	{	int iv0 = mSubMesh.MergeVertex( Triangle.Vertex[0] );
		int iv1 = mSubMesh.MergeVertex( Triangle.Vertex[1] );
		int iv2 = mSubMesh.MergeVertex( Triangle.Vertex[2] );
		ork::MeshUtil::poly the_poly( iv0, iv1, iv2 );
		mSubMesh.MergePoly( the_poly );
	}
	return bAddTriangle;
}

///////////////////////////////////////////////////////////////////////////////

bool XgmRigidClusterBuilder::AddTriangle( const XgmClusterTri& Triangle )
{
	size_t ivcount = mSubMesh.RefVertexPool().GetNumVertices();
	int iicount = (int) mSubMesh.GetNumPolys();

	static const int kvtresh = (1<<16)-4;
	static const int kithresh = (1<<16)/3;

	if( ivcount > kvtresh )
	{
		return false;
	}
	if( iicount > kithresh )
	{
		return false;
	}

	int iv0 = mSubMesh.MergeVertex( Triangle.Vertex[0] );
	int iv1 = mSubMesh.MergeVertex( Triangle.Vertex[1] );
	int iv2 = mSubMesh.MergeVertex( Triangle.Vertex[2] );
	ork::MeshUtil::poly the_poly( iv0, iv1, iv2 );
	mSubMesh.MergePoly( the_poly );

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void XgmClusterBuilder::Dump( void )
{
	/*orkprintf( "[CLUSDUMP] Cluster[%08x] NUBI %02d\n", this, GetNumUniqueBonIndices() );
	int iNumBones = mmBoneRegMap.size();
	orkprintf( "/////////////////////////////////////////\n" );
	orkprintf( "[CLUSDUMP] [Cluster %08x] [NumBones %d]\n", this, iNumBones );
	orkprintf( "//////////////////\n" );
	orkprintf( "[CLUSDUMP] " );
	/////////////////////////////////////////////////////////////////
	static int RegMap[256];
	for( orkmap<int,int>::const_iterator it=mmBoneRegMap.begin(); it!=mmBoneRegMap.end(); it++ )
	{	std::pair<int,int> BoneMapItem = *it;
		int BoneIDX = BoneMapItem.first;
		int BoneREG = BoneMapItem.second;
		RegMap[ BoneREG ] = BoneIDX;
		orkprintf( " B%02d:R%02d", BoneMapItem.first, BoneMapItem.second );
	}
	orkprintf( "\n[CLUSDUMP] " );
	////////////////////////////////////////////////////////////////
	for( int r=0; r<iNumBones; r++ )
	{	orkprintf( " R%02d:B%02d", r, RegMap[r] );
	}
	orkprintf( "\n//////////////////\n" );*/
}

} }
