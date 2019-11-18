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

namespace ork::MeshUtil {
///////////////////////////////////////////////////////////////////////////////

XgmClusterBuilder::XgmClusterBuilder() : _vertexBuffer(NULL)
{
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterBuilder::~XgmClusterBuilder()
{

}

///////////////////////////////////////////////////////////////////////////////

void XgmClusterBuilder::Dump( void )
{
	/*orkprintf( "[CLUSDUMP] Cluster[%08x] NUBI %02d\n", this, GetNumUniqueBonIndices() );
	int iNumBones = _boneRegisterMap.size();
	orkprintf( "/////////////////////////////////////////\n" );
	orkprintf( "[CLUSDUMP] [Cluster %08x] [NumBones %d]\n", this, iNumBones );
	orkprintf( "//////////////////\n" );
	orkprintf( "[CLUSDUMP] " );
	/////////////////////////////////////////////////////////////////
	static int RegMap[256];
	for( orkmap<int,int>::const_iterator it=_boneRegisterMap.begin(); it!=_boneRegisterMap.end(); it++ )
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

void BuildXgmClusterPrimGroups( lev2::XgmCluster & XgmCluster, const std::vector<unsigned int> & TriangleIndices )
{
	lev2::GfxTargetDummy DummyTarget;

	const int imaxvtx = XgmCluster._vertexBuffer->GetNumVertices();

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
} //namespace ork::MeshUtil {
