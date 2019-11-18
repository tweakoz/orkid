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
		poly the_poly( iv0, iv1, iv2 );
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
	poly the_poly( iv0, iv1, iv2 );
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void XgmSkinnedClusterBuilder::BuildVertexBuffer( const MeshUtil::ToolMaterialGroup& matgroup )
{
	switch( matgroup.GetVtxStreamFormat() )
	{
		case lev2::EVTXSTREAMFMT_V12N12T8I4W4: // PC skinned format
		{	BuildVertexBuffer_V12N12T8I4W4();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12B12T8I4W4: // PC binormal skinned format
		{	BuildVertexBuffer_V12N12B12T8I4W4();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N6I1T4: // WII skinned format
		{	BuildVertexBuffer_V12N6I1T4();
			break;
		}
		default:
		{
			orkerrorlog("ERROR: Unknown or unsupported vertex stream format (%s : %s)\n"
				, matgroup.mShadingGroupName.c_str()
				, matgroup._orkMaterial ? matgroup._orkMaterial->GetName().c_str() : "null");
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkinnedClusterBuilder::BuildVertexBuffer_V12N12B12T8I4W4() // binormal pc skinned
{
	lev2::GfxTargetDummy DummyTarget;
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12B12T8I4W4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	lev2::VtxWriter<ork::lev2::SVtxV12N12B12T8I4W4> vwriter;
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );

	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N12B12T8I4W4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;
		OutVtx.mBiNormal = InVtx.mUV[0].mMapBiNormal;

		const std::string& jn0 = InVtx.mJointNames[0];
		const std::string& jn1 = InVtx.mJointNames[1];
		const std::string& jn2 = InVtx.mJointNames[2];
		const std::string& jn3 = InVtx.mJointNames[3];

		int index0 = FindNewBoneIndex( jn0 );
		int index1 = FindNewBoneIndex( jn1 );
		int index2 = FindNewBoneIndex( jn2 );
		int index3 = FindNewBoneIndex( jn3 );

		index0 = (index0==-1) ? 0 : index0;
		index1 = (index1==-1) ? 0 : index1;
		index2 = (index2==-1) ? 0 : index2;
		index3 = (index3==-1) ? 0 : index3;

		OutVtx.mBoneIndices = (index0) | (index1<<8) | (index2<<16) | (index3<<24);

		fvec4 vw;
		vw.SetX(InVtx.mJointWeights[3]);
		vw.SetY(InVtx.mJointWeights[2]);
		vw.SetZ(InVtx.mJointWeights[1]);
		vw.SetW(InVtx.mJointWeights[0]);

		OutVtx.mBoneWeights = vw.GetRGBAU32();

		vwriter.AddVertex( OutVtx );

	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkinnedClusterBuilder::BuildVertexBuffer_V12N12T8I4W4() // basic pc skinned
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();

	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N12T8I4W4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12T8I4W4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{
		ork::lev2::SVtxV12N12T8I4W4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;

		const std::string& jn0 = InVtx.mJointNames[0];
		const std::string& jn1 = InVtx.mJointNames[1];
		const std::string& jn2 = InVtx.mJointNames[2];
		const std::string& jn3 = InVtx.mJointNames[3];

		int index0 = FindNewBoneIndex( jn0 );
		int index1 = FindNewBoneIndex( jn1 );
		int index2 = FindNewBoneIndex( jn2 );
		int index3 = FindNewBoneIndex( jn3 );

		index0 = (index0==-1) ? 0 : index0;
		index1 = (index1==-1) ? 0 : index1;
		index2 = (index2==-1) ? 0 : index2;
		index3 = (index3==-1) ? 0 : index3;

		OutVtx.mBoneIndices = (index0) | (index1<<8) | (index2<<16) | (index3<<24);

		fvec4 vw;
		vw.SetX(InVtx.mJointWeights[3]);
		vw.SetY(InVtx.mJointWeights[2]);
		vw.SetZ(InVtx.mJointWeights[1]);
		vw.SetW(InVtx.mJointWeights[0]);

		OutVtx.mBoneWeights = vw.GetRGBAU32();
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkinnedClusterBuilder::BuildVertexBuffer_V12N6I1T4() // basic wii skinned
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N6I1T4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N6I1T4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N6I1T4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);

		OutVtx.mX = InVtx.mPos.GetX()*kVertexScale;
		OutVtx.mY = InVtx.mPos.GetY()*kVertexScale;
		OutVtx.mZ = InVtx.mPos.GetZ()*kVertexScale;

		OutVtx.mNX = s16( InVtx.mNrm.GetX() * float(32767.0f) );
		OutVtx.mNY = s16( InVtx.mNrm.GetY() * float(32767.0f) );
		OutVtx.mNZ = s16( InVtx.mNrm.GetZ() * float(32767.0f) );

		OutVtx.mU = s16( InVtx.mUV[0].mMapTexCoord.GetX() * float(1024.0f) );
		OutVtx.mV = s16( InVtx.mUV[0].mMapTexCoord.GetY() * float(1024.0f) );

		///////////////////////////////////////

		const std::string& jn0 = InVtx.mJointNames[0];
		const std::string& jn1 = InVtx.mJointNames[1];
		const std::string& jn2 = InVtx.mJointNames[2];
		const std::string& jn3 = InVtx.mJointNames[3];

		int index0 = FindNewBoneIndex( jn0 );
		int index1 = FindNewBoneIndex( jn1 );
		int index2 = FindNewBoneIndex( jn2 );
		int index3 = FindNewBoneIndex( jn3 );

		index0 = (index0==-1) ? 0 : index0;
		index1 = (index1==-1) ? 0 : index1;
		index2 = (index2==-1) ? 0 : index2;
		index3 = (index3==-1) ? 0 : index3;

		orkset<int> BoneSet;
		BoneSet.insert(index0);
		BoneSet.insert(index1);
		BoneSet.insert(index2);
		BoneSet.insert(index3);

		OrkAssertI(BoneSet.size()==1, "Sorry, wii does not support hardware weighting!!!" );
		OrkAssertI(index0<8, "Sorry, wii only has 8 matrix registers!!!" );

		OutVtx.mBone = u8(index0);
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////

void XgmRigidClusterBuilder::BuildVertexBuffer( const MeshUtil::ToolMaterialGroup& matgroup )
{
	switch( matgroup.GetVtxStreamFormat() )
	{
		case lev2::EVTXSTREAMFMT_V12N6C2T4: // basic wii environmen
		{	BuildVertexBuffer_V12N6C2T4();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12B12T8C4: // basic pc environment
		{	BuildVertexBuffer_V12N12B12T8C4();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12B12T16: // basic pc environment
		{	BuildVertexBuffer_V12N12B12T16();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12T16C4: // basic pc environment
		{	BuildVertexBuffer_V12N12T16C4();
			break;
		}
		default:
		{	OrkAssert(false);
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void XgmRigidClusterBuilder::BuildVertexBuffer_V12N6C2T4() // basic wii environment
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N6C2T4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N6C2T4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N6C2T4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);

		OutVtx.mX = InVtx.mPos.GetX()*kVertexScale;
		OutVtx.mY = InVtx.mPos.GetY()*kVertexScale;
		OutVtx.mZ = InVtx.mPos.GetZ()*kVertexScale;

		OutVtx.mNX = s16( InVtx.mNrm.GetX() * float(32767.0f) );
		OutVtx.mNY = s16( InVtx.mNrm.GetY() * float(32767.0f) );
		OutVtx.mNZ = s16( InVtx.mNrm.GetZ() * float(32767.0f) );

		OutVtx.mU = s16( InVtx.mUV[0].mMapTexCoord.GetX() * float(1024.0f) );
		OutVtx.mV = s16( InVtx.mUV[0].mMapTexCoord.GetY() * float(1024.0f) );

		int ir = int(InVtx.mCol[0].GetY()*255.0f);
		int ig = int(InVtx.mCol[0].GetZ()*255.0f);
		int ib = int(InVtx.mCol[0].GetW()*255.0f);

		OutVtx.mColor = U16(((ir>>3)<<11)|((ig>>2)<<5)|((ib>>3)<<0));
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////

void XgmRigidClusterBuilder::BuildVertexBuffer_V12N12B12T8C4() // basic pc environment
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N12B12T8C4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12B12T8C4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N12B12T8C4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;
		OutVtx.mBiNormal = InVtx.mUV[0].mMapBiNormal;
		OutVtx.mColor = InVtx.mCol[0].GetRGBAU32();
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////

void XgmRigidClusterBuilder::BuildVertexBuffer_V12N12T16C4() // basic pc environment
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N12T16C4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12T16C4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N12T16C4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mUV1 = InVtx.mUV[1].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;
		OutVtx.mColor = InVtx.mCol[0].GetRGBAU32();
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////

void XgmRigidClusterBuilder::BuildVertexBuffer_V12N12B12T16() // basic pc environment
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N12B12T16> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12B12T16>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N12B12T16 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mUV1 = InVtx.mUV[1].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;
		OutVtx.mBiNormal = InVtx.mUV[0].mMapBiNormal;
		//OutVtx.mColor = InVtx.mCol[0].GetRGBAU32();
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////

void BuildXgmClusterPrimGroups( lev2::XgmCluster & XgmCluster, const std::vector<unsigned int> & TriangleIndices )
{
	lev2::GfxTargetDummy DummyTarget;

	const int imaxvtx = XgmCluster.mpVertexBuffer->GetNumVertices();

	const ColladaExportPolicy* policy = ColladaExportPolicy::GetContext();
	// TODO: Is this correct? Why?
	static const int WII_PRIM_GROUP_MAX_INDICES = 0xFFFF;

	////////////////////////////////////////////////////////////
	// Build TriStrips

	MeshUtil::TriStripper MyStripper( TriangleIndices, 16, 4 );

	bool bhastris = (MyStripper.GetTriIndices().size()>0);

	int inumstripgroups = MyStripper.GetStripGroups().size();

	bool bhasstrips = (inumstripgroups>0);

	int inumpg = inumstripgroups + int(bhastris);

	////////////////////////////////////////////////////////////
	// Create PrimGroups

	XgmCluster.mpPrimGroups = new ork::lev2::XgmPrimGroup[ inumpg ];
	XgmCluster.miNumPrimGroups = inumpg;

	////////////////////////////////////////////////////////////

	int ipg = 0;

	////////////////////////////////////////////////////////////
	if( bhasstrips )
	////////////////////////////////////////////////////////////
	{
		const orkvector<MeshUtil::TriStripperPrimGroup>& StripGroups = MyStripper.GetStripGroups();
		for( int i=0; i<inumstripgroups; i++ )
		{
			const orkvector<unsigned int>& StripIndices = MyStripper.GetStripIndices(i);
			int inumidx = StripIndices.size();

			/////////////////////////////////
			// check index buffer size policy
			//  (some platforms do not have 32bit indices)
			/////////////////////////////////

			if(ColladaExportPolicy::GetContext()->mPrimGroupPolicy.mMaxIndices == ColladaPrimGroupPolicy::EPOLICY_MAXINDICES_WII)
			{
				if(inumidx > WII_PRIM_GROUP_MAX_INDICES)
				{
					orkerrorlog("ERROR: <%s> Wii prim group max indices exceeded: %d\n", policy->mColladaOutName.c_str(), inumidx);
					throw std::exception();
				}
			}

			/////////////////////////////////

			ork::lev2::StaticIndexBuffer<U16> *pidxbuf = new ork::lev2::StaticIndexBuffer<U16>(inumidx);
			U16 *pidx = (U16*) DummyTarget.GBI()->LockIB( *pidxbuf );
			OrkAssert(pidx!=0);
			{
				for( int ii=0; ii<inumidx; ii++ )
				{
					int index = StripIndices[ii];
					OrkAssert(index<imaxvtx);
					pidx[ii] = U16(index);
				}
			}
			DummyTarget.GBI()->UnLockIB( *pidxbuf );

			/////////////////////////////////

			ork::lev2::XgmPrimGroup & StripGroup = XgmCluster.mpPrimGroups[ ipg++ ];

			StripGroup.miNumIndices = inumidx;
			StripGroup.mpIndices = pidxbuf;
			StripGroup.mePrimType = lev2::EPRIM_TRIANGLESTRIP;
		}
	}

	////////////////////////////////////////////////////////////
	if( bhastris )
	////////////////////////////////////////////////////////////
	{
		int inumidx = MyStripper.GetTriIndices().size();

		/////////////////////////////////////////////////////
		ork::lev2::StaticIndexBuffer<U16> *pidxbuf = new ork::lev2::StaticIndexBuffer<U16>(inumidx);
		U16 *pidx = (U16*) DummyTarget.GBI()->LockIB( *pidxbuf );
		OrkAssert(pidx!=0);
		for( int ii=0; ii<inumidx; ii++ )
		{
			pidx[ii] = U16(MyStripper.GetTriIndices()[ii]);
		}
		DummyTarget.GBI()->UnLockIB( *pidxbuf );
		/////////////////////////////////////////////////////

		ork::lev2::XgmPrimGroup & StripGroup = XgmCluster.mpPrimGroups[ ipg++ ];

		if(ColladaExportPolicy::GetContext()->mPrimGroupPolicy.mMaxIndices == ColladaPrimGroupPolicy::EPOLICY_MAXINDICES_WII)
			if(inumidx > WII_PRIM_GROUP_MAX_INDICES)
			{
				orkerrorlog("ERROR: <%s> Wii prim group max indices exceeded: %d\n", policy->mColladaOutName.c_str(), inumidx);
				throw std::exception();
			}

		StripGroup.miNumIndices = inumidx;
		StripGroup.mpIndices = pidxbuf;
		StripGroup.mePrimType = lev2::EPRIM_TRIANGLES;

	}
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::MeshUtil
